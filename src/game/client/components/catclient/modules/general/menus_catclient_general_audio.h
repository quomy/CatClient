#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_AUDIO_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_AUDIO_H

static const SCatClientMenuFlagOption gs_aMuteSoundOptions[] = {
	{CCLocalizable("Others hook sound"), CCatClient::MUTE_SOUND_OTHERS_HOOK},
	{CCLocalizable("Others hammer sound"), CCatClient::MUTE_SOUND_OTHERS_HAMMER},
	{CCLocalizable("Local hammer sound"), CCatClient::MUTE_SOUND_LOCAL_HAMMER},
	{CCLocalizable("Weapon switch sound"), CCatClient::MUTE_SOUND_WEAPON_SWITCH},
	{CCLocalizable("Jump sound"), CCatClient::MUTE_SOUND_JUMP},
};

static void RenderMuteSoundsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuBitmaskButtonState s_MuteSoundsButtons;

	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 169.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Mute Sounds"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CatClientMenuDoBitmaskButtonGroup(pMenus, Content, &s_MuteSoundsButtons, &g_Config.m_CcMuteSounds, gs_aMuteSoundOptions, std::size(gs_aMuteSoundOptions), 1);
}

#endif
