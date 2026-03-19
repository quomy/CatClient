#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_VISUALS_MENUS_CATCLIENT_VISUALS_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_VISUALS_MENUS_CATCLIENT_VISUALS_EFFECTS_H

static const SCatClientMenuFlagOption gs_aHideEffectOptions[] = {
	{CCLocalizable("Freeze Flakes"), CCatClient::HIDE_EFFECT_FREEZE_FLAKES},
	{CCLocalizable("Hammer hits"), CCatClient::HIDE_EFFECT_HAMMER_HITS},
	{CCLocalizable("Jumps"), CCatClient::HIDE_EFFECT_JUMPS},
};

static const SCatClientMenuFlagOption gs_aChatAnimationOptions[] = {
	{CCLocalizable("Open and close"), CCatClient::CHAT_ANIM_OPEN_CLOSE},
	{CCLocalizable("Typing"), CCatClient::CHAT_ANIM_TYPING},
};

static void RenderHideEffectsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuBitmaskButtonState s_HideEffectsButtons;

	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 94.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Hide Effects"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CatClientMenuDoBitmaskButtonGroup(pMenus, Content, &s_HideEffectsButtons, &g_Config.m_CcHideEffects, gs_aHideEffectOptions, std::size(gs_aHideEffectOptions), 2);
}

static void RenderChatAnimationsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuBitmaskButtonState s_ChatAnimationsButtons;

	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 69.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Chat Animations"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CatClientMenuDoBitmaskButtonGroup(pMenus, Content, &s_ChatAnimationsButtons, &g_Config.m_CcChatAnimations, gs_aChatAnimationOptions, std::size(gs_aChatAnimationOptions), 2);
}

static void RenderModernUiSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_UiScaleSlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 119.0f);
	if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_UI_SCALE))
		pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_UI_SCALE, Section);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Modern UI"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcHorizontalSettingsTabs, CCLocalize("New settings menu"), &g_Config.m_CcHorizontalSettingsTabs, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcNewMainMenu, CCLocalize("New start menu"), &g_Config.m_CcNewMainMenu, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcUiScale, &s_UiScaleSlider, &g_Config.m_CcUiScale, Button, 50, 100, &CUi::ms_LinearScrollbarScale, true, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %d%%", CCLocalize("UI Scale"), Value);
	});
}

static void RenderEnhancedLaserSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 69.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Enhanced Laser"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcEnhancedLaser, CCLocalize("Enable"), &g_Config.m_CcEnhancedLaser, &Content, CATCLIENT_MENU_LINE_SIZE);
}

#endif
