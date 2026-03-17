#include "catclient_nametags.h"
#include <base/system.h>
#include <base/time.h>
#include <engine/client.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/storage.h>
#include <game/client/gameclient.h>
#include <chrono>
#include <string>

static constexpr const char *CATCLIENT_NAME_TAGS_URL = "https://tags.quomy.win/users.json";
static constexpr const char *CATCLIENT_ICON_URL = "https://tags.quomy.win/catproject.png";
static constexpr const char *CATCLIENT_ICON_PATH = "catclient/catproject.png";
static constexpr auto FETCH_INTERVAL = std::chrono::seconds(5);
static constexpr auto POST_INTERVAL = std::chrono::seconds(10);
static constexpr auto SERVERS_FETCH_INTERVAL = std::chrono::seconds(15);
static constexpr auto ICON_RETRY_INTERVAL = std::chrono::seconds(30);
static constexpr auto REMOVE_RETRY_INTERVAL = std::chrono::seconds(3);
static constexpr CTimeout REQUEST_TIMEOUT{5000, 0, 512, 5};
static constexpr CTimeout REMOVE_TIMEOUT{1500, 0, 512, 3};

static void NormalizeServerAddress(const char *pAddress, char *pOutput, size_t OutputSize)
{
	pOutput[0] = '\0';
	if(pAddress == nullptr || pAddress[0] == '\0')
	{
		return;
	}

	char aTrimmed[256];
	str_copy(aTrimmed, pAddress, sizeof(aTrimmed));
	char *pTrimmed = aTrimmed;
	while(*pTrimmed == ' ' || *pTrimmed == '\t')
	{
		++pTrimmed;
	}
	str_utf8_trim_right(pTrimmed);

	const char *pNormalized = str_startswith(pTrimmed, "tw-0.7+udp://");
	if(!pNormalized)
	{
		pNormalized = str_startswith(pTrimmed, "udp://");
	}
	if(!pNormalized)
	{
		pNormalized = pTrimmed;
	}

	str_copy(pOutput, pNormalized, OutputSize);
}

void CCatClientNameTags::AbortTask(std::shared_ptr<CHttpRequest> &pTask)
{
	if(pTask)
	{
		pTask->Abort();
		pTask = nullptr;
	}
}

void CCatClientNameTags::ResetKnownPlayers()
{
	m_aKnownPlayers.fill(false);
}

void CCatClientNameTags::ResetKnownServers()
{
	m_KnownServers.clear();
}

void CCatClientNameTags::ResetOnlineState()
{
	AbortTask(m_pFetchTask);
	AbortTask(m_pPostTask);
	ResetKnownPlayers();
	m_aCurrentServer[0] = '\0';
	m_aFetchServer[0] = '\0';
	m_aPostServer[0] = '\0';
	m_LastFetch = std::chrono::nanoseconds::zero();
	m_LastPost = std::chrono::nanoseconds::zero();
}

bool CCatClientNameTags::CollectLocalUser(SLocalUser &User) const
{
	User = {};
	if(m_pGameClient == nullptr)
	{
		return false;
	}

	int MainClientId = m_pGameClient->m_aLocalIds[0];
	if(MainClientId < 0)
	{
		MainClientId = m_pGameClient->m_Snap.m_LocalClientId;
	}

	if(MainClientId < 0 || MainClientId >= MAX_CLIENTS || !m_pGameClient->m_aClients[MainClientId].m_Active)
	{
		return false;
	}

	User.m_ClientId = MainClientId;

	const int DummyClientId = m_pGameClient->m_aLocalIds[1];
	if(DummyClientId >= 0 &&
		DummyClientId < MAX_CLIENTS &&
		DummyClientId != MainClientId &&
		m_pGameClient->m_aClients[DummyClientId].m_Active)
	{
		User.m_DummyId = DummyClientId;
	}

	return true;
}

void CCatClientNameTags::CacheLocalUser(const SLocalUser &User)
{
	if(User.m_ClientId >= 0)
	{
		m_LastLocalUser = User;
	}
}

void CCatClientNameTags::ClearPendingRemoval()
{
	m_aPendingRemovalServer[0] = '\0';
	m_PendingRemovalUser = {};
}

bool CCatClientNameTags::SameUser(const SLocalUser &First, const SLocalUser &Second) const
{
	return First.m_ClientId == Second.m_ClientId && First.m_DummyId == Second.m_DummyId;
}

void CCatClientNameTags::MarkUserKnown(const SLocalUser &User)
{
	if(User.m_ClientId >= 0 && User.m_ClientId < MAX_CLIENTS)
	{
		m_aKnownPlayers[User.m_ClientId] = true;
	}
	if(User.m_DummyId >= 0 && User.m_DummyId < MAX_CLIENTS)
	{
		m_aKnownPlayers[User.m_DummyId] = true;
	}
}

void CCatClientNameTags::QueueRemoval(const char *pServer)
{
	if(pServer == nullptr || pServer[0] == '\0')
	{
		return;
	}

	SLocalUser User;
	if(CollectLocalUser(User))
	{
		CacheLocalUser(User);
	}
	else if(m_LastLocalUser.m_ClientId >= 0)
	{
		User = m_LastLocalUser;
	}
	else
	{
		return;
	}

	str_copy(m_aPendingRemovalServer, pServer, sizeof(m_aPendingRemovalServer));
	m_PendingRemovalUser = User;
	m_LastRemoveAttempt = std::chrono::nanoseconds::zero();
}

bool CCatClientNameTags::UpdateCurrentServer()
{
	if(m_pGameClient == nullptr || m_pGameClient->Client()->State() != IClient::STATE_ONLINE)
	{
		return false;
	}

	SLocalUser User;
	if(CollectLocalUser(User))
	{
		CacheLocalUser(User);
	}

	char aServer[NETADDR_MAXSTRSIZE];
	net_addr_str(&m_pGameClient->Client()->ServerAddress(), aServer, sizeof(aServer), true);
	if(aServer[0] == '\0')
	{
		return false;
	}

	if(str_comp(m_aCurrentServer, aServer) != 0)
	{
		if(m_aCurrentServer[0] != '\0')
		{
			QueueRemoval(m_aCurrentServer);
		}

		str_copy(m_aCurrentServer, aServer, sizeof(m_aCurrentServer));
		m_aFetchServer[0] = '\0';
		m_aPostServer[0] = '\0';
		m_LastFetch = std::chrono::nanoseconds::zero();
		m_LastPost = std::chrono::nanoseconds::zero();
		AbortTask(m_pFetchTask);
		AbortTask(m_pPostTask);
		ResetKnownPlayers();
	}

	return true;
}

void CCatClientNameTags::StartFetch()
{
	if(m_pGameClient == nullptr || m_aCurrentServer[0] == '\0')
	{
		return;
	}

	char aEscapedServer[NETADDR_MAXSTRSIZE * 3];
	EscapeUrl(aEscapedServer, m_aCurrentServer);

	char aUrl[512];
	str_format(aUrl, sizeof(aUrl), "%s?server=%s", CATCLIENT_NAME_TAGS_URL, aEscapedServer);

	m_pFetchTask = HttpGet(aUrl);
	m_pFetchTask->Timeout(REQUEST_TIMEOUT);
	m_pFetchTask->IpResolve(IPRESOLVE::V4);
	m_pFetchTask->MaxResponseSize(64 * 1024);
	m_pFetchTask->LogProgress(HTTPLOG::NONE);
	m_pFetchTask->FailOnErrorStatus(false);
	str_copy(m_aFetchServer, m_aCurrentServer, sizeof(m_aFetchServer));
	m_LastFetch = time_get_nanoseconds();
	m_pGameClient->Http()->Run(m_pFetchTask);
}

void CCatClientNameTags::FinishFetch()
{
	if(!m_pFetchTask || m_pFetchTask->State() != EHttpState::DONE || str_comp(m_aFetchServer, m_aCurrentServer) != 0 || m_pFetchTask->StatusCode() >= 400)
	{
		return;
	}

	json_value *pJson = m_pFetchTask->ResultJson();
	if(pJson == nullptr || pJson->type != json_object)
	{
		if(pJson != nullptr)
		{
			json_value_free(pJson);
		}
		return;
	}

	const char *pServer = json_string_get(json_object_get(pJson, "server"));
	if(pServer != nullptr && pServer[0] != '\0' && str_comp(pServer, m_aCurrentServer) != 0)
	{
		json_value_free(pJson);
		return;
	}

	std::array<bool, MAX_CLIENTS> aKnownPlayers{};
	const json_value *pUsers = json_object_get(pJson, "users");
	if(pUsers != &json_value_none && pUsers->type == json_array)
	{
		for(int Index = 0; Index < json_array_length(pUsers); ++Index)
		{
			const json_value *pUser = json_array_get(pUsers, Index);
			if(pUser == &json_value_none || pUser->type != json_object)
			{
				continue;
			}

			const json_value *pClientId = json_object_get(pUser, "clientId");
			if(pClientId == &json_value_none || pClientId->type != json_integer)
			{
				continue;
			}

			const int ClientId = json_int_get(pClientId);
			if(ClientId >= 0 && ClientId < MAX_CLIENTS)
			{
				aKnownPlayers[ClientId] = true;
			}

			const json_value *pDummyId = json_object_get(pUser, "dummyId");
			if(pDummyId != &json_value_none && pDummyId->type == json_integer)
			{
				const int DummyId = json_int_get(pDummyId);
				if(DummyId >= 0 && DummyId < MAX_CLIENTS)
				{
					aKnownPlayers[DummyId] = true;
				}
			}
		}
	}

	m_aKnownPlayers = aKnownPlayers;
	json_value_free(pJson);
}

void CCatClientNameTags::StartPost()
{
	if(m_pGameClient == nullptr || m_aCurrentServer[0] == '\0')
	{
		return;
	}

	SLocalUser User;
	if(!CollectLocalUser(User))
	{
		return;
	}
	CacheLocalUser(User);

	CJsonStringWriter Writer;
	Writer.BeginObject();
	Writer.WriteAttribute("server");
	Writer.WriteStrValue(m_aCurrentServer);
	Writer.WriteAttribute("users");
	Writer.BeginArray();
	Writer.BeginObject();
	Writer.WriteAttribute("clientId");
	Writer.WriteIntValue(User.m_ClientId);
	Writer.WriteAttribute("dummyId");
	if(User.m_DummyId >= 0)
	{
		Writer.WriteIntValue(User.m_DummyId);
	}
	else
	{
		Writer.WriteNullValue();
	}
	Writer.EndObject();
	Writer.EndArray();
	Writer.EndObject();

	const std::string Body = Writer.GetOutputString();
	m_pPostTask = HttpPostJson(CATCLIENT_NAME_TAGS_URL, Body.c_str());
	m_pPostTask->Timeout(REQUEST_TIMEOUT);
	m_pPostTask->IpResolve(IPRESOLVE::V4);
	m_pPostTask->MaxResponseSize(16 * 1024);
	m_pPostTask->LogProgress(HTTPLOG::NONE);
	m_pPostTask->FailOnErrorStatus(false);
	str_copy(m_aPostServer, m_aCurrentServer, sizeof(m_aPostServer));
	m_LastPost = time_get_nanoseconds();
	m_pGameClient->Http()->Run(m_pPostTask);
}

void CCatClientNameTags::FinishPost()
{
	if(!m_pPostTask || m_pPostTask->State() != EHttpState::DONE || str_comp(m_aPostServer, m_aCurrentServer) != 0 || m_pPostTask->StatusCode() >= 400)
	{
		return;
	}

	SLocalUser User;
	if(CollectLocalUser(User))
	{
		CacheLocalUser(User);
		MarkUserKnown(User);
	}
	else
	{
		MarkUserKnown(m_LastLocalUser);
	}
}

void CCatClientNameTags::StartRemove()
{
	if(m_pGameClient == nullptr || m_pRemoveTask || m_aPendingRemovalServer[0] == '\0' || m_PendingRemovalUser.m_ClientId < 0)
	{
		return;
	}

	CJsonStringWriter Writer;
	Writer.BeginObject();
	Writer.WriteAttribute("server");
	Writer.WriteStrValue(m_aPendingRemovalServer);
	Writer.WriteAttribute("remove");
	Writer.WriteBoolValue(true);
	Writer.WriteAttribute("users");
	Writer.BeginArray();
	Writer.BeginObject();
	Writer.WriteAttribute("clientId");
	Writer.WriteIntValue(m_PendingRemovalUser.m_ClientId);
	Writer.WriteAttribute("dummyId");
	if(m_PendingRemovalUser.m_DummyId >= 0)
	{
		Writer.WriteIntValue(m_PendingRemovalUser.m_DummyId);
	}
	else
	{
		Writer.WriteNullValue();
	}
	Writer.EndObject();
	Writer.EndArray();
	Writer.EndObject();

	const std::string Body = Writer.GetOutputString();
	m_pRemoveTask = HttpPostJson(CATCLIENT_NAME_TAGS_URL, Body.c_str());
	m_pRemoveTask->Timeout(REMOVE_TIMEOUT);
	m_pRemoveTask->IpResolve(IPRESOLVE::V4);
	m_pRemoveTask->MaxResponseSize(16 * 1024);
	m_pRemoveTask->LogProgress(HTTPLOG::NONE);
	m_pRemoveTask->FailOnErrorStatus(false);
	str_copy(m_aRemoveServer, m_aPendingRemovalServer, sizeof(m_aRemoveServer));
	m_RemoveUser = m_PendingRemovalUser;
	m_LastRemoveAttempt = time_get_nanoseconds();
	m_pGameClient->Http()->Run(m_pRemoveTask);
}

void CCatClientNameTags::FinishRemove()
{
	if(!m_pRemoveTask || m_pRemoveTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pRemoveTask->StatusCode() < 400 &&
		str_comp(m_aPendingRemovalServer, m_aRemoveServer) == 0 &&
		SameUser(m_PendingRemovalUser, m_RemoveUser))
	{
		ClearPendingRemoval();
		m_LastRemoveAttempt = std::chrono::nanoseconds::zero();
	}

	m_aRemoveServer[0] = '\0';
	m_RemoveUser = {};
}

void CCatClientNameTags::StartServersFetch()
{
	if(m_pGameClient == nullptr)
	{
		return;
	}

	m_pServersTask = HttpGet(CATCLIENT_NAME_TAGS_URL);
	m_pServersTask->Timeout(REQUEST_TIMEOUT);
	m_pServersTask->IpResolve(IPRESOLVE::V4);
	m_pServersTask->MaxResponseSize(256 * 1024);
	m_pServersTask->LogProgress(HTTPLOG::NONE);
	m_pServersTask->FailOnErrorStatus(false);
	m_LastServersFetch = time_get_nanoseconds();
	m_pGameClient->Http()->Run(m_pServersTask);
}

void CCatClientNameTags::FinishServersFetch()
{
	if(!m_pServersTask || m_pServersTask->State() != EHttpState::DONE || m_pServersTask->StatusCode() >= 400)
	{
		return;
	}

	json_value *pJson = m_pServersTask->ResultJson();
	if(pJson == nullptr || pJson->type != json_object)
	{
		if(pJson != nullptr)
		{
			json_value_free(pJson);
		}
		return;
	}

	std::unordered_set<std::string> KnownServers;
	const json_value *pServers = json_object_get(pJson, "servers");
	if(pServers != &json_value_none && pServers->type == json_array)
	{
		for(int Index = 0; Index < json_array_length(pServers); ++Index)
		{
			const json_value *pServer = json_array_get(pServers, Index);
			if(pServer == &json_value_none || pServer->type != json_object)
			{
				continue;
			}

			const char *pAddress = json_string_get(json_object_get(pServer, "server"));
			char aNormalized[NETADDR_MAXSTRSIZE];
			NormalizeServerAddress(pAddress, aNormalized, sizeof(aNormalized));
			if(aNormalized[0] != '\0')
			{
				KnownServers.insert(aNormalized);
			}
		}
	}

	if(m_aCurrentServer[0] != '\0')
	{
		KnownServers.insert(m_aCurrentServer);
	}

	m_KnownServers = std::move(KnownServers);
	json_value_free(pJson);
}

bool CCatClientNameTags::LoadCatIconFromFile()
{
	if(m_pGameClient == nullptr || !m_pGameClient->Storage()->FileExists(CATCLIENT_ICON_PATH, IStorage::TYPE_SAVE))
	{
		return false;
	}

	const IGraphics::CTextureHandle Texture = m_pGameClient->Graphics()->LoadTexture(CATCLIENT_ICON_PATH, IStorage::TYPE_SAVE);
	if(Texture.IsNullTexture())
	{
		return false;
	}

	if(m_HasCatIconTexture)
	{
		m_pGameClient->Graphics()->UnloadTexture(&m_CatIconTexture);
	}

	m_CatIconTexture = Texture;
	m_HasCatIconTexture = true;
	return true;
}

void CCatClientNameTags::StartIconDownload()
{
	if(m_pGameClient == nullptr || m_pIconTask || m_HasCatIconTexture)
	{
		return;
	}

	m_pIconTask = HttpGetBoth(CATCLIENT_ICON_URL, m_pGameClient->Storage(), CATCLIENT_ICON_PATH, IStorage::TYPE_SAVE);
	m_pIconTask->Timeout(REQUEST_TIMEOUT);
	m_pIconTask->IpResolve(IPRESOLVE::V4);
	m_pIconTask->MaxResponseSize(256 * 1024);
	m_pIconTask->LogProgress(HTTPLOG::NONE);
	m_pIconTask->FailOnErrorStatus(false);
	m_LastIconAttempt = time_get_nanoseconds();
	m_pGameClient->Http()->Run(m_pIconTask);
}

void CCatClientNameTags::FinishIconDownload()
{
	if(!m_pIconTask || m_pIconTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pIconTask->StatusCode() < 400)
	{
		LoadCatIconFromFile();
	}
}

void CCatClientNameTags::UnloadCatIcon()
{
	if(m_pGameClient != nullptr && m_HasCatIconTexture)
	{
		m_pGameClient->Graphics()->UnloadTexture(&m_CatIconTexture);
	}
	m_CatIconTexture = IGraphics::CTextureHandle();
	m_HasCatIconTexture = false;
}

void CCatClientNameTags::Init(CGameClient *pGameClient)
{
	m_pGameClient = pGameClient;
	Reset();
	LoadCatIconFromFile();
	StartIconDownload();
	StartServersFetch();
}

void CCatClientNameTags::Reset()
{
	ResetOnlineState();
}

void CCatClientNameTags::Update()
{
	if(m_pFetchTask && m_pFetchTask->Done())
	{
		FinishFetch();
		m_pFetchTask = nullptr;
	}

	if(m_pPostTask && m_pPostTask->Done())
	{
		FinishPost();
		m_pPostTask = nullptr;
	}

	if(m_pRemoveTask && m_pRemoveTask->Done())
	{
		FinishRemove();
		m_pRemoveTask = nullptr;
	}

	if(m_pServersTask && m_pServersTask->Done())
	{
		FinishServersFetch();
		m_pServersTask = nullptr;
	}

	if(m_pIconTask && m_pIconTask->Done())
	{
		FinishIconDownload();
		m_pIconTask = nullptr;
	}

	const auto Now = time_get_nanoseconds();
	if(!m_pServersTask && (m_LastServersFetch == std::chrono::nanoseconds::zero() || Now - m_LastServersFetch >= SERVERS_FETCH_INTERVAL))
	{
		StartServersFetch();
	}

	if(!m_HasCatIconTexture && !m_pIconTask)
	{
		if(!LoadCatIconFromFile() && (m_LastIconAttempt == std::chrono::nanoseconds::zero() || Now - m_LastIconAttempt >= ICON_RETRY_INTERVAL))
		{
			StartIconDownload();
		}
	}

	if(!UpdateCurrentServer())
	{
		if(!m_pRemoveTask &&
			m_aPendingRemovalServer[0] != '\0' &&
			(m_LastRemoveAttempt == std::chrono::nanoseconds::zero() || Now - m_LastRemoveAttempt >= REMOVE_RETRY_INTERVAL))
		{
			StartRemove();
		}
		ResetOnlineState();
		return;
	}

	if(!m_pRemoveTask &&
		m_aPendingRemovalServer[0] != '\0' &&
		(m_LastRemoveAttempt == std::chrono::nanoseconds::zero() || Now - m_LastRemoveAttempt >= REMOVE_RETRY_INTERVAL))
	{
		StartRemove();
	}
	if(!m_pFetchTask && (m_LastFetch == std::chrono::nanoseconds::zero() || Now - m_LastFetch >= FETCH_INTERVAL))
	{
		StartFetch();
	}

	if(!m_pPostTask && (m_LastPost == std::chrono::nanoseconds::zero() || Now - m_LastPost >= POST_INTERVAL))
	{
		StartPost();
	}
}

void CCatClientNameTags::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE && NewState != IClient::STATE_ONLINE)
	{
		QueueRemoval(m_aCurrentServer);
	}

	if(NewState != IClient::STATE_ONLINE)
	{
		ResetOnlineState();
	}
}

void CCatClientNameTags::Shutdown()
{
	if(m_aCurrentServer[0] != '\0')
	{
		QueueRemoval(m_aCurrentServer);
	}

	if(!m_pRemoveTask && m_aPendingRemovalServer[0] != '\0')
	{
		StartRemove();
	}

	if(m_pRemoveTask)
	{
		m_pRemoveTask->Wait();
		FinishRemove();
		m_pRemoveTask = nullptr;
	}

	ResetOnlineState();
	AbortTask(m_pServersTask);
	AbortTask(m_pIconTask);
	AbortTask(m_pRemoveTask);
	ClearPendingRemoval();
	ResetKnownServers();
	m_aRemoveServer[0] = '\0';
	m_RemoveUser = {};
	m_LastLocalUser = {};
	m_LastRemoveAttempt = std::chrono::nanoseconds::zero();
	m_LastServersFetch = std::chrono::nanoseconds::zero();
	m_LastIconAttempt = std::chrono::nanoseconds::zero();
	UnloadCatIcon();
}

bool CCatClientNameTags::HasCatTag(int ClientId) const
{
	if(m_pGameClient == nullptr || ClientId < 0 || ClientId >= MAX_CLIENTS)
	{
		return false;
	}

	if(m_aKnownPlayers[ClientId])
	{
		return true;
	}

	if(m_aCurrentServer[0] == '\0' || m_pGameClient->Client()->State() != IClient::STATE_ONLINE || !m_pGameClient->m_aClients[ClientId].m_Active)
	{
		return false;
	}

	for(const int LocalId : m_pGameClient->m_aLocalIds)
	{
		if(LocalId == ClientId)
		{
			return true;
		}
	}

	return false;
}

bool CCatClientNameTags::HasCatServer(const char *pAddress) const
{
	if(pAddress == nullptr || pAddress[0] == '\0')
	{
		return false;
	}

	const char *pCursor = pAddress;
	char aToken[NETADDR_MAXSTRSIZE + 32];
	while((pCursor = str_next_token(pCursor, ",", aToken, sizeof(aToken))))
	{
		char aNormalized[NETADDR_MAXSTRSIZE];
		NormalizeServerAddress(aToken, aNormalized, sizeof(aNormalized));
		if(aNormalized[0] == '\0')
		{
			continue;
		}

		if(m_KnownServers.contains(aNormalized))
		{
			return true;
		}

		if(m_aCurrentServer[0] != '\0' && str_comp(m_aCurrentServer, aNormalized) == 0)
		{
			return true;
		}
	}

	char aNormalized[NETADDR_MAXSTRSIZE];
	NormalizeServerAddress(pAddress, aNormalized, sizeof(aNormalized));
	return aNormalized[0] != '\0' && (m_KnownServers.contains(aNormalized) || (m_aCurrentServer[0] != '\0' && str_comp(m_aCurrentServer, aNormalized) == 0));
}

bool CCatClientNameTags::HasCatIconTexture() const
{
	return m_HasCatIconTexture;
}

const IGraphics::CTextureHandle &CCatClientNameTags::CatIconTexture() const
{
	return m_CatIconTexture;
}

int CCatClientNameTags::KnownCatServerCount() const
{
	return (int)m_KnownServers.size();
}
