#include "menus_catclient_common.h"

#include <engine/graphics.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

#include <algorithm>

void CMenus::RenderSettingsCatClient(CUIRect MainView)
{
	CUIRect TabBar, Tabs, Button;
	MainView.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &TabBar, &MainView);
	CatClientMenuConstrainWidth(TabBar, Tabs, CATCLIENT_MENU_TAB_WIDTH * (float)NUM_CATCLIENT_TABS);
	static CButtonContainer s_aTabs[NUM_CATCLIENT_TABS] = {};
	const char *apTabNames[NUM_CATCLIENT_TABS] = {
		Localize("General"),
		Localize("Visuals"),
		Localize("Shop"),
		Localize("Streamer"),
		Localize("Info"),
	};

	for(int Tab = 0; Tab < NUM_CATCLIENT_TABS; ++Tab)
	{
		Tabs.VSplitLeft(CATCLIENT_MENU_TAB_WIDTH, &Button, &Tabs);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : (Tab == NUM_CATCLIENT_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aTabs[Tab], apTabNames[Tab], m_CatClientTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			m_CatClientTab = Tab;
		}
	}

	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CUIRect AnimatedMainView;
	BeginPageTransition(m_CatClientTransition, m_CatClientTab, MainView, AnimatedMainView);
	if(m_CatClientTab == CATCLIENT_TAB_GENERAL)
	{
		RenderSettingsCatClientGeneral(AnimatedMainView);
	}
	else if(m_CatClientTab == CATCLIENT_TAB_VISUALS)
	{
		RenderSettingsCatClientVisuals(AnimatedMainView);
	}
	else if(m_CatClientTab == CATCLIENT_TAB_SHOP)
	{
		RenderSettingsCatClientShop(AnimatedMainView);
	}
	else if(m_CatClientTab == CATCLIENT_TAB_STREAMER)
	{
		RenderSettingsCatClientStreamer(AnimatedMainView);
	}
	else
	{
		RenderSettingsCatClientInfo(AnimatedMainView);
	}
	EndPageTransition();
}
