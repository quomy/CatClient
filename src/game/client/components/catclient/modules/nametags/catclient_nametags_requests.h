#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_REQUESTS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_REQUESTS_H

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

#endif
