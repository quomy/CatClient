#include "catclient.h"

#include <engine/gfx/image_loader.h>
#include <base/system.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/config.h>
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

static constexpr const char *CATCLIENT_AUTO_CURSOR_URL = "https://data.teeworlds.xyz/api/skins/dcfe12f4-fdbd-41d4-b287-a5af431ebf28?image=true";
static constexpr const char *CATCLIENT_AUTO_CURSOR_PATH = "assets/cursors/catproject_auto.png";
static constexpr CTimeout AUTO_CURSOR_REQUEST_TIMEOUT{10000, 0, 0, 0};
static constexpr const char *CATCLIENT_DEFAULT_BACKGROUND_URL = "https://tags.quomy.win/firstbg.jpg";
static constexpr const char *CATCLIENT_DEFAULT_BACKGROUND_NAME = "firstbg.jpg";
static constexpr const char *CATCLIENT_DEFAULT_BACKGROUND_PATH = "catclient/backgrounds/firstbg.jpg";
static constexpr CTimeout DEFAULT_BACKGROUND_REQUEST_TIMEOUT{10000, 0, 0, 0};
static constexpr auto DEFAULT_BACKGROUND_RETRY_INTERVAL = std::chrono::seconds(30);

static void EscapeConfigParam(char *pDst, const char *pSrc, size_t Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

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

void CCatClient::ResetAntiKill()
{
	m_AntiKillTeam = TEAM_FLOCK;
	m_AntiKillStart = std::chrono::nanoseconds::zero();
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

void CCatClient::UpdateIgnoredPlayers()
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		auto &Client = GameClient()->m_aClients[ClientId];
		Client.m_ChatIgnore = Client.m_Active && IsPlayerIgnored(Client.m_aName);
	}
}

bool CCatClient::LoadAutomaticCursorAsset()
{
	if(!Storage()->FileExists(CATCLIENT_AUTO_CURSOR_PATH, IStorage::TYPE_SAVE))
	{
		return false;
	}

	m_CursorTexture = Graphics()->LoadTexture(CATCLIENT_AUTO_CURSOR_PATH, IStorage::TYPE_SAVE);
	if(m_CursorTexture.IsNullTexture())
	{
		return false;
	}

	m_HasCustomCursor = true;
	return true;
}

void CCatClient::StartAutomaticCursorDownload()
{
	if(m_pCursorDownloadTask)
	{
		return;
	}

	m_pCursorDownloadTask = HttpGetBoth(CATCLIENT_AUTO_CURSOR_URL, Storage(), CATCLIENT_AUTO_CURSOR_PATH, IStorage::TYPE_SAVE);
	m_pCursorDownloadTask->Timeout(AUTO_CURSOR_REQUEST_TIMEOUT);
	m_pCursorDownloadTask->IpResolve(IPRESOLVE::V4);
	m_pCursorDownloadTask->MaxResponseSize(256 * 1024);
	m_pCursorDownloadTask->LogProgress(HTTPLOG::NONE);
	m_pCursorDownloadTask->FailOnErrorStatus(false);
	m_pCursorDownloadTask->SkipByFileTime(false);
	Http()->Run(m_pCursorDownloadTask);
}

void CCatClient::FinishAutomaticCursorDownload()
{
	if(!m_pCursorDownloadTask || m_pCursorDownloadTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pCursorDownloadTask->StatusCode() < 400 && str_comp(g_Config.m_ClAssetCursor, "default") == 0)
	{
		LoadCursorAsset(g_Config.m_ClAssetCursor);
	}
}

void CCatClient::EnsureCustomBackgroundFolder() const
{
	Storage()->CreateFolder("catclient", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("catclient/backgrounds", IStorage::TYPE_SAVE);
}

bool CCatClient::LoadCustomBackgroundTexture(const char *pImageName)
{
	if(pImageName == nullptr || pImageName[0] == '\0')
	{
		return false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "catclient/backgrounds/%s", pImageName);

	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
	{
		return false;
	}

	CImageInfo Image;
	bool Loaded = false;
	if(str_endswith_nocase(aPath, ".png"))
	{
		int PngliteIncompatible = 0;
		Loaded = CImageLoader::LoadPng(File, aPath, Image, PngliteIncompatible);
	}
	else if(str_endswith_nocase(aPath, ".jpg") || str_endswith_nocase(aPath, ".jpeg"))
	{
		Loaded = CImageLoader::LoadJpg(File, aPath, Image);
	}
	else
	{
		int PngliteIncompatible = 0;
		Loaded = CImageLoader::LoadPng(File, aPath, Image, PngliteIncompatible);
		if(!Loaded)
		{
			File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			Loaded = CImageLoader::LoadJpg(File, aPath, Image);
		}
	}

	if(!Loaded)
	{
		return false;
	}

	UnloadCustomBackgroundTexture();
	m_CustomBackgroundImageSize = vec2((float)Image.m_Width, (float)Image.m_Height);
	m_CustomBackgroundTexture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
	if(m_CustomBackgroundTexture.IsNullTexture())
	{
		m_CustomBackgroundImageSize = vec2(0.0f, 0.0f);
		return false;
	}

	m_HasCustomBackgroundTexture = true;
	return true;
}

void CCatClient::UnloadCustomBackgroundTexture()
{
	if(m_HasCustomBackgroundTexture)
	{
		Graphics()->UnloadTexture(&m_CustomBackgroundTexture);
		m_HasCustomBackgroundTexture = false;
	}
	m_CustomBackgroundTexture = IGraphics::CTextureHandle();
	m_CustomBackgroundImageSize = vec2(0.0f, 0.0f);
}

void CCatClient::StartDefaultBackgroundDownload()
{
	if(m_pBackgroundDownloadTask)
	{
		return;
	}

	EnsureCustomBackgroundFolder();
	m_pBackgroundDownloadTask = HttpGetBoth(CATCLIENT_DEFAULT_BACKGROUND_URL, Storage(), CATCLIENT_DEFAULT_BACKGROUND_PATH, IStorage::TYPE_SAVE);
	m_pBackgroundDownloadTask->Timeout(DEFAULT_BACKGROUND_REQUEST_TIMEOUT);
	m_pBackgroundDownloadTask->IpResolve(IPRESOLVE::V4);
	m_pBackgroundDownloadTask->MaxResponseSize(16 * 1024 * 1024);
	m_pBackgroundDownloadTask->LogProgress(HTTPLOG::NONE);
	m_pBackgroundDownloadTask->FailOnErrorStatus(false);
	m_LastBackgroundAttempt = time_get_nanoseconds();
	Http()->Run(m_pBackgroundDownloadTask);
}

void CCatClient::FinishDefaultBackgroundDownload()
{
	if(!m_pBackgroundDownloadTask || m_pBackgroundDownloadTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pBackgroundDownloadTask->StatusCode() < 400)
	{
		ReloadCustomBackground();
	}
}

void CCatClient::EnsureSelectedCustomBackgroundLoaded()
{
	const bool NeedTexture = g_Config.m_CcCustomBackgroundMainMenu != 0 || g_Config.m_CcCustomBackgroundGame != 0;
	if(!NeedTexture)
	{
		UnloadCustomBackgroundTexture();
		m_aLoadedBackgroundImage[0] = '\0';
		return;
	}

	const char *pSelectedImage = g_Config.m_CcCustomBackgroundImage[0] != '\0' ? g_Config.m_CcCustomBackgroundImage : CATCLIENT_DEFAULT_BACKGROUND_NAME;
	if(str_comp(m_aLoadedBackgroundImage, pSelectedImage) == 0)
	{
		return;
	}

	str_copy(m_aLoadedBackgroundImage, pSelectedImage, sizeof(m_aLoadedBackgroundImage));
	if(!LoadCustomBackgroundTexture(pSelectedImage))
	{
		UnloadCustomBackgroundTexture();
	}
}

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

void CCatClient::LoadCursorAsset(const char *pPath)
{
	if(m_HasCustomCursor)
	{
		Graphics()->UnloadTexture(&m_CursorTexture);
		m_HasCustomCursor = false;
	}

	if(str_comp(pPath, "default") == 0)
	{
		LoadAutomaticCursorAsset();
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

bool CCatClient::HasMenuCustomBackground() const
{
	return g_Config.m_CcCustomBackgroundMainMenu != 0 && m_HasCustomBackgroundTexture;
}

bool CCatClient::HasGameCustomBackground() const
{
	return g_Config.m_CcCustomBackgroundGame != 0 && m_HasCustomBackgroundTexture;
}

bool CCatClient::IsDefaultBackgroundDownloading() const
{
	return m_pBackgroundDownloadTask != nullptr;
}

void CCatClient::ReloadCustomBackground()
{
	UnloadCustomBackgroundTexture();
	m_aLoadedBackgroundImage[0] = '\0';
	EnsureSelectedCustomBackgroundLoaded();
}

void CCatClient::RenderCustomBackground()
{
	if(!m_HasCustomBackgroundTexture)
	{
		return;
	}

	const float ScreenHeight = 300.0f;
	const float ScreenWidth = ScreenHeight * ((float)Graphics()->WindowWidth() / (float)maximum(Graphics()->WindowHeight(), 1));
	Graphics()->MapScreen(0.0f, 0.0f, ScreenWidth, ScreenHeight);

	Graphics()->BlendNormal();
	Graphics()->WrapClamp();
	Graphics()->TextureSet(m_CustomBackgroundTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	const IGraphics::CQuadItem Quad(0.0f, 0.0f, ScreenWidth, ScreenHeight);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
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
