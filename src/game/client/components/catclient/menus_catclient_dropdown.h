#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_DROPDOWN_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_DROPDOWN_H

#include "menus_catclient_common.h"

#include <base/math.h>
#include <base/system.h>

#include <game/client/components/menus.h>

namespace CatClientMenu
{
	struct SFlagOption
	{
		const char *m_pLabel;
		int m_Flag;
	};

	struct CBitmaskButtonState
	{
		static constexpr int MAX_OPTIONS = 8;
		CButtonContainer m_aButtons[MAX_OPTIONS];
	};

	inline int GetBitmaskButtonRows(int NumOptions, int Columns)
	{
		const int SafeColumns = minimum(2, maximum(1, Columns));
		return maximum(1, (NumOptions + SafeColumns - 1) / SafeColumns);
	}

	inline float GetBitmaskButtonHeight(int NumOptions, int Columns)
	{
		const int Rows = GetBitmaskButtonRows(NumOptions, Columns);
		return Rows * LINE_SIZE + maximum(0, Rows - 1) * MARGIN_SMALL;
	}

	inline void DoBitmaskButtonGroup(CMenus *pMenus, CUIRect View, CBitmaskButtonState *pState, int *pFlags, const SFlagOption *pOptions, int NumOptions, int Columns)
	{
		const int SafeColumns = minimum(2, maximum(1, Columns));
		int OptionIndex = 0;

		while(OptionIndex < NumOptions)
		{
			CUIRect Row;
			View.HSplitTop(LINE_SIZE, &Row, &View);

			if(SafeColumns == 1 || OptionIndex == NumOptions - 1)
			{
				const bool Active = (*pFlags & pOptions[OptionIndex].m_Flag) != 0;
				if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], pOptions[OptionIndex].m_pLabel, Active, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
				{
					*pFlags ^= pOptions[OptionIndex].m_Flag;
				}
				++OptionIndex;
			}
			else
			{
				CUIRect LeftButton;
				CUIRect RightButton;
				Row.VSplitMid(&LeftButton, &RightButton, MARGIN_SMALL);

				const bool LeftActive = (*pFlags & pOptions[OptionIndex].m_Flag) != 0;
				if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], pOptions[OptionIndex].m_pLabel, LeftActive, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
				{
					*pFlags ^= pOptions[OptionIndex].m_Flag;
				}
				++OptionIndex;

				if(OptionIndex < NumOptions)
				{
					const bool RightActive = (*pFlags & pOptions[OptionIndex].m_Flag) != 0;
					if(pMenus->DoButton_Menu(&pState->m_aButtons[OptionIndex], pOptions[OptionIndex].m_pLabel, RightActive, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.15f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
					{
						*pFlags ^= pOptions[OptionIndex].m_Flag;
					}
					++OptionIndex;
				}
			}

			if(OptionIndex < NumOptions)
			{
				View.HSplitTop(MARGIN_SMALL, nullptr, &View);
			}
		}
	}
}

#endif