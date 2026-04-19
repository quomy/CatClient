#include "menus_catclient_common.h"
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/client/components/menus.h>
#include <game/localization.h>

void CMenus::RenderSettingsCatClientInfo(CUIRect MainView)
{
	CUIRect TopView, BottomView, LeftView, RightView, Section, Content, Button, Label;
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CatClientMenuConstrainWidth(MainView, MainView, 760.0f);
	MainView.HSplitTop(145.0f, &TopView, &BottomView);
	TopView.VSplitMid(&LeftView, &RightView, CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS);

	CatClientMenuBeginSection(LeftView, Section, Content, 145.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	Ui()->DoLabel(&Label, CCLocalize("CatClient Links"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	static CButtonContainer s_WebsiteButton;
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	if(DoButtonLineSize_Menu(&s_WebsiteButton, CCLocalize("Website"), 0, &Button, CATCLIENT_MENU_LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://teeworlds.xyz");
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	static CButtonContainer s_DiscordButton;
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	if(DoButtonLineSize_Menu(&s_DiscordButton, CCLocalize("Discord"), 0, &Button, CATCLIENT_MENU_LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/28YFTUW5Jg");

	CatClientMenuBeginSection(RightView, Section, Content, 145.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	Ui()->DoLabel(&Label, CCLocalize("CatClient Developer"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CUIRect TeeRect, DevCardRect;
	Content.HSplitTop(CATCLIENT_MENU_DEVELOPER_CARD_SIZE, &DevCardRect, &Content);
	DevCardRect.VSplitLeft(CATCLIENT_MENU_DEVELOPER_CARD_SIZE, &TeeRect, &Label);
	Ui()->DoLabel(&Label, "quomy", CATCLIENT_MENU_LINE_SIZE, TEXTALIGN_ML);
	RenderDevSkin(TeeRect.Center(), CATCLIENT_MENU_DEVELOPER_TEE_SIZE, "xp", "default", true, 255, 4980530, EMOTE_NORMAL, false, true);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_DEVELOPER_CARD_SIZE, &DevCardRect, &Content);
	DevCardRect.VSplitLeft(CATCLIENT_MENU_DEVELOPER_CARD_SIZE, &TeeRect, &Label);
	Ui()->DoLabel(&Label, "rXelelo", CATCLIENT_MENU_LINE_SIZE, TEXTALIGN_ML);
	RenderDevSkin(TeeRect.Center(), CATCLIENT_MENU_DEVELOPER_TEE_SIZE, "mushkitt", "default", false, 0, 0, EMOTE_NORMAL, false, true);
	BottomView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &BottomView);
	CatClientMenuBeginSection(BottomView, Section, Content, 90.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	Ui()->DoLabel(&Label, CCLocalize("Inspired By"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_MC);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	static CButtonContainer s_BestClientButton, s_RushieClientButton;
	CUIRect ButtonLeft, ButtonRight;
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, CATCLIENT_MENU_MARGIN_SMALL);
	if(DoButtonLineSize_Menu(&s_BestClientButton, "BestClient", 0, &ButtonLeft, CATCLIENT_MENU_LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/tpGeput2sa");
	if(DoButtonLineSize_Menu(&s_RushieClientButton, "RushieClient", 0, &ButtonRight, CATCLIENT_MENU_LINE_SIZE, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/bRvQYeuJUD");
}
