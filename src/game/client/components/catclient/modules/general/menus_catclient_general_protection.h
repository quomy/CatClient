#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_PROTECTION_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_PROTECTION_H

static void RenderTeamProtectionSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_LockDelaySlider;
	static CCatClientMenuSliderState s_AntiKillDelaySlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 143.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Team Protection"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAutoTeamLock, CCLocalize("Auto Team Lock"), &g_Config.m_CcAutoTeamLock, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAutoTeamLockDelay, &s_LockDelaySlider, &g_Config.m_CcAutoTeamLockDelay, Button, 0, 60, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %ds", CCLocalize("Lock Delay"), Value);
	});
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAntiKill, CCLocalize("Anti Kill"), &g_Config.m_CcAntiKill, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAntiKillDelay, &s_AntiKillDelaySlider, &g_Config.m_CcAntiKillDelay, Button, 1, 60, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %d min", CCLocalize("Kill Delay"), Value);
	});
}

static void RenderAntiQuitSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 69.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Anti Quit"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAntiQuit, CCLocalize("Ask before quitting"), &g_Config.m_CcAntiQuit, &Content, CATCLIENT_MENU_LINE_SIZE);
}

#endif
