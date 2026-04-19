/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

namespace
{
#if defined(CONF_AUTOUPDATE)
void StartOrRetryCatClientUpdate(CGameClient *pGameClient)
{
	pGameClient->Updater()->InitiateUpdate();
}

bool ShouldShowCatClientUpdateButton(CGameClient *pGameClient)
{
	const IUpdater::EUpdaterState State = pGameClient->Updater()->GetCurrentState();
	return (State == IUpdater::CLEAN || State == IUpdater::FAIL) && pGameClient->m_TClient.NeedUpdate();
}

const char *GetCatClientUpdateButtonText(CGameClient *pGameClient)
{
	return pGameClient->Updater()->GetCurrentState() == IUpdater::FAIL ? CCLocalize("Retry update") : CCLocalize("Update now");
}
#endif

const char *GetCatClientUpdateBadgeText(CGameClient *pGameClient)
{
#if defined(CONF_AUTOUPDATE)
	switch(pGameClient->Updater()->GetCurrentState())
	{
	case IUpdater::GETTING_MANIFEST:
	case IUpdater::PARSING_UPDATE:
	case IUpdater::DOWNLOADING:
	case IUpdater::MOVE_FILES:
		return CCLocalize("(Updating)");
	case IUpdater::NEED_RESTART:
		return CCLocalize("(Restart required)");
	case IUpdater::FAIL:
		return CCLocalize("(Update failed)");
	default:
		break;
	}
#endif

	if(!pGameClient->m_TClient.m_FetchedTClientInfo)
	{
		return CCLocalize("(Checking for updates)");
	}
	if(pGameClient->m_TClient.NoPublishedRelease())
	{
		return CCLocalize("(No published release)");
	}
	if(pGameClient->m_TClient.NeedUpdate())
	{
		return CCLocalize("(Update available)");
	}
	if(pGameClient->m_TClient.UpdateCheckFailed())
	{
		return CCLocalize("(Update check unavailable)");
	}
	return CCLocalize("(Up to date)");
}

} // namespace

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	const float LogoHeight = 103.0f;
	const float LogoWidth = 360.0f;
	const float Spacing = 20.0f;
	const float ButtonsHeight = 40.0f;
	const float StartY = (MainView.h - ButtonsHeight) / 2.0f - Spacing - LogoHeight;

	// render logo
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BANNER].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(MainView.x + (MainView.w - LogoWidth) / 2.0f, MainView.y + StartY, LogoWidth, LogoHeight);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	const float Rounding = 5.0f;
	int NewPage = -1;

	CUIRect ButtonBox;
	MainView.HSplitTop(StartY + LogoHeight + Spacing, nullptr, &ButtonBox);
	ButtonBox.HSplitTop(ButtonsHeight, &ButtonBox, nullptr);
	float ButtonsWidth = 4 * 40.0f + 3 * 5.0f;
	ButtonBox.VMargin((MainView.w - ButtonsWidth) / 2.0f, &ButtonBox);

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	CUIRect Button;
	ButtonBox.VSplitLeft(40.0f, &Button, &ButtonBox);
	static CButtonContainer s_PlayButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_PlayButton, FontIcon::PLAY, 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P))
	{
		NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
	}
	ButtonBox.VSplitLeft(5.0f, nullptr, &ButtonBox);

	ButtonBox.VSplitLeft(40.0f, &Button, &ButtonBox);
	static CButtonContainer s_DemoButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_DemoButton, FontIcon::CLAPPERBOARD, 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_D))
	{
		NewPage = CMenus::PAGE_DEMOS;
	}
	ButtonBox.VSplitLeft(5.0f, nullptr, &ButtonBox);

	ButtonBox.VSplitLeft(40.0f, &Button, &ButtonBox);
	static CButtonContainer s_LocalServerButton;
	const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
	if(GameClient()->m_Menus.DoButton_Menu(&s_LocalServerButton, FontIcon::NETWORK_WIRED, 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, LocalServerRunning ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (CheckHotKey(KEY_R) && Input()->KeyPress(KEY_R)))
	{
		if(LocalServerRunning)
			GameClient()->m_LocalServer.KillServer();
		else
			GameClient()->m_LocalServer.RunServer({});
	}
	ButtonBox.VSplitLeft(5.0f, nullptr, &ButtonBox);

	ButtonBox.VSplitLeft(40.0f, &Button, &ButtonBox);
	static CButtonContainer s_SettingsButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_SettingsButton, FontIcon::GEAR, 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_S))
		NewPage = CMenus::PAGE_SETTINGS;

	CUIRect BottomRight;
	MainView.HSplitBottom(30.0f, nullptr, &BottomRight);
	BottomRight.VSplitRight(30.0f, &BottomRight, &Button);
	Button.Margin(5.0f, &Button);

	static CButtonContainer s_QuitButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_QuitButton, FontIcon::POWER_OFF, 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		if(GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
			GameClient()->m_Menus.ShowQuitPopup();
		else
			Client()->Quit();
	}

	BottomRight.VSplitRight(3.0f, &BottomRight, nullptr);
	BottomRight.VSplitRight(30.0f, &BottomRight, &Button);
	Button.Margin(5.0f, &Button);

	static CButtonContainer s_ConsoleButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_ConsoleButton, FontIcon::TERMINAL, 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

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
	Ui()->DoLabel(&UpdateToDateText, GetCatClientUpdateBadgeText(GameClient()), 14.0f, TEXTALIGN_MR);
#endif
#if defined(CONF_AUTOUPDATE) || defined(CONF_INFORM_UPDATE)
	bool ShowVersionAction = false;
#if defined(CONF_AUTOUPDATE)
	ShowVersionAction = GameClient()->Updater()->GetCurrentState() == IUpdater::NEED_RESTART || ShouldShowCatClientUpdateButton(GameClient());
#else
	ShowVersionAction = GameClient()->m_TClient.NeedUpdate();
#endif
	if(ShowVersionAction)
	{
		CUIRect VersionUpdate;
		MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
		VersionUpdate.VSplitRight(150.0f, &VersionUpdate, &VersionUpdate);
#if defined(CONF_AUTOUPDATE)
		if(GameClient()->Updater()->GetCurrentState() == IUpdater::NEED_RESTART)
		{
			CUIRect RestartButton;
			VersionUpdate.VSplitRight(110.0f, nullptr, &RestartButton);

			static CButtonContainer s_RestartAfterUpdateButton;
			if(GameClient()->m_Menus.DoButton_Menu(&s_RestartAfterUpdateButton, Localize("Restart"), 0, &RestartButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				Client()->Restart();
			}
		}
		else if(ShouldShowCatClientUpdateButton(GameClient()))
		{
			CUIRect UpdateButton;
			VersionUpdate.VSplitRight(120.0f, nullptr, &UpdateButton);

			static CButtonContainer s_UpdateButton;
			if(GameClient()->m_Menus.DoButton_Menu(&s_UpdateButton, GetCatClientUpdateButtonText(GameClient()), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				StartOrRetryCatClientUpdate(GameClient());
			}
		}
#endif
	}
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
