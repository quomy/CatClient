/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <base/math.h>

#include <engine/client/updater.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	// render logo
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BANNER].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(MainView.x + MainView.w / 2.0f - 170.0f, MainView.y + 60.0f, 360.0f, 103.0f);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	const float Rounding = 10.0f;
	const float ContentMargin = maximum(40.0f, MainView.w / 2.0f - 190.0f);
	const bool UseNewMainMenu = g_Config.m_CcNewMainMenu != 0;
	const bool UseButtonImages = !UseNewMainMenu && g_Config.m_ClShowStartMenuImages;
	float ActionBottomY = 0.0f;

	int NewPage = -1;
	static CButtonContainer s_QuitButton;
	static CButtonContainer s_SettingsButton;
	static CButtonContainer s_LocalServerButton;
	static CButtonContainer s_MapEditorButton;
	static CButtonContainer s_DemoButton;
	static CButtonContainer s_PlayButton;

	const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
	auto DoMenuButton = [&](CButtonContainer *pButton, const char *pText, CUIRect Rect, const char *pImage, const ColorRGBA &Color) {
		return GameClient()->m_Menus.DoButton_Menu(pButton, pText, 0, &Rect, BUTTONFLAG_LEFT, UseButtonImages ? pImage : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, Color);
	};
	auto OpenLocalServer = [&]() {
		if(LocalServerRunning)
			GameClient()->m_LocalServer.KillServer();
		else
			GameClient()->m_LocalServer.RunServer({});
	};

	if(UseNewMainMenu)
	{
		const float Gap = 10.0f;
		const float ButtonHeight = 42.0f;
		const float ButtonWidth = minimum(190.0f, maximum(140.0f, (MainView.w - 80.0f - Gap * 2.0f) / 3.0f));
		const float TotalWidth = ButtonWidth * 3.0f + Gap * 2.0f;
		const float StartX = MainView.x + MainView.w / 2.0f - TotalWidth / 2.0f;
		const float StartY = MainView.y + 185.0f;
		CUIRect Row = {StartX, StartY, TotalWidth, ButtonHeight};
		CUIRect RowButton;

		Row.VSplitLeft(ButtonWidth, &RowButton, &Row);
		if(DoMenuButton(&s_PlayButton, Localize("PLAY", "Start menu"), RowButton, nullptr, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P))
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
		Row.VSplitLeft(Gap, nullptr, &Row);
		Row.VSplitLeft(ButtonWidth, &RowButton, &Row);
		if(DoMenuButton(&s_SettingsButton, Localize("SETTINGS"), RowButton, nullptr, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_S))
			NewPage = CMenus::PAGE_SETTINGS;
		Row.VSplitLeft(Gap, nullptr, &Row);
		Row.VSplitLeft(ButtonWidth, &RowButton, &Row);
		bool UsedEscape = false;
		if(DoMenuButton(&s_QuitButton, Localize("QUIT"), RowButton, nullptr, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
		{
			if(UsedEscape || GameClient()->m_Menus.ShouldConfirmQuit())
				GameClient()->m_Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}

		ActionBottomY = StartY + ButtonHeight;
	}
	else
	{
		const float Gap = 5.0f;
		const float ButtonHeight = 40.0f;
		const float ButtonWidth = minimum(380.0f, maximum(220.0f, MainView.w - 80.0f));
		const float StartX = MainView.x + MainView.w / 2.0f - ButtonWidth / 2.0f;
		const float StartY = MainView.y + 185.0f;
		CUIRect ColumnButton = {StartX, StartY, ButtonWidth, ButtonHeight};

		if(DoMenuButton(&s_PlayButton, Localize("Play", "Start menu"), ColumnButton, "play_game", ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P))
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;

		ColumnButton.y += ButtonHeight + Gap;
		if(DoMenuButton(&s_DemoButton, Localize("Demos"), ColumnButton, "demos", ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_D))
			NewPage = CMenus::PAGE_DEMOS;

		ColumnButton.y += ButtonHeight + Gap;
		if(DoMenuButton(&s_MapEditorButton, Localize("Editor"), ColumnButton, "editor", GameClient()->Editor()->HasUnsavedData() ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_E))
		{
			g_Config.m_ClEditor = 1;
			Input()->MouseModeRelative();
		}

		ColumnButton.y += ButtonHeight + Gap;
		if(DoMenuButton(&s_LocalServerButton, LocalServerRunning ? Localize("Stop server") : Localize("Run server"), ColumnButton, "local_server", LocalServerRunning ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (CheckHotKey(KEY_R) && Input()->KeyPress(KEY_R)))
			OpenLocalServer();

		ColumnButton.y += ButtonHeight + Gap;
		if(DoMenuButton(&s_SettingsButton, Localize("Settings"), ColumnButton, "settings", ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_S))
			NewPage = CMenus::PAGE_SETTINGS;

		ColumnButton.y += ButtonHeight + Gap;
		bool UsedEscape = false;
		if(DoMenuButton(&s_QuitButton, Localize("Quit"), ColumnButton, nullptr, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
		{
			if(UsedEscape || GameClient()->m_Menus.ShouldConfirmQuit())
				GameClient()->m_Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}

		ActionBottomY = ColumnButton.y + ColumnButton.h;
	}

	const float PlayerCardWidth = minimum(380.0f, maximum(260.0f, MainView.w - 80.0f));
	CUIRect PlayerCard = {MainView.x + MainView.w / 2.0f - PlayerCardWidth / 2.0f, ActionBottomY + 14.0f, PlayerCardWidth, 82.0f};
	static CButtonContainer s_PlayerTeeButton;
	static int s_PlayerCardEmote = EMOTE_NORMAL;
	static double s_PlayerCardEmoteUntil = 0.0;
	if(s_PlayerCardEmoteUntil <= Client()->GlobalTime())
	{
		s_PlayerCardEmote = EMOTE_NORMAL;
	}
	PlayerCard.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f), IGraphics::CORNER_ALL, Rounding);

	CUIRect PlayerContent;
	PlayerCard.Margin(10.0f, &PlayerContent);

	CUIRect TeeBox, PlayerInfo;
	PlayerContent.VSplitLeft(78.0f, &TeeBox, &PlayerInfo);
	TeeBox.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.035f), IGraphics::CORNER_ALL, 8.0f);
	if(Ui()->DoButtonLogic(&s_PlayerTeeButton, 0, &TeeBox, BUTTONFLAG_LEFT))
	{
		static const int s_aRandomEmotes[] = {EMOTE_PAIN, EMOTE_HAPPY, EMOTE_SURPRISE, EMOTE_ANGRY, EMOTE_BLINK};
		s_PlayerCardEmote = s_aRandomEmotes[(int)random_float((float)(sizeof(s_aRandomEmotes) / sizeof(s_aRandomEmotes[0])))];
		s_PlayerCardEmoteUntil = Client()->GlobalTime() + 1.2;
	}

	CUIRect PlayerText, PlayerIcon;
	PlayerInfo.VSplitLeft(18.0f, nullptr, &PlayerInfo);
	PlayerInfo.VSplitRight(30.0f, &PlayerText, &PlayerIcon);

	const char *pPlayerName = g_Config.m_PlayerName[0] ? g_Config.m_PlayerName : Client()->PlayerName();
	const char *pPlayerClan = g_Config.m_PlayerClan;
	CTeeRenderInfo TeeInfo;
	TeeInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin));
	TeeInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
	TeeInfo.m_Size = 58.0f;
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &TeeInfo, OffsetToMid);
	const vec2 TeeRenderPos = vec2(TeeBox.x + TeeBox.w / 2.0f, TeeBox.y + TeeBox.h / 2.0f + OffsetToMid.y);
	vec2 TeeDir = Ui()->MousePos() - TeeRenderPos;
	if(TeeInfo.m_Size > 0.0f)
	{
		TeeDir /= TeeInfo.m_Size;
	}
	const float TeeDirLength = length(TeeDir);
	if(TeeDirLength > 1.0f)
	{
		TeeDir /= TeeDirLength;
	}
	if(TeeDir == vec2(0.0f, 0.0f))
	{
		TeeDir = vec2(1.0f, 0.0f);
	}
	int TeeEmote = s_PlayerCardEmote;
	if(TeeDirLength < 0.4f && TeeEmote == EMOTE_NORMAL)
	{
		TeeEmote = EMOTE_HAPPY;
	}
	GameClient()->RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, TeeEmote, TeeDir, TeeRenderPos);

	CUIRect NameLabel, ClanLabel;
	PlayerText.HSplitTop(11.0f, nullptr, &PlayerText);
	PlayerText.HSplitTop(31.0f, &NameLabel, &PlayerText);
	PlayerText.HSplitTop(18.0f, &ClanLabel, nullptr);

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Ui()->DoLabel(&NameLabel, pPlayerName, 24.0f, TEXTALIGN_ML);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.72f);
	Ui()->DoLabel(&ClanLabel, pPlayerClan[0] ? pPlayerClan : Localize("No clan"), 14.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	if(GameClient()->m_CatClient.HasCatIconTexture())
	{
		const float IconSize = 20.0f;
		const CUIRect CatIcon = {
			PlayerIcon.x + (PlayerIcon.w - IconSize) / 2.0f,
			PlayerCard.y + (PlayerCard.h - IconSize) / 2.0f,
			IconSize,
			IconSize,
		};
		GameClient()->m_CatClient.RenderCatIcon(CatIcon, 0.95f);
	}

	CUIRect TClientVersion;
	MainView.HSplitTop(15.0f, &TClientVersion, &MainView);
	TClientVersion.VSplitRight(40.0f, &TClientVersion, nullptr);
	char aTBuf[64];
	str_format(aTBuf, sizeof(aTBuf), CLIENT_NAME " %s", CLIENT_RELEASE_VERSION);
	Ui()->DoLabel(&TClientVersion, aTBuf, 14.0f, TEXTALIGN_MR);
#if defined(CONF_AUTOUPDATE) || defined(CONF_INFORM_UPDATE)
	CUIRect UpdateToDateText;
	MainView.HSplitTop(15.0f, &UpdateToDateText, nullptr);
	UpdateToDateText.VSplitRight(40.0f, &UpdateToDateText, nullptr);
	if(!GameClient()->m_TClient.m_FetchedTClientInfo)
	{
		Ui()->DoLabel(&UpdateToDateText, Localize("(Checking GitHub release)"), 14.0f, TEXTALIGN_MR);
	}
	else if(GameClient()->m_TClient.NoPublishedRelease())
	{
		Ui()->DoLabel(&UpdateToDateText, Localize("(No published release)"), 14.0f, TEXTALIGN_MR);
	}
	else if(GameClient()->m_TClient.NeedUpdate())
	{
		Ui()->DoLabel(&UpdateToDateText, Localize("(Update available)"), 14.0f, TEXTALIGN_MR);
	}
	else if(GameClient()->m_TClient.UpdateCheckFailed())
	{
		Ui()->DoLabel(&UpdateToDateText, Localize("(Update check unavailable)"), 14.0f, TEXTALIGN_MR);
	}
	else
	{
		Ui()->DoLabel(&UpdateToDateText, Localize("(On Latest)"), 14.0f, TEXTALIGN_MR);
	}
#endif
	CUIRect VersionUpdate;
	MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
	VersionUpdate.VMargin(ContentMargin, &VersionUpdate);
#if defined(CONF_AUTOUPDATE) || defined(CONF_INFORM_UPDATE)
	char aBuf[256] = "";
	const bool NeedUpdate = GameClient()->m_TClient.NeedUpdate();
	if(NeedUpdate)
	{
		CUIRect DownloadButton;
		VersionUpdate.VSplitRight(135.0f, &VersionUpdate, &DownloadButton);
		VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

		static CButtonContainer s_DownloadButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_DownloadButton, Localize("Download update"), 0, &DownloadButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Client()->ViewLink(GameClient()->m_TClient.LatestReleaseUrl());
		}

		str_format(aBuf, sizeof(aBuf), Localize("CatClient %s is available on GitHub."), GameClient()->m_TClient.m_aVersionStr);
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(!GameClient()->m_TClient.m_FetchedTClientInfo)
	{
		str_copy(aBuf, Localize("Checking CatClient updates…"));
	}
	else if(GameClient()->m_TClient.NoPublishedRelease())
	{
		str_copy(aBuf, Localize("No CatClient release has been published on GitHub yet."));
	}
	else if(GameClient()->m_TClient.UpdateCheckFailed())
	{
		str_copy(aBuf, Localize("Couldn't check CatClient updates."));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}

	Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
#endif

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

bool CMenusStart::CheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && // no modifier
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}
