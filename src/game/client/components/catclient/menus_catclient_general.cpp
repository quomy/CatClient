#include "catclient.h"
#include "menus_catclient_dropdown.h"
#include "menus_catclient_slider.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/lineinput.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <iterator>

#include "modules/general/menus_catclient_general_protection.h"
#include "modules/general/menus_catclient_general_fast_inputs.h"
#include "modules/general/menus_catclient_general_better_movement.h"
#include "modules/general/menus_catclient_general_browser.h"
#include "modules/general/menus_catclient_general_audio.h"
#include "modules/general/menus_catclient_general_voice.h"

void CMenus::RenderSettingsCatClientGeneral(CUIRect MainView)
{
	static CScrollRegion s_GeneralScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = CATCLIENT_MENU_MARGIN_SMALL;
	s_GeneralScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	CUIRect LeftView, RightView;
	MainView.y += ScrollOffset.y;
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	MainView.VSplitLeft(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	MainView.VSplitRight(CATCLIENT_MENU_MARGIN_SMALL, &MainView, nullptr);
	CatClientMenuConstrainWidth(MainView, MainView, 760.0f);
	const CUIRect ContentView = MainView;
	MainView.VSplitMid(&LeftView, &RightView, CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS);

	RenderVoiceChatSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);

	RenderTeamProtectionSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);

	RenderFastInputsSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);

	RenderBetterMovementSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);

	RenderServerBrowserSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderAutoLagMessageSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderMuteSoundsSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderAntiQuitSection(this, Ui(), RightView);

	CUIRect ScrollRegion;
	ScrollRegion.x = ContentView.x;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + CATCLIENT_MENU_MARGIN_SMALL * 2.0f;
	ScrollRegion.w = ContentView.w;
	ScrollRegion.h = 0.0f;
	s_GeneralScrollRegion.AddRect(ScrollRegion);
	s_GeneralScrollRegion.End();
}
