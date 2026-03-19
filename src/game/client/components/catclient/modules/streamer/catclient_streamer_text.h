#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_STREAMER_CATCLIENT_STREAMER_TEXT_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_STREAMER_CATCLIENT_STREAMER_TEXT_H

static void AppendSanitizedChunk(char **ppDst, char *pDstEnd, const char *pChunkStart, const char *pChunkEnd)
{
	while(pChunkStart < pChunkEnd && *ppDst < pDstEnd)
	{
		*(*ppDst)++ = *pChunkStart++;
	}
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

const char *CCatClient::MaskServerAddress(const char *pAddress, char *pOutput, size_t OutputSize)
{
	if(HasStreamerFlag(STREAMER_HIDE_SERVER_IP))
	{
		str_copy(pOutput, CCLocalize("Hidden"), OutputSize);
		return pOutput;
	}

	str_copy(pOutput, pAddress, OutputSize);
	return pOutput;
}

#endif
