#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_VOICE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_VOICE_H

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

static void RenderVoiceChatSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	CUIRect Section, Content, Row;
	CatClientMenuBeginSection(View, Section, Content, 74.0f);

	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Row, &Content);
	pUi->DoLabel(&Row, CCLocalize("Voice Chat"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(24.0f, &Row, &Content);
	pMenus->MenuGameClient()->m_VoiceChat.RenderMenuPanelToggleBind(Row);
}

#endif
