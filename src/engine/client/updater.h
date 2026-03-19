#ifndef ENGINE_CLIENT_UPDATER_H
#define ENGINE_CLIENT_UPDATER_H

#include <base/detect.h>
#include <base/hash.h>
#include <base/lock.h>

#include <engine/updater.h>

#include <game/version.h>

#include <forward_list>
#include <memory>
#include <string>
#include <unordered_set>

#define CLIENT_EXEC CLIENT_NAME
#define SERVER_EXEC CLIENT_NAME "-Server"

#if defined(CONF_PLATFORM_WIN64)
#define PLAT_EXT ".exe"
#define PLAT_NAME CONF_PLATFORM_STRING
#define UPDATER_RELEASE_PLATFORM "win64"
#elif defined(CONF_PLATFORM_LINUX) && defined(CONF_ARCH_AMD64)
#define PLAT_EXT ""
#define PLAT_NAME CONF_PLATFORM_STRING "-x86_64"
#define UPDATER_RELEASE_PLATFORM "linux_x86_64"
#else
#if defined(CONF_AUTOUPDATE)
#error Compiling with autoupdater on an unsupported platform
#endif
#define PLAT_EXT ""
#define PLAT_NAME "unsupported-unsupported"
#define UPDATER_RELEASE_PLATFORM "unsupported"
#endif

#define PLAT_CLIENT_DOWN CLIENT_EXEC "-" PLAT_NAME PLAT_EXT
#define PLAT_SERVER_DOWN SERVER_EXEC "-" PLAT_NAME PLAT_EXT

#define PLAT_CLIENT_EXEC CLIENT_EXEC PLAT_EXT
#define PLAT_SERVER_EXEC SERVER_EXEC PLAT_EXT

class CUpdaterFetchTask;

class CUpdater : public IUpdater
{
	friend class CUpdaterFetchTask;

	class IClient *m_pClient;
	class IStorage *m_pStorage;
	class IEngine *m_pEngine;
	class CHttp *m_pHttp;

	CLock m_Lock;

	EUpdaterState m_State GUARDED_BY(m_Lock);
	char m_aStatus[256] GUARDED_BY(m_Lock);
	int m_Percent GUARDED_BY(m_Lock);
	char m_aClientExecTmp[64];
	char m_aServerExecTmp[64];

	struct SUpdateManifest
	{
		std::string m_Version;
		std::string m_DownloadUrl;
		std::string m_Filename;
		std::string m_Format;
		std::string m_ExtractRoot;
		SHA256_DIGEST m_Sha256{};
		bool m_HasSha256 = false;
		std::unordered_set<std::string> m_PreservedFiles;

		void Reset()
		{
			m_Version.clear();
			m_DownloadUrl.clear();
			m_Filename.clear();
			m_Format.clear();
			m_ExtractRoot.clear();
			m_HasSha256 = false;
			m_PreservedFiles.clear();
		}

		bool IsValid() const
		{
			return !m_Version.empty() &&
				!m_DownloadUrl.empty() &&
				!m_Filename.empty() &&
				!m_Format.empty() &&
				!m_ExtractRoot.empty() &&
				m_HasSha256;
		}
	};

	SUpdateManifest m_Manifest;
	std::forward_list<std::pair<std::string, bool>> m_FileJobs;
	std::shared_ptr<CUpdaterFetchTask> m_pCurrentTask;

	bool m_ClientUpdate;
	bool m_ServerUpdate;

	void AddFileJob(const char *pFile, bool Job);
	void FetchUpdaterFile(const char *pFile, const char *pDestPath) REQUIRES(!m_Lock);
	void FetchUrl(const char *pUrl, const char *pDestPath, const SHA256_DIGEST *pExpectedSha256 = nullptr) REQUIRES(!m_Lock);
	bool MoveFile(const char *pFile);
	bool PrepareUpdateDirectory();
	bool ParseUpdate() REQUIRES(!m_Lock);
	bool ExtractArchive();
	bool StageExtractedFiles();
	bool ShouldPreserveFile(const char *pFile) const;
	void ResetUpdateData() REQUIRES(!m_Lock);

	void PerformUpdate() REQUIRES(!m_Lock);
	void RunningUpdate() REQUIRES(!m_Lock);
	void CommitUpdate() REQUIRES(!m_Lock);

	bool ReplaceClient();
	bool ReplaceServer();

	void SetCurrentStatus(const char *pStatus) REQUIRES(!m_Lock);
	void SetCurrentState(EUpdaterState NewState) REQUIRES(!m_Lock);

public:
	CUpdater();

	EUpdaterState GetCurrentState() override REQUIRES(!m_Lock);
	void GetCurrentFile(char *pBuf, int BufSize) override REQUIRES(!m_Lock);
	int GetCurrentPercent() override REQUIRES(!m_Lock);

	void InitiateUpdate() REQUIRES(!m_Lock) override;
	void Init(CHttp *pHttp);
	void Update() REQUIRES(!m_Lock) override;
};

#endif
