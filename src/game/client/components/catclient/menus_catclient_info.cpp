#include "menus_catclient_common.h"
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/client/components/menus.h>
#include <game/localization.h>

void CMenus::RenderSettingsCatClientInfo(CUIRect MainView)
{
	using namespace CatClientMenu;
	CUIRect TopView, BottomView, LeftView, RightView, Section, Content, Button, Label;
	MainView.HSplitTop(MARGIN_SMALL, nullptr, &MainView);
	ConstrainWidth(MainView, MainView, 760.0f);
	MainView.HSplitTop(115.0f, &TopView, &BottomView);
	TopView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	BeginSection(LeftView, Section, Content, 115.0f);
	Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
	Ui()->DoLabel(&Label, "CatClient Links", HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

	static CButtonContainer s_WebsiteButton;
	Content.HSplitTop(LINE_SIZE, &Button, &Content);
	if(DoButtonLineSize_Menu(&s_WebsiteButton, "Website", 0, &Button, LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://teeworlds.xyz");
	Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
	static CButtonContainer s_DiscordButton;
	Content.HSplitTop(LINE_SIZE, &Button, &Content);
	if(DoButtonLineSize_Menu(&s_DiscordButton, "Discord", 0, &Button, LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/28YFTUW5Jg");
	

	BeginSection(RightView, Section, Content, 115.0f);
	Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
	Ui()->DoLabel(&Label, "CatClient Developer", HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

	CUIRect TeeRect, DevCardRect;
	Content.HSplitTop(DEVELOPER_CARD_SIZE, &DevCardRect, &Content);
	DevCardRect.VSplitLeft(DEVELOPER_CARD_SIZE, &TeeRect, &Label);
	Ui()->DoLabel(&Label, "quomy", LINE_SIZE, TEXTALIGN_ML);
	RenderDevSkin(TeeRect.Center(), DEVELOPER_TEE_SIZE, "xp", "default", true, 255, 4980530, EMOTE_NORMAL, false, true);
	BottomView.HSplitTop(SECTION_SPACING, nullptr, &BottomView);
	BeginSection(BottomView, Section, Content, 90.0f);
	Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
	Ui()->DoLabel(&Label, "Inspired By", HEADLINE_FONT_SIZE, TEXTALIGN_MC);
	Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

	static CButtonContainer s_BestClientButton, s_RushieClientButton;
	CUIRect ButtonLeft, ButtonRight;
	Content.HSplitTop(LINE_SIZE, &Button, &Content);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MARGIN_SMALL);
	if(DoButtonLineSize_Menu(&s_BestClientButton, "BestClient", 0, &ButtonLeft, LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/tpGeput2sa");
	if(DoButtonLineSize_Menu(&s_RushieClientButton, "RushieClient", 0, &ButtonRight, LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/bRvQYeuJUD");
}
