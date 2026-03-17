#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_COMMON_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_COMMON_H

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

constexpr float CATCLIENT_MENU_FONT_SIZE = 14.0f;
constexpr float CATCLIENT_MENU_SMALL_FONT_SIZE = 13.0f;
constexpr float CATCLIENT_MENU_LINE_SIZE = 20.0f;
constexpr float CATCLIENT_MENU_HEADLINE_FONT_SIZE = 20.0f;
constexpr float CATCLIENT_MENU_HEADLINE_HEIGHT = CATCLIENT_MENU_HEADLINE_FONT_SIZE;
constexpr float CATCLIENT_MENU_MARGIN = 10.0f;
constexpr float CATCLIENT_MENU_MARGIN_SMALL = 5.0f;
constexpr float CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS = 20.0f;
constexpr float CATCLIENT_MENU_SECTION_PADDING = 12.0f;
constexpr float CATCLIENT_MENU_SECTION_ROUNDING = 10.0f;
constexpr float CATCLIENT_MENU_SECTION_SPACING = 14.0f;
constexpr float CATCLIENT_MENU_TAB_WIDTH = 96.0f;
constexpr float CATCLIENT_MENU_CONTENT_WIDTH_NARROW = 460.0f;
constexpr float CATCLIENT_MENU_CONTENT_WIDTH_WIDE = 560.0f;
constexpr float CATCLIENT_MENU_DEVELOPER_TEE_SIZE = 50.0f;
constexpr float CATCLIENT_MENU_DEVELOPER_CARD_SIZE = CATCLIENT_MENU_DEVELOPER_TEE_SIZE + CATCLIENT_MENU_MARGIN_SMALL;

inline void CatClientMenuConstrainWidth(const CUIRect &View, CUIRect &Result, float MaxWidth)
{
	Result = View;
	if(Result.w > MaxWidth)
	{
		Result.VMargin((Result.w - MaxWidth) / 2.0f, &Result);
	}
}

inline void CatClientMenuBeginSection(CUIRect &View, CUIRect &Section, CUIRect &Content, float Height)
{
	View.HSplitTop(Height, &Section, &View);
	Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, CATCLIENT_MENU_SECTION_ROUNDING);
	Section.Margin(CATCLIENT_MENU_SECTION_PADDING, &Content);
}

#endif
