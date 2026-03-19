#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_STREAMER_MENUS_CATCLIENT_STREAMER_SETTINGS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_STREAMER_MENUS_CATCLIENT_STREAMER_SETTINGS_H

static bool DoStreamerFlagCheckBox(CMenus *pMenus, const void *pId, const char *pLabel, int Flag, int &Flags, CUIRect &Content)
{
	CUIRect Button;
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	const int Checked = (Flags & Flag) != 0;
	if(pMenus->DoButton_CheckBox(pId, pLabel, Checked, &Button))
	{
		Flags ^= Flag;
		return true;
	}
	return false;
}

static void RenderStreamerSettingsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 145.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Streamer Mode"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcStreamerMode, CCLocalize("Enable Streamer Mode"), &g_Config.m_CcStreamerMode, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	DoStreamerFlagCheckBox(pMenus, (void *)(intptr_t)1001, CCLocalize("Hide Server IP"), CCatClient::STREAMER_HIDE_SERVER_IP, g_Config.m_CcStreamerFlags, Content);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	DoStreamerFlagCheckBox(pMenus, (void *)(intptr_t)1002, CCLocalize("Hide Chat"), CCatClient::STREAMER_HIDE_CHAT, g_Config.m_CcStreamerFlags, Content);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	DoStreamerFlagCheckBox(pMenus, (void *)(intptr_t)1003, CCLocalize("Hide Friend/Whisper Info"), CCatClient::STREAMER_HIDE_FRIEND_WHISPER, g_Config.m_CcStreamerFlags, Content);
}

#endif
