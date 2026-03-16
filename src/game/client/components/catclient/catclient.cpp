#include "catclient.h"

#include <base/system.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/teamscore.h>

#include <algorithm>
#include <chrono>

void CCatClient::ResetAutoTeamLock()
{
	m_AutoTeamLockTeam = TEAM_FLOCK;
	m_AutoTeamLockStart = std::chrono::nanoseconds::zero();
	m_AutoTeamLockIssued = false;
}

void CCatClient::ResetAntiKill()
{
	m_AntiKillTeam = TEAM_FLOCK;
	m_AntiKillStart = std::chrono::nanoseconds::zero();
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

bool CCatClient::IsLocalPlayerInTeam() const
{
	const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalClientId < 0)
	{
		return false;
	}

	const int Team = GameClient()->m_Teams.Team(LocalClientId);
	return Team > TEAM_FLOCK && Team < TEAM_SUPER;
}

bool CCatClient::HasActiveTeammateInLocalTeam() const
{
	const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalClientId < 0)
	{
		return false;
	}

	if(!IsLocalPlayerInTeam())
	{
		return false;
	}

	const int Team = GameClient()->m_Teams.Team(LocalClientId);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(ClientId == LocalClientId || !GameClient()->m_aClients[ClientId].m_Active)
		{
			continue;
		}

		if(GameClient()->m_Teams.Team(ClientId) == Team)
		{
			return true;
		}
	}

	return false;
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
		Graphics()->ClearScreenAspectOverride();
		return;
	}

	Graphics()->SetScreenAspectOverride(std::clamp(g_Config.m_CcAspectRatio / 100.0f, 1.0f, 4.0f));
}

void CCatClient::UpdateAntiKillState()
{
	if(!g_Config.m_CcAntiKill ||
		Client()->State() != IClient::STATE_ONLINE ||
		GameClient()->m_Snap.m_LocalClientId < 0 ||
		!GameClient()->m_Snap.m_pLocalCharacter ||
		!GameClient()->m_Snap.m_pLocalInfo)
	{
		ResetAntiKill();
		return;
	}

	const int Team = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId);
	if(!IsLocalPlayerInTeam() || !HasActiveTeammateInLocalTeam())
	{
		ResetAntiKill();
		return;
	}

	if(m_AntiKillTeam != Team)
	{
		m_AntiKillTeam = Team;
		m_AntiKillStart = time_get_nanoseconds();
	}
	else if(m_AntiKillStart == std::chrono::nanoseconds::zero())
	{
		m_AntiKillStart = time_get_nanoseconds();
	}
}

void CCatClient::OnInit()
{
	ResetAutoTeamLock();
	ResetAntiKill();
	m_NameTags.Init(GameClient());
	LoadCursorAsset(g_Config.m_ClAssetCursor);
	UpdateAspectRatioOverride();
}

void CCatClient::OnUpdate()
{
	UpdateAspectRatioOverride();
	UpdateAntiKillState();
	m_NameTags.Update();
}

void CCatClient::OnReset()
{
	ResetAutoTeamLock();
	ResetAntiKill();
	m_NameTags.Reset();
}

void CCatClient::OnRender()
{
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

	if(m_HasCustomCursor)
	{
		Graphics()->UnloadTexture(&m_CursorTexture);
		m_HasCustomCursor = false;
	}
}

void CCatClient::OnNewSnapshot()
{
	m_NameTags.Update();
}

void CCatClient::LoadCursorAsset(const char *pPath)
{
	if(m_HasCustomCursor)
	{
		Graphics()->UnloadTexture(&m_CursorTexture);
		m_HasCustomCursor = false;
	}

	if(str_comp(pPath, "default") == 0)
	{
		return;
	}

	char aAssetPath[IO_MAX_PATH_LENGTH];
	str_format(aAssetPath, sizeof(aAssetPath), "assets/cursors/%s.png", pPath);
	m_CursorTexture = Graphics()->LoadTexture(aAssetPath, IStorage::TYPE_ALL);
	if(m_CursorTexture.IsNullTexture())
	{
		str_format(aAssetPath, sizeof(aAssetPath), "assets/cursors/%s/gui_cursor.png", pPath);
		m_CursorTexture = Graphics()->LoadTexture(aAssetPath, IStorage::TYPE_ALL);
	}

	if(!m_CursorTexture.IsNullTexture())
	{
		m_HasCustomCursor = true;
	}
}

const IGraphics::CTextureHandle &CCatClient::CursorTexture() const
{
	if(m_HasCustomCursor)
	{
		return m_CursorTexture;
	}
	return g_pData->m_aImages[IMAGE_CURSOR].m_Id;
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

bool CCatClient::ShouldBlockKill()
{
	UpdateAntiKillState();
	if(!g_Config.m_CcAntiKill || m_AntiKillStart == std::chrono::nanoseconds::zero())
	{
		return false;
	}

	return time_get_nanoseconds() - m_AntiKillStart >= std::chrono::minutes(g_Config.m_CcAntiKillDelay);
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
