#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_CORE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_CORE_H

static constexpr const char *CATCLIENT_NAME_TAGS_URL = "https://catclient-tags-api.itsquomy.workers.dev/users.json";
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

#endif
