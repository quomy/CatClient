#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_BETTER_MOVEMENT_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_BETTER_MOVEMENT_H

static int BetterMovementWeaponToDropdownIndex(int Weapon)
{
	switch(Weapon)
	{
	case WEAPON_HAMMER: return 0;
	case WEAPON_GUN: return 1;
	case WEAPON_SHOTGUN: return 2;
	case WEAPON_LASER: return 3;
	case WEAPON_GRENADE: return 4;
	default: return 0;
	}
}

static int BetterMovementDropdownIndexToWeapon(int Index)
{
	switch(Index)
	{
	case 0: return WEAPON_HAMMER;
	case 1: return WEAPON_GUN;
	case 2: return WEAPON_SHOTGUN;
	case 3: return WEAPON_LASER;
	case 4: return WEAPON_GRENADE;
	default: return WEAPON_HAMMER;
	}
}

static void RenderBetterMovementWeaponDropDown(CUi *pUi, CUIRect &Rect, int *pWeapon, CUi::SDropDownState &State, CScrollRegion &ScrollRegion)
{
	const char *apWeaponLabels[] = {
		CCLocalize("Hammer"),
		CCLocalize("Pistol"),
		CCLocalize("Shotgun"),
		CCLocalize("Laser"),
		CCLocalize("Grenade"),
	};

	State.m_SelectionPopupContext.m_pScrollRegion = &ScrollRegion;

	const int CurrentIndex = BetterMovementWeaponToDropdownIndex(*pWeapon);
	const int NewIndex = pUi->DoDropDown(&Rect, CurrentIndex, apWeaponLabels, std::size(apWeaponLabels), State);
	*pWeapon = BetterMovementDropdownIndexToWeapon(NewIndex);
}

static void RenderBetterMovementDropDownLine(CUi *pUi, CUIRect &Content, const char *pLabel, int *pValue, const char **ppLabels, int NumLabels, CUi::SDropDownState &State, CScrollRegion &ScrollRegion)
{
	CUIRect Row, Label, DropDown;
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Row, &Content);
	Row.VSplitLeft(120.0f, &Label, &DropDown);
	pUi->DoLabel(&Label, pLabel, CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_ML);
	State.m_SelectionPopupContext.m_pScrollRegion = &ScrollRegion;
	*pValue = pUi->DoDropDown(&DropDown, *pValue, ppLabels, NumLabels, State);
}

static void RenderBetterMovementSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CButtonContainer s_AutoSwitchReader;
	static CButtonContainer s_AutoSwitchClear;
	static CUi::SDropDownState s_FirstWeaponDropDownState;
	static CUi::SDropDownState s_SecondWeaponDropDownState;
	static CUi::SDropDownState s_LimitMouseDropDownState;
	static CUi::SDropDownState s_HammerModeDropDownState;
	static CScrollRegion s_FirstWeaponDropDownScrollRegion;
	static CScrollRegion s_SecondWeaponDropDownScrollRegion;
	static CScrollRegion s_LimitMouseDropDownScrollRegion;
	static CScrollRegion s_HammerModeDropDownScrollRegion;
	const char *apLimitMouseLabels[] = {
		CCLocalize("Off"),
		CCLocalize("Screen"),
		CCLocalize("Square"),
	};
	const char *apHammerModeLabels[] = {
		CCLocalize("Normal"),
		CCLocalize("Rotate with cursor"),
		CCLocalize("Rotate with cursor like gun"),
	};

	CUIRect Section, Content, Row, DropDownRow, FirstDropDown, SecondDropDown;
	CatClientMenuBeginSection(View, Section, Content, 174.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Row, &Content);
	pUi->DoLabel(&Row, CCLocalize("Better Movement"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoLine_KeyReader(Content, s_AutoSwitchReader, s_AutoSwitchClear, CCLocalize("Auto Switch"), "+catclient_auto_switch_weapon");
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	g_Config.m_CcAutoSwitchFirstWeapon = maximum<int>(WEAPON_HAMMER, minimum<int>(WEAPON_LASER, g_Config.m_CcAutoSwitchFirstWeapon));
	g_Config.m_CcAutoSwitchSecondWeapon = maximum<int>(WEAPON_HAMMER, minimum<int>(WEAPON_LASER, g_Config.m_CcAutoSwitchSecondWeapon));
	if(g_Config.m_CcAutoSwitchFirstWeapon == g_Config.m_CcAutoSwitchSecondWeapon)
		g_Config.m_CcAutoSwitchSecondWeapon = g_Config.m_CcAutoSwitchFirstWeapon == WEAPON_LASER ? WEAPON_HAMMER : g_Config.m_CcAutoSwitchFirstWeapon + 1;

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &DropDownRow, &Content);
	DropDownRow.VSplitMid(&FirstDropDown, &SecondDropDown, CATCLIENT_MENU_MARGIN_SMALL);
	const int PrevFirstWeapon = g_Config.m_CcAutoSwitchFirstWeapon;
	RenderBetterMovementWeaponDropDown(pUi, FirstDropDown, &g_Config.m_CcAutoSwitchFirstWeapon, s_FirstWeaponDropDownState, s_FirstWeaponDropDownScrollRegion);
	if(g_Config.m_CcAutoSwitchFirstWeapon == g_Config.m_CcAutoSwitchSecondWeapon)
		g_Config.m_CcAutoSwitchFirstWeapon = PrevFirstWeapon;

	const int PrevSecondWeapon = g_Config.m_CcAutoSwitchSecondWeapon;
	RenderBetterMovementWeaponDropDown(pUi, SecondDropDown, &g_Config.m_CcAutoSwitchSecondWeapon, s_SecondWeaponDropDownState, s_SecondWeaponDropDownScrollRegion);
	if(g_Config.m_CcAutoSwitchFirstWeapon == g_Config.m_CcAutoSwitchSecondWeapon)
		g_Config.m_CcAutoSwitchSecondWeapon = PrevSecondWeapon;
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	g_Config.m_TcLimitMouseToScreen = maximum(0, minimum(2, g_Config.m_TcLimitMouseToScreen));
	RenderBetterMovementDropDownLine(pUi, Content, CCLocalize("Limit Mouse"), &g_Config.m_TcLimitMouseToScreen, apLimitMouseLabels, std::size(apLimitMouseLabels), s_LimitMouseDropDownState, s_LimitMouseDropDownScrollRegion);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	g_Config.m_TcHammerRotatesWithCursor = maximum(0, minimum(2, g_Config.m_TcHammerRotatesWithCursor));
	RenderBetterMovementDropDownLine(pUi, Content, CCLocalize("Hammer Mode"), &g_Config.m_TcHammerRotatesWithCursor, apHammerModeLabels, std::size(apHammerModeLabels), s_HammerModeDropDownState, s_HammerModeDropDownScrollRegion);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcScaleMouseDistance, CCLocalize("Scale Mouse Distance"), &g_Config.m_TcScaleMouseDistance, &Content, CATCLIENT_MENU_LINE_SIZE);
}

#endif
