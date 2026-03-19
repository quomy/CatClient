#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_BROWSER_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_BROWSER_H

static void RenderServerBrowserSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_RefreshIntervalSlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 94.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Server Browser"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcServerBrowserAutoRefresh, CCLocalize("Auto Refresh"), &g_Config.m_CcServerBrowserAutoRefresh, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcServerBrowserRefreshInterval, &s_RefreshIntervalSlider, &g_Config.m_CcServerBrowserRefreshInterval, Button, 1, 30, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %ds", CCLocalize("Refresh Every"), Value);
	});
}

static void RenderAutoLagMessageSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_PingSlider;
	static CCatClientMenuSliderState s_ThresholdSlider;
	static CLineInput s_MessageInput;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 145.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Auto Lag Message"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAutoLagMessage, CCLocalize("Enable"), &g_Config.m_CcAutoLagMessage, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	Button.VSplitLeft(100.0f, &Label, &Button);
	pUi->DoLabel(&Label, CCLocalize("Message"), 13.0f, TEXTALIGN_ML);
	s_MessageInput.SetBuffer(g_Config.m_CcAutoLagMessageText, sizeof(g_Config.m_CcAutoLagMessageText));
	s_MessageInput.SetEmptyText("lag");
	pUi->DoClearableEditBox(&s_MessageInput, &Button, 13.0f);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAutoLagMessagePing, &s_PingSlider, &g_Config.m_CcAutoLagMessagePing, Button, 100, 500, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %dms", CCLocalize("Min Ping"), Value);
	});
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAutoLagMessageThreshold, &s_ThresholdSlider, &g_Config.m_CcAutoLagMessageThreshold, Button, 100, 500, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %dms", CCLocalize("Lag Threshold"), Value);
	});
}

#endif
