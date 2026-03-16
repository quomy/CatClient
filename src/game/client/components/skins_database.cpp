#include "skins.h"

#include <base/log.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/engine.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace
{
	constexpr const char *DATABASE_SKINS_HOST = "https://data.teeworlds.xyz";
	constexpr const char *DATABASE_SKINS_SEARCH_URL = "https://data.teeworlds.xyz/api/skins?limit=100&search=";
	constexpr std::chrono::nanoseconds DATABASE_REQUEST_COOLDOWN = 4s;
	constexpr int64_t DATABASE_JSON_MAX_RESPONSE_SIZE = 1024 * 1024;
	constexpr int64_t DATABASE_IMAGE_MAX_RESPONSE_SIZE = 10 * 1024 * 1024;

	bool IsTokenBoundary(char c)
	{
		return c == '\0' || c == ' ';
	}

	std::string NormalizeSkinIdentifier(const char *pText)
	{
		if(pText == nullptr || pText[0] == '\0')
		{
			return {};
		}

		char aLower[IO_MAX_PATH_LENGTH];
		str_utf8_tolower(pText, aLower, sizeof(aLower));

		std::string Result;
		Result.reserve(str_length(aLower));
		bool LastWasSeparator = true;
		for(const char *pRead = aLower; *pRead != '\0'; ++pRead)
		{
			const unsigned char Character = static_cast<unsigned char>(*pRead);
			if(Character == '_' || Character == '-' || Character == ' ' || Character == '\t')
			{
				if(!LastWasSeparator)
				{
					Result.push_back(' ');
					LastWasSeparator = true;
				}
				continue;
			}

			Result.push_back(*pRead);
			LastWasSeparator = false;
		}

		while(!Result.empty() && Result.back() == ' ')
		{
			Result.pop_back();
		}
		return Result;
	}

	std::string NormalizeFilenameStem(const char *pFilename)
	{
		if(pFilename == nullptr)
		{
			return {};
		}

		char aFilename[IO_MAX_PATH_LENGTH];
		if(const char *pSuffix = str_endswith(pFilename, ".png"))
		{
			str_truncate(aFilename, sizeof(aFilename), pFilename, pSuffix - pFilename);
		}
		else
		{
			str_copy(aFilename, pFilename, sizeof(aFilename));
		}
		return NormalizeSkinIdentifier(aFilename);
	}

	void AddSearchTerm(std::vector<std::string> &vTerms, const std::string &Term)
	{
		if(Term.empty())
		{
			return;
		}

		if(std::find(vTerms.begin(), vTerms.end(), Term) == vTerms.end())
		{
			vTerms.push_back(Term);
		}
	}

	std::vector<std::string> BuildDatabaseSearchTerms(const char *pName)
	{
		std::vector<std::string> vTerms;
		if(pName == nullptr || pName[0] == '\0')
		{
			return vTerms;
		}

		AddSearchTerm(vTerms, pName);

		const std::string Normalized = NormalizeSkinIdentifier(pName);
		AddSearchTerm(vTerms, Normalized);

		const char *pSeparator = str_find(pName, "_");
		if(pSeparator == nullptr)
		{
			pSeparator = str_find(pName, "-");
		}
		if(pSeparator == nullptr)
		{
			pSeparator = str_find(pName, " ");
		}
		if(pSeparator != nullptr && pSeparator[1] != '\0')
		{
			AddSearchTerm(vTerms, pSeparator + 1);
			AddSearchTerm(vTerms, NormalizeSkinIdentifier(pSeparator + 1));
		}

		return vTerms;
	}

	bool StartsWithToken(const std::string &Haystack, const std::string &Needle)
	{
		return Haystack.size() >= Needle.size() &&
		       Haystack.compare(0, Needle.size(), Needle) == 0 &&
		       IsTokenBoundary(Haystack.size() == Needle.size() ? '\0' : Haystack[Needle.size()]);
	}

	bool ContainsWholeToken(const std::string &Haystack, const std::string &Needle)
	{
		if(Haystack.empty() || Needle.empty())
		{
			return false;
		}

		size_t Pos = Haystack.find(Needle);
		while(Pos != std::string::npos)
		{
			const char Before = Pos == 0 ? '\0' : Haystack[Pos - 1];
			const size_t EndPos = Pos + Needle.size();
			const char After = EndPos >= Haystack.size() ? '\0' : Haystack[EndPos];
			if(IsTokenBoundary(Before) && IsTokenBoundary(After))
			{
				return true;
			}
			Pos = Haystack.find(Needle, Pos + 1);
		}
		return false;
	}

	int GetDatabaseSkinMatchScore(const char *pSearchName, const char *pCandidateName, const char *pCandidateFilename)
	{
		const std::string Search = NormalizeSkinIdentifier(pSearchName);
		if(Search.empty())
		{
			return 0;
		}

		const std::string CandidateName = NormalizeSkinIdentifier(pCandidateName);
		const std::string CandidateFilename = NormalizeFilenameStem(pCandidateFilename);

		int Score = 0;
		if(!CandidateName.empty())
		{
			if(CandidateName == Search)
			{
				Score = std::max(Score, 1000);
			}
			else if(StartsWithToken(CandidateName, Search))
			{
				Score = std::max(Score, 900);
			}
			else if(ContainsWholeToken(CandidateName, Search))
			{
				Score = std::max(Score, 820);
			}
		}

		if(!CandidateFilename.empty())
		{
			if(CandidateFilename == Search)
			{
				Score = std::max(Score, 990);
			}
			else if(StartsWithToken(CandidateFilename, Search))
			{
				Score = std::max(Score, 880);
			}
			else if(ContainsWholeToken(CandidateFilename, Search))
			{
				Score = std::max(Score, 800);
			}
		}

		return Score;
	}

	void BuildSkinDownloadUrl(char *pUrl, size_t UrlSize, const char *pName)
	{
		const char *pBaseUrl = g_Config.m_ClDownloadCommunitySkins != 0 ? g_Config.m_ClSkinCommunityDownloadUrl : g_Config.m_ClSkinDownloadUrl;
		char aEscapedName[256];
		EscapeUrl(aEscapedName, pName);
		str_format(pUrl, UrlSize, "%s%s.png", pBaseUrl, aEscapedName);
	}
}

bool CSkins::LoadSkinPng(CImageInfo &Info, const char *pName, int StorageType, char *pLoadedPath, size_t LoadedPathSize) const
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "downloadedskins/%s.png", pName);
	if(Graphics()->LoadPng(Info, aPath, IStorage::TYPE_SAVE))
	{
		str_copy(pLoadedPath, aPath, LoadedPathSize);
		return true;
	}

	str_format(aPath, sizeof(aPath), "skins/%s.png", pName);
	if(Graphics()->LoadPng(Info, aPath, StorageType))
	{
		str_copy(pLoadedPath, aPath, LoadedPathSize);
		return true;
	}

	if(LoadedPathSize > 0)
	{
		pLoadedPath[0] = '\0';
	}
	return false;
}

bool CSkins::ResolveCatClientDatabaseUrl(const char *pName, char *pUrl, size_t UrlSize) const
{
	int BestScore = 0;
	char aBestImageUrl[256] = {0};

	const std::vector<std::string> vSearchTerms = BuildDatabaseSearchTerms(pName);
	for(const std::string &SearchTerm : vSearchTerms)
	{
		char aEscapedName[256];
		EscapeUrl(aEscapedName, SearchTerm.c_str());

		char aSearchUrl[512];
		str_format(aSearchUrl, sizeof(aSearchUrl), "%s%s", DATABASE_SKINS_SEARCH_URL, aEscapedName);

		std::shared_ptr<CHttpRequest> pSearch = HttpGet(aSearchUrl);
		pSearch->Timeout(CTimeout{10000, 0, 8192, 10});
		pSearch->MaxResponseSize(DATABASE_JSON_MAX_RESPONSE_SIZE);
		pSearch->LogProgress(HTTPLOG::NONE);
		pSearch->FailOnErrorStatus(false);
		Http()->Run(pSearch);
		pSearch->Wait();
		if(pSearch->State() != EHttpState::DONE || pSearch->StatusCode() >= 400)
		{
			continue;
		}

		json_value *pJson = pSearch->ResultJson();
		if(pJson == nullptr || pJson->type != json_object)
		{
			if(pJson != nullptr)
			{
				json_value_free(pJson);
			}
			continue;
		}

		const json_value *pSkins = json_object_get(pJson, "skins");
		if(pSkins != &json_value_none && pSkins->type == json_array)
		{
			for(int Index = 0; Index < json_array_length(pSkins); ++Index)
			{
				const json_value *pSkin = json_array_get(pSkins, Index);
				if(pSkin == &json_value_none || pSkin->type != json_object)
				{
					continue;
				}

				const char *pStatus = json_string_get(json_object_get(pSkin, "status"));
				if(pStatus != nullptr && pStatus[0] != '\0' && str_comp(pStatus, "approved") != 0)
				{
					continue;
				}

				const char *pImageUrl = json_string_get(json_object_get(pSkin, "imageUrl"));
				if(pImageUrl == nullptr || pImageUrl[0] == '\0')
				{
					continue;
				}

				const char *pCandidateName = json_string_get(json_object_get(pSkin, "name"));
				const char *pCandidateFilename = json_string_get(json_object_get(pSkin, "filename"));
				int Score = GetDatabaseSkinMatchScore(pName, pCandidateName, pCandidateFilename);
				if(str_comp(SearchTerm.c_str(), pName) != 0)
				{
					Score -= 15;
				}
				if(Score > BestScore)
				{
					BestScore = Score;
					str_copy(aBestImageUrl, pImageUrl, sizeof(aBestImageUrl));
				}
			}
		}

		json_value_free(pJson);
		if(BestScore >= 950)
		{
			break;
		}
	}

	if(BestScore == 0)
	{
		return false;
	}

	if(str_startswith(aBestImageUrl, "https://") != nullptr || str_startswith(aBestImageUrl, "http://") != nullptr)
	{
		str_copy(pUrl, aBestImageUrl, UrlSize);
	}
	else if(aBestImageUrl[0] == '/')
	{
		str_format(pUrl, UrlSize, "%s%s", DATABASE_SKINS_HOST, aBestImageUrl);
	}
	else
	{
		str_format(pUrl, UrlSize, "%s/%s", DATABASE_SKINS_HOST, aBestImageUrl);
	}

	return true;
}

void CSkins::UnloadSkin(CSkinContainer *pSkinContainer)
{
	if(pSkinContainer->m_pLoadJob != nullptr)
	{
		pSkinContainer->m_pLoadJob->Abort();
		pSkinContainer->m_pLoadJob = nullptr;
	}

	if(pSkinContainer->m_pSkin != nullptr)
	{
		pSkinContainer->m_pSkin->m_OriginalSkin.Unload(Graphics());
		pSkinContainer->m_pSkin->m_ColorableSkin.Unload(Graphics());
		pSkinContainer->m_pSkin = nullptr;
	}
}

void CSkins::RequestDatabaseSkin(const char *pName)
{
	if(!g_Config.m_ClDownloadSkins || !CSkin::IsValidName(pName))
	{
		return;
	}

	if(m_DatabaseResolvedSkins.contains(pName) || m_DatabaseFailedDownloads.contains(pName) || m_DatabaseDownloadJobs.contains(pName))
	{
		return;
	}

	const auto Now = time_get_nanoseconds();
	if(const auto RequestTimeIt = m_DatabaseRequestTimes.find(pName); RequestTimeIt != m_DatabaseRequestTimes.end() && Now - RequestTimeIt->second < DATABASE_REQUEST_COOLDOWN)
	{
		return;
	}

	char aDownloadedPath[IO_MAX_PATH_LENGTH];
	str_format(aDownloadedPath, sizeof(aDownloadedPath), "downloadedskins/%s.png", pName);
	if(Storage()->FileExists(aDownloadedPath, IStorage::TYPE_SAVE))
	{
		m_DatabaseResolvedSkins.insert(pName);
		return;
	}

	const CSkinContainer *pSkinContainer = FindContainerImpl(pName);
	if(pSkinContainer == nullptr)
	{
		return;
	}

	if(pSkinContainer->Type() != CSkinContainer::EType::LOCAL)
	{
		return;
	}

	m_DatabaseRequestTimes[pName] = Now;
	auto pDatabaseJob = std::make_shared<CSkinDownloadJob>(this, pName);
	m_DatabaseDownloadJobs[pName] = pDatabaseJob;
	Engine()->AddJob(pDatabaseJob);
}

void CSkins::UpdateDatabaseDownloads(std::chrono::nanoseconds StartTime, std::chrono::nanoseconds MaxTime)
{
	for(auto It = m_DatabaseDownloadJobs.begin(); It != m_DatabaseDownloadJobs.end();)
	{
		std::shared_ptr<CSkinDownloadJob> pDatabaseJob = It->second;
		if(!pDatabaseJob->Done())
		{
			++It;
			continue;
		}

		const std::string SkinName = It->first;
		if(pDatabaseJob->State() == IJob::STATE_DONE && pDatabaseJob->m_Data.m_Info.m_pData != nullptr)
		{
			auto SkinIt = m_Skins.find(SkinName);
			if(SkinIt == m_Skins.end())
			{
				FindContainerImpl(SkinName.c_str());
				SkinIt = m_Skins.find(SkinName);
			}

			if(SkinIt != m_Skins.end())
			{
				UnloadSkin(SkinIt->second.get());
				LoadSkinFinish(SkinIt->second.get(), pDatabaseJob->m_Data);
				GameClient()->OnSkinUpdate(SkinName.c_str());
				m_DatabaseResolvedSkins.insert(SkinName);
				m_DatabaseFailedDownloads.erase(SkinName);
			}
		}
		else if(pDatabaseJob->State() == IJob::STATE_DONE && pDatabaseJob->m_NotFound)
		{
			m_DatabaseFailedDownloads.insert(SkinName);
		}

		It = m_DatabaseDownloadJobs.erase(It);
		if(time_get_nanoseconds() - StartTime >= MaxTime)
		{
			break;
		}
	}
}

void CSkins::CSkinDownloadJob::Run()
{
	char aUrl[IO_MAX_PATH_LENGTH];
	if(!m_pSkins->ResolveCatClientDatabaseUrl(m_aName, aUrl, sizeof(aUrl)))
	{
		BuildSkinDownloadUrl(aUrl, sizeof(aUrl), m_aName);
	}

	char aPathReal[IO_MAX_PATH_LENGTH];
	str_format(aPathReal, sizeof(aPathReal), "downloadedskins/%s.png", m_aName);

	const CTimeout Timeout{10000, 0, 8192, 10};
	const auto &&StartRequest = [&](bool SkipByFileTime) {
		std::shared_ptr<CHttpRequest> pGet = HttpGetBoth(aUrl, m_pSkins->Storage(), aPathReal, IStorage::TYPE_SAVE);
		pGet->Timeout(Timeout);
		pGet->MaxResponseSize(DATABASE_IMAGE_MAX_RESPONSE_SIZE);
		pGet->ValidateBeforeOverwrite(true);
		pGet->SkipByFileTime(SkipByFileTime);
		pGet->LogProgress(HTTPLOG::NONE);
		pGet->FailOnErrorStatus(false);
		{
			const CLockScope LockScope(m_Lock);
			m_pGetRequest = pGet;
		}
		m_pSkins->Http()->Run(pGet);
		return pGet;
	};

	std::shared_ptr<CHttpRequest> pGet = StartRequest(true);

	// Keep existing downloaded skin active while the slower database request is running.
	{
		void *pPngData = nullptr;
		unsigned PngSize = 0;
		if(m_pSkins->Storage()->ReadFile(aPathReal, IStorage::TYPE_SAVE, &pPngData, &PngSize))
		{
			if(m_pSkins->Graphics()->LoadPng(m_Data.m_Info, static_cast<uint8_t *>(pPngData), PngSize, aPathReal))
			{
				if(State() == IJob::STATE_ABORTED)
				{
					free(pPngData);
					return;
				}
				m_pSkins->LoadSkinData(m_aName, m_Data);
			}
			free(pPngData);
		}
	}

	pGet->Wait();
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = nullptr;
	}
	if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED || pGet->StatusCode() >= 400)
	{
		m_NotFound = pGet->State() == EHttpState::DONE && pGet->StatusCode() == 404;
		return;
	}
	if(pGet->StatusCode() == 304)
	{
		const bool Success = m_Data.m_Info.m_pData != nullptr;
		pGet->OnValidation(Success);
		if(Success)
		{
			return;
		}

		log_error("skins", "Failed to load cached downloaded skin '%s' from '%s', downloading it again", m_aName, aPathReal);
		pGet = StartRequest(false);
		pGet->Wait();
		{
			const CLockScope LockScope(m_Lock);
			m_pGetRequest = nullptr;
		}
		if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED || pGet->StatusCode() >= 400)
		{
			m_NotFound = pGet->State() == EHttpState::DONE && pGet->StatusCode() == 404;
			return;
		}
	}

	unsigned char *pResult = nullptr;
	size_t ResultSize = 0;
	pGet->Result(&pResult, &ResultSize);

	m_Data.m_Info.Free();
	m_Data.m_InfoGrayscale.Free();
	const bool Success = m_pSkins->Graphics()->LoadPng(m_Data.m_Info, pResult, ResultSize, aUrl);
	if(Success)
	{
		if(State() == IJob::STATE_ABORTED)
		{
			return;
		}
		m_pSkins->LoadSkinData(m_aName, m_Data);
	}
	else
	{
		log_error("skins", "Failed to load PNG of skin '%s' downloaded from '%s' (size %" PRIzu ")", m_aName, aUrl, ResultSize);
	}
	pGet->OnValidation(Success);
}
