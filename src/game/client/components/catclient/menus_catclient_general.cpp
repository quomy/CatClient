#include "catclient.h"
#include "menus_catclient_dropdown.h"
#include "menus_catclient_slider.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

#include <iterator>

namespace
{
	using namespace CatClientMenu;
	const SFlagOption s_aMuteSoundOptions[] = {
		{"Others hook sound", CCatClient::MUTE_SOUND_OTHERS_HOOK},
		{"Others hammer sound", CCatClient::MUTE_SOUND_OTHERS_HAMMER},
		{"Local hammer sound", CCatClient::MUTE_SOUND_LOCAL_HAMMER},
		{"Weapon switch sound", CCatClient::MUTE_SOUND_WEAPON_SWITCH},
		{"Jump sound", CCatClient::MUTE_SOUND_JUMP},
	};

	void RenderTeamProtectionSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CSliderState s_LockDelaySlider;
		static CSliderState s_AntiKillDelaySlider;

		CUIRect Section, Content, Label, Button;
		BeginSection(View, Section, Content, 143.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Team Protection"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAutoTeamLock, Localize("Auto Team Lock"), &g_Config.m_CcAutoTeamLock, &Content, LINE_SIZE);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(LINE_SIZE, &Button, &Content);
		DoSliderOption(pUi, &g_Config.m_CcAutoTeamLockDelay, &s_LockDelaySlider, &g_Config.m_CcAutoTeamLockDelay, Button, 0, 60, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %ds", Localize("Lock Delay"), Value);
		});
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAntiKill, Localize("Anti Kill"), &g_Config.m_CcAntiKill, &Content, LINE_SIZE);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(LINE_SIZE, &Button, &Content);
		DoSliderOption(pUi, &g_Config.m_CcAntiKillDelay, &s_AntiKillDelaySlider, &g_Config.m_CcAntiKillDelay, Button, 1, 60, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %d min", Localize("Kill Delay"), Value);
		});
	}

	void RenderAntiQuitSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		CUIRect Section, Content, Label;
		BeginSection(View, Section, Content, 69.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Anti Quit"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAntiQuit, Localize("Ask before quitting"), &g_Config.m_CcAntiQuit, &Content, LINE_SIZE);
	}

	void RenderServerBrowserSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CSliderState s_RefreshIntervalSlider;

		CUIRect Section, Content, Label, Button;
		BeginSection(View, Section, Content, 94.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Server Browser"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcServerBrowserAutoRefresh, Localize("Auto Refresh"), &g_Config.m_CcServerBrowserAutoRefresh, &Content, LINE_SIZE);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(LINE_SIZE, &Button, &Content);
		DoSliderOption(pUi, &g_Config.m_CcServerBrowserRefreshInterval, &s_RefreshIntervalSlider, &g_Config.m_CcServerBrowserRefreshInterval, Button, 1, 30, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %ds", Localize("Refresh Every"), Value);
		});
	}

	void RenderMuteSoundsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CBitmaskButtonState s_MuteSoundsButtons;

		CUIRect Section, Content, Label;
		BeginSection(View, Section, Content, 169.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Mute Sounds"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		DoBitmaskButtonGroup(pMenus, Content, &s_MuteSoundsButtons, &g_Config.m_CcMuteSounds, s_aMuteSoundOptions, std::size(s_aMuteSoundOptions), 1);
	}
}

void CMenus::RenderSettingsCatClientGeneral(CUIRect MainView)
{
	using namespace CatClientMenu;

	CUIRect LeftView, RightView;
	MainView.HSplitTop(MARGIN_SMALL, nullptr, &MainView);
	ConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	RenderTeamProtectionSection(this, Ui(), LeftView);
	LeftView.HSplitTop(SECTION_SPACING, nullptr, &LeftView);

	RenderAntiQuitSection(this, Ui(), LeftView);

	RenderServerBrowserSection(this, Ui(), RightView);
	RightView.HSplitTop(SECTION_SPACING, nullptr, &RightView);

	RenderMuteSoundsSection(this, Ui(), RightView);
}
