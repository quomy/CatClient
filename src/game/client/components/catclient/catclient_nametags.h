#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_CATCLIENT_NAMETAGS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_CATCLIENT_NAMETAGS_H

#include <base/types.h>

#include <engine/graphics.h>
#include <engine/shared/http.h>
#include <engine/shared/protocol.h>

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>

class CGameClient;

class CCatClientNameTags
{
	struct SLocalUser
	{
		int m_ClientId = -1;
		int m_DummyId = -1;
	};

	CGameClient *m_pGameClient = nullptr;
	std::shared_ptr<CHttpRequest> m_pFetchTask;
	std::shared_ptr<CHttpRequest> m_pPostTask;
	std::shared_ptr<CHttpRequest> m_pRemoveTask;
	std::shared_ptr<CHttpRequest> m_pServersTask;
	std::shared_ptr<CHttpRequest> m_pIconTask;
	std::array<bool, MAX_CLIENTS> m_aKnownPlayers{};
	std::unordered_set<std::string> m_KnownServers;
	char m_aCurrentServer[NETADDR_MAXSTRSIZE] = "";
	char m_aFetchServer[NETADDR_MAXSTRSIZE] = "";
	char m_aPostServer[NETADDR_MAXSTRSIZE] = "";
	char m_aRemoveServer[NETADDR_MAXSTRSIZE] = "";
	char m_aPendingRemovalServer[NETADDR_MAXSTRSIZE] = "";
	IGraphics::CTextureHandle m_CatIconTexture;
	bool m_HasCatIconTexture = false;
	std::chrono::nanoseconds m_LastFetch = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_LastPost = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_LastServersFetch = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_LastIconAttempt = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_LastRemoveAttempt = std::chrono::nanoseconds::zero();
	SLocalUser m_PendingRemovalUser{};
	SLocalUser m_RemoveUser{};
	SLocalUser m_LastLocalUser{};

	void AbortTask(std::shared_ptr<CHttpRequest> &pTask);
	void ResetKnownPlayers();
	void ResetKnownServers();
	void ResetOnlineState();
	bool UpdateCurrentServer();
	bool CollectLocalUser(SLocalUser &User) const;
	void CacheLocalUser(const SLocalUser &User);
	void ClearPendingRemoval();
	bool SameUser(const SLocalUser &First, const SLocalUser &Second) const;
	void MarkUserKnown(const SLocalUser &User);
	void QueueRemoval(const char *pServer);
	void StartFetch();
	void FinishFetch();
	void StartPost();
	void FinishPost();
	void StartRemove();
	void FinishRemove();
	void StartServersFetch();
	void FinishServersFetch();
	bool LoadCatIconFromFile();
	void StartIconDownload();
	void FinishIconDownload();
	void UnloadCatIcon();

public:
	void Init(CGameClient *pGameClient);
	void Reset();
	void Update();
	void OnStateChange(int NewState, int OldState);
	void Shutdown();

	bool HasCatTag(int ClientId) const;
	bool HasCatServer(const char *pAddress) const;
	bool HasCatIconTexture() const;
	const IGraphics::CTextureHandle &CatIconTexture() const;
	int KnownCatServerCount() const;
};

#endif
