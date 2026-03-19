#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_LIFECYCLE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_LIFECYCLE_H

void CCatClient::OnConsoleInit()
{
	if(ConfigManager() != nullptr)
	{
		ConfigManager()->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::CATCLIENT);
	}

	Console()->Register("catclient_ignore_player", "s[player_name]", CFGFLAG_CLIENT, ConIgnorePlayer, this, "Add a player name to the CatClient ignore list");
	Console()->Register("catclient_unignore_player", "s[player_name]", CFGFLAG_CLIENT, ConUnignorePlayer, this, "Remove a player name from the CatClient ignore list");
}

void CCatClient::OnInit()
{
	ResetAutoTeamLock();
	ResetAntiKill();
	m_NameTags.Init(GameClient());
	LoadCursorAsset(g_Config.m_ClAssetCursor);
	EnsureCustomBackgroundFolder();
	EnsureSelectedCustomBackgroundLoaded();
	StartAutomaticCursorDownload();
	if(!Storage()->FileExists(CATCLIENT_DEFAULT_BACKGROUND_PATH, IStorage::TYPE_SAVE))
	{
		StartDefaultBackgroundDownload();
	}
	UpdateAspectRatioOverride();
}

void CCatClient::OnUpdate()
{
	if(m_pCursorDownloadTask && m_pCursorDownloadTask->Done())
	{
		FinishAutomaticCursorDownload();
		m_pCursorDownloadTask = nullptr;
	}
	if(m_pBackgroundDownloadTask && m_pBackgroundDownloadTask->Done())
	{
		FinishDefaultBackgroundDownload();
		m_pBackgroundDownloadTask = nullptr;
	}

	if(!Storage()->FileExists(CATCLIENT_DEFAULT_BACKGROUND_PATH, IStorage::TYPE_SAVE) && !m_pBackgroundDownloadTask)
	{
		const auto Now = time_get_nanoseconds();
		if(m_LastBackgroundAttempt == std::chrono::nanoseconds::zero() || Now - m_LastBackgroundAttempt >= DEFAULT_BACKGROUND_RETRY_INTERVAL)
		{
			StartDefaultBackgroundDownload();
		}
	}

	UpdateIgnoredPlayers();
	UpdateAspectRatioOverride();
	UpdateAntiKillState();
	EnsureSelectedCustomBackgroundLoaded();
	m_NameTags.Update();
	CheckAndSendLagMessage();
}

void CCatClient::OnReset()
{
	ResetAutoTeamLock();
	ResetAntiKill();
	m_NameTags.Reset();
}

void CCatClient::OnRender()
{
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		if(HasGameCustomBackground())
		{
			RenderCustomBackground();
		}
	}

	if(!g_Config.m_CcAutoTeamLock ||
		Client()->State() != IClient::STATE_ONLINE ||
		GameClient()->m_Snap.m_LocalClientId < 0 ||
		!GameClient()->m_Snap.m_pLocalCharacter ||
		!GameClient()->m_Snap.m_pLocalInfo)
	{
		ResetAutoTeamLock();
		return;
	}

	const int Team = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId);
	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		ResetAutoTeamLock();
		return;
	}

	if(IsLocalTeamLocked())
	{
		m_AutoTeamLockTeam = Team;
		m_AutoTeamLockStart = std::chrono::nanoseconds::zero();
		m_AutoTeamLockIssued = false;
		return;
	}

	const auto Now = time_get_nanoseconds();
	if(m_AutoTeamLockTeam != Team || m_AutoTeamLockStart == std::chrono::nanoseconds::zero())
	{
		m_AutoTeamLockTeam = Team;
		m_AutoTeamLockStart = Now;
		m_AutoTeamLockIssued = false;
	}

	if(m_AutoTeamLockIssued)
	{
		return;
	}

	if(Now - m_AutoTeamLockStart < std::chrono::seconds(g_Config.m_CcAutoTeamLockDelay))
	{
		return;
	}

	Console()->ExecuteLine("say /lock 1", IConsole::CLIENT_ID_UNSPECIFIED);
	m_AutoTeamLockIssued = true;
}

void CCatClient::OnStateChange(int NewState, int OldState)
{
	if(NewState != IClient::STATE_ONLINE)
	{
		ResetAutoTeamLock();
		ResetAntiKill();
	}

	m_NameTags.OnStateChange(NewState, OldState);
}

void CCatClient::OnShutdown()
{
	Graphics()->ClearScreenAspectOverride();
	m_NameTags.Shutdown();
	AbortTask(m_pCursorDownloadTask);
	AbortTask(m_pBackgroundDownloadTask);

	if(m_HasCustomCursor)
	{
		Graphics()->UnloadTexture(&m_CursorTexture);
		m_HasCustomCursor = false;
	}
	UnloadCustomBackgroundTexture();
	m_aLoadedBackgroundImage[0] = '\0';
}

void CCatClient::OnNewSnapshot()
{
	UpdateIgnoredPlayers();
	m_NameTags.Update();
}

#endif
