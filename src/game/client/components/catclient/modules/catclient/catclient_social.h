#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_SOCIAL_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_SOCIAL_H

bool CCatClient::SetPlayerIgnoredInternal(const char *pPlayerName, bool Ignored, bool SaveConfig)
{
	if(pPlayerName == nullptr || pPlayerName[0] == '\0')
	{
		return false;
	}

	const auto It = std::find_if(m_vIgnoredPlayers.begin(), m_vIgnoredPlayers.end(), [pPlayerName](const std::string &IgnoredPlayer) {
		return str_comp_nocase(IgnoredPlayer.c_str(), pPlayerName) == 0;
	});

	if(Ignored)
	{
		if(It != m_vIgnoredPlayers.end())
		{
			return false;
		}

		m_vIgnoredPlayers.emplace_back(pPlayerName);
		std::sort(m_vIgnoredPlayers.begin(), m_vIgnoredPlayers.end(), [](const std::string &Left, const std::string &Right) {
			return str_comp_nocase(Left.c_str(), Right.c_str()) < 0;
		});
	}
	else
	{
		if(It == m_vIgnoredPlayers.end())
		{
			return false;
		}

		m_vIgnoredPlayers.erase(It);
	}

	UpdateIgnoredPlayers();
	if(SaveConfig && ConfigManager() != nullptr)
	{
		ConfigManager()->Save();
	}
	return true;
}

bool CCatClient::IsPlayerIgnored(const char *pPlayerName) const
{
	if(pPlayerName == nullptr || pPlayerName[0] == '\0')
	{
		return false;
	}

	return std::any_of(m_vIgnoredPlayers.begin(), m_vIgnoredPlayers.end(), [pPlayerName](const std::string &IgnoredPlayer) {
		return str_comp_nocase(IgnoredPlayer.c_str(), pPlayerName) == 0;
	});
}

bool CCatClient::IsPlayerIgnored(int ClientId) const
{
	return ClientId >= 0 && ClientId < MAX_CLIENTS && IsPlayerIgnored(GameClient()->m_aClients[ClientId].m_aName);
}

bool CCatClient::IgnorePlayer(const char *pPlayerName)
{
	return SetPlayerIgnoredInternal(pPlayerName, true, true);
}

bool CCatClient::UnignorePlayer(const char *pPlayerName)
{
	return SetPlayerIgnoredInternal(pPlayerName, false, true);
}

bool CCatClient::InvitePlayer(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GameClient()->m_aClients[ClientId].m_Active)
	{
		return false;
	}

	const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalClientId < 0)
	{
		return false;
	}

	const int LocalTeam = GameClient()->m_Teams.Team(LocalClientId);
	if(LocalTeam <= TEAM_FLOCK || LocalTeam >= TEAM_SUPER || GameClient()->m_Teams.Team(ClientId) == LocalTeam)
	{
		return false;
	}

	char aCommand[2 * MAX_NAME_LENGTH + 32];
	char *pDst = aCommand;
	str_copy(aCommand, "say /invite \"", sizeof(aCommand));
	pDst = aCommand + str_length(aCommand);
	str_escape(&pDst, GameClient()->m_aClients[ClientId].m_aName, aCommand + sizeof(aCommand));
	str_append(aCommand, "\"", sizeof(aCommand));
	Console()->ExecuteLine(aCommand, IConsole::CLIENT_ID_UNSPECIFIED);
	return true;
}

bool CCatClient::HasCatTag(int ClientId) const
{
	return m_NameTags.HasCatTag(ClientId);
}

bool CCatClient::HasCatServer(const char *pAddress) const
{
	return m_NameTags.HasCatServer(pAddress);
}

int CCatClient::KnownCatServerCount() const
{
	return m_NameTags.KnownCatServerCount();
}

bool CCatClient::HasCatIconTexture() const
{
	return m_NameTags.HasCatIconTexture();
}

const IGraphics::CTextureHandle &CCatClient::CatIconTexture() const
{
	return m_NameTags.CatIconTexture();
}

void CCatClient::RenderCatIcon(const CUIRect &Rect, float Alpha) const
{
	if(!HasCatIconTexture())
	{
		return;
	}

	CUIRect Icon = Rect;
	Icon.VMargin(std::max(0.0f, (Icon.w - Icon.h) / 2.0f), &Icon);

	Graphics()->WrapClamp();
	Graphics()->TextureSet(CatIconTexture());
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	const IGraphics::CQuadItem QuadItem(Icon.x, Icon.y, Icon.w, Icon.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

const char *CCatClient::ResolveAudioFile(const char *pDefaultPath, char *pBuffer, size_t BufferSize) const
{
	if(str_comp(g_Config.m_ClAssetAudio, "default") == 0)
	{
		return pDefaultPath;
	}

	const char *pRelativePath = str_startswith(pDefaultPath, "audio/");
	if(!pRelativePath)
	{
		return pDefaultPath;
	}

	str_format(pBuffer, BufferSize, "assets/audio/%s/%s", g_Config.m_ClAssetAudio, pRelativePath);
	if(Storage()->FileExists(pBuffer, IStorage::TYPE_ALL))
	{
		return pBuffer;
	}

	str_format(pBuffer, BufferSize, "audio/%s/%s", g_Config.m_ClAssetAudio, pRelativePath);
	return Storage()->FileExists(pBuffer, IStorage::TYPE_ALL) ? pBuffer : pDefaultPath;
}

bool CCatClient::HasMuteSoundFlag(int Flag) const
{
	return (g_Config.m_CcMuteSounds & Flag) != 0;
}

bool CCatClient::HasHideEffectFlag(int Flag) const
{
	return (g_Config.m_CcHideEffects & Flag) != 0;
}

bool CCatClient::ShouldMuteSound(int SoundId, int OwnerId, const vec2 *pSoundPos) const
{
	const bool IsLocalSound = IsLocalClientId(OwnerId);

	switch(SoundId)
	{
	case SOUND_HOOK_ATTACH_PLAYER:
	case SOUND_HOOK_ATTACH_GROUND:
	case SOUND_HOOK_NOATTACH:
		return HasMuteSoundFlag(MUTE_SOUND_OTHERS_HOOK) && !IsLocalSound;
	case SOUND_HAMMER_HIT:
		if(OwnerId < 0 && pSoundPos != nullptr && IsLikelyLocalHammerHit(*pSoundPos))
		{
			return HasMuteSoundFlag(MUTE_SOUND_LOCAL_HAMMER);
		}
		return IsLocalSound ? HasMuteSoundFlag(MUTE_SOUND_LOCAL_HAMMER) : HasMuteSoundFlag(MUTE_SOUND_OTHERS_HAMMER);
	case SOUND_WEAPON_SWITCH:
		return HasMuteSoundFlag(MUTE_SOUND_WEAPON_SWITCH);
	case SOUND_PLAYER_JUMP:
		return HasMuteSoundFlag(MUTE_SOUND_JUMP);
	default:
		return false;
	}
}

void CCatClient::CheckAndSendLagMessage()
{
	if(!g_Config.m_CcAutoLagMessage)
		return;

	if(!GameClient()->m_Snap.m_pLocalInfo || !GameClient()->m_Snap.m_pLocalCharacter)
		return;

	static int64_t s_LastLagMessageTime = 0;
	const int64_t Now = time_get();

	if(Now - s_LastLagMessageTime < time_freq() * 5)
		return;

	const int Ping = GameClient()->m_Snap.m_pLocalInfo->m_Latency;
	if(Ping >= g_Config.m_CcAutoLagMessagePing)
	{
		const char *pMessage = g_Config.m_CcAutoLagMessageText[0] ? g_Config.m_CcAutoLagMessageText : "lag";
		GameClient()->m_Chat.SendChat(0, pMessage);
		s_LastLagMessageTime = Now;
	}
}

#endif
