#include "catclient.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <algorithm>

static constexpr const char *STREAMER_WORDS_FILE = "nwords.txt";
static const char *const gs_apDefaultStreamerWords[] = {
		"пидор",
		"чурка",
		"петух",
		"петушок",
		"петушня",
		"пидорасы",
		"пидрилы",
		"негры",
		"нигеры",
		"негор",
		"негар",
		"nigger",
		"niggers",
		"nigga",
		"niga",
		"sniggers",
		"niggerz",
		"пидорасня",
		"пидорасина",
		"kys",
		"kill your self",
		"suicide",
		"суицид",
		"суициднись",
		"убейся",
		"вскройся",
		"нiгер",
		"HuGGER",
};

static bool WordExists(const std::vector<std::string> &vWords, const char *pWord)
{
	return std::any_of(vWords.begin(), vWords.end(), [pWord](const std::string &Word) {
		return str_utf8_comp_nocase(Word.c_str(), pWord) == 0;
	});
}

static void AppendSanitizedChunk(char **ppDst, char *pDstEnd, const char *pChunkStart, const char *pChunkEnd)
{
	while(pChunkStart < pChunkEnd && *ppDst < pDstEnd)
	{
		*(*ppDst)++ = *pChunkStart++;
	}
}

bool CCatClient::IsStreamerModeEnabled() const
{
	return g_Config.m_CcStreamerMode != 0;
}

bool CCatClient::HasStreamerFlag(int Flag) const
{
	return IsStreamerModeEnabled() && (g_Config.m_CcStreamerFlags & Flag) != 0;
}

void CCatClient::EnsureStreamerWordsLoaded()
{
	if(m_StreamerWordsLoaded)
	{
		return;
	}

	m_StreamerWordsLoaded = true;
	m_vStreamerBlockedWords.clear();

	char *pFileData = Storage()->ReadFileStr(STREAMER_WORDS_FILE, IStorage::TYPE_SAVE);
	if(pFileData != nullptr)
	{
		const char *pCursor = pFileData;
		while(*pCursor != '\0')
		{
			const char *pLineEnd = pCursor;
			while(*pLineEnd != '\0' && *pLineEnd != '\n' && *pLineEnd != '\r')
			{
				++pLineEnd;
			}

			char aWord[128];
			str_truncate(aWord, sizeof(aWord), pCursor, pLineEnd - pCursor);
			str_utf8_trim_right(aWord);
			const char *pTrimmedWord = str_utf8_skip_whitespaces(aWord);
			if(*pTrimmedWord != '\0' && !WordExists(m_vStreamerBlockedWords, pTrimmedWord))
			{
				m_vStreamerBlockedWords.emplace_back(pTrimmedWord);
			}

			pCursor = pLineEnd;
			while(*pCursor == '\n' || *pCursor == '\r')
			{
				++pCursor;
			}
		}

		free(pFileData);
	}

	if(m_vStreamerBlockedWords.empty())
	{
		for(const char *pWord : gs_apDefaultStreamerWords)
		{
			m_vStreamerBlockedWords.emplace_back(pWord);
		}
		SaveStreamerWords();
	}
}

void CCatClient::SaveStreamerWords() const
{
	IOHANDLE File = Storage()->OpenFile(STREAMER_WORDS_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		return;
	}

	for(const std::string &Word : m_vStreamerBlockedWords)
	{
		io_write(File, Word.c_str(), str_length(Word.c_str()));
		io_write_newline(File);
	}

	io_close(File);
}

void CCatClient::AddStreamerBlockedWord(const char *pWord)
{
	EnsureStreamerWordsLoaded();

	char aWord[128];
	str_copy(aWord, pWord, sizeof(aWord));
	str_utf8_trim_right(aWord);
	const char *pTrimmedWord = str_utf8_skip_whitespaces(aWord);
	if(*pTrimmedWord == '\0' || WordExists(m_vStreamerBlockedWords, pTrimmedWord))
	{
		return;
	}

	m_vStreamerBlockedWords.emplace_back(pTrimmedWord);
	SaveStreamerWords();
}

void CCatClient::RemoveStreamerBlockedWord(int Index)
{
	EnsureStreamerWordsLoaded();

	if(Index < 0 || Index >= (int)m_vStreamerBlockedWords.size())
	{
		return;
	}

	m_vStreamerBlockedWords.erase(m_vStreamerBlockedWords.begin() + Index);
	SaveStreamerWords();
}

const std::vector<std::string> &CCatClient::StreamerBlockedWords()
{
	EnsureStreamerWordsLoaded();
	return m_vStreamerBlockedWords;
}

void CCatClient::SanitizeText(const char *pInput, char *pOutput, size_t OutputSize)
{
	EnsureStreamerWordsLoaded();

	if(!IsStreamerModeEnabled() || pInput == nullptr || OutputSize == 0)
	{
		if(OutputSize > 0)
		{
			str_copy(pOutput, pInput != nullptr ? pInput : "", OutputSize);
		}
		return;
	}

	char *pDst = pOutput;
	char *pDstEnd = pOutput + OutputSize - 1;
	const char *pCursor = pInput;

	while(*pCursor != '\0' && pDst < pDstEnd)
	{
		const char *pBestStart = nullptr;
		const char *pBestEnd = nullptr;
		for(const std::string &Word : m_vStreamerBlockedWords)
		{
			if(Word.empty())
			{
				continue;
			}

			const char *pMatchEnd = nullptr;
			const char *pFound = str_utf8_find_nocase(pCursor, Word.c_str(), &pMatchEnd);
			if(pFound != nullptr &&
				(pBestStart == nullptr || pFound < pBestStart || (pFound == pBestStart && pMatchEnd > pBestEnd)))
			{
				pBestStart = pFound;
				pBestEnd = pMatchEnd;
			}
		}

		if(pBestStart == nullptr)
		{
			AppendSanitizedChunk(&pDst, pDstEnd, pCursor, pCursor + str_length(pCursor));
			break;
		}

		AppendSanitizedChunk(&pDst, pDstEnd, pCursor, pBestStart);

		const char *pWalk = pBestStart;
		while(pWalk < pBestEnd && pDst < pDstEnd)
		{
			str_utf8_decode(&pWalk);
			*pDst++ = '*';
		}

		pCursor = pBestEnd;
	}

	*pDst = '\0';
}

void CCatClient::BuildStreamerBlockedWordsPreview(char *pOutput, size_t OutputSize)
{
	EnsureStreamerWordsLoaded();

	pOutput[0] = '\0';
	for(size_t i = 0; i < m_vStreamerBlockedWords.size(); ++i)
	{
		if(i != 0)
		{
			str_append(pOutput, ", ", OutputSize);
		}
		str_append(pOutput, m_vStreamerBlockedWords[i].c_str(), OutputSize);
	}
}

int CCatClient::StreamerBlockedWordCount()
{
	EnsureStreamerWordsLoaded();
	return (int)m_vStreamerBlockedWords.size();
}

const char *CCatClient::MaskServerAddress(const char *pAddress, char *pOutput, size_t OutputSize)
{
	if(HasStreamerFlag(STREAMER_HIDE_SERVER_IP))
	{
		str_copy(pOutput, "Hidden", OutputSize);
		return pOutput;
	}

	str_copy(pOutput, pAddress, OutputSize);
	return pOutput;
}
