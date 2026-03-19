#include "menus_catclient_common.h"

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

void CMenus::RenderSettingsCatClientVoice(CUIRect MainView)
{
	CatClientMenuConstrainWidth(MainView, MainView, 980.0f);

	CUIRect BindCard, PanelView;
	MainView.HSplitTop(56.0f, &BindCard, &PanelView);
	BindCard.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f), IGraphics::CORNER_ALL, 6.0f);

	CUIRect BindInner = BindCard;
	BindInner.Margin(12.0f, &BindInner);
	CUIRect BindLabel, BindRow;
	BindInner.HSplitTop(14.0f, &BindLabel, &BindInner);
	Ui()->DoLabel(&BindLabel, CCLocalize("Overlay"), 13.0f, TEXTALIGN_ML);
	BindInner.HSplitTop(6.0f, nullptr, &BindInner);
	BindInner.HSplitTop(24.0f, &BindRow, nullptr);
	GameClient()->m_VoiceChat.RenderMenuPanelToggleBind(BindRow);

	PanelView.HSplitTop(10.0f, nullptr, &PanelView);
	GameClient()->m_VoiceChat.RenderMenuPanel(PanelView);
}
