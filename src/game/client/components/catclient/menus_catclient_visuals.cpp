#include "catclient.h"
#include "menus_catclient_dropdown.h"
#include "menus_catclient_slider.h"

#include <base/types.h>
#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/components/menus.h>
#include <game/localization.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "modules/visuals/menus_catclient_visuals_aspect.h"
#include "modules/visuals/menus_catclient_visuals_effects.h"
#include "modules/visuals/menus_catclient_visuals_background.h"

void CMenus::RenderSettingsCatClientVisuals(CUIRect MainView)
{
	CUIRect LeftView, RightView;
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CatClientMenuConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS);

	RenderAspectRatioSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);
	RenderModernUiSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);
	RenderEnhancedLaserSection(this, Ui(), LeftView);

	RenderHideEffectsSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderChatAnimationsSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderCustomBackgroundSection(this, Ui(), RightView);
}
