#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_COMMON_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_COMMON_H

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

namespace CatClientMenu
{
	constexpr float FONT_SIZE = 14.0f;
	constexpr float SMALL_FONT_SIZE = 13.0f;
	constexpr float LINE_SIZE = 20.0f;
	constexpr float HEADLINE_FONT_SIZE = 20.0f;
	constexpr float HEADLINE_HEIGHT = HEADLINE_FONT_SIZE;
	constexpr float MARGIN = 10.0f;
	constexpr float MARGIN_SMALL = 5.0f;
	constexpr float MARGIN_BETWEEN_VIEWS = 20.0f;
	constexpr float SECTION_PADDING = 12.0f;
	constexpr float SECTION_ROUNDING = 10.0f;
	constexpr float SECTION_SPACING = 14.0f;
	constexpr float TAB_WIDTH = 96.0f;
	constexpr float CONTENT_WIDTH_NARROW = 460.0f;
	constexpr float CONTENT_WIDTH_WIDE = 560.0f;
	constexpr float DEVELOPER_TEE_SIZE = 50.0f;
	constexpr float DEVELOPER_CARD_SIZE = DEVELOPER_TEE_SIZE + MARGIN_SMALL;

	inline void ConstrainWidth(const CUIRect &View, CUIRect &Result, float MaxWidth)
	{
		Result = View;
		if(Result.w > MaxWidth)
		{
			Result.VMargin((Result.w - MaxWidth) / 2.0f, &Result);
		}
	}

	inline void BeginSection(CUIRect &View, CUIRect &Section, CUIRect &Content, float Height)
	{
		View.HSplitTop(Height, &Section, &View);
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, SECTION_ROUNDING);
		Section.Margin(SECTION_PADDING, &Content);
	}
}
#endif
