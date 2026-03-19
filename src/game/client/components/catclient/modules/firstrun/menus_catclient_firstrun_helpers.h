#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_FIRSTRUN_MENUS_CATCLIENT_FIRSTRUN_HELPERS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_FIRSTRUN_MENUS_CATCLIENT_FIRSTRUN_HELPERS_H

static void ExpandAndClampRect(CUIRect &Rect, const CUIRect &Bounds, float Padding)
{
	Rect.x -= Padding;
	Rect.y -= Padding;
	Rect.w += Padding * 2.0f;
	Rect.h += Padding * 2.0f;

	if(Rect.x < Bounds.x)
		Rect.x = Bounds.x;
	if(Rect.y < Bounds.y)
		Rect.y = Bounds.y;
	if(Rect.x + Rect.w > Bounds.x + Bounds.w)
		Rect.w = Bounds.x + Bounds.w - Rect.x;
	if(Rect.y + Rect.h > Bounds.y + Bounds.h)
		Rect.h = Bounds.y + Bounds.h - Rect.y;
}

static const char *FirstRunTitle(CMenus::EFirstRunSetupStep Step)
{
	switch(Step)
	{
	case CMenus::FIRST_RUN_SETUP_UI_SCALE:
		return CCLocalize("Choose UI Scale");
	case CMenus::FIRST_RUN_SETUP_ASPECT_RATIO:
		return CCLocalize("Choose Aspect Ratio");
	case CMenus::FIRST_RUN_SETUP_FAST_INPUTS:
		return CCLocalize("Fast Inputs Moved");
	case CMenus::FIRST_RUN_SETUP_CURSORS:
		return CCLocalize("Choose Cursor Preset");
	case CMenus::FIRST_RUN_SETUP_AUDIO:
		return CCLocalize("Choose Audio Preset");
	default:
		return "";
	}
}

static const char *FirstRunDescription(CMenus::EFirstRunSetupStep Step)
{
	switch(Step)
	{
	case CMenus::FIRST_RUN_SETUP_UI_SCALE:
		return CCLocalize("Set the menu size that feels comfortable. You can keep the default and continue.");
	case CMenus::FIRST_RUN_SETUP_ASPECT_RATIO:
		return CCLocalize("Pick the ingame stretch ratio you want to use. The menu itself stays unaffected.");
	case CMenus::FIRST_RUN_SETUP_FAST_INPUTS:
		return CCLocalize("Fast Inputs moved from TClient into CatClient > General.");
	case CMenus::FIRST_RUN_SETUP_CURSORS:
		return CCLocalize("Choose a custom cursor preset or leave Default to keep the original game cursor.");
	case CMenus::FIRST_RUN_SETUP_AUDIO:
		return CCLocalize("Choose an audio preset or leave Default to use sounds from the original game data.");
	default:
		return "";
	}
}

bool CMenus::IsFirstRunSetupActive() const
{
	return str_comp(g_Config.m_CcFirstRun, "false") != 0;
}

bool CMenus::IsFirstRunSetupStepActive(EFirstRunSetupStep Step) const
{
	return IsFirstRunSetupActive() && m_FirstRunSetupStep == Step;
}

void CMenus::RegisterFirstRunFocus(EFirstRunSetupStep Step, const CUIRect &Rect)
{
	if(!IsFirstRunSetupStepActive(Step))
		return;

	m_aFirstRunFocus[Step].m_Valid = true;
	m_aFirstRunFocus[Step].m_Rect = Rect;
}

void CMenus::ResetFirstRunFocus()
{
	for(auto &Focus : m_aFirstRunFocus)
		Focus.m_Valid = false;
}

#endif
