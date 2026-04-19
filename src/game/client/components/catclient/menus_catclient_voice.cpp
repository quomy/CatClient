#include "menus_catclient_common.h"

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

void CMenus::RenderSettingsCatClientVoice(CUIRect MainView)
{
	CatClientMenuConstrainWidth(MainView, MainView, 980.0f);
	GameClient()->m_VoiceChat.RenderSettingsPage(MainView);
}
