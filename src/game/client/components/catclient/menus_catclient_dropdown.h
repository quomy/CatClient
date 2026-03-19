#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_DROPDOWN_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_DROPDOWN_H

#include "menus_catclient_common.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/localization.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

struct SCatClientMenuFlagOption
{
	const char *m_pLabel;
	int m_Flag;
};

struct SCatClientMenuChoiceOption
{
	const char *m_pLabel;
	int m_Value;
};

struct CCatClientMenuBitmaskButtonState
{
	static constexpr int MAX_OPTIONS = 8;
	CButtonContainer m_aButtons[MAX_OPTIONS];
};

struct CCatClientMenuChoiceButtonState
{
	static constexpr int MAX_OPTIONS = 4;
	CButtonContainer m_aButtons[MAX_OPTIONS];
};

inline int CatClientMenuGetBitmaskButtonRows(int NumOptions, int Columns)
{
	const int SafeColumns = minimum(2, maximum(1, Columns));
	return maximum(1, (NumOptions + SafeColumns - 1) / SafeColumns);
}

inline float CatClientMenuGetBitmaskButtonHeight(int NumOptions, int Columns)
{
	const int Rows = CatClientMenuGetBitmaskButtonRows(NumOptions, Columns);
	return Rows * CATCLIENT_MENU_LINE_SIZE + maximum(0, Rows - 1) * CATCLIENT_MENU_MARGIN_SMALL;
}

inline void CatClientMenuDoBitmaskButtonGroup(CMenus *pMenus, CUIRect View, CCatClientMenuBitmaskButtonState *pState, int *pFlags, const SCatClientMenuFlagOption *pOptions, int NumOptions, int Columns)
{
	const int SafeColumns = minimum(2, maximum(1, Columns));
	int OptionIndex = 0;

	while(OptionIndex < NumOptions)
	{
		CUIRect Row;
		View.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Row, &View);

		if(SafeColumns == 1 || OptionIndex == NumOptions - 1)
		{
			const bool Active = (*pFlags & pOptions[OptionIndex].m_Flag) != 0;
			if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], CCLocalize(pOptions[OptionIndex].m_pLabel), Active, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				*pFlags ^= pOptions[OptionIndex].m_Flag;
			}
			++OptionIndex;
		}
		else
		{
			CUIRect LeftButton;
			CUIRect RightButton;
			Row.VSplitMid(&LeftButton, &RightButton, CATCLIENT_MENU_MARGIN_SMALL);

			const bool LeftActive = (*pFlags & pOptions[OptionIndex].m_Flag) != 0;
			if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], CCLocalize(pOptions[OptionIndex].m_pLabel), LeftActive, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				*pFlags ^= pOptions[OptionIndex].m_Flag;
			}
			++OptionIndex;

			if(OptionIndex < NumOptions)
			{
				const bool RightActive = (*pFlags & pOptions[OptionIndex].m_Flag) != 0;
				if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], CCLocalize(pOptions[OptionIndex].m_pLabel), RightActive, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
				{
					*pFlags ^= pOptions[OptionIndex].m_Flag;
				}
				++OptionIndex;
			}
		}

		if(OptionIndex < NumOptions)
		{
			View.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &View);
		}
	}
}

inline void CatClientMenuDoChoiceButtonGroup(CMenus *pMenus, CUIRect View, CCatClientMenuChoiceButtonState *pState, int *pValue, const SCatClientMenuChoiceOption *pOptions, int NumOptions, int Columns)
{
	dbg_assert(NumOptions <= CCatClientMenuChoiceButtonState::MAX_OPTIONS, "too many choice options");
	const int SafeColumns = minimum(2, maximum(1, Columns));
	int OptionIndex = 0;

	while(OptionIndex < NumOptions)
	{
		CUIRect Row;
		View.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Row, &View);

		if(SafeColumns == 1 || OptionIndex == NumOptions - 1)
		{
			const bool Active = *pValue == pOptions[OptionIndex].m_Value;
			if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], CCLocalize(pOptions[OptionIndex].m_pLabel), Active, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				*pValue = pOptions[OptionIndex].m_Value;
			}
			++OptionIndex;
		}
		else
		{
			CUIRect LeftButton;
			CUIRect RightButton;
			Row.VSplitMid(&LeftButton, &RightButton, CATCLIENT_MENU_MARGIN_SMALL);

			const bool LeftActive = *pValue == pOptions[OptionIndex].m_Value;
			if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], CCLocalize(pOptions[OptionIndex].m_pLabel), LeftActive, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				*pValue = pOptions[OptionIndex].m_Value;
			}
			++OptionIndex;

			if(OptionIndex < NumOptions)
			{
				const bool RightActive = *pValue == pOptions[OptionIndex].m_Value;
				if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], CCLocalize(pOptions[OptionIndex].m_pLabel), RightActive, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
				{
					*pValue = pOptions[OptionIndex].m_Value;
				}
				++OptionIndex;
			}
		}

		if(OptionIndex < NumOptions)
		{
			View.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &View);
		}
	}
}

#endif
