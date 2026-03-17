#include "catclient.h"
#include "menus_catclient_dropdown.h"
#include "menus_catclient_slider.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

#include <iterator>

static const SCatClientMenuFlagOption gs_aMuteSoundOptions[] = {
	{"Others hook sound", CCatClient::MUTE_SOUND_OTHERS_HOOK},
	{"Others hammer sound", CCatClient::MUTE_SOUND_OTHERS_HAMMER},
	{"Local hammer sound", CCatClient::MUTE_SOUND_LOCAL_HAMMER},
	{"Weapon switch sound", CCatClient::MUTE_SOUND_WEAPON_SWITCH},
	{"Jump sound", CCatClient::MUTE_SOUND_JUMP},
};

static void RenderTeamProtectionSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_LockDelaySlider;
	static CCatClientMenuSliderState s_AntiKillDelaySlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 143.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Team Protection"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAutoTeamLock, Localize("Auto Team Lock"), &g_Config.m_CcAutoTeamLock, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAutoTeamLockDelay, &s_LockDelaySlider, &g_Config.m_CcAutoTeamLockDelay, Button, 0, 60, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %ds", Localize("Lock Delay"), Value);
	});
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAntiKill, Localize("Anti Kill"), &g_Config.m_CcAntiKill, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAntiKillDelay, &s_AntiKillDelaySlider, &g_Config.m_CcAntiKillDelay, Button, 1, 60, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %d min", Localize("Kill Delay"), Value);
	});
}

static void RenderAntiQuitSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 69.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Anti Quit"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAntiQuit, Localize("Ask before quitting"), &g_Config.m_CcAntiQuit, &Content, CATCLIENT_MENU_LINE_SIZE);
}

static void RenderFastInputsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_TClientFastInputAmountSlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 144.0f);
	if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_FAST_INPUTS))
		pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_FAST_INPUTS, Section);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Fast Inputs"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInput, Localize("Fast Inputs"), &g_Config.m_TcFastInput, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_TcFastInputAmount, &s_TClientFastInputAmountSlider, &g_Config.m_TcFastInputAmount, Button, 1, 40, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %dms", Localize("Amount"), Value);
	});
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputOthers, Localize("Fast Input others"), &g_Config.m_TcFastInputOthers, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, Localize("Sub-Tick aiming"), &g_Config.m_ClSubTickAiming, &Content, CATCLIENT_MENU_LINE_SIZE);
}

static void RenderServerBrowserSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_RefreshIntervalSlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 94.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Server Browser"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcServerBrowserAutoRefresh, Localize("Auto Refresh"), &g_Config.m_CcServerBrowserAutoRefresh, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcServerBrowserRefreshInterval, &s_RefreshIntervalSlider, &g_Config.m_CcServerBrowserRefreshInterval, Button, 1, 30, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %ds", Localize("Refresh Every"), Value);
	});
}

static void RenderMuteSoundsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuBitmaskButtonState s_MuteSoundsButtons;

	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 169.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Mute Sounds"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CatClientMenuDoBitmaskButtonGroup(pMenus, Content, &s_MuteSoundsButtons, &g_Config.m_CcMuteSounds, gs_aMuteSoundOptions, std::size(gs_aMuteSoundOptions), 1);
}

void CMenus::RenderSettingsCatClientGeneral(CUIRect MainView)
{
	CUIRect LeftView, RightView;
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CatClientMenuConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS);

	RenderTeamProtectionSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);

	RenderFastInputsSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);

	RenderAntiQuitSection(this, Ui(), LeftView);

	RenderServerBrowserSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderMuteSoundsSection(this, Ui(), RightView);
}
