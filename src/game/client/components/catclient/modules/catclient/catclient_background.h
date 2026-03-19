#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_BACKGROUND_H

void CCatClient::EnsureCustomBackgroundFolder() const
{
	Storage()->CreateFolder("catclient", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("catclient/backgrounds", IStorage::TYPE_SAVE);
}

bool CCatClient::LoadStillFfmpegBackground(IOHANDLE File, const char *pTextureName)
{
#if !defined(CONF_VIDEORECORDER)
	if(File)
	{
		io_close(File);
	}
	log_error("catclient/gif", "ffmpeg image fallback is not available in this build. filename='%s'", pTextureName);
	return false;
#else
	void *pFileData = nullptr;
	CAvByteBufferReader Reader;
	AVIOContext *pAvioContext = nullptr;
	AVFormatContext *pFormatContext = nullptr;
	AVCodecContext *pCodecContext = nullptr;
	SwsContext *pSwsContext = nullptr;
	AVPacket *pPacket = nullptr;
	AVFrame *pFrame = nullptr;

	const auto Cleanup = [&]() {
		av_packet_free(&pPacket);
		av_frame_free(&pFrame);
		sws_freeContext(pSwsContext);
		avcodec_free_context(&pCodecContext);
		avformat_close_input(&pFormatContext);
		if(pAvioContext != nullptr)
		{
			av_freep(&pAvioContext->buffer);
		}
		avio_context_free(&pAvioContext);
		free(pFileData);
	};

	if(!OpenFfmpegInputFromFile(File, pTextureName, nullptr, &pFileData, &pAvioContext, &pFormatContext, Reader))
	{
		Cleanup();
		return false;
	}

	int Result = avformat_find_stream_info(pFormatContext, nullptr);
	if(Result < 0)
	{
		LogBackgroundDecodeError("reading image stream info", pTextureName, Result);
		Cleanup();
		return false;
	}

	const int StreamIndex = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if(StreamIndex < 0)
	{
		LogBackgroundDecodeError("finding image video stream", pTextureName, StreamIndex);
		Cleanup();
		return false;
	}

	const AVStream *pStream = pFormatContext->streams[StreamIndex];
	const AVCodec *pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
	if(pCodec == nullptr)
	{
		log_error("catclient/gif", "failed to find image decoder. filename='%s' codec='%s'", pTextureName, avcodec_get_name(pStream->codecpar->codec_id));
		Cleanup();
		return false;
	}

	pCodecContext = avcodec_alloc_context3(pCodec);
	if(pCodecContext == nullptr)
	{
		log_error("catclient/gif", "failed to allocate image decoder context. filename='%s'", pTextureName);
		Cleanup();
		return false;
	}

	Result = avcodec_parameters_to_context(pCodecContext, pStream->codecpar);
	if(Result < 0)
	{
		LogBackgroundDecodeError("copying image codec parameters", pTextureName, Result);
		Cleanup();
		return false;
	}

	Result = avcodec_open2(pCodecContext, pCodec, nullptr);
	if(Result < 0)
	{
		LogBackgroundDecodeError("opening image decoder", pTextureName, Result);
		Cleanup();
		return false;
	}

	pPacket = av_packet_alloc();
	pFrame = av_frame_alloc();
	if(pPacket == nullptr || pFrame == nullptr)
	{
		log_error("catclient/gif", "failed to allocate image decode buffers. filename='%s'", pTextureName);
		Cleanup();
		return false;
	}

	bool GotFrame = false;
	while((Result = av_read_frame(pFormatContext, pPacket)) >= 0 && !GotFrame)
	{
		if(pPacket->stream_index == StreamIndex)
		{
			Result = avcodec_send_packet(pCodecContext, pPacket);
			if(Result < 0)
			{
				LogBackgroundDecodeError("sending image packet to decoder", pTextureName, Result);
				av_packet_unref(pPacket);
				Cleanup();
				return false;
			}

			while(!GotFrame)
			{
				Result = avcodec_receive_frame(pCodecContext, pFrame);
				if(Result == 0)
				{
					GotFrame = true;
					break;
				}
				if(Result == AVERROR(EAGAIN) || Result == AVERROR_EOF)
				{
					break;
				}

				LogBackgroundDecodeError("decoding image frame", pTextureName, Result);
				av_packet_unref(pPacket);
				Cleanup();
				return false;
			}
		}
		av_packet_unref(pPacket);
	}

	if(!GotFrame)
	{
		Result = avcodec_send_packet(pCodecContext, nullptr);
		if(Result >= 0)
		{
			Result = avcodec_receive_frame(pCodecContext, pFrame);
			GotFrame = Result == 0;
		}
		if(!GotFrame)
		{
			if(Result < 0 && Result != AVERROR_EOF && Result != AVERROR(EAGAIN))
			{
				LogBackgroundDecodeError("flushing image decoder", pTextureName, Result);
			}
			else
			{
				log_error("catclient/gif", "failed to decode an image frame. filename='%s'", pTextureName);
			}
			Cleanup();
			return false;
		}
	}

	if(pFrame->width <= 0 || pFrame->height <= 0)
	{
		log_error("catclient/gif", "decoded image has invalid size. filename='%s' width=%d height=%d", pTextureName, pFrame->width, pFrame->height);
		Cleanup();
		return false;
	}

	pSwsContext = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if(pSwsContext == nullptr)
	{
		log_error("catclient/gif", "failed to create image color conversion context. filename='%s'", pTextureName);
		Cleanup();
		return false;
	}

	CImageInfo Image;
	Image.m_Width = pFrame->width;
	Image.m_Height = pFrame->height;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = static_cast<uint8_t *>(malloc(Image.DataSize()));
	if(Image.m_pData == nullptr)
	{
		log_error("catclient/gif", "out of memory while loading '%s'", pTextureName);
		Cleanup();
		return false;
	}

	uint8_t *apDestData[4] = {Image.m_pData, nullptr, nullptr, nullptr};
	int aDestLineSize[4] = {(int)(Image.m_Width * Image.PixelSize()), 0, 0, 0};
	Result = sws_scale(pSwsContext, pFrame->data, pFrame->linesize, 0, pFrame->height, apDestData, aDestLineSize);
	if(Result != pFrame->height)
	{
		log_error("catclient/gif", "failed to convert decoded image. filename='%s' result=%d", pTextureName, Result);
		Image.Free();
		Cleanup();
		return false;
	}

	UnloadCustomBackgroundTexture();
	m_CustomBackgroundImageSize = vec2((float)Image.m_Width, (float)Image.m_Height);
	m_CustomBackgroundTexture = Graphics()->LoadTextureRawMove(Image, 0, pTextureName);
	if(m_CustomBackgroundTexture.IsNullTexture())
	{
		Image.Free();
		m_CustomBackgroundImageSize = vec2(0.0f, 0.0f);
		Cleanup();
		return false;
	}

	m_HasCustomBackgroundTexture = true;
	Cleanup();
	return true;
#endif
}

bool CCatClient::LoadAnimatedBackground(IOHANDLE File, const char *pTextureName, const char *pInputFormatName)
{
#if !defined(CONF_VIDEORECORDER)
	if(File)
	{
		io_close(File);
	}
	log_error("catclient/gif", "animated background support is not available in this build. filename='%s'", pTextureName);
	return false;
#else
	bool Success = false;
	std::vector<SCustomBackgroundFrame> vFrames;
	std::vector<std::chrono::nanoseconds> vFrameTimestamps;
	vec2 ImageSize = vec2(0.0f, 0.0f);
	void *pFileData = nullptr;
	CAvByteBufferReader Reader;
	AVIOContext *pAvioContext = nullptr;
	AVFormatContext *pFormatContext = nullptr;
	AVCodecContext *pCodecContext = nullptr;
	SwsContext *pSwsContext = nullptr;
	AVPacket *pPacket = nullptr;
	AVFrame *pFrame = nullptr;
	int ConvertedWidth = 0;
	int ConvertedHeight = 0;
	AVPixelFormat ConvertedFormat = AV_PIX_FMT_NONE;

	const auto Cleanup = [&]() {
		av_packet_free(&pPacket);
		av_frame_free(&pFrame);
		sws_freeContext(pSwsContext);
		avcodec_free_context(&pCodecContext);
		avformat_close_input(&pFormatContext);
		if(pAvioContext != nullptr)
		{
			av_freep(&pAvioContext->buffer);
		}
		avio_context_free(&pAvioContext);
		free(pFileData);
		if(!Success)
		{
			for(auto &Frame : vFrames)
			{
				Graphics()->UnloadTexture(&Frame.m_Texture);
			}
		}
	};

	if(!OpenFfmpegInputFromFile(File, pTextureName, pInputFormatName, &pFileData, &pAvioContext, &pFormatContext, Reader))
	{
		Cleanup();
		return false;
	}

	int Result = avformat_find_stream_info(pFormatContext, nullptr);
	if(Result < 0)
	{
		LogBackgroundDecodeError("reading gif stream info", pTextureName, Result);
		Cleanup();
		return false;
	}

	const int StreamIndex = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if(StreamIndex < 0)
	{
		LogBackgroundDecodeError("finding gif video stream", pTextureName, StreamIndex);
		Cleanup();
		return false;
	}

	const AVStream *pStream = pFormatContext->streams[StreamIndex];
	const AVCodec *pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
	if(pCodec == nullptr)
	{
		log_error("catclient/gif", "failed to find decoder. filename='%s' codec='%s'", pTextureName, avcodec_get_name(pStream->codecpar->codec_id));
		Cleanup();
		return false;
	}

	pCodecContext = avcodec_alloc_context3(pCodec);
	if(pCodecContext == nullptr)
	{
		log_error("catclient/gif", "failed to allocate decoder context. filename='%s'", pTextureName);
		Cleanup();
		return false;
	}

	Result = avcodec_parameters_to_context(pCodecContext, pStream->codecpar);
	if(Result < 0)
	{
		LogBackgroundDecodeError("copying gif codec parameters", pTextureName, Result);
		Cleanup();
		return false;
	}

	pCodecContext->thread_count = 1;
	Result = avcodec_open2(pCodecContext, pCodec, nullptr);
	if(Result < 0)
	{
		LogBackgroundDecodeError("opening gif decoder", pTextureName, Result);
		Cleanup();
		return false;
	}

	pPacket = av_packet_alloc();
	pFrame = av_frame_alloc();
	if(pPacket == nullptr || pFrame == nullptr)
	{
		log_error("catclient/gif", "failed to allocate decode buffers. filename='%s'", pTextureName);
		Cleanup();
		return false;
	}

	const auto DecodeAvailableFrames = [&](const AVPacket *pDurationPacket) {
		while(true)
		{
			Result = avcodec_receive_frame(pCodecContext, pFrame);
			if(Result == 0)
			{
				if(pFrame->width <= 0 || pFrame->height <= 0)
				{
					log_error("catclient/gif", "decoded frame has invalid size. filename='%s' width=%d height=%d", pTextureName, pFrame->width, pFrame->height);
					return false;
				}

				if(pSwsContext == nullptr || ConvertedWidth != pFrame->width || ConvertedHeight != pFrame->height || ConvertedFormat != (AVPixelFormat)pFrame->format)
				{
					sws_freeContext(pSwsContext);
					pSwsContext = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
					if(pSwsContext == nullptr)
					{
						log_error("catclient/gif", "failed to create color conversion context. filename='%s'", pTextureName);
						return false;
					}
					ConvertedWidth = pFrame->width;
					ConvertedHeight = pFrame->height;
					ConvertedFormat = (AVPixelFormat)pFrame->format;
				}

				CImageInfo Image;
				Image.m_Width = pFrame->width;
				Image.m_Height = pFrame->height;
				Image.m_Format = CImageInfo::FORMAT_RGBA;
				Image.m_pData = static_cast<uint8_t *>(malloc(Image.DataSize()));
				if(Image.m_pData == nullptr)
				{
					log_error("catclient/gif", "out of memory while loading '%s'", pTextureName);
					return false;
				}

				uint8_t *apDestData[4] = {Image.m_pData, nullptr, nullptr, nullptr};
				int aDestLineSize[4] = {(int)(Image.m_Width * Image.PixelSize()), 0, 0, 0};
				Result = sws_scale(pSwsContext, pFrame->data, pFrame->linesize, 0, pFrame->height, apDestData, aDestLineSize);
				if(Result != pFrame->height)
				{
					log_error("catclient/gif", "failed to convert decoded frame. filename='%s' result=%d", pTextureName, Result);
					Image.Free();
					return false;
				}

				const std::chrono::nanoseconds Timestamp = FrameTimestamp(pFrame, pStream->time_base);
				const std::chrono::nanoseconds Duration = GifFrameDuration(pFrame, pDurationPacket, pStream->time_base);
				SCustomBackgroundFrame Frame;
				Frame.m_Duration = Duration;
				Frame.m_Texture = Graphics()->LoadTextureRawMove(Image, 0, pTextureName);
				if(Frame.m_Texture.IsNullTexture())
				{
					Image.Free();
					log_error("catclient/gif", "failed to upload gif frame texture. filename='%s'", pTextureName);
					return false;
				}

				if(ImageSize == vec2(0.0f, 0.0f))
				{
					ImageSize = vec2((float)pFrame->width, (float)pFrame->height);
				}
				vFrames.emplace_back(std::move(Frame));
				vFrameTimestamps.emplace_back(Timestamp);
			}
			else if(Result == AVERROR(EAGAIN) || Result == AVERROR_EOF)
			{
				return true;
			}
			else
			{
				LogBackgroundDecodeError("decoding gif frame", pTextureName, Result);
				return false;
			}
		}
	};

	while((Result = av_read_frame(pFormatContext, pPacket)) >= 0)
	{
		if(pPacket->stream_index == StreamIndex)
		{
			Result = avcodec_send_packet(pCodecContext, pPacket);
			if(Result < 0)
			{
				LogBackgroundDecodeError("sending gif packet to decoder", pTextureName, Result);
				av_packet_unref(pPacket);
				Cleanup();
				return false;
			}

			if(!DecodeAvailableFrames(pPacket))
			{
				av_packet_unref(pPacket);
				Cleanup();
				return false;
			}
		}
		av_packet_unref(pPacket);
	}

	if(Result != AVERROR_EOF)
	{
		LogBackgroundDecodeError("reading gif packets", pTextureName, Result);
		Cleanup();
		return false;
	}

	Result = avcodec_send_packet(pCodecContext, nullptr);
	if(Result < 0)
	{
		LogBackgroundDecodeError("flushing gif decoder", pTextureName, Result);
		Cleanup();
		return false;
	}

	if(!DecodeAvailableFrames(nullptr))
	{
		Cleanup();
		return false;
	}

	if(vFrames.empty())
	{
		log_error("catclient/gif", "failed to decode any frames. filename='%s'", pTextureName);
		Cleanup();
		return false;
	}

	std::chrono::nanoseconds FallbackDuration = std::chrono::milliseconds(33);
	for(size_t i = 0; i < vFrames.size(); ++i)
	{
		if(vFrames[i].m_Duration > std::chrono::nanoseconds::zero())
		{
			FallbackDuration = vFrames[i].m_Duration;
			break;
		}
	}

	for(size_t i = 0; i + 1 < vFrames.size(); ++i)
	{
		if(vFrameTimestamps[i] != std::chrono::nanoseconds::min() && vFrameTimestamps[i + 1] != std::chrono::nanoseconds::min())
		{
			const std::chrono::nanoseconds Delta = vFrameTimestamps[i + 1] - vFrameTimestamps[i];
			if(Delta > std::chrono::nanoseconds::zero())
			{
				vFrames[i].m_Duration = Delta;
				FallbackDuration = Delta;
			}
		}
		if(vFrames[i].m_Duration <= std::chrono::nanoseconds::zero())
		{
			vFrames[i].m_Duration = FallbackDuration;
		}
	}

	if(vFrames.back().m_Duration <= std::chrono::nanoseconds::zero())
	{
		vFrames.back().m_Duration = FallbackDuration;
	}

	std::chrono::nanoseconds AnimationDuration = std::chrono::nanoseconds::zero();
	for(const auto &Frame : vFrames)
	{
		AnimationDuration += Frame.m_Duration;
	}

	UnloadCustomBackgroundTexture();
	m_CustomBackgroundTexture = IGraphics::CTextureHandle();
	m_vCustomBackgroundFrames = std::move(vFrames);
	m_CustomBackgroundImageSize = ImageSize;
	m_CustomBackgroundAnimationStart = time_get_nanoseconds();
	m_CustomBackgroundAnimationDuration = AnimationDuration;
	m_HasCustomBackgroundTexture = true;

	Success = true;
	Cleanup();
	return true;
#endif
}

bool CCatClient::LoadCustomBackgroundTexture(const char *pImageName)
{
	if(pImageName == nullptr || pImageName[0] == '\0')
	{
		return false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "catclient/backgrounds/%s", pImageName);

	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	CImageInfo Image;
	bool Loaded = false;
	if(str_endswith_nocase(aPath, ".png"))
	{
		int PngliteIncompatible = 0;
		Loaded = CImageLoader::LoadPng(File, aPath, Image, PngliteIncompatible);
	}
	else if(str_endswith_nocase(aPath, ".jpg") || str_endswith_nocase(aPath, ".jpeg"))
	{
#if defined(CONF_JPEG)
		Loaded = CImageLoader::LoadJpg(File, aPath, Image);
		if(!Loaded)
		{
			File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			return LoadStillFfmpegBackground(File, aPath);
		}
#else
		return LoadStillFfmpegBackground(File, aPath);
#endif
	}
	else if(str_endswith_nocase(aPath, ".gif"))
	{
		return LoadAnimatedBackground(File, aPath, "gif");
	}
	else
	{
		io_close(File);
		return false;
	}

	if(!Loaded)
	{
		return false;
	}

	UnloadCustomBackgroundTexture();
	m_CustomBackgroundImageSize = vec2((float)Image.m_Width, (float)Image.m_Height);
	m_CustomBackgroundTexture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
	if(m_CustomBackgroundTexture.IsNullTexture())
	{
		m_CustomBackgroundImageSize = vec2(0.0f, 0.0f);
		return false;
	}

	m_HasCustomBackgroundTexture = true;
	return true;
}

void CCatClient::UnloadCustomBackgroundTexture()
{
	if(m_HasCustomBackgroundTexture)
	{
		Graphics()->UnloadTexture(&m_CustomBackgroundTexture);
		for(auto &Frame : m_vCustomBackgroundFrames)
		{
			Graphics()->UnloadTexture(&Frame.m_Texture);
		}
	}
	m_CustomBackgroundTexture = IGraphics::CTextureHandle();
	m_vCustomBackgroundFrames.clear();
	m_CustomBackgroundImageSize = vec2(0.0f, 0.0f);
	m_CustomBackgroundAnimationStart = std::chrono::nanoseconds::zero();
	m_CustomBackgroundAnimationDuration = std::chrono::nanoseconds::zero();
	m_HasCustomBackgroundTexture = false;
}

void CCatClient::StartDefaultBackgroundDownload()
{
	if(m_pBackgroundDownloadTask)
	{
		return;
	}

	EnsureCustomBackgroundFolder();
	m_pBackgroundDownloadTask = HttpGetBoth(CATCLIENT_DEFAULT_BACKGROUND_URL, Storage(), CATCLIENT_DEFAULT_BACKGROUND_PATH, IStorage::TYPE_SAVE);
	m_pBackgroundDownloadTask->Timeout(DEFAULT_BACKGROUND_REQUEST_TIMEOUT);
	m_pBackgroundDownloadTask->IpResolve(IPRESOLVE::V4);
	m_pBackgroundDownloadTask->MaxResponseSize(16 * 1024 * 1024);
	m_pBackgroundDownloadTask->LogProgress(HTTPLOG::NONE);
	m_pBackgroundDownloadTask->FailOnErrorStatus(false);
	m_LastBackgroundAttempt = time_get_nanoseconds();
	Http()->Run(m_pBackgroundDownloadTask);
}

void CCatClient::FinishDefaultBackgroundDownload()
{
	if(!m_pBackgroundDownloadTask || m_pBackgroundDownloadTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pBackgroundDownloadTask->StatusCode() < 400)
	{
		ReloadCustomBackground();
	}
}

void CCatClient::EnsureSelectedCustomBackgroundLoaded()
{
	const bool NeedTexture = g_Config.m_CcCustomBackgroundMainMenu != 0 || g_Config.m_CcCustomBackgroundGame != 0;
	if(!NeedTexture)
	{
		UnloadCustomBackgroundTexture();
		m_aLoadedBackgroundImage[0] = '\0';
		return;
	}

	const char *pSelectedImage = g_Config.m_CcCustomBackgroundImage[0] != '\0' ? g_Config.m_CcCustomBackgroundImage : CATCLIENT_DEFAULT_BACKGROUND_NAME;
	if(str_comp(m_aLoadedBackgroundImage, pSelectedImage) == 0)
	{
		return;
	}

	str_copy(m_aLoadedBackgroundImage, pSelectedImage, sizeof(m_aLoadedBackgroundImage));
	if(!LoadCustomBackgroundTexture(pSelectedImage))
	{
		UnloadCustomBackgroundTexture();
	}
}

bool CCatClient::HasMenuCustomBackground() const
{
	return g_Config.m_CcCustomBackgroundMainMenu != 0 && m_HasCustomBackgroundTexture;
}

bool CCatClient::HasGameCustomBackground() const
{
	return g_Config.m_CcCustomBackgroundGame != 0 && m_HasCustomBackgroundTexture;
}

bool CCatClient::IsDefaultBackgroundDownloading() const
{
	return m_pBackgroundDownloadTask != nullptr;
}

void CCatClient::ReloadCustomBackground()
{
	UnloadCustomBackgroundTexture();
	m_aLoadedBackgroundImage[0] = '\0';
	EnsureSelectedCustomBackgroundLoaded();
}

IGraphics::CTextureHandle CCatClient::CurrentCustomBackgroundTexture() const
{
	if(m_vCustomBackgroundFrames.empty())
	{
		return m_CustomBackgroundTexture;
	}

	if(m_vCustomBackgroundFrames.size() == 1 || m_CustomBackgroundAnimationDuration <= std::chrono::nanoseconds::zero())
	{
		return m_vCustomBackgroundFrames.front().m_Texture;
	}

	const int64_t TotalDuration = m_CustomBackgroundAnimationDuration.count();
	if(TotalDuration <= 0)
	{
		return m_vCustomBackgroundFrames.front().m_Texture;
	}

	int64_t Elapsed = (time_get_nanoseconds() - m_CustomBackgroundAnimationStart).count();
	if(Elapsed < 0)
	{
		Elapsed = 0;
	}
	Elapsed %= TotalDuration;

	for(const auto &Frame : m_vCustomBackgroundFrames)
	{
		if(Elapsed < Frame.m_Duration.count())
		{
			return Frame.m_Texture;
		}
		Elapsed -= Frame.m_Duration.count();
	}

	return m_vCustomBackgroundFrames.back().m_Texture;
}

void CCatClient::RenderCustomBackground()
{
	if(!m_HasCustomBackgroundTexture)
	{
		return;
	}

	const float ScreenHeight = 300.0f;
	const float ScreenWidth = ScreenHeight * ((float)Graphics()->WindowWidth() / (float)maximum(Graphics()->WindowHeight(), 1));
	Graphics()->MapScreen(0.0f, 0.0f, ScreenWidth, ScreenHeight);

	Graphics()->BlendNormal();
	Graphics()->WrapClamp();
	Graphics()->TextureSet(CurrentCustomBackgroundTexture());
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	const IGraphics::CQuadItem Quad(0.0f, 0.0f, ScreenWidth, ScreenHeight);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->WrapNormal();
}

#endif
