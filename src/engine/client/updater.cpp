#include "updater.h"

#include <base/fs.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/version.h>

#include <algorithm>
#include <cstdlib> // system
#include <vector>

using std::string;

namespace
{
constexpr const char *UPDATER_BASE_URL = "https://tags.quomy.win";
constexpr const char *UPDATER_LOCAL_MANIFEST = "update/update.json";
constexpr const char *UPDATER_LOCAL_EXTRACTED = "update/extracted";

bool IsUnreserved(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '-' || c == '_' ||
		c == '.' || c == '~' || c == '/';
}

void UrlEncodePath(const char *pIn, char *pOut, size_t OutSize)
{
	if(!pIn || !pOut || OutSize == 0)
	{
		return;
	}

	static const char HEX[] = "0123456789ABCDEF";
	size_t WriteIndex = 0;
	for(size_t i = 0; pIn[i] != '\0'; ++i)
	{
		const unsigned char c = static_cast<unsigned char>(pIn[i]);
		if(IsUnreserved(c))
		{
			if(OutSize - WriteIndex < 2)
			{
				break;
			}
			pOut[WriteIndex++] = static_cast<char>(c);
		}
		else
		{
			if(OutSize - WriteIndex < 4)
			{
				break;
			}
			pOut[WriteIndex++] = '%';
			pOut[WriteIndex++] = HEX[c >> 4];
			pOut[WriteIndex++] = HEX[c & 0x0F];
		}
	}
	pOut[WriteIndex] = '\0';
}

const char *GetUpdaterUrl(char *pBuf, int BufSize, const char *pFile)
{
	char aEncoded[1024];
	UrlEncodePath(pFile, aEncoded, sizeof(aEncoded));
	str_format(pBuf, BufSize, "%s/%s", UPDATER_BASE_URL, aEncoded);
	return pBuf;
}

const char *GetUpdaterManifestPath()
{
	return "update/" UPDATER_RELEASE_PLATFORM ".json";
}

const char *FormatFetchUrl(char *pBuf, int BufSize, const char *pUrl, bool UpdaterPath)
{
	if(UpdaterPath)
	{
		return GetUpdaterUrl(pBuf, BufSize, pUrl);
	}

	str_copy(pBuf, pUrl, BufSize);
	return pBuf;
}

bool IsSpecialDirEntry(const char *pName)
{
	return str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0;
}

struct SRemovePathData
{
	const char *m_pPath;
	bool m_Success;
};

bool RemovePathRecursively(const char *pPath);

int RemovePathRecursivelyCallback(const CFsFileInfo *pInfo, int IsDir, int Type, void *pUser)
{
	(void)IsDir;
	(void)Type;

	SRemovePathData *pData = static_cast<SRemovePathData *>(pUser);
	if(IsSpecialDirEntry(pInfo->m_pName))
	{
		return 0;
	}

	char aChildPath[IO_MAX_PATH_LENGTH];
	str_format(aChildPath, sizeof(aChildPath), "%s/%s", pData->m_pPath, pInfo->m_pName);
	if(!RemovePathRecursively(aChildPath))
	{
		pData->m_Success = false;
		return 1;
	}
	return 0;
}

bool RemovePathRecursively(const char *pPath)
{
	if(fs_is_file(pPath))
	{
		return fs_remove(pPath) == 0;
	}

	if(!fs_is_dir(pPath))
	{
		return true;
	}

	SRemovePathData Data{pPath, true};
	fs_listdir_fileinfo(pPath, RemovePathRecursivelyCallback, 0, &Data);
	if(!Data.m_Success)
	{
		return false;
	}

	return fs_removedir(pPath) == 0;
}

struct SCollectFilesData
{
	const char *m_pRootPath;
	const char *m_pCurrentPath;
	std::vector<string> *m_pFiles;
	bool m_Success;
};

bool CollectFilesRecursively(const char *pRootPath, const char *pCurrentPath, std::vector<string> &vFiles);

int CollectFilesRecursivelyCallback(const CFsFileInfo *pInfo, int IsDir, int Type, void *pUser)
{
	(void)Type;

	SCollectFilesData *pData = static_cast<SCollectFilesData *>(pUser);
	if(IsSpecialDirEntry(pInfo->m_pName))
	{
		return 0;
	}

	char aChildPath[IO_MAX_PATH_LENGTH];
	str_format(aChildPath, sizeof(aChildPath), "%s/%s", pData->m_pCurrentPath, pInfo->m_pName);
	if(IsDir)
	{
		if(!CollectFilesRecursively(pData->m_pRootPath, aChildPath, *pData->m_pFiles))
		{
			pData->m_Success = false;
			return 1;
		}
		return 0;
	}

	const size_t RootLength = str_length(pData->m_pRootPath);
	if(static_cast<size_t>(str_length(aChildPath)) <= RootLength + 1)
	{
		pData->m_Success = false;
		return 1;
	}

	pData->m_pFiles->emplace_back(aChildPath + RootLength + 1);
	return 0;
}

bool CollectFilesRecursively(const char *pRootPath, const char *pCurrentPath, std::vector<string> &vFiles)
{
	SCollectFilesData Data{pRootPath, pCurrentPath, &vFiles, true};
	fs_listdir_fileinfo(pCurrentPath, CollectFilesRecursivelyCallback, 0, &Data);
	return Data.m_Success;
}

bool EnsureDirectoryExists(const char *pPath)
{
	char aDummyPath[IO_MAX_PATH_LENGTH];
	str_format(aDummyPath, sizeof(aDummyPath), "%s/.catclient-updater", pPath);
	return fs_makedir_rec_for(aDummyPath) == 0;
}

#if !defined(CONF_FAMILY_WINDOWS)
string EscapeUnixShellArgument(const char *pText)
{
	string Result = "'";
	for(const char *pCurrent = pText; *pCurrent; ++pCurrent)
	{
		if(*pCurrent == '\'')
		{
			Result += "'\\''";
		}
		else
		{
			Result.push_back(*pCurrent);
		}
	}
	Result += "'";
	return Result;
}
#else
string EscapePowerShellSingleQuoted(const char *pText)
{
	string Result;
	for(const char *pCurrent = pText; *pCurrent; ++pCurrent)
	{
		if(*pCurrent == '\'')
		{
			Result += "''";
		}
		else
		{
			Result.push_back(*pCurrent);
		}
	}
	return Result;
}
#endif
} // namespace

class CUpdaterFetchTask : public CHttpRequest
{
	char m_aUrl[1024];
	CUpdater *m_pUpdater;

	void OnProgress() override;

protected:
	void OnCompletion(EHttpState State) override;

public:
	CUpdaterFetchTask(CUpdater *pUpdater, const char *pUrl, const char *pDestPath, bool UpdaterPath, const SHA256_DIGEST *pExpectedSha256 = nullptr);
};

CUpdaterFetchTask::CUpdaterFetchTask(CUpdater *pUpdater, const char *pUrl, const char *pDestPath, bool UpdaterPath, const SHA256_DIGEST *pExpectedSha256) :
	CHttpRequest(FormatFetchUrl(m_aUrl, sizeof(m_aUrl), pUrl, UpdaterPath)),
	m_pUpdater(pUpdater)
{
	WriteToFile(pUpdater->m_pStorage, pDestPath, IStorage::TYPE_ABSOLUTE);
	if(pExpectedSha256)
	{
		ExpectSha256(*pExpectedSha256);
	}
}

void CUpdaterFetchTask::OnProgress()
{
	const CLockScope LockScope(m_pUpdater->m_Lock);
	m_pUpdater->m_Percent = Progress();
}

void CUpdaterFetchTask::OnCompletion(EHttpState State)
{
	if(!str_comp(fs_filename(Dest()), "update.json"))
	{
		if(State == EHttpState::DONE)
		{
			m_pUpdater->SetCurrentState(IUpdater::GOT_MANIFEST);
		}
		else if(State == EHttpState::ERROR)
		{
			m_pUpdater->SetCurrentState(IUpdater::FAIL);
		}
	}
}

CUpdater::CUpdater()
{
	m_pClient = nullptr;
	m_pStorage = nullptr;
	m_pEngine = nullptr;
	m_pHttp = nullptr;
	m_State = CLEAN;
	m_Percent = 0;
	m_pCurrentTask = nullptr;

	m_ClientUpdate = false;
	m_ServerUpdate = false;
	m_aStatus[0] = '\0';

	IStorage::FormatTmpPath(m_aClientExecTmp, sizeof(m_aClientExecTmp), CLIENT_EXEC);
	IStorage::FormatTmpPath(m_aServerExecTmp, sizeof(m_aServerExecTmp), SERVER_EXEC);
}

void CUpdater::Init(CHttp *pHttp)
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pHttp = pHttp;
}

void CUpdater::SetCurrentState(EUpdaterState NewState)
{
	const CLockScope LockScope(m_Lock);
	m_State = NewState;
}

void CUpdater::SetCurrentStatus(const char *pStatus)
{
	const CLockScope LockScope(m_Lock);
	str_copy(m_aStatus, pStatus, sizeof(m_aStatus));
}

IUpdater::EUpdaterState CUpdater::GetCurrentState()
{
	const CLockScope LockScope(m_Lock);
	return m_State;
}

void CUpdater::GetCurrentFile(char *pBuf, int BufSize)
{
	const CLockScope LockScope(m_Lock);
	str_copy(pBuf, m_aStatus, BufSize);
}

int CUpdater::GetCurrentPercent()
{
	const CLockScope LockScope(m_Lock);
	return m_Percent;
}

void CUpdater::ResetUpdateData()
{
	if(m_pCurrentTask && !m_pCurrentTask->Done())
	{
		m_pCurrentTask->Abort();
	}
	m_pCurrentTask = nullptr;
	m_FileJobs.clear();
	m_Manifest.Reset();
	m_ClientUpdate = false;
	m_ServerUpdate = false;
	const CLockScope LockScope(m_Lock);
	m_aStatus[0] = '\0';
	m_Percent = 0;
}

void CUpdater::FetchUpdaterFile(const char *pFile, const char *pDestPath)
{
	m_pCurrentTask = std::make_shared<CUpdaterFetchTask>(this, pFile, pDestPath, true);
	SetCurrentStatus(pDestPath);
	m_pHttp->Run(m_pCurrentTask);
}

void CUpdater::FetchUrl(const char *pUrl, const char *pDestPath, const SHA256_DIGEST *pExpectedSha256)
{
	m_pCurrentTask = std::make_shared<CUpdaterFetchTask>(this, pUrl, pDestPath, false, pExpectedSha256);
	SetCurrentStatus(pDestPath);
	m_pHttp->Run(m_pCurrentTask);
}

bool CUpdater::MoveFile(const char *pFile)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	const size_t Length = str_length(pFile);
	bool Success = true;

	const bool IsDll = Length >= 4 && !str_comp_nocase(pFile + Length - 4, ".dll");
	const bool IsTtf = Length >= 4 && !str_comp_nocase(pFile + Length - 4, ".ttf");
	const bool IsSo = Length >= 3 && !str_comp_nocase(pFile + Length - 3, ".so");

#if !defined(CONF_FAMILY_WINDOWS)
	if(IsDll)
		return Success;
#endif

#if !defined(CONF_PLATFORM_LINUX)
	if(IsSo)
		return Success;
#endif

	if(IsDll || IsTtf || IsSo)
	{
		str_format(aBuf, sizeof(aBuf), "%s.old", pFile);
		m_pStorage->RenameBinaryFile(pFile, aBuf);
		str_format(aBuf, sizeof(aBuf), "update/%s", pFile);
		Success &= m_pStorage->RenameBinaryFile(aBuf, pFile);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "update/%s", pFile);
		Success &= m_pStorage->RenameBinaryFile(aBuf, pFile);
	}

	return Success;
}

bool CUpdater::PrepareUpdateDirectory()
{
	char aUpdatePath[IO_MAX_PATH_LENGTH];
	m_pStorage->GetBinaryPath("update", aUpdatePath, sizeof(aUpdatePath));

	if(!RemovePathRecursively(aUpdatePath))
	{
		dbg_msg("updater", "ERROR: failed to clean update directory");
		return false;
	}

	if(fs_makedir(aUpdatePath) != 0 && !fs_is_dir(aUpdatePath))
	{
		dbg_msg("updater", "ERROR: failed to create update directory");
		return false;
	}

	return true;
}

void CUpdater::Update()
{
	switch(GetCurrentState())
	{
	case IUpdater::GOT_MANIFEST:
		PerformUpdate();
		break;
	case IUpdater::DOWNLOADING:
		RunningUpdate();
		break;
	case IUpdater::MOVE_FILES:
		CommitUpdate();
		break;
	default:
		return;
	}
}

void CUpdater::AddFileJob(const char *pFile, bool Job)
{
	m_FileJobs.emplace_front(pFile, Job);
}

bool CUpdater::ReplaceClient()
{
	dbg_msg("updater", "replacing " PLAT_CLIENT_EXEC);
	bool Success = true;
	char aPath[IO_MAX_PATH_LENGTH];

	m_pStorage->RemoveBinaryFile(CLIENT_EXEC ".old");
	Success &= m_pStorage->RenameBinaryFile(PLAT_CLIENT_EXEC, CLIENT_EXEC ".old");
	str_format(aPath, sizeof(aPath), "update/%s", m_aClientExecTmp);
	Success &= m_pStorage->RenameBinaryFile(aPath, PLAT_CLIENT_EXEC);
	m_pStorage->RemoveBinaryFile(CLIENT_EXEC ".old");
#if !defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPath(PLAT_CLIENT_EXEC, aPath, sizeof(aPath));
	char aBuf[1024];
	const string EscapedPath = EscapeUnixShellArgument(aPath);
	str_format(aBuf, sizeof(aBuf), "chmod +x %s", EscapedPath.c_str());
	if(system(aBuf))
	{
		dbg_msg("updater", "ERROR: failed to set client executable bit");
		Success = false;
	}
#endif
	return Success;
}

bool CUpdater::ReplaceServer()
{
	dbg_msg("updater", "replacing " PLAT_SERVER_EXEC);
	bool Success = true;
	char aPath[IO_MAX_PATH_LENGTH];

	m_pStorage->RemoveBinaryFile(SERVER_EXEC ".old");
	Success &= m_pStorage->RenameBinaryFile(PLAT_SERVER_EXEC, SERVER_EXEC ".old");
	str_format(aPath, sizeof(aPath), "update/%s", m_aServerExecTmp);
	Success &= m_pStorage->RenameBinaryFile(aPath, PLAT_SERVER_EXEC);
	m_pStorage->RemoveBinaryFile(SERVER_EXEC ".old");
#if !defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPath(PLAT_SERVER_EXEC, aPath, sizeof(aPath));
	char aBuf[1024];
	const string EscapedPath = EscapeUnixShellArgument(aPath);
	str_format(aBuf, sizeof(aBuf), "chmod +x %s", EscapedPath.c_str());
	if(system(aBuf))
	{
		dbg_msg("updater", "ERROR: failed to set server executable bit");
		Success = false;
	}
#endif
	return Success;
}

bool CUpdater::ParseUpdate()
{
	char aPath[IO_MAX_PATH_LENGTH];
	void *pBuf = nullptr;
	unsigned Length = 0;
	if(!m_pStorage->ReadFile(m_pStorage->GetBinaryPath(UPDATER_LOCAL_MANIFEST, aPath, sizeof(aPath)), IStorage::TYPE_ABSOLUTE, &pBuf, &Length))
	{
		dbg_msg("updater", "ERROR: failed to read update manifest");
		return false;
	}

	json_value *pManifest = json_parse(static_cast<json_char *>(pBuf), Length);
	free(pBuf);

	if(!pManifest || pManifest->type != json_object)
	{
		if(pManifest)
		{
			json_value_free(pManifest);
		}
		dbg_msg("updater", "ERROR: update manifest has invalid JSON");
		return false;
	}

	const char *pVersion = json_string_get(json_object_get(pManifest, "version"));
	const json_value *pArchive = json_object_get(pManifest, "archive");
	const char *pDownloadUrl = pArchive ? json_string_get(json_object_get(pArchive, "download_url")) : nullptr;
	const char *pFilename = pArchive ? json_string_get(json_object_get(pArchive, "filename")) : nullptr;
	const char *pFormat = pArchive ? json_string_get(json_object_get(pArchive, "format")) : nullptr;
	const char *pExtractRoot = pArchive ? json_string_get(json_object_get(pArchive, "extract_root")) : nullptr;
	const char *pSha256 = pArchive ? json_string_get(json_object_get(pArchive, "sha256")) : nullptr;

	m_Manifest.Reset();
	if(pVersion)
	{
		m_Manifest.m_Version = pVersion;
	}
	if(pDownloadUrl)
	{
		m_Manifest.m_DownloadUrl = pDownloadUrl;
	}
	if(pFilename)
	{
		m_Manifest.m_Filename = pFilename;
	}
	if(pFormat)
	{
		m_Manifest.m_Format = pFormat;
	}
	if(pExtractRoot)
	{
		m_Manifest.m_ExtractRoot = pExtractRoot;
	}
	if(pSha256 && sha256_from_str(&m_Manifest.m_Sha256, pSha256) == 0)
	{
		m_Manifest.m_HasSha256 = true;
	}

	const json_value *pPreserve = json_object_get(pManifest, "preserve");
	if(pPreserve && pPreserve->type == json_array)
	{
		for(int i = 0; i < json_array_length(pPreserve); ++i)
		{
			const char *pName = json_string_get(json_array_get(pPreserve, i));
			if(pName && pName[0] != '\0')
			{
				m_Manifest.m_PreservedFiles.insert(pName);
			}
		}
	}

	json_value_free(pManifest);

	if(!m_Manifest.IsValid())
	{
		dbg_msg("updater", "ERROR: update manifest is missing required fields");
		return false;
	}

	return true;
}

bool CUpdater::ExtractArchive()
{
	char aArchiveRelative[IO_MAX_PATH_LENGTH];
	str_format(aArchiveRelative, sizeof(aArchiveRelative), "update/%s", m_Manifest.m_Filename.c_str());

	char aArchiveAbsolute[IO_MAX_PATH_LENGTH];
	m_pStorage->GetBinaryPath(aArchiveRelative, aArchiveAbsolute, sizeof(aArchiveAbsolute));

	char aExtractedAbsolute[IO_MAX_PATH_LENGTH];
	m_pStorage->GetBinaryPath(UPDATER_LOCAL_EXTRACTED, aExtractedAbsolute, sizeof(aExtractedAbsolute));

	if(!RemovePathRecursively(aExtractedAbsolute))
	{
		dbg_msg("updater", "ERROR: failed to clear extracted update directory");
		return false;
	}

	if(!EnsureDirectoryExists(aExtractedAbsolute))
	{
		dbg_msg("updater", "ERROR: failed to create extracted update directory");
		return false;
	}

	char aCommand[4096];
#if defined(CONF_FAMILY_WINDOWS)
	if(str_comp(m_Manifest.m_Format.c_str(), "zip") != 0)
	{
		dbg_msg("updater", "ERROR: unsupported Windows update format '%s'", m_Manifest.m_Format.c_str());
		return false;
	}

	const string EscapedArchive = EscapePowerShellSingleQuoted(aArchiveAbsolute);
	const string EscapedDestination = EscapePowerShellSingleQuoted(aExtractedAbsolute);
	str_format(
		aCommand,
		sizeof(aCommand),
		"powershell -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\"",
		EscapedArchive.c_str(),
		EscapedDestination.c_str());
#else
	if(str_comp(m_Manifest.m_Format.c_str(), "tar.gz") != 0)
	{
		dbg_msg("updater", "ERROR: unsupported Unix update format '%s'", m_Manifest.m_Format.c_str());
		return false;
	}

	const string EscapedArchive = EscapeUnixShellArgument(aArchiveAbsolute);
	const string EscapedDestination = EscapeUnixShellArgument(aExtractedAbsolute);
	str_format(aCommand, sizeof(aCommand), "tar -xzf %s -C %s", EscapedArchive.c_str(), EscapedDestination.c_str());
#endif

	dbg_msg("updater", "extracting archive '%s'", aArchiveAbsolute);
	if(system(aCommand) != 0)
	{
		dbg_msg("updater", "ERROR: failed to extract update archive");
		return false;
	}

	return true;
}

bool CUpdater::ShouldPreserveFile(const char *pFile) const
{
	return m_Manifest.m_PreservedFiles.find(pFile) != m_Manifest.m_PreservedFiles.end();
}

bool CUpdater::StageExtractedFiles()
{
	char aSourceRootRelative[IO_MAX_PATH_LENGTH];
	str_format(aSourceRootRelative, sizeof(aSourceRootRelative), "%s/%s", UPDATER_LOCAL_EXTRACTED, m_Manifest.m_ExtractRoot.c_str());

	char aSourceRootAbsolute[IO_MAX_PATH_LENGTH];
	m_pStorage->GetBinaryPath(aSourceRootRelative, aSourceRootAbsolute, sizeof(aSourceRootAbsolute));
	if(!fs_is_dir(aSourceRootAbsolute))
	{
		str_copy(aSourceRootRelative, UPDATER_LOCAL_EXTRACTED, sizeof(aSourceRootRelative));
		m_pStorage->GetBinaryPath(aSourceRootRelative, aSourceRootAbsolute, sizeof(aSourceRootAbsolute));
		if(!fs_is_dir(aSourceRootAbsolute))
		{
			dbg_msg("updater", "ERROR: extracted update root is missing");
			return false;
		}
	}

	std::vector<string> vFiles;
	if(!CollectFilesRecursively(aSourceRootAbsolute, aSourceRootAbsolute, vFiles))
	{
		dbg_msg("updater", "ERROR: failed to list extracted update files");
		return false;
	}
	if(vFiles.empty())
	{
		dbg_msg("updater", "ERROR: extracted update archive is empty");
		return false;
	}
	std::sort(vFiles.begin(), vFiles.end());

	for(const string &File : vFiles)
	{
		if(ShouldPreserveFile(File.c_str()))
		{
			continue;
		}

		char aSourceRelative[IO_MAX_PATH_LENGTH];
		str_format(aSourceRelative, sizeof(aSourceRelative), "%s/%s", aSourceRootRelative, File.c_str());

		char aDestinationRelative[IO_MAX_PATH_LENGTH];
		if(str_comp(File.c_str(), PLAT_CLIENT_EXEC) == 0)
		{
			str_format(aDestinationRelative, sizeof(aDestinationRelative), "update/%s", m_aClientExecTmp);
			m_ClientUpdate = true;
		}
		else if(str_comp(File.c_str(), PLAT_SERVER_EXEC) == 0)
		{
			str_format(aDestinationRelative, sizeof(aDestinationRelative), "update/%s", m_aServerExecTmp);
			m_ServerUpdate = true;
		}
		else
		{
			str_format(aDestinationRelative, sizeof(aDestinationRelative), "update/%s", File.c_str());
			AddFileJob(File.c_str(), true);
		}

		if(!m_pStorage->RenameBinaryFile(aSourceRelative, aDestinationRelative))
		{
			dbg_msg("updater", "ERROR: failed to stage '%s'", File.c_str());
			return false;
		}
	}

	return true;
}

void CUpdater::InitiateUpdate()
{
	const EUpdaterState State = GetCurrentState();
	if(State != IUpdater::CLEAN && State != IUpdater::FAIL)
	{
		return;
	}

	ResetUpdateData();
	if(!PrepareUpdateDirectory())
	{
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	SetCurrentState(IUpdater::GETTING_MANIFEST);
	FetchUpdaterFile(GetUpdaterManifestPath(), UPDATER_LOCAL_MANIFEST);
}

void CUpdater::PerformUpdate()
{
	SetCurrentState(IUpdater::PARSING_UPDATE);
	dbg_msg("updater", "parsing update manifest");
	if(!ParseUpdate())
	{
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	char aArchiveDestination[IO_MAX_PATH_LENGTH];
	str_format(aArchiveDestination, sizeof(aArchiveDestination), "update/%s", m_Manifest.m_Filename.c_str());

	SetCurrentState(IUpdater::DOWNLOADING);
	FetchUrl(m_Manifest.m_DownloadUrl.c_str(), aArchiveDestination, &m_Manifest.m_Sha256);
}

void CUpdater::RunningUpdate()
{
	if(!m_pCurrentTask)
	{
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	if(!m_pCurrentTask->Done())
	{
		return;
	}

	if(m_pCurrentTask->State() == EHttpState::ERROR || m_pCurrentTask->State() == EHttpState::ABORTED)
	{
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	m_pCurrentTask = nullptr;
	SetCurrentStatus("");
	if(!ExtractArchive() || !StageExtractedFiles())
	{
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	SetCurrentState(IUpdater::MOVE_FILES);
}

void CUpdater::CommitUpdate()
{
	bool Success = true;

	for(auto &FileJob : m_FileJobs)
	{
		if(FileJob.second)
		{
			Success &= MoveFile(FileJob.first.c_str());
		}
	}

	if(m_ClientUpdate)
	{
		Success &= ReplaceClient();
	}
	if(m_ServerUpdate)
	{
		Success &= ReplaceServer();
	}

	if(Success)
	{
		for(const auto &[Filename, JobSuccess] : m_FileJobs)
		{
			if(!JobSuccess)
			{
				m_pStorage->RemoveBinaryFile(Filename.c_str());
			}
		}

		char aUpdatePath[IO_MAX_PATH_LENGTH];
		m_pStorage->GetBinaryPath("update", aUpdatePath, sizeof(aUpdatePath));
		if(!RemovePathRecursively(aUpdatePath))
		{
			dbg_msg("updater", "WARNING: failed to clean update directory after install");
		}
	}

	if(!Success)
	{
		SetCurrentState(IUpdater::FAIL);
	}
	else if(m_pClient->State() == IClient::STATE_ONLINE || m_pClient->EditorHasUnsavedData())
	{
		SetCurrentState(IUpdater::NEED_RESTART);
	}
	else
	{
		m_pClient->Restart();
	}
}