#include "catclient.h"
#include "menus_catclient_common.h"

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "modules/streamer/menus_catclient_streamer_settings.h"
#include "modules/streamer/menus_catclient_streamer_words.h"

void CMenus::RenderSettingsCatClientStreamer(CUIRect MainView)
{
	CUIRect LeftView, RightView;
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CatClientMenuConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS);

	RenderStreamerSettingsSection(this, Ui(), LeftView);
	const EBlockedWordsAction Action = RenderBlockedWordsSection(this, Ui(), &GameClient()->m_CatClient, RightView);
	if(Action == BLOCKED_WORDS_ACTION_OPEN)
	{
		PopupConfirm(
			CCLocalize("Open blocked words?"),
			CCLocalize("Are you sure you want to open the blocked words list, and did you hide your screen on stream?"),
			CCLocalize("Open"),
			CCLocalize("Cancel"),
			&CMenus::PopupConfirmOpenBlockedWords);
	}
	else if(Action == BLOCKED_WORDS_ACTION_HIDE)
	{
		gs_ShowBlockedWordsList = false;
	}
}
