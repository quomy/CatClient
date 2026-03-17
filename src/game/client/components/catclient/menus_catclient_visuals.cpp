#include "catclient.h"
#include "menus_catclient_dropdown.h"
#include "menus_catclient_slider.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

#include <iterator>

static const SCatClientMenuFlagOption gs_aHideEffectOptions[] = {
	{"Freeze Flakes", CCatClient::HIDE_EFFECT_FREEZE_FLAKES},
	{"Hammer hits", CCatClient::HIDE_EFFECT_HAMMER_HITS},
	{"Jumps", CCatClient::HIDE_EFFECT_JUMPS},
};

static const SCatClientMenuFlagOption gs_aChatAnimationOptions[] = {
	{"Open and close", CCatClient::CHAT_ANIM_OPEN_CLOSE},
	{"Typing", CCatClient::CHAT_ANIM_TYPING},
};

static void RenderAspectRatioSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CButtonContainer s_Aspect43Button, s_Aspect1610Button, s_Aspect169Button, s_Aspect54Button;
	static CCatClientMenuSliderState s_AspectRatioSlider;

	CUIRect Section, Content, Label, Button, ButtonLeft, ButtonRight;
	CatClientMenuBeginSection(View, Section, Content, 119.0f);
	if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_ASPECT_RATIO))
		pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_ASPECT_RATIO, Section);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Aspect Ratio"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAspectRatioEnabled, Localize("Stretch Ingame Aspect"), &g_Config.m_CcAspectRatioEnabled, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, CATCLIENT_MENU_MARGIN_SMALL);
	CUIRect Aspect43, Aspect1610, Aspect169, Aspect54;
	ButtonLeft.VSplitMid(&Aspect43, &Aspect1610, CATCLIENT_MENU_MARGIN_SMALL);
	ButtonRight.VSplitMid(&Aspect169, &Aspect54, CATCLIENT_MENU_MARGIN_SMALL);

	if(pMenus->DoButton_Menu(&s_Aspect43Button, "4:3", 0, &Aspect43, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatio = 130;
	}
	if(pMenus->DoButton_Menu(&s_Aspect1610Button, "16:10", 0, &Aspect1610, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatio = 160;
	}
	if(pMenus->DoButton_Menu(&s_Aspect169Button, "16:9", 0, &Aspect169, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatio = 177;
	}
	if(pMenus->DoButton_Menu(&s_Aspect54Button, "5:4", 0, &Aspect54, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatio = 115;
	}

	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);

	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAspectRatio, &s_AspectRatioSlider, &g_Config.m_CcAspectRatio, Button, 100, 250, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %d.%02d", Localize("Ratio"), Value / 100, Value % 100);
	});
}

static void RenderHideEffectsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuBitmaskButtonState s_HideEffectsButtons;

	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 94.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Hide Effects"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CatClientMenuDoBitmaskButtonGroup(pMenus, Content, &s_HideEffectsButtons, &g_Config.m_CcHideEffects, gs_aHideEffectOptions, std::size(gs_aHideEffectOptions), 2);
}

static void RenderChatAnimationsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuBitmaskButtonState s_ChatAnimationsButtons;

	CUIRect Section, Content, Label;
	CatClientMenuBeginSection(View, Section, Content, 69.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Chat Animations"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	CatClientMenuDoBitmaskButtonGroup(pMenus, Content, &s_ChatAnimationsButtons, &g_Config.m_CcChatAnimations, gs_aChatAnimationOptions, std::size(gs_aChatAnimationOptions), 2);
}

static void RenderModernUiSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CCatClientMenuSliderState s_UiScaleSlider;

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 90.0f);
	if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_UI_SCALE))
		pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_UI_SCALE, Section);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, Localize("Modern UI"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcHorizontalSettingsTabs, Localize("New settings menu"), &g_Config.m_CcHorizontalSettingsTabs, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CatClientMenuDoSliderOption(pUi, &g_Config.m_CcUiScale, &s_UiScaleSlider, &g_Config.m_CcUiScale, Button, 50, 100, &CUi::ms_LinearScrollbarScale, true, [](char *pBuf, size_t BufSize, int Value) {
		str_format(pBuf, BufSize, "%s: %d%%", Localize("UI Scale"), Value);
	});
}

void CMenus::RenderSettingsCatClientVisuals(CUIRect MainView)
{
	CUIRect LeftView, RightView;
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CatClientMenuConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, CATCLIENT_MENU_MARGIN_BETWEEN_VIEWS);

	RenderAspectRatioSection(this, Ui(), LeftView);
	LeftView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &LeftView);
	RenderModernUiSection(this, Ui(), LeftView);

	RenderHideEffectsSection(this, Ui(), RightView);
	RightView.HSplitTop(CATCLIENT_MENU_SECTION_SPACING, nullptr, &RightView);

	RenderChatAnimationsSection(this, Ui(), RightView);
}
