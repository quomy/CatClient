#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_FAST_INPUTS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_FAST_INPUTS_H

static const SCatClientMenuChoiceOption gs_aFastInputModeOptions[] = {
	{CCLocalizable("CatClient"), 0},
	{CCLocalizable("Saiko"), 1},
};

static void RenderFastInputsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_TClientFastInputAmountSlider;
	static CCatClientMenuSliderState s_SaikoFastInputAmountSlider;
	static CCatClientMenuChoiceButtonState s_FastInputModeButtons;

	CUIRect Section, Content, Header, Label, ModeButtons, Button;
	CatClientMenuBeginSection(View, Section, Content, 144.0f);
	if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_FAST_INPUTS))
		pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_FAST_INPUTS, Section);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Header, &Content);
	Header.VSplitRight(180.0f, &Label, &ModeButtons);
	pUi->DoLabel(&Label, CCLocalize("Fast Inputs"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	CatClientMenuDoChoiceButtonGroup(pMenus, ModeButtons, &s_FastInputModeButtons, &g_Config.m_TcFastInputMode, gs_aFastInputModeOptions, std::size(gs_aFastInputModeOptions), 2);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	if(g_Config.m_TcFastInputMode == 0)
	{
		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInput, CCLocalize("Fast Inputs"), &g_Config.m_TcFastInput, &Content, CATCLIENT_MENU_LINE_SIZE);
		Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
		CatClientMenuDoSliderOption(pUi, &g_Config.m_TcFastInputAmount, &s_TClientFastInputAmountSlider, &g_Config.m_TcFastInputAmount, Button, 1, 40, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %dms", CCLocalize("Amount"), Value);
		});
		Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	}
	else
	{
		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInput, CCLocalize("Fast Inputs"), &g_Config.m_TcFastInput, &Content, CATCLIENT_MENU_LINE_SIZE);
		Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
		CatClientMenuDoSliderOption(pUi, &g_Config.m_TcFastInputSaikoAmount, &s_SaikoFastInputAmountSlider, &g_Config.m_TcFastInputSaikoAmount, Button, 0, 500, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			if(Value == 0)
			{
				str_format(pBuf, BufSize, "%s: %s", CCLocalize("Amount"), CCLocalize("Off"));
				return;
			}

			const float Ticks = Value / 100.0f;
			str_format(pBuf, BufSize, "%s: %.2ft (%.1fms)", CCLocalize("Amount"), Ticks, Ticks * 20.0f);
		});
		Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	}

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputOthers, CCLocalize("Fast Input others"), &g_Config.m_TcFastInputOthers, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, CCLocalize("Sub-Tick aiming"), &g_Config.m_ClSubTickAiming, &Content, CATCLIENT_MENU_LINE_SIZE);
}

#endif
