#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_LIFECYCLE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_LIFECYCLE_H

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

#endif
