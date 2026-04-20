#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_STATE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_STATE_H

void CCatClient::AbortTask(std::shared_ptr<CHttpRequest> &pTask)
{
	if(!pTask)
	{
		return;
	}

	if(!pTask->Done())
	{
		pTask->Abort();
	}
	pTask = nullptr;
}

void CCatClient::ResetAutoTeamLock()
{
	m_AutoTeamLockTeam = TEAM_FLOCK;
	m_AutoTeamLockStart = std::chrono::nanoseconds::zero();
	m_AutoTeamLockIssued = false;
}

void CCatClient::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CCatClient *pThis = static_cast<CCatClient *>(pUserData);
	char aBuf[IConsole::CMDLINE_LENGTH];
	for(const std::string &IgnoredPlayer : pThis->m_vIgnoredPlayers)
	{
		char aEscapedName[MAX_NAME_LENGTH * 2];
		EscapeConfigParam(aEscapedName, IgnoredPlayer.c_str(), sizeof(aEscapedName));
		str_format(aBuf, sizeof(aBuf), "catclient_ignore_player \"%s\"", aEscapedName);
		pConfigManager->WriteLine(aBuf, ConfigDomain::CATCLIENT);
	}
}

void CCatClient::ConIgnorePlayer(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCatClient *>(pUserData)->SetPlayerIgnoredInternal(pResult->GetString(0), true, false);
}

void CCatClient::ConUnignorePlayer(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCatClient *>(pUserData)->SetPlayerIgnoredInternal(pResult->GetString(0), false, false);
}

bool CCatClient::IsLocalTeamLocked() const
{
	const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalClientId < 0)
	{
		return false;
	}

	const auto &LocalCharacter = GameClient()->m_Snap.m_aCharacters[LocalClientId];
	return LocalCharacter.m_Active &&
	       LocalCharacter.m_HasExtendedDisplayInfo &&
	       (LocalCharacter.m_ExtendedData.m_Flags & CHARACTERFLAG_LOCK_MODE) != 0;
}

bool CCatClient::IsLocalClientId(int ClientId) const
{
	if(ClientId < 0)
	{
		return false;
	}

	for(const int LocalId : GameClient()->m_aLocalIds)
	{
		if(LocalId == ClientId)
		{
			return true;
		}
	}
	return false;
}

bool CCatClient::IsLikelyLocalHammerHit(vec2 Pos) const
{
	for(const int LocalId : GameClient()->m_aLocalIds)
	{
		if(LocalId < 0 || !GameClient()->m_aClients[LocalId].m_Active)
		{
			continue;
		}

		if(distance(GameClient()->m_aClients[LocalId].m_RenderPos, Pos) <= 96.0f)
		{
			return true;
		}
	}

	return false;
}

void CCatClient::UpdateAspectRatioOverride()
{
	if(!g_Config.m_CcAspectRatioEnabled)
	{
		if(m_LastAspectRatioEnabled)
		{
			Graphics()->ClearScreenAspectOverride();
			m_LastAspectRatioEnabled = false;
			m_LastAppliedAspectRatio = 0.0f;
		}
		return;
	}

	float AspectRatio = g_Config.m_CcAspectRatio / 100.0f;
	if(g_Config.m_CcAspectRatioCustom != 0 && g_Config.m_CcAspectRatioCustomY > 0)
	{
		AspectRatio = (float)maximum(g_Config.m_CcAspectRatioCustomX, 1) / (float)g_Config.m_CcAspectRatioCustomY;
	}

	AspectRatio = std::clamp(AspectRatio, 1.0f, 4.0f);
	const float AspectRatioDiff = m_LastAppliedAspectRatio - AspectRatio;
	if(!m_LastAspectRatioEnabled || AspectRatioDiff < -0.0001f || AspectRatioDiff > 0.0001f)
	{
		Graphics()->SetScreenAspectOverride(AspectRatio);
		m_LastAspectRatioEnabled = true;
		m_LastAppliedAspectRatio = AspectRatio;
	}
}

void CCatClient::UpdateIgnoredPlayers()
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		auto &Client = GameClient()->m_aClients[ClientId];
		Client.m_ChatIgnore = Client.m_Active && IsPlayerIgnored(Client.m_aName);
	}
}

#endif
