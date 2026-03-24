#include "chat.h"

#include <base/io.h>
#include <base/log.h>
#include <base/math.h>
#include <base/time.h>

#include <engine/editor.h>
#include <engine/external/regex.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/csv.h>
#include <engine/textrender.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/animstate.h>
#include <game/client/components/catclient/catclient.h>
#include <game/client/components/censor.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/components/tclient/colored_parts.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cctype>

#if defined(CONF_VIDEORECORDER)
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
}
#endif

char CChat::ms_aDisplayText[MAX_LINE_LENGTH] = "";

namespace
{
	static constexpr size_t CHAT_GIF_MAX_RESPONSE_SIZE = 12 * 1024 * 1024;
	static constexpr size_t CHAT_GIF_MAX_FRAMES = 64;
	static constexpr float CHAT_GIF_MAX_WIDTH = 120.0f;
	static constexpr float CHAT_GIF_MAX_HEIGHT = 68.0f;
	static constexpr float CHAT_GIF_SPACING = 3.0f;

	float SmoothAnimation(float Progress)
	{
		Progress = std::clamp(Progress, 0.0f, 1.0f);
		return Progress * Progress * (3.0f - 2.0f * Progress);
	}

	bool HasChatAnimationFlag(int Flag)
	{
		return (g_Config.m_CcChatAnimations & Flag) != 0;
	}

	bool IsUrlBoundary(char c)
	{
		return c == '\0' || std::isspace((unsigned char)c) || c == '"' || c == '\'' || c == '<' || c == '>';
	}

	bool UrlLooksLikeGif(const char *pUrl)
	{
		if(pUrl == nullptr || pUrl[0] == '\0')
		{
			return false;
		}

		const char *pGif = str_find_nocase(pUrl, ".gif");
		if(pGif == nullptr)
		{
			return false;
		}

		const char NextChar = pGif[4];
		return NextChar == '\0' || NextChar == '?' || NextChar == '#' || NextChar == '&' || NextChar == '/';
	}

#if defined(CONF_VIDEORECORDER)
	struct CChatAvByteBufferReader
	{
		const uint8_t *m_pData = nullptr;
		size_t m_Size = 0;
		size_t m_ReadOffset = 0;
	};

	static int ChatAvReadPacket(void *pOpaque, uint8_t *pBuf, int BufSize)
	{
		auto *pReader = static_cast<CChatAvByteBufferReader *>(pOpaque);
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

	static int64_t ChatAvSeek(void *pOpaque, int64_t Offset, int Whence)
	{
		auto *pReader = static_cast<CChatAvByteBufferReader *>(pOpaque);
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

	static std::chrono::nanoseconds ChatGifFrameDuration(const AVFrame *pFrame, const AVPacket *pPacket, AVRational TimeBase)
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

	static std::chrono::nanoseconds ChatFrameTimestamp(const AVFrame *pFrame, AVRational TimeBase)
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

	static void LogChatGifDecodeError(const char *pOperation, const char *pUrl, int Result)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Result, aError, sizeof(aError));
		log_log(LEVEL_ERROR, "chat/gif", "%s failed. url='%s' error='%s'", pOperation, pUrl, aError);
	}
#endif
}

CChat::CLine::CLine()
{
	m_TextContainerIndex.Reset();
	m_QuadContainerIndex = -1;
}

void CChat::CLine::Reset(CChat &This)
{
	This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	This.Graphics()->DeleteQuadContainer(m_QuadContainerIndex);
	This.AbortGifTask(*this);
	This.UnloadGifPreview(*this);
	m_Initialized = false;
	m_Time = 0;
	m_aText[0] = '\0';
	m_aName[0] = '\0';
	m_aWhisperName[0] = '\0';
	m_Friend = false;
	m_TimesRepeated = 0;
	m_pManagedTeeRenderInfo = nullptr;
	m_TextHeight = 0.0f;
	m_NameOffsetX = 0.0f;
	m_NameWidth = 0.0f;
	m_NameHeight = 0.0f;
	m_PrefixWidth = 0.0f;
	m_ContentWidth = 0.0f;
	m_pTranslateResponse = nullptr;
	m_aGifUrl[0] = '\0';
	m_GifPreviewFailed = false;
	m_GifImageSize = vec2(0.0f, 0.0f);
	m_GifRenderWidth = 0.0f;
	m_GifRenderHeight = 0.0f;
	m_GifRenderOffsetY = 0.0f;
	m_GifAnimationStart = std::chrono::nanoseconds::zero();
	m_GifAnimationDuration = std::chrono::nanoseconds::zero();
}

CChat::CChat()
{
	m_Mode = MODE_NONE;
	m_MouseIsPress = false;
	m_MousePress = vec2(0.0f, 0.0f);
	m_MouseRelease = vec2(0.0f, 0.0f);
	m_InputAnimationProgress = 0.0f;
	m_AnimatedMode = MODE_NONE;
	m_aAnimatedInputText[0] = '\0';
	m_LastInputAnimationTime = 0;
	m_LastTypingAnimationTime = 0;
	m_TypingAnimationStartWidth = 0.0f;
	m_TypingAnimationTargetWidth = 0.0f;
	m_NameContextPopup.m_pChat = this;

	m_Input.SetCalculateOffsetCallback([this]() { return m_IsInputCensored; });
	m_Input.SetDisplayTextCallback([this](char *pStr, size_t NumChars) {
		m_IsInputCensored = false;
		if(
			g_Config.m_ClStreamerMode &&
			(str_startswith(pStr, "/login ") ||
				str_startswith(pStr, "/register ") ||
				str_startswith(pStr, "/code ") ||
				str_startswith(pStr, "/timeout ") ||
				str_startswith(pStr, "/save ") ||
				str_startswith(pStr, "/load ")))
		{
			bool Censor = false;
			const size_t NumLetters = minimum(NumChars, sizeof(ms_aDisplayText) - 1);
			for(size_t i = 0; i < NumLetters; ++i)
			{
				if(Censor)
					ms_aDisplayText[i] = '*';
				else
					ms_aDisplayText[i] = pStr[i];
				if(pStr[i] == ' ')
				{
					Censor = true;
					m_IsInputCensored = true;
				}
			}
			ms_aDisplayText[NumLetters] = '\0';
			return ms_aDisplayText;
		}
		return pStr;
	});
}

void CChat::RegisterCommand(const char *pName, const char *pParams, const char *pHelpText)
{
	// Don't allow duplicate commands.
	for(const auto &Command : m_vServerCommands)
		if(str_comp(Command.m_aName, pName) == 0)
			return;

	m_vServerCommands.emplace_back(pName, pParams, pHelpText);
	m_ServerCommandsNeedSorting = true;
}

void CChat::UnregisterCommand(const char *pName)
{
	m_vServerCommands.erase(std::remove_if(m_vServerCommands.begin(), m_vServerCommands.end(), [pName](const CCommand &Command) { return str_comp(Command.m_aName, pName) == 0; }), m_vServerCommands.end());
}

void CChat::RebuildChat()
{
	for(auto &Line : m_aLines)
	{
		if(!Line.m_Initialized)
			continue;
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		// recalculate sizes
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
	}
}

void CChat::ClearLines()
{
	for(auto &Line : m_aLines)
		Line.Reset(*this);
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;
}

void CChat::OnWindowResize()
{
	RebuildChat();
}

void CChat::Reset()
{
	ClearLines();

	m_Show = false;
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_MouseIsPress = false;
	m_MousePress = vec2(0.0f, 0.0f);
	m_MouseRelease = vec2(0.0f, 0.0f);
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = nullptr;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;
	m_CurrentLine = 0;
	m_IsInputCensored = false;
	m_EditingNewLine = true;
	m_ServerSupportsCommandInfo = false;
	m_ServerCommandsNeedSorting = false;
	m_aCurrentInputText[0] = '\0';
	m_InputAnimationProgress = 0.0f;
	m_AnimatedMode = MODE_NONE;
	m_aAnimatedInputText[0] = '\0';
	m_LastInputAnimationTime = 0;
	m_LastTypingAnimationTime = 0;
	m_TypingAnimationStartWidth = 0.0f;
	m_TypingAnimationTargetWidth = 0.0f;
	CloseNameContextMenu();
	DisableMode();
	m_vServerCommands.clear();

	for(int64_t &LastSoundPlayed : m_aLastSoundPlayed)
		LastSoundPlayed = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
	m_MouseIsPress = false;
	m_InputAnimationProgress = 0.0f;
	m_AnimatedMode = MODE_NONE;
	m_aAnimatedInputText[0] = '\0';
	m_LastInputAnimationTime = 0;
	m_LastTypingAnimationTime = 0;
	m_TypingAnimationStartWidth = 0.0f;
	m_TypingAnimationTargetWidth = 0.0f;
	CloseNameContextMenu();
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat *)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat *)pUserData)->EnableMode(1);
	else
		((CChat *)pUserData)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat *)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Echo(pResult->GetString(0));
}

void CChat::ConClearChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->ClearLines();
}

void CChat::ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	((CChat *)pUserData)->RebuildChat();
}

void CChat::ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentWidth();
	pChat->RebuildChat();
}

void CChat::ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentFontSize();
	pChat->RebuildChat();
}

void CChat::PrepareWhisperCommand(const char *pPlayerName)
{
	if(pPlayerName == nullptr || pPlayerName[0] == '\0')
	{
		return;
	}

	if(m_Mode == MODE_NONE)
	{
		EnableMode(0);
	}

	char aEscapedName[128];
	char *pDst = aEscapedName;
	str_escape(&pDst, pPlayerName, aEscapedName + sizeof(aEscapedName));
	*pDst = '\0';

	char aBuf[MAX_LINE_LENGTH];
	str_format(aBuf, sizeof(aBuf), "/w \"%s\" ", aEscapedName);
	m_Input.Set(aBuf);
	m_Input.SetCursorOffset(str_length(aBuf));
	m_CompletionChosen = -1;
	m_CompletionUsed = false;
	m_pHistoryEntry = nullptr;
	m_EditingNewLine = true;
	str_copy(m_aAnimatedInputText, aBuf);

	const float ChatHeight = 300.0f;
	const float ChatWidth = ChatHeight * Graphics()->ScreenAspect();
	const float InputFontSize = FontSize() * (8.0f / 6.0f);
	const float MessageMaxWidth = maximum(ChatWidth - 190.0f, 190.0f);
	m_TypingAnimationStartWidth = minimum(TextRender()->TextWidth(InputFontSize, aBuf, -1, MessageMaxWidth), MessageMaxWidth);
	m_TypingAnimationTargetWidth = m_TypingAnimationStartWidth;
	m_LastTypingAnimationTime = time_get_nanoseconds().count();
}

void CChat::OpenNameContextMenu(const char *pPlayerName)
{
	if(pPlayerName == nullptr || pPlayerName[0] == '\0')
	{
		return;
	}

	CloseNameContextMenu();
	str_copy(m_NameContextPopup.m_aPlayerName, pPlayerName, sizeof(m_NameContextPopup.m_aPlayerName));
	Ui()->DoPopupMenu(&m_NameContextPopup, Ui()->MouseX(), Ui()->MouseY(), CNameContextPopup::POPUP_WIDTH, CNameContextPopup::POPUP_HEIGHT, &m_NameContextPopup, CNameContextPopup::Render);
}

void CChat::CloseNameContextMenu()
{
	if(GameClient() != nullptr)
	{
		Ui()->ClosePopupMenu(&m_NameContextPopup);
	}
	m_NameContextPopup.m_aPlayerName[0] = '\0';
}

void CChat::AbortGifTask(CLine &Line)
{
	if(Line.m_pGifTask)
	{
		Line.m_pGifTask->Abort();
		Line.m_pGifTask = nullptr;
	}
}

void CChat::UnloadGifPreview(CLine &Line)
{
	for(auto &Frame : Line.m_vGifFrames)
	{
		Graphics()->UnloadTexture(&Frame.m_Texture);
	}
	Line.m_vGifFrames.clear();
	Line.m_GifImageSize = vec2(0.0f, 0.0f);
	Line.m_GifRenderWidth = 0.0f;
	Line.m_GifRenderHeight = 0.0f;
	Line.m_GifRenderOffsetY = 0.0f;
	Line.m_GifAnimationStart = std::chrono::nanoseconds::zero();
	Line.m_GifAnimationDuration = std::chrono::nanoseconds::zero();
}

bool CChat::FindGifUrl(const char *pText, char *pUrl, size_t UrlSize) const
{
	if(pText == nullptr || pUrl == nullptr || UrlSize == 0)
	{
		return false;
	}

	pUrl[0] = '\0';
	const char *pCursor = pText;
	while(*pCursor != '\0')
	{
		const char *pHttp = str_startswith(pCursor, "https://");
		if(pHttp == nullptr)
		{
			pHttp = str_startswith(pCursor, "http://");
		}

		if(pHttp == nullptr)
		{
			++pCursor;
			continue;
		}

		const char *pEnd = pHttp;
		while(*pEnd != '\0' && !IsUrlBoundary(*pEnd))
		{
			++pEnd;
		}

		while(pEnd > pHttp)
		{
			const char Tail = pEnd[-1];
			if(Tail == '.' || Tail == ',' || Tail == '!' || Tail == '?' || Tail == ':' || Tail == ';' || Tail == ')' || Tail == ']' || Tail == '}')
			{
				--pEnd;
			}
			else
			{
				break;
			}
		}

		char aCandidate[512];
		str_truncate(aCandidate, sizeof(aCandidate), pHttp, pEnd - pHttp);
		if(UrlLooksLikeGif(aCandidate))
		{
			str_copy(pUrl, aCandidate, UrlSize);
			return true;
		}

		pCursor = pEnd;
	}

	return false;
}

void CChat::StartGifPreview(CLine &Line)
{
#if !defined(CONF_VIDEORECORDER)
	Line.m_GifPreviewFailed = true;
	return;
#else
	if(Line.m_aGifUrl[0] == '\0' || Line.m_pGifTask || Line.m_GifPreviewFailed || !Line.m_vGifFrames.empty())
	{
		return;
	}

	auto pTask = HttpGet(Line.m_aGifUrl);
	pTask->Timeout(CTimeout{4000, 12000, 500, 5});
	pTask->MaxResponseSize((int64_t)CHAT_GIF_MAX_RESPONSE_SIZE);
	pTask->FailOnErrorStatus(false);
	Line.m_pGifTask = std::shared_ptr<CHttpRequest>(pTask.release());
	Http()->Run(Line.m_pGifTask);
#endif
}

bool CChat::LoadGifPreviewFromMemory(CLine &Line, const unsigned char *pData, size_t DataSize)
{
#if !defined(CONF_VIDEORECORDER)
	(void)Line;
	(void)pData;
	(void)DataSize;
	return false;
#else
	if(pData == nullptr || DataSize == 0)
	{
		return false;
	}

	bool Success = false;
	std::vector<CLine::SGifFrame> vFrames;
	std::vector<std::chrono::nanoseconds> vFrameTimestamps;
	vec2 ImageSize = vec2(0.0f, 0.0f);
	CChatAvByteBufferReader Reader{pData, DataSize, 0};
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
		if(!Success)
		{
			for(auto &Frame : vFrames)
			{
				Graphics()->UnloadTexture(&Frame.m_Texture);
			}
		}
	};

	uint8_t *pAvioBuffer = static_cast<uint8_t *>(av_malloc(4096));
	if(pAvioBuffer == nullptr)
	{
		return false;
	}

	pAvioContext = avio_alloc_context(pAvioBuffer, 4096, 0, &Reader, ChatAvReadPacket, nullptr, ChatAvSeek);
	if(pAvioContext == nullptr)
	{
		av_free(pAvioBuffer);
		return false;
	}

	pFormatContext = avformat_alloc_context();
	if(pFormatContext == nullptr)
	{
		Cleanup();
		return false;
	}

	pFormatContext->pb = pAvioContext;
	pFormatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
	auto *pInputFormat = av_find_input_format("gif");
	int Result = avformat_open_input(&pFormatContext, nullptr, pInputFormat, nullptr);
	if(Result < 0)
	{
		LogChatGifDecodeError("opening gif", Line.m_aGifUrl, Result);
		Cleanup();
		return false;
	}

	Result = avformat_find_stream_info(pFormatContext, nullptr);
	if(Result < 0)
	{
		LogChatGifDecodeError("reading gif stream info", Line.m_aGifUrl, Result);
		Cleanup();
		return false;
	}

	const int StreamIndex = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if(StreamIndex < 0)
	{
		LogChatGifDecodeError("finding gif video stream", Line.m_aGifUrl, StreamIndex);
		Cleanup();
		return false;
	}

	const AVStream *pStream = pFormatContext->streams[StreamIndex];
	const AVCodec *pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
	if(pCodec == nullptr)
	{
		log_log(LEVEL_ERROR, "chat/gif", "failed to find decoder. url='%s' codec='%s'", Line.m_aGifUrl, avcodec_get_name(pStream->codecpar->codec_id));
		Cleanup();
		return false;
	}

	pCodecContext = avcodec_alloc_context3(pCodec);
	if(pCodecContext == nullptr)
	{
		Cleanup();
		return false;
	}

	Result = avcodec_parameters_to_context(pCodecContext, pStream->codecpar);
	if(Result < 0)
	{
		LogChatGifDecodeError("copying gif codec parameters", Line.m_aGifUrl, Result);
		Cleanup();
		return false;
	}

	pCodecContext->thread_count = 1;
	Result = avcodec_open2(pCodecContext, pCodec, nullptr);
	if(Result < 0)
	{
		LogChatGifDecodeError("opening gif decoder", Line.m_aGifUrl, Result);
		Cleanup();
		return false;
	}

	pPacket = av_packet_alloc();
	pFrame = av_frame_alloc();
	if(pPacket == nullptr || pFrame == nullptr)
	{
		Cleanup();
		return false;
	}

	const auto DecodeAvailableFrames = [&](const AVPacket *pDurationPacket) {
		while(vFrames.size() < CHAT_GIF_MAX_FRAMES)
		{
			Result = avcodec_receive_frame(pCodecContext, pFrame);
			if(Result == 0)
			{
				if(pFrame->width <= 0 || pFrame->height <= 0)
				{
					return false;
				}

				if(pSwsContext == nullptr || ConvertedWidth != pFrame->width || ConvertedHeight != pFrame->height || ConvertedFormat != (AVPixelFormat)pFrame->format)
				{
					sws_freeContext(pSwsContext);
					pSwsContext = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
					if(pSwsContext == nullptr)
					{
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
					return false;
				}

				uint8_t *apDestData[4] = {Image.m_pData, nullptr, nullptr, nullptr};
				int aDestLineSize[4] = {(int)(Image.m_Width * Image.PixelSize()), 0, 0, 0};
				Result = sws_scale(pSwsContext, pFrame->data, pFrame->linesize, 0, pFrame->height, apDestData, aDestLineSize);
				if(Result != pFrame->height)
				{
					Image.Free();
					return false;
				}

				CLine::SGifFrame Frame;
				Frame.m_Duration = ChatGifFrameDuration(pFrame, pDurationPacket, pStream->time_base);
				Frame.m_Texture = Graphics()->LoadTextureRawMove(Image, 0, Line.m_aGifUrl);
				if(Frame.m_Texture.IsNullTexture())
				{
					Image.Free();
					return false;
				}

				if(ImageSize == vec2(0.0f, 0.0f))
				{
					ImageSize = vec2((float)pFrame->width, (float)pFrame->height);
				}
				vFrameTimestamps.emplace_back(ChatFrameTimestamp(pFrame, pStream->time_base));
				vFrames.emplace_back(std::move(Frame));
			}
			else if(Result == AVERROR(EAGAIN) || Result == AVERROR_EOF)
			{
				return true;
			}
			else
			{
				LogChatGifDecodeError("decoding gif frame", Line.m_aGifUrl, Result);
				return false;
			}
		}

		return true;
	};

	while((Result = av_read_frame(pFormatContext, pPacket)) >= 0)
	{
		if(pPacket->stream_index == StreamIndex)
		{
			Result = avcodec_send_packet(pCodecContext, pPacket);
			if(Result < 0)
			{
				LogChatGifDecodeError("sending gif packet", Line.m_aGifUrl, Result);
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

			if(vFrames.size() >= CHAT_GIF_MAX_FRAMES)
			{
				av_packet_unref(pPacket);
				break;
			}
		}
		av_packet_unref(pPacket);
	}

	Result = avcodec_send_packet(pCodecContext, nullptr);
	if(Result < 0)
	{
		LogChatGifDecodeError("flushing gif decoder", Line.m_aGifUrl, Result);
		Cleanup();
		return false;
	}

	if(!DecodeAvailableFrames(nullptr) || vFrames.empty())
	{
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

	UnloadGifPreview(Line);
	Line.m_vGifFrames = std::move(vFrames);
	Line.m_GifImageSize = ImageSize;
	Line.m_GifAnimationStart = time_get_nanoseconds();
	Line.m_GifAnimationDuration = AnimationDuration;
	Success = true;
	Cleanup();
	return true;
#endif
}

void CChat::UpdateGifPreview(CLine &Line)
{
	if(Line.m_pGifTask == nullptr)
	{
		return;
	}

	if(!Line.m_pGifTask->Done())
	{
		return;
	}

	if(Line.m_pGifTask->State() != EHttpState::DONE || Line.m_pGifTask->StatusCode() >= 400)
	{
		Line.m_GifPreviewFailed = true;
		Line.m_pGifTask = nullptr;
		return;
	}

	unsigned char *pData = nullptr;
	size_t DataSize = 0;
	Line.m_pGifTask->Result(&pData, &DataSize);
	const bool Loaded = LoadGifPreviewFromMemory(Line, pData, DataSize);
	Line.m_pGifTask = nullptr;
	if(!Loaded)
	{
		Line.m_GifPreviewFailed = true;
		return;
	}

	Line.m_GifPreviewFailed = false;
	Line.m_aYOffset[0] = -1.0f;
	Line.m_aYOffset[1] = -1.0f;
	TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
	Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
}

const IGraphics::CTextureHandle *CChat::CurrentGifTexture(const CLine &Line) const
{
	if(Line.m_vGifFrames.empty())
	{
		return nullptr;
	}

	if(Line.m_vGifFrames.size() == 1 || Line.m_GifAnimationDuration <= std::chrono::nanoseconds::zero())
	{
		return &Line.m_vGifFrames.front().m_Texture;
	}

	const int64_t TotalDuration = Line.m_GifAnimationDuration.count();
	if(TotalDuration <= 0)
	{
		return &Line.m_vGifFrames.front().m_Texture;
	}

	int64_t Elapsed = (time_get_nanoseconds() - Line.m_GifAnimationStart).count();
	if(Elapsed < 0)
	{
		Elapsed = 0;
	}
	Elapsed %= TotalDuration;

	for(const auto &Frame : Line.m_vGifFrames)
	{
		if(Elapsed < Frame.m_Duration.count())
		{
			return &Frame.m_Texture;
		}
		Elapsed -= Frame.m_Duration.count();
	}

	return &Line.m_vGifFrames.back().m_Texture;
}

CUi::EPopupMenuFunctionResult CChat::CNameContextPopup::Render(void *pContext, CUIRect View, bool Active)
{
	CNameContextPopup *pPopup = static_cast<CNameContextPopup *>(pContext);
	CChat *pChat = pPopup->m_pChat;
	if(pChat == nullptr || pPopup->m_aPlayerName[0] == '\0')
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}

	char aSanitizedName[64];
	pChat->GameClient()->m_CatClient.SanitizeText(pPopup->m_aPlayerName, aSanitizedName, sizeof(aSanitizedName));

	CUi *pUi = pChat->Ui();
	const float FontSize = 10.0f;
	const float ItemSpacing = 3.0f;
	const float ButtonHeight = 14.0f;

	CUIRect Header, Button;
	SLabelProperties Props;
	Props.m_MaxWidth = View.w;
	Props.m_EllipsisAtEnd = true;

	View.HSplitTop(FontSize, &Header, &View);
	pUi->DoLabel(&Header, aSanitizedName, FontSize, TEXTALIGN_ML, Props);

	View.HSplitTop(ItemSpacing, nullptr, &View);
	View.HSplitTop(ButtonHeight, &Button, &View);
	if(pUi->DoButton_PopupMenu(&pPopup->m_CopyButton, Localize("Copy nickname"), &Button, FontSize, TEXTALIGN_ML, 4.0f))
	{
		pChat->Input()->SetClipboardText(pPopup->m_aPlayerName);
		return CUi::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(ItemSpacing, nullptr, &View);
	View.HSplitTop(ButtonHeight, &Button, &View);
	if(pUi->DoButton_PopupMenu(&pPopup->m_WhisperButton, Localize("Open whisper"), &Button, FontSize, TEXTALIGN_ML, 4.0f) || (Active && pUi->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		pChat->PrepareWhisperCommand(pPopup->m_aPlayerName);
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CChat::Echo(const char *pString)
{
	AddLine(CLIENT_MSG, 0, pString);
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConEcho, this, "Echo the text in chat window");
	Console()->Register("clear_chat", "", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClearChat, this, "Clear chat messages");
}

void CChat::OnInit()
{
	Reset();
	Console()->Chain("cl_chat_old", ConchainChatOld, this);
	Console()->Chain("cl_chat_size", ConchainChatFontSize, this);
	Console()->Chain("cl_chat_width", ConchainChatWidth, this);
}

void CChat::OnShutdown()
{
	ClearLines();
	CloseNameContextMenu();
}

bool CChat::OnInput(const IInput::CEvent &Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_UP)
	{
		int VisibleLines = 0;
		float TotalHeight = 0.0f;
		bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f);
		int OffsetType = IsScoreBoardOpen ? 1 : 0;
		
		for(int i = 0; i < MAX_LINES; i++)
		{
			const CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
			if(!Line.m_Initialized)
				break;
			TotalHeight += Line.m_aYOffset[OffsetType];
			VisibleLines++;
		}
		
		if(VisibleLines > 0)
		{
			const float MaxScroll = maximum(0.0f, TotalHeight - 150.0f);
			m_ChatScrollTarget = minimum(m_ChatScrollTarget + 36.0f, MaxScroll);
			return true;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
	{
		if(m_ChatScrollTarget > 0.0f)
		{
			m_ChatScrollTarget = maximum(0.0f, m_ChatScrollTarget - 36.0f);
			return true;
		}
	}

	char aOldInput[MAX_LINE_LENGTH];
	str_copy(aOldInput, m_Input.GetString());

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		if(Ui()->IsPopupOpen(&m_NameContextPopup))
		{
			CloseNameContextMenu();
			return true;
		}

		DisableMode();
		GameClient()->OnRelease();
		if(g_Config.m_ClChatReset)
		{
			m_Input.Clear();
			m_pHistoryEntry = nullptr;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_ServerCommandsNeedSorting)
		{
			std::sort(m_vServerCommands.begin(), m_vServerCommands.end());
			m_ServerCommandsNeedSorting = false;
		}

		if(GameClient()->m_TClient.ChatDoSpecId(m_Input.GetString()))
			; // Do nothing as specid was executed
		else
			SendChatQueued(m_Input.GetString());
		m_pHistoryEntry = nullptr;
		DisableMode();
		GameClient()->OnRelease();
		m_Input.Clear();
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		const bool ShiftPressed = Input()->ShiftIsPressed();

		// fill the completion buffer
		if(!m_CompletionUsed)
		{
			const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
			for(size_t Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
				;
			m_PlaceholderOffset = pCursor - m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_truncate(m_aCompletionBuffer, sizeof(m_aCompletionBuffer), m_Input.GetString() + m_PlaceholderOffset, m_PlaceholderLength);
		}

		if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')
		{
			// Create the completion list of player names through which the player can iterate
			const char *PlayerName, *FoundInput;
			m_PlayerCompletionListLength = 0;
			for(auto &PlayerInfo : GameClient()->m_Snap.m_apInfoByName)
			{
				if(PlayerInfo)
				{
					PlayerName = GameClient()->m_aClients[PlayerInfo->m_ClientId].m_aName;
					FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
					if(FoundInput != nullptr)
					{
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_ClientId = PlayerInfo->m_ClientId;
						// The score for suggesting a player name is determined by the distance of the search input to the beginning of the player name
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_Score = (int)(FoundInput - PlayerName);
						m_PlayerCompletionListLength++;
					}
				}
			}
			std::stable_sort(m_aPlayerCompletionList, m_aPlayerCompletionList + m_PlayerCompletionListLength,
				[](const CRateablePlayer &Player1, const CRateablePlayer &Player2) -> bool {
					return Player1.m_Score < Player2.m_Score;
				});
		}

		if(m_aCompletionBuffer[0] == '/' && !m_vServerCommands.empty())
		{
			CCommand *pCompletionCommand = nullptr;

			const size_t NumCommands = m_vServerCommands.size();

			if(ShiftPressed && m_CompletionUsed)
				m_CompletionChosen--;
			else if(!ShiftPressed)
				m_CompletionChosen++;
			m_CompletionChosen = (m_CompletionChosen + 2 * NumCommands) % (2 * NumCommands);

			m_CompletionUsed = true;

			const char *pCommandStart = m_aCompletionBuffer + 1;
			for(size_t i = 0; i < 2 * NumCommands; ++i)
			{
				int SearchType;
				int Index;

				if(ShiftPressed)
				{
					SearchType = ((m_CompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen - i + NumCommands) % NumCommands;
				}
				else
				{
					SearchType = ((m_CompletionChosen + i) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen + i) % NumCommands;
				}

				auto &Command = m_vServerCommands[Index];

				if(str_startswith_nocase(Command.m_aName, pCommandStart))
				{
					pCompletionCommand = &Command;
					m_CompletionChosen = Index + SearchType * NumCommands;
					break;
				}
			}

			// insert the command
			if(pCompletionCommand)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// add the command
				str_append(aBuf, "/");
				str_append(aBuf, pCompletionCommand->m_aName);

				// add separator
				const char *pSeparator = pCompletionCommand->m_aParams[0] == '\0' ? "" : " ";
				str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_aName) + 1;
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
		else
		{
			// find next possible name
			const char *pCompletionString = nullptr;
			if(m_PlayerCompletionListLength > 0)
			{
				// We do this in a loop, if a player left the game during the repeated pressing of Tab, they are skipped
				CGameClient::CClientData *pCompletionClientData;
				for(int i = 0; i < m_PlayerCompletionListLength; ++i)
				{
					if(ShiftPressed && m_CompletionUsed)
					{
						m_CompletionChosen--;
					}
					else if(!ShiftPressed)
					{
						m_CompletionChosen++;
					}
					if(m_CompletionChosen < 0)
					{
						m_CompletionChosen += m_PlayerCompletionListLength;
					}
					m_CompletionChosen %= m_PlayerCompletionListLength;
					m_CompletionUsed = true;

					pCompletionClientData = &GameClient()->m_aClients[m_aPlayerCompletionList[m_CompletionChosen].m_ClientId];
					if(!pCompletionClientData->m_Active)
					{
						continue;
					}

					pCompletionString = pCompletionClientData->m_aName;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// quote the name
				char aQuoted[128];
				if(m_Input.GetString()[0] == '/' && (str_find(pCompletionString, " ") || str_find(pCompletionString, "\"")))
				{
					// escape the name
					str_copy(aQuoted, "\"");
					char *pDst = aQuoted + str_length(aQuoted);
					str_escape(&pDst, pCompletionString, aQuoted + sizeof(aQuoted));
					str_append(aQuoted, "\"");

					pCompletionString = aQuoted;
				}

				// add the name
				str_append(aBuf, pCompletionString);

				// add separator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			m_CompletionChosen = -1;
			m_CompletionUsed = false;
		}

		m_Input.ProcessInput(Event);
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_EditingNewLine)
		{
			str_copy(m_aCurrentInputText, m_Input.GetString());
			m_EditingNewLine = false;
		}

		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
				m_pHistoryEntry = pTest;
		}
		else
			m_pHistoryEntry = m_History.Last();

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
		else if(!m_EditingNewLine)
		{
			m_Input.Set(m_aCurrentInputText);
			m_EditingNewLine = true;
		}
	}

	if(str_comp(aOldInput, m_Input.GetString()) != 0)
	{
		str_copy(m_aAnimatedInputText, m_Input.GetString());
		if(HasChatAnimationFlag(CCatClient::CHAT_ANIM_TYPING))
		{
			const float ChatHeight = 300.0f;
			const float ChatWidth = ChatHeight * Graphics()->ScreenAspect();
			const float InputFontSize = FontSize() * (8.0f / 6.0f);
			const float MessageMaxWidth = maximum(ChatWidth - 190.0f, 190.0f);
			const int64_t NowNanoseconds = time_get_nanoseconds().count();
			const float Progress = m_LastTypingAnimationTime > 0 ? SmoothAnimation(std::clamp((NowNanoseconds - m_LastTypingAnimationTime) / 120'000'000.0f, 0.0f, 1.0f)) : 1.0f;
			const float CurrentWidth = mix(m_TypingAnimationStartWidth, m_TypingAnimationTargetWidth, Progress);
			m_TypingAnimationStartWidth = CurrentWidth;
			m_TypingAnimationTargetWidth = minimum(TextRender()->TextWidth(InputFontSize, m_Input.GetString(), -1, MessageMaxWidth), MessageMaxWidth);
			m_LastTypingAnimationTime = time_get_nanoseconds().count();
		}
		else
		{
			m_TypingAnimationStartWidth = 0.0f;
			m_TypingAnimationTargetWidth = 0.0f;
		}
	}

	return true;
}

void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;
	if(m_Mode == MODE_NONE)
	{
		if(Team)
			m_Mode = MODE_TEAM;
		else
			m_Mode = MODE_ALL;
		Input()->Clear();
		m_CompletionChosen = -1;
		m_CompletionUsed = false;
		m_Input.Activate(EInputPriority::CHAT);
		Input()->MouseModeAbsolute();
		m_AnimatedMode = m_Mode;
		m_LastInputAnimationTime = time_get_nanoseconds().count();
		m_TypingAnimationStartWidth = 0.0f;
		m_TypingAnimationTargetWidth = 0.0f;
	}
}

void CChat::DisableMode()
{
	if(m_Mode != MODE_NONE)
	{
		str_copy(m_aAnimatedInputText, m_Input.GetString());
		m_AnimatedMode = m_Mode;
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
		Input()->MouseModeRelative();
		m_MouseIsPress = false;
		CloseNameContextMenu();
		m_LastInputAnimationTime = time_get_nanoseconds().count();
		m_TypingAnimationStartWidth = 0.0f;
		m_TypingAnimationTargetWidth = 0.0f;
		m_ChatScrollOffset = 0.0f;
		m_ChatScrollTarget = 0.0f;
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

		auto &Re = GameClient()->m_TClient.m_RegexChatIgnore;
		if(Re.error().empty() && Re.test(pMsg->m_pMessage))
			return;

		/*
		if(g_Config.m_ClCensorChat)
		{
			char aMessage[MAX_LINE_LENGTH];
			str_copy(aMessage, pMsg->m_pMessage);
			GameClient()->m_Censor.CensorMessage(aMessage);
			AddLine(pMsg->m_ClientId, pMsg->m_Team, aMessage);
		}
		else
			AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
		*/

		AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK &&
			pMsg->m_ClientId == SERVER_MSG)
		{
			StoreSave(pMsg->m_pMessage);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(!m_ServerSupportsCommandInfo)
		{
			m_vServerCommands.clear();
			m_ServerSupportsCommandInfo = true;
		}
		RegisterCommand(pMsg->m_pName, pMsg->m_pArgsFormat, pMsg->m_pHelpText);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		UnregisterCommand(pMsg->m_pName);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHit = str_utf8_find_nocase(pLine, pName);

	while(pHit)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHit || pHit[-1] == ' ') && (pHit[Length] == 0 || pHit[Length] == ' ' || pHit[Length] == '.' || pHit[Length] == '!' || pHit[Length] == ',' || pHit[Length] == '?' || pHit[Length] == ':'))
			return true;

		pHit = str_utf8_find_nocase(pHit + 1, pName);
	}

	return false;
}

static constexpr const char *SAVES_HEADER[] = {
	"Time",
	"Player",
	"Map",
	"Code",
};

// TODO: remove this in a few releases (in 2027 or later)
//       it got deprecated by CGameClient::StoreSave
void CChat::StoreSave(const char *pText)
{
	const char *pStart = str_find(pText, "Team successfully saved by ");
	const char *pMid = str_find(pText, ". Use '/load ");
	const char *pOn = str_find(pText, "' on ");
	const char *pEnd = str_find(pText, pOn ? " to continue" : "' to continue");

	if(!pStart || !pMid || !pEnd || pMid < pStart || pEnd < pMid || (pOn && (pOn < pMid || pEnd < pOn)))
		return;

	char aName[16];
	str_truncate(aName, sizeof(aName), pStart + 27, pMid - pStart - 27);

	char aSaveCode[64];

	str_truncate(aSaveCode, sizeof(aSaveCode), pMid + 13, (pOn ? pOn : pEnd) - pMid - 13);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);

	const bool SavesFileExists = Storage()->FileExists(SAVES_FILE, IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const char *apColumns[4] = {
		aTimestamp,
		aName,
		GameClient()->Map()->BaseName(),
		aSaveCode,
	};

	if(!SavesFileExists)
	{
		CsvWrite(File, 4, SAVES_HEADER);
	}
	CsvWrite(File, 4, apColumns);
	io_close(File);
}

void CChat::AddLine(int ClientId, int Team, const char *pLine)
{
	if(pLine == nullptr || *pLine == 0 ||
		(ClientId == SERVER_MSG && !g_Config.m_ClShowChatSystem) ||
		(ClientId >= 0 && (GameClient()->m_aClients[ClientId].m_aName[0] == '\0' ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatTeamMembersOnly && GameClient()->IsOtherTeam(ClientId) && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && GameClient()->m_aClients[ClientId].m_Foe))))
		return;

	// TClient
	if(ClientId == CLIENT_MSG && !g_Config.m_TcShowChatClient)
		return;

	char aLineBuffer[2048];
	str_copy(aLineBuffer, pLine, sizeof(aLineBuffer));
	char *pMutableLine = aLineBuffer;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pMutableLine;
	const char *pEnd = nullptr;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = nullptr;
		}
		else if(pEnd == nullptr)
			pEnd = pStrOld;

		if(++Length >= MAX_LINE_LENGTH)
		{
			*const_cast<char *>(pStr) = '\0';
			break;
		}
	}
	if(pEnd != nullptr)
		*const_cast<char *>(pEnd) = '\0';

	char aSanitizedText[1024];
	GameClient()->m_CatClient.SanitizeText(pMutableLine, aSanitizedText, sizeof(aSanitizedText));
	pMutableLine = aSanitizedText;

	if(*pMutableLine == 0)
		return;

	bool Highlighted = false;

	auto &&FChatMsgCheckAndPrint = [this](const CLine &Line) {
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s%s", Line.m_aName, Line.m_ClientId >= 0 ? ": " : "", Line.m_aText);

		ColorRGBA ChatLogColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		if(Line.m_CustomColor)
		{
			ChatLogColor = *Line.m_CustomColor;
		}
		else if(Line.m_Highlighted)
		{
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		}
		else
		{
			if(Line.m_Friend && g_Config.m_ClMessageFriend)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
			else if(Line.m_Team)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
			else if(Line.m_ClientId == SERVER_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
			else if(Line.m_ClientId == CLIENT_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
			else // regular message
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		}

		const char *pFrom;
		if(Line.m_Whisper)
			pFrom = "chat/whisper";
		else if(Line.m_Team)
			pFrom = "chat/team";
		else if(Line.m_ClientId == SERVER_MSG)
			pFrom = "chat/server";
		else if(Line.m_ClientId == CLIENT_MSG)
			pFrom = "chat/client";
		else
			pFrom = "chat/all";

		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pFrom, aBuf, ChatLogColor);
	};

	// Custom color for new line
	std::optional<ColorRGBA> CustomColor = std::nullopt;
	if(ClientId == CLIENT_MSG)
		CustomColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
	const bool IgnoredPlayer = ClientId >= 0 && GameClient()->m_aClients[ClientId].m_ChatIgnore;
	if(IgnoredPlayer)
	{
		CustomColor = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f);
	}

	CLine &PreviousLine = m_aLines[m_CurrentLine];

	// Team Number:
	// 0 = global; 1 = team; 2 = sending whisper; 3 = receiving whisper

	// If it's a client message, m_aText will have ": " prepended so we have to work around it.
	if(PreviousLine.m_Initialized &&
		PreviousLine.m_TeamNumber == Team &&
		PreviousLine.m_ClientId == ClientId &&
		str_comp(PreviousLine.m_aText, pMutableLine) == 0 &&
		PreviousLine.m_CustomColor == CustomColor)
	{
		PreviousLine.m_TimesRepeated++;
		TextRender()->DeleteTextContainer(PreviousLine.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(PreviousLine.m_QuadContainerIndex);
		PreviousLine.m_Time = time();
		PreviousLine.m_aYOffset[0] = -1.0f;
		PreviousLine.m_aYOffset[1] = -1.0f;

		FChatMsgCheckAndPrint(PreviousLine);
		return;
	}

	m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;

	CLine &CurrentLine = m_aLines[m_CurrentLine];
	CurrentLine.Reset(*this);
	CurrentLine.m_Initialized = true;
	CurrentLine.m_Time = time();
	CurrentLine.m_aYOffset[0] = -1.0f;
	CurrentLine.m_aYOffset[1] = -1.0f;
	CurrentLine.m_ClientId = ClientId;
	CurrentLine.m_TeamNumber = Team;
	CurrentLine.m_Team = Team == 1;
	CurrentLine.m_Whisper = Team >= 2;
	CurrentLine.m_NameColor = -2;
	CurrentLine.m_CustomColor = CustomColor;

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && ClientId != GameClient()->m_aLocalIds[1])
		{
			for(int LocalId : GameClient()->m_aLocalIds)
			{
				Highlighted |= LocalId >= 0 && LineShouldHighlight(pMutableLine, GameClient()->m_aClients[LocalId].m_aName);
			}
		}
	}
	else
	{
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= GameClient()->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pMutableLine, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName);
	}
	Highlighted = Highlighted && !IgnoredPlayer;
	CurrentLine.m_Highlighted = Highlighted;

	str_copy(CurrentLine.m_aText, pMutableLine);
	FindGifUrl(pMutableLine, CurrentLine.m_aGifUrl, sizeof(CurrentLine.m_aGifUrl));

	if(CurrentLine.m_ClientId == SERVER_MSG)
	{
		str_copy(CurrentLine.m_aName, "*** ");
	}
	else if(CurrentLine.m_ClientId == CLIENT_MSG)
	{
		str_copy(CurrentLine.m_aName, "— ");
	}
	else
	{
		const auto &LineAuthor = GameClient()->m_aClients[CurrentLine.m_ClientId];

		if(LineAuthor.m_Active)
		{
			if(LineAuthor.m_Team == TEAM_SPECTATORS)
				CurrentLine.m_NameColor = TEAM_SPECTATORS;

			if(GameClient()->IsTeamPlay())
			{
				if(LineAuthor.m_Team == TEAM_RED)
					CurrentLine.m_NameColor = TEAM_RED;
				else if(LineAuthor.m_Team == TEAM_BLUE)
					CurrentLine.m_NameColor = TEAM_BLUE;
			}
		}

			if(Team == TEAM_WHISPER_SEND)
			{
				str_copy(CurrentLine.m_aName, "→");
				if(LineAuthor.m_Active)
				{
					str_copy(CurrentLine.m_aWhisperName, LineAuthor.m_aName, sizeof(CurrentLine.m_aWhisperName));
					char aSanitizedName[64];
					GameClient()->m_CatClient.SanitizeText(LineAuthor.m_aName, aSanitizedName, sizeof(aSanitizedName));
					str_append(CurrentLine.m_aName, " ");
					str_append(CurrentLine.m_aName, aSanitizedName);
				}
				CurrentLine.m_NameColor = TEAM_BLUE;
				CurrentLine.m_Highlighted = false;
				Highlighted = false;
			}
			else if(Team == TEAM_WHISPER_RECV)
			{
				str_copy(CurrentLine.m_aName, "←");
				if(LineAuthor.m_Active)
				{
					str_copy(CurrentLine.m_aWhisperName, LineAuthor.m_aName, sizeof(CurrentLine.m_aWhisperName));
					char aSanitizedName[64];
					GameClient()->m_CatClient.SanitizeText(LineAuthor.m_aName, aSanitizedName, sizeof(aSanitizedName));
					str_append(CurrentLine.m_aName, " ");
					str_append(CurrentLine.m_aName, aSanitizedName);
				}
				CurrentLine.m_NameColor = TEAM_RED;
				CurrentLine.m_Highlighted = !IgnoredPlayer;
				Highlighted = !IgnoredPlayer;
			}
			else
			{
				GameClient()->m_CatClient.SanitizeText(LineAuthor.m_aName, CurrentLine.m_aName, sizeof(CurrentLine.m_aName));
				str_copy(CurrentLine.m_aWhisperName, LineAuthor.m_aName, sizeof(CurrentLine.m_aWhisperName));
			}

		if(LineAuthor.m_Active)
		{
			CurrentLine.m_Friend = LineAuthor.m_Friend;
			CurrentLine.m_pManagedTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(LineAuthor);
		}
	}

	if(CurrentLine.m_aGifUrl[0] != '\0')
	{
		StartGifPreview(CurrentLine);
	}

	FChatMsgCheckAndPrint(CurrentLine);

	// play sound
	int64_t Now = time();
	if(ClientId == SERVER_MSG)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			if(g_Config.m_SndServerMessage)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 1.0f);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientId == CLIENT_MSG)
	{
		// No sound yet
	}
	else if(Highlighted && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s: %s", CurrentLine.m_aName, CurrentLine.m_aText);
			Client()->Notify("DDNet Chat", aBuf);
			if(g_Config.m_SndHighlight)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}

			if(g_Config.m_ClEditor)
			{
				GameClient()->Editor()->UpdateMentions();
			}
		}
	}
	else if(Team != TEAM_WHISPER_SEND)
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			bool PlaySound = CurrentLine.m_Team ? g_Config.m_SndTeamChat : g_Config.m_SndChat;
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
			{
				PlaySound &= (bool)g_Config.m_ClVideoShowChat;
			}
#endif
			if(PlaySound)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 1.0f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}

	// TClient
	GameClient()->m_Translate.AutoTranslate(CurrentLine);
}

void CChat::OnPrepareLines(float y)
{
	float x = 5.0f;
	float FontSize = this->FontSize();

	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	const bool ForceRecreate = IsScoreBoardOpen != m_PrevScoreBoardShowed || ShowLargeArea != m_PrevShowChat;
	m_PrevScoreBoardShowed = IsScoreBoardOpen;
	m_PrevShowChat = ShowLargeArea;

	const int TeeSize = MessageTeeSize();
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
		RealMsgPaddingTee = 0;
	}

	int64_t Now = time();
	float LineWidth = (IsScoreBoardOpen ? maximum(85.0f, (FontSize * 85.0f / 6.0f)) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	float Begin = x;
	float TextBegin = Begin + RealMsgPaddingX / 2.0f;
	int OffsetType = IsScoreBoardOpen ? 1 : 0;
	const bool HideFriendWhisperInfo = GameClient()->m_CatClient.HasStreamerFlag(CCatClient::STREAMER_HIDE_FRIEND_WHISPER) && m_Mode == MODE_NONE;

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(HideFriendWhisperInfo && Line.m_Whisper)
			continue;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		UpdateGifPreview(Line);

		if(Line.m_TextContainerIndex.Valid() && !ForceRecreate)
			continue;

		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);

		char aClientId[16] = "";
		if(g_Config.m_ClShowIds && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
		}

		char aCount[12];
		if(Line.m_ClientId < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		const char *pText = Line.m_aText;
		if(Config()->m_ClStreamerMode && Line.m_ClientId == SERVER_MSG)
		{
			if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load ") && str_endswith(Line.m_aText, "'"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***'";
			}
			else if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load") && str_endswith(Line.m_aText, "if it fails"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***' if save is successful or with '/load *** *** ***' if it fails";
			}
			else if(str_startswith(Line.m_aText, "Team successfully saved by ") && str_endswith(Line.m_aText, " to continue"))
			{
				pText = "Team successfully saved by ***. Use '/load *** *** ***' to continue";
			}
		}

		const CColoredParts ColoredParts(pText, Line.m_ClientId == CLIENT_MSG);
		if(!ColoredParts.Colors().empty() && ColoredParts.Colors()[0].m_Index == 0)
			Line.m_CustomColor = ColoredParts.Colors()[0].m_Color;
		pText = ColoredParts.Text();

		const char *pTranslatedError = nullptr;
		const char *pTranslatedText = nullptr;
		const char *pTranslatedLanguage = nullptr;
		if(Line.m_pTranslateResponse != nullptr && Line.m_pTranslateResponse->m_Text[0])
		{
			// If hidden and there is translated text
			if(pText != Line.m_aText)
			{
				pTranslatedError = TCLocalize("Translated text hidden due to streamer mode");
			}
			else if(Line.m_pTranslateResponse->m_Error)
			{
				pTranslatedError = Line.m_pTranslateResponse->m_Text;
			}
			else
			{
				pTranslatedText = Line.m_pTranslateResponse->m_Text;
				if(Line.m_pTranslateResponse->m_Language[0] != '\0')
					pTranslatedLanguage = Line.m_pTranslateResponse->m_Language;
			}
		}

		// get the y offset (calculate it if we haven't done that yet)
		if(Line.m_aYOffset[OffsetType] < 0.0f)
		{
			CTextCursor MeasureCursor;
			MeasureCursor.SetPosition(vec2(TextBegin, 0.0f));
			MeasureCursor.m_FontSize = FontSize;
			MeasureCursor.m_Flags = 0;
			MeasureCursor.m_LineWidth = LineWidth;

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				MeasureCursor.m_X += RealMsgPaddingTee;

				if(Line.m_Friend && g_Config.m_ClMessageFriend && !HideFriendWhisperInfo)
				{
					TextRender()->TextEx(&MeasureCursor, "♥ ");
				}
			}

			TextRender()->TextEx(&MeasureCursor, aClientId);
			TextRender()->TextEx(&MeasureCursor, Line.m_aName);
			if(Line.m_TimesRepeated > 0)
				TextRender()->TextEx(&MeasureCursor, aCount);

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				TextRender()->TextEx(&MeasureCursor, ": ");
			}

			CTextCursor AppendCursor = MeasureCursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = MeasureCursor.m_X;
				AppendCursor.m_LineWidth -= MeasureCursor.m_LongestLineWidth;
			}

			if(pTranslatedText)
			{
				TextRender()->TextEx(&AppendCursor, pTranslatedText);
				if(pTranslatedLanguage)
				{
					TextRender()->TextEx(&AppendCursor, " [");
					TextRender()->TextEx(&AppendCursor, pTranslatedLanguage);
					TextRender()->TextEx(&AppendCursor, "]");
				}
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pText);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else if(pTranslatedError)
			{
				TextRender()->TextEx(&AppendCursor, pText);
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pTranslatedError);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else
			{
				TextRender()->TextEx(&AppendCursor, pText);
			}

			Line.m_PrefixWidth = MeasureCursor.m_LongestLineWidth;
			Line.m_ContentWidth = AppendCursor.m_LongestLineWidth;
			Line.m_TextHeight = AppendCursor.Height();
			Line.m_aYOffset[OffsetType] = Line.m_TextHeight + RealMsgPaddingY;
			Line.m_GifRenderWidth = 0.0f;
			Line.m_GifRenderHeight = 0.0f;
			Line.m_GifRenderOffsetY = 0.0f;
			if(!Line.m_vGifFrames.empty() && Line.m_GifImageSize.x > 0.0f && Line.m_GifImageSize.y > 0.0f)
			{
				const float Scale = minimum(CHAT_GIF_MAX_WIDTH / Line.m_GifImageSize.x, CHAT_GIF_MAX_HEIGHT / Line.m_GifImageSize.y);
				const float ClampedScale = std::clamp(Scale, 0.05f, 1.0f);
				Line.m_GifRenderWidth = maximum(Line.m_GifImageSize.x * ClampedScale, 1.0f);
				Line.m_GifRenderHeight = maximum(Line.m_GifImageSize.y * ClampedScale, 1.0f);
				Line.m_GifRenderOffsetY = Line.m_TextHeight + CHAT_GIF_SPACING;
				Line.m_aYOffset[OffsetType] += CHAT_GIF_SPACING + Line.m_GifRenderHeight;
			}
		}

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		// the position the text was created
		Line.m_TextYOffset = y + RealMsgPaddingY / 2.0f;

		int CurRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(CurRenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD);

		// reset the cursor
		CTextCursor LineCursor;
		LineCursor.SetPosition(vec2(TextBegin, Line.m_TextYOffset));
		LineCursor.m_FontSize = FontSize;
		LineCursor.m_LineWidth = LineWidth;

		// Message is from valid player
		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			LineCursor.m_X += RealMsgPaddingTee;

			if(Line.m_Friend && g_Config.m_ClMessageFriend && !HideFriendWhisperInfo)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor)).WithAlpha(1.0f));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, "♥ ");
			}
		}

		// render name
		ColorRGBA NameColor;
		if(Line.m_CustomColor)
			NameColor = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Team)
			NameColor = CalculateNameColor(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else if(Line.m_NameColor == TEAM_RED)
			NameColor = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
		else if(Line.m_NameColor == TEAM_BLUE)
			NameColor = ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f);
		else if(Line.m_NameColor == TEAM_SPECTATORS)
			NameColor = ColorRGBA(0.75f, 0.5f, 0.75f, 1.0f);
		else if(Line.m_ClientId >= 0 && g_Config.m_ClChatTeamColors && GameClient()->m_Teams.Team(Line.m_ClientId))
			NameColor = GameClient()->GetDDTeamColor(GameClient()->m_Teams.Team(Line.m_ClientId), 0.75f);
		else
			NameColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);

		TextRender()->TextColor(NameColor);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aClientId);
		const float NameStartX = LineCursor.m_X;
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, Line.m_aName);
		Line.m_NameOffsetX = NameStartX - TextBegin;
		Line.m_NameWidth = maximum(LineCursor.m_X - NameStartX, 0.0f);
		Line.m_NameHeight = FontSize;

		if(Line.m_TimesRepeated > 0)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aCount);
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			TextRender()->TextColor(NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, ": ");
		}

		ColorRGBA Color;
		if(Line.m_CustomColor)
			Color = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Highlighted)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		else if(Line.m_Team)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else // regular message
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		TextRender()->TextColor(Color);

		CTextCursor AppendCursor = LineCursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = LineCursor.m_X;
			AppendCursor.m_LineWidth -= LineCursor.m_LongestLineWidth;
		}

		if(pTranslatedText)
		{
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedText);
			if(pTranslatedLanguage)
			{
				ColorRGBA ColorLang = Color;
				ColorLang.r *= 0.8f;
				ColorLang.g *= 0.8f;
				ColorLang.b *= 0.8f;
				TextRender()->TextColor(ColorLang);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, " [");
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedLanguage);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "]");
			}
			ColorRGBA ColorSub = Color;
			ColorSub.r *= 0.7f;
			ColorSub.g *= 0.7f;
			ColorSub.b *= 0.7f;
			TextRender()->TextColor(ColorSub);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else if(pTranslatedError)
		{
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			ColorRGBA ColorSub = Color;
			ColorSub.r = 0.7f;
			ColorSub.g = 0.6f;
			ColorSub.b = 0.6f;
			TextRender()->TextColor(ColorSub);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedError);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else
		{
			ColoredParts.AddSplitsToCursor(AppendCursor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_vColorSplits.clear();
		}

		Line.m_PrefixWidth = LineCursor.m_LongestLineWidth;
		Line.m_ContentWidth = AppendCursor.m_LongestLineWidth;

		if(!g_Config.m_ClChatOld && (Line.m_aText[0] != '\0' || Line.m_aName[0] != '\0'))
		{
			float FullWidth = RealMsgPaddingX * 1.5f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				FullWidth += LineCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth;
			}
			else
			{
				FullWidth += maximum(LineCursor.m_LongestLineWidth, AppendCursor.m_LongestLineWidth);
			}
			if(Line.m_GifRenderWidth > 0.0f)
			{
				FullWidth = maximum(FullWidth, RealMsgPaddingX * 1.5f + Line.m_PrefixWidth + Line.m_GifRenderWidth);
			}
			Graphics()->SetColor(1, 1, 1, 1);
			Line.m_QuadContainerIndex = Graphics()->CreateRectQuadContainer(Begin, y, FullWidth, Line.m_aYOffset[OffsetType], MessageRounding(), IGraphics::CORNER_ALL);
		}

		TextRender()->SetRenderFlags(CurRenderFlags);
		if(Line.m_TextContainerIndex.Valid())
			TextRender()->UploadTextContainer(Line.m_TextContainerIndex);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CChat::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(GameClient()->m_CatClient.HasStreamerFlag(CCatClient::STREAMER_HIDE_CHAT) && m_Mode == MODE_NONE)
		return;

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend + time_freq() < time())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter - 1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				SendChat(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	float x = 5.0f;
	if(m_Mode != MODE_NONE)
	{
		Input()->MouseModeAbsolute();
	}

	const bool UseUiPopups = m_Mode != MODE_NONE && !GameClient()->m_Menus.IsActive();
	if(UseUiPopups)
	{
		const vec2 NativeMousePos = Input()->NativeMousePos();
		const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();
		Ui()->OnCursorMove(NativeMousePos.x - UpdatedMousePos.x, NativeMousePos.y - UpdatedMousePos.y);
		Ui()->StartCheck();
		Ui()->Update();
	}

	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const vec2 ScreenSize = vec2(Width, Height);
	const bool MousePressedNow = m_Mode != MODE_NONE && Input()->NativeMousePressed(1);
	if(m_Mode != MODE_NONE)
	{
		const vec2 MousePosition = Input()->NativeMousePos() / WindowSize * ScreenSize;
		if(!m_MouseIsPress && MousePressedNow)
		{
			m_MouseIsPress = true;
			m_MousePress = MousePosition;
		}
		if(m_MouseIsPress)
		{
			m_MouseRelease = MousePosition;
		}
	}
	else
	{
		m_MouseIsPress = false;
	}
	const bool MouseReleasedNow = m_MouseIsPress && !MousePressedNow;

	// TClient
	float y = 300.0f - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f));
	// float y = 300.0f - 20.0f * FontSize() / 6.0f;
	float ScaledFontSize = FontSize() * (8.0f / 6.0f);
	const bool AnimateOpenClose = HasChatAnimationFlag(CCatClient::CHAT_ANIM_OPEN_CLOSE);
	const bool AnimateTyping = HasChatAnimationFlag(CCatClient::CHAT_ANIM_TYPING);
	const int64_t NowNanoseconds = time_get_nanoseconds().count();
	if(m_LastInputAnimationTime == 0)
	{
		m_LastInputAnimationTime = NowNanoseconds;
	}

	const float DeltaSeconds = std::clamp((NowNanoseconds - m_LastInputAnimationTime) / 1'000'000'000.0f, 0.0f, 0.1f);
	m_LastInputAnimationTime = NowNanoseconds;

	if(AnimateOpenClose)
	{
		const float TargetProgress = m_Mode != MODE_NONE ? 1.0f : 0.0f;
		const float AnimationStep = DeltaSeconds * 9.0f;
		if(m_InputAnimationProgress < TargetProgress)
		{
			m_InputAnimationProgress = minimum(m_InputAnimationProgress + AnimationStep, TargetProgress);
		}
		else if(m_InputAnimationProgress > TargetProgress)
		{
			m_InputAnimationProgress = maximum(m_InputAnimationProgress - AnimationStep, TargetProgress);
		}
	}
	else
	{
		m_InputAnimationProgress = m_Mode != MODE_NONE ? 1.0f : 0.0f;
		if(m_Mode != MODE_NONE)
		{
			m_AnimatedMode = m_Mode;
		}
	}
	const float InputOpenProgress = SmoothAnimation(m_InputAnimationProgress);
	const bool RenderAnimatedInput = m_Mode != MODE_NONE || (AnimateOpenClose && m_InputAnimationProgress > 0.001f && m_AnimatedMode != MODE_NONE);
	if(RenderAnimatedInput)
	{
		const float InputOffsetY = (1.0f - InputOpenProgress) * 10.0f;
		const float InputAlpha = AnimateOpenClose ? InputOpenProgress : 1.0f;

		const ColorRGBA DefaultTextColor = TextRender()->DefaultTextColor();
		const ColorRGBA DefaultTextOutlineColor = TextRender()->DefaultTextOutlineColor();
		TextRender()->TextColor(DefaultTextColor.WithMultipliedAlpha(InputAlpha));
		TextRender()->TextOutlineColor(DefaultTextOutlineColor.WithMultipliedAlpha(InputAlpha));

		// render chat input
		CTextCursor InputCursor;
		InputCursor.SetPosition(vec2(x, y + InputOffsetY));
		InputCursor.m_FontSize = ScaledFontSize;
		InputCursor.m_LineWidth = Width - 190.0f;

		// TClient
		InputCursor.m_LineWidth = std::max(Width - 190.0f, 190.0f);

		const int RenderMode = m_Mode != MODE_NONE ? m_Mode : m_AnimatedMode;
		if(RenderMode == MODE_ALL)
			TextRender()->TextEx(&InputCursor, Localize("All"));
		else if(RenderMode == MODE_TEAM)
			TextRender()->TextEx(&InputCursor, Localize("Team"));
		else
			TextRender()->TextEx(&InputCursor, Localize("Chat"));

		TextRender()->TextEx(&InputCursor, ": ");

		const float MessageMaxWidth = InputCursor.m_LineWidth - (InputCursor.m_X - InputCursor.m_StartX);
		const CUIRect InputInteractionRect = {InputCursor.m_X, y + InputOffsetY, MessageMaxWidth, 2.25f * InputCursor.m_FontSize};
		CLineInput::SMouseSelection *pMouseSelection = m_Input.GetMouseSelection();
		if(m_Mode != MODE_NONE && InputInteractionRect.Inside(m_MousePress))
		{
			if(pMouseSelection->m_Selecting && MouseReleasedNow && m_Input.IsActive())
			{
				Input()->EnsureScreenKeyboardShown();
			}
			pMouseSelection->m_Selecting = m_MouseIsPress;
			pMouseSelection->m_PressMouse = m_MousePress;
			pMouseSelection->m_ReleaseMouse = m_MouseRelease;
		}
		else if(m_MouseIsPress)
		{
			pMouseSelection->m_Selecting = false;
		}

		const float TypingProgress = AnimateTyping && m_LastTypingAnimationTime > 0 ? SmoothAnimation(std::clamp((NowNanoseconds - m_LastTypingAnimationTime) / 120'000'000.0f, 0.0f, 1.0f)) : 1.0f;
		const float AnimatedTypingWidth = AnimateTyping ? mix(m_TypingAnimationStartWidth, m_TypingAnimationTargetWidth, TypingProgress) : MessageMaxWidth;
		const float VisibleTypingWidth = AnimateTyping && m_Mode != MODE_NONE ? minimum(maximum(AnimatedTypingWidth + 4.0f, 4.0f), MessageMaxWidth) : MessageMaxWidth;
		const CUIRect ClippingRect = {InputCursor.m_X, InputCursor.m_Y, VisibleTypingWidth, 2.25f * InputCursor.m_FontSize};
		const float XScale = Graphics()->ScreenWidth() / Width;
		const float YScale = Graphics()->ScreenHeight() / Height;
		Graphics()->ClipEnable((int)(ClippingRect.x * XScale), (int)(ClippingRect.y * YScale), (int)(ClippingRect.w * XScale), (int)(ClippingRect.h * YScale));

		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollOffsetChange = m_Input.GetScrollOffsetChange();
		STextBoundingBox BoundingBox{};
		if(m_Mode != MODE_NONE)
		{
			m_Input.Activate(EInputPriority::CHAT); // Ensure that the input is active
			const CUIRect InputCursorRect = {InputCursor.m_X, InputCursor.m_Y - ScrollOffset, 0.0f, 0.0f};
			const bool WasChanged = m_Input.WasChanged();
			const bool WasCursorChanged = m_Input.WasCursorChanged();
			const bool Changed = WasChanged || WasCursorChanged;
			BoundingBox = m_Input.Render(&InputCursorRect, InputCursor.m_FontSize, TEXTALIGN_TL, Changed, MessageMaxWidth, 0.0f);
			str_copy(m_aAnimatedInputText, m_Input.GetString());
			m_AnimatedMode = m_Mode;
			if(!AnimateTyping || TypingProgress >= 1.0f)
			{
				m_TypingAnimationStartWidth = m_TypingAnimationTargetWidth;
			}
		}
		else
		{
			TextRender()->TextEx(&InputCursor, m_aAnimatedInputText);
			BoundingBox.m_H = InputCursor.m_Y + InputCursor.m_FontSize - ClippingRect.y;
		}

		Graphics()->ClipDisable();

		if(m_Mode != MODE_NONE)
		{
			// Scroll up or down to keep the caret inside the clipping rect
			const float CaretPositionY = m_Input.GetCaretPosition().y - ScrollOffsetChange;
			if(CaretPositionY < ClippingRect.y)
				ScrollOffsetChange -= ClippingRect.y - CaretPositionY;
			else if(CaretPositionY + InputCursor.m_FontSize > ClippingRect.y + ClippingRect.h)
				ScrollOffsetChange += CaretPositionY + InputCursor.m_FontSize - (ClippingRect.y + ClippingRect.h);

			Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, ClippingRect.h, BoundingBox.m_H);

			m_Input.SetScrollOffset(ScrollOffset);
			m_Input.SetScrollOffsetChange(ScrollOffsetChange);

			// Autocompletion hint
			if(m_Input.GetString()[0] == '/' && m_Input.GetString()[1] != '\0' && !m_vServerCommands.empty())
			{
				for(const auto &Command : m_vServerCommands)
				{
					if(str_startswith_nocase(Command.m_aName, m_Input.GetString() + 1))
					{
						InputCursor.m_X = InputCursor.m_X + TextRender()->TextWidth(InputCursor.m_FontSize, m_Input.GetString(), -1, InputCursor.m_LineWidth);
						InputCursor.m_Y = m_Input.GetCaretPosition().y;
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f * InputAlpha);
						TextRender()->TextEx(&InputCursor, Command.m_aName + str_length(m_Input.GetString() + 1));
						TextRender()->TextColor(DefaultTextColor.WithMultipliedAlpha(InputAlpha));
						break;
					}
				}
			}
		}

		TextRender()->TextColor(DefaultTextColor);
		TextRender()->TextOutlineColor(DefaultTextOutlineColor);
		if(m_Mode == MODE_NONE && m_InputAnimationProgress <= 0.001f)
		{
			m_AnimatedMode = MODE_NONE;
			m_aAnimatedInputText[0] = '\0';
		}
	}

#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
	{
		if(UseUiPopups)
		{
			Ui()->FinishCheck();
		}
		return;
	}

	y -= ScaledFontSize;
	
	const float InputOffsetY = (1.0f - InputOpenProgress) * 10.0f;
	const float MessageCutoffY = y + InputOffsetY;
	
	y += m_ChatScrollOffset;

	OnPrepareLines(y);

	bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)

	int64_t Now = time();
	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	float TotalScrollableHeight = 0.0f;
	for(int i = 0; i < MAX_LINES; i++)
	{
		const CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		TotalScrollableHeight += maximum(Line.m_aYOffset[OffsetType], 0.0f);
	}
	const float ChatViewportHeight = maximum(0.0f, y - HeightLimit);
	const float MaxChatScroll = maximum(0.0f, TotalScrollableHeight - ChatViewportHeight);
	m_ChatScrollTarget = minimum(m_ChatScrollTarget, MaxChatScroll);
	m_ChatScrollOffset = std::clamp(mix(m_ChatScrollOffset, m_ChatScrollTarget, std::clamp(Client()->RenderFrameTime() * 18.0f, 0.0f, 1.0f)), 0.0f, MaxChatScroll);
	if(absolute(m_ChatScrollOffset - m_ChatScrollTarget) < 0.01f)
	{
		m_ChatScrollOffset = m_ChatScrollTarget;
	}
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	bool NameContextOpenedThisFrame = false;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
	}

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		y -= Line.m_aYOffset[OffsetType];

		if(y < HeightLimit)
			break;
		
		if(m_Mode != MODE_NONE && y > MessageCutoffY)
			continue;

		if(MouseReleasedNow && !NameContextOpenedThisFrame && Line.m_ClientId >= 0 && Line.m_aWhisperName[0] != '\0' && Line.m_NameWidth > 0.0f)
		{
			const CUIRect NameRect = {x + Line.m_NameOffsetX, y + RealMsgPaddingY / 2.0f, Line.m_NameWidth, maximum(Line.m_NameHeight, FontSize())};
			if(distance(m_MousePress, m_MouseRelease) <= 4.0f && NameRect.Inside(m_MousePress) && NameRect.Inside(m_MouseRelease))
			{
				OpenNameContextMenu(Line.m_aWhisperName);
				NameContextOpenedThisFrame = true;
			}
		}

		float Blend = Now > Line.m_Time + 14 * time_freq() && !m_PrevShowChat ? 1.0f - (Now - Line.m_Time - 14 * time_freq()) / (2.0f * time_freq()) : 1.0f;

		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			if(Line.m_QuadContainerIndex != -1)
			{
				Graphics()->SetColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true)).WithMultipliedAlpha(Blend));
				Graphics()->RenderQuadContainerEx(Line.m_QuadContainerIndex, 0, -1, 0, ((y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset));
			}
		}

		if(Line.m_TextContainerIndex.Valid())
		{
			if(!g_Config.m_ClChatOld && Line.m_pManagedTeeRenderInfo != nullptr)
			{
				CTeeRenderInfo &TeeRenderInfo = Line.m_pManagedTeeRenderInfo->TeeRenderInfo();
				const int TeeSize = MessageTeeSize();
				TeeRenderInfo.m_Size = TeeSize;

				float RowHeight = FontSize() + RealMsgPaddingY;
				float OffsetTeeY = TeeSize / 2.0f;
				float FullHeightMinusTee = RowHeight - TeeSize;

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeRenderInfo, OffsetToMid);
				vec2 TeeRenderPos(x + (RealMsgPaddingX + TeeSize) / 2.0f, y + OffsetTeeY + FullHeightMinusTee / 2.0f + OffsetToMid.y);
				RenderTools()->RenderTee(pIdleState, &TeeRenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), TeeRenderPos, Blend);
			}

			const ColorRGBA TextColor = TextRender()->DefaultTextColor().WithMultipliedAlpha(Blend);
			const ColorRGBA TextOutlineColor = TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Blend);
			TextRender()->RenderTextContainer(Line.m_TextContainerIndex, TextColor, TextOutlineColor, 0, (y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset);
		}

		if(Line.m_GifRenderWidth > 0.0f && Line.m_GifRenderHeight > 0.0f)
		{
			const IGraphics::CTextureHandle *pTexture = CurrentGifTexture(Line);
			if(pTexture != nullptr)
			{
				Graphics()->BlendNormal();
				Graphics()->WrapClamp();
				Graphics()->TextureSet(*pTexture);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Blend);
				Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
				const float PreviewX = x + RealMsgPaddingX / 2.0f + Line.m_PrefixWidth;
				const float PreviewY = Line.m_TextYOffset + Line.m_GifRenderOffsetY + ((y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset);
				const IGraphics::CQuadItem Quad(PreviewX, PreviewY, Line.m_GifRenderWidth, Line.m_GifRenderHeight);
				Graphics()->QuadsDrawTL(&Quad, 1);
				Graphics()->QuadsEnd();
				Graphics()->TextureClear();
				Graphics()->WrapNormal();
			}
		}
	}

		if(MouseReleasedNow)
		{
			m_MouseIsPress = false;
		}

	if(UseUiPopups)
	{
		Ui()->MapScreen();
		Ui()->RenderPopupMenus();
		Ui()->FinishCheck();
	}
}

void CChat::EnsureCoherentFontSize() const
{
	// Adjust font size based on width
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatFontSize = g_Config.m_ClChatWidth / CHAT_FONTSIZE_WIDTH_RATIO;
}

void CChat::EnsureCoherentWidth() const
{
	// Adjust width based on font size
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatWidth = CHAT_FONTSIZE_WIDTH_RATIO * g_Config.m_ClChatFontSize;
}

// ----- send functions -----

void CChat::SendChat(int Team, const char *pLine)
{
	// don't send empty messages
	if(*str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	m_LastChatSend = time();

	if(GameClient()->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		Client()->SendPackMsgActive(&Msg7, MSGFLAG_VITAL, true);
		return;
	}

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CChat::SendChatQueued(const char *pLine)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	if(GameClient()->m_VoiceChat.TryHandleChatCommand(pLine))
		return;

	bool AddEntry = false;

	if(m_LastChatSend + time_freq() < time())
	{
		SendChat(m_Mode == MODE_ALL ? 0 : 1, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		const int Length = str_length(pLine);
		CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + Length);
		pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
		str_copy(pEntry->m_aText, pLine, Length + 1);
	}
}
