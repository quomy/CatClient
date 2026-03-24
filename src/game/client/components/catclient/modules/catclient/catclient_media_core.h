#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_MEDIA_CORE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_MEDIA_CORE_H

static constexpr const char *CATCLIENT_DEFAULT_BACKGROUND_URL = "https://tags.quomy.win/firstbg.jpg";
static constexpr const char *CATCLIENT_DEFAULT_BACKGROUND_NAME = "firstbg.jpg";
static constexpr const char *CATCLIENT_DEFAULT_BACKGROUND_PATH = "catclient/backgrounds/firstbg.jpg";
static constexpr CTimeout DEFAULT_BACKGROUND_REQUEST_TIMEOUT{10000, 0, 0, 0};
static constexpr auto DEFAULT_BACKGROUND_RETRY_INTERVAL = std::chrono::seconds(30);

#if defined(CONF_VIDEORECORDER)
static void LogBackgroundDecodeError(const char *pOperation, const char *pFilename, int Result)
{
	char aError[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(Result, aError, sizeof(aError));
	log_error("catclient/gif", "%s failed. filename='%s' error='%s'", pOperation, pFilename, aError);
}

static int AvReadPacket(void *pOpaque, uint8_t *pBuf, int BufSize)
{
	auto *pReader = static_cast<CAvByteBufferReader *>(pOpaque);
	if(pReader->m_ReadOffset >= pReader->m_Size)
	{
		return AVERROR_EOF;
	}

	const size_t Remaining = pReader->m_Size - pReader->m_ReadOffset;
	const size_t ReadSize = minimum<size_t>((size_t)BufSize, Remaining);
	mem_copy(pBuf, pReader->m_pData + pReader->m_ReadOffset, ReadSize);
	pReader->m_ReadOffset += ReadSize;
	return (int)ReadSize;
}

static int64_t AvSeek(void *pOpaque, int64_t Offset, int Whence)
{
	auto *pReader = static_cast<CAvByteBufferReader *>(pOpaque);
	if(Whence == AVSEEK_SIZE)
	{
		return (int64_t)pReader->m_Size;
	}

	size_t NewOffset = 0;
	switch(Whence & ~AVSEEK_FORCE)
	{
	case SEEK_SET:
		if(Offset < 0)
		{
			return AVERROR(EINVAL);
		}
		NewOffset = (size_t)Offset;
		break;
	case SEEK_CUR:
		if((Offset < 0 && (uint64_t)(-Offset) > pReader->m_ReadOffset) || (Offset > 0 && (uint64_t)Offset > pReader->m_Size - pReader->m_ReadOffset))
		{
			return AVERROR(EINVAL);
		}
		NewOffset = pReader->m_ReadOffset + (ptrdiff_t)Offset;
		break;
	case SEEK_END:
		if((Offset < 0 && (uint64_t)(-Offset) > pReader->m_Size) || Offset > 0)
		{
			return AVERROR(EINVAL);
		}
		NewOffset = pReader->m_Size + (ptrdiff_t)Offset;
		break;
	default:
		return AVERROR(EINVAL);
	}

	if(NewOffset > pReader->m_Size)
	{
		return AVERROR(EINVAL);
	}

	pReader->m_ReadOffset = NewOffset;
	return (int64_t)pReader->m_ReadOffset;
}

static bool OpenFfmpegInputFromFile(IOHANDLE File, const char *pTextureName, const char *pInputFormatName, void **ppFileData, AVIOContext **ppAvioContext, AVFormatContext **ppFormatContext, CAvByteBufferReader &Reader)
{
	*ppFileData = nullptr;
	*ppAvioContext = nullptr;
	*ppFormatContext = nullptr;

	if(!File)
	{
		log_error("catclient/gif", "failed to open file for reading. filename='%s'", pTextureName);
		return false;
	}

	unsigned FileDataSize = 0;
	if(!io_read_all(File, ppFileData, &FileDataSize) || *ppFileData == nullptr || FileDataSize == 0)
	{
		io_close(File);
		log_error("catclient/gif", "failed to read file. filename='%s'", pTextureName);
		free(*ppFileData);
		*ppFileData = nullptr;
		return false;
	}
	io_close(File);

	Reader = {static_cast<const uint8_t *>(*ppFileData), FileDataSize, 0};

	uint8_t *pAvioBuffer = static_cast<uint8_t *>(av_malloc(4096));
	if(pAvioBuffer == nullptr)
	{
		log_error("catclient/gif", "out of memory while preparing decoder. filename='%s'", pTextureName);
		free(*ppFileData);
		*ppFileData = nullptr;
		return false;
	}

	*ppAvioContext = avio_alloc_context(pAvioBuffer, 4096, 0, &Reader, AvReadPacket, nullptr, AvSeek);
	if(*ppAvioContext == nullptr)
	{
		log_error("catclient/gif", "failed to allocate AVIO context. filename='%s'", pTextureName);
		av_free(pAvioBuffer);
		free(*ppFileData);
		*ppFileData = nullptr;
		return false;
	}

	*ppFormatContext = avformat_alloc_context();
	if(*ppFormatContext == nullptr)
	{
		log_error("catclient/gif", "failed to allocate format context. filename='%s'", pTextureName);
		av_freep(&(*ppAvioContext)->buffer);
		avio_context_free(ppAvioContext);
		free(*ppFileData);
		*ppFileData = nullptr;
		return false;
	}

	(*ppFormatContext)->pb = *ppAvioContext;
	(*ppFormatContext)->flags |= AVFMT_FLAG_CUSTOM_IO;

	auto *pInputFormat = pInputFormatName != nullptr ? av_find_input_format(pInputFormatName) : nullptr;
	const int Result = avformat_open_input(ppFormatContext, nullptr, pInputFormat, nullptr);
	if(Result < 0)
	{
		LogBackgroundDecodeError("opening background media", pTextureName, Result);
		avformat_close_input(ppFormatContext);
		av_freep(&(*ppAvioContext)->buffer);
		avio_context_free(ppAvioContext);
		free(*ppFileData);
		*ppFileData = nullptr;
		return false;
	}

	return true;
}

static std::chrono::nanoseconds GifFrameDuration(const AVFrame *pFrame, const AVPacket *pPacket, AVRational TimeBase)
{
	int64_t DurationTicks = 0;
	if(pFrame->duration > 0 && pFrame->duration != AV_NOPTS_VALUE)
	{
		DurationTicks = pFrame->duration;
	}
	else if(pPacket != nullptr && pPacket->duration > 0 && pPacket->duration != AV_NOPTS_VALUE)
	{
		DurationTicks = pPacket->duration;
	}

	if(DurationTicks <= 0)
	{
		return std::chrono::milliseconds(10);
	}

	const int64_t DurationNanoseconds = av_rescale_q(DurationTicks, TimeBase, AVRational{1, 1000000000});
	return DurationNanoseconds > 0 ? std::chrono::nanoseconds(DurationNanoseconds) : std::chrono::milliseconds(10);
}

static std::chrono::nanoseconds FrameTimestamp(const AVFrame *pFrame, AVRational TimeBase)
{
	int64_t Timestamp = AV_NOPTS_VALUE;
	if(pFrame->best_effort_timestamp != AV_NOPTS_VALUE)
	{
		Timestamp = pFrame->best_effort_timestamp;
	}
	else if(pFrame->pts != AV_NOPTS_VALUE)
	{
		Timestamp = pFrame->pts;
	}

	if(Timestamp == AV_NOPTS_VALUE)
	{
		return std::chrono::nanoseconds::min();
	}

	return std::chrono::nanoseconds(av_rescale_q(Timestamp, TimeBase, AVRational{1, 1000000000}));
}
#endif

static void EscapeConfigParam(char *pDst, const char *pSrc, size_t Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

#endif
