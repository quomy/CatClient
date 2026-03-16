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

	const SFlagOption s_aHideEffectOptions[] = {
		{"Freeze Flakes", CCatClient::HIDE_EFFECT_FREEZE_FLAKES},
		{"Hammer hits", CCatClient::HIDE_EFFECT_HAMMER_HITS},
		{"Jumps", CCatClient::HIDE_EFFECT_JUMPS},
	};

	const SFlagOption s_aChatAnimationOptions[] = {
		{"Open and close", CCatClient::CHAT_ANIM_OPEN_CLOSE},
		{"Typing", CCatClient::CHAT_ANIM_TYPING},
	};

	void RenderAspectRatioSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CButtonContainer s_Aspect43Button, s_Aspect1610Button, s_Aspect169Button, s_Aspect54Button;
		static CSliderState s_AspectRatioSlider;

		CUIRect Section, Content, Label, Button, ButtonLeft, ButtonRight;
		BeginSection(View, Section, Content, 119.0f);
		if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_ASPECT_RATIO))
			pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_ASPECT_RATIO, Section);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Aspect Ratio"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAspectRatioEnabled, Localize("Stretch Ingame Aspect"), &g_Config.m_CcAspectRatioEnabled, &Content, LINE_SIZE);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(LINE_SIZE, &Button, &Content);
		Button.VSplitMid(&ButtonLeft, &ButtonRight, MARGIN_SMALL);
		CUIRect Aspect43, Aspect1610, Aspect169, Aspect54;
		ButtonLeft.VSplitMid(&Aspect43, &Aspect1610, MARGIN_SMALL);
		ButtonRight.VSplitMid(&Aspect169, &Aspect54, MARGIN_SMALL);

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

		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
		Content.HSplitTop(LINE_SIZE, &Button, &Content);

		DoSliderOption(pUi, &g_Config.m_CcAspectRatio, &s_AspectRatioSlider, &g_Config.m_CcAspectRatio, Button, 100, 250, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %d.%02d", Localize("Ratio"), Value / 100, Value % 100);
		});
	}

	void RenderHideEffectsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CBitmaskButtonState s_HideEffectsButtons;

		CUIRect Section, Content, Label;
		BeginSection(View, Section, Content, 94.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Hide Effects"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		DoBitmaskButtonGroup(pMenus, Content, &s_HideEffectsButtons, &g_Config.m_CcHideEffects, s_aHideEffectOptions, std::size(s_aHideEffectOptions), 2);
	}

	void RenderChatAnimationsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CBitmaskButtonState s_ChatAnimationsButtons;

		CUIRect Section, Content, Label;
		BeginSection(View, Section, Content, 69.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Chat Animations"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		DoBitmaskButtonGroup(pMenus, Content, &s_ChatAnimationsButtons, &g_Config.m_CcChatAnimations, s_aChatAnimationOptions, std::size(s_aChatAnimationOptions), 2);
	}

	void RenderModernUiSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		static CSliderState s_UiScaleSlider;

		CUIRect Section, Content, Label, Button;
		BeginSection(View, Section, Content, 90.0f);
		if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_UI_SCALE))
			pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_UI_SCALE, Section);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Modern UI"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcHorizontalSettingsTabs, Localize("New settings menu"), &g_Config.m_CcHorizontalSettingsTabs, &Content, LINE_SIZE);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		Content.HSplitTop(LINE_SIZE, &Button, &Content);
		DoSliderOption(pUi, &g_Config.m_CcUiScale, &s_UiScaleSlider, &g_Config.m_CcUiScale, Button, 50, 100, &CUi::ms_LinearScrollbarScale, true, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %d%%", Localize("UI Scale"), Value);
		});
	}
}

void CMenus::RenderSettingsCatClientVisuals(CUIRect MainView)
{
	using namespace CatClientMenu;

	CUIRect LeftView, RightView;
	MainView.HSplitTop(MARGIN_SMALL, nullptr, &MainView);
	ConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	RenderAspectRatioSection(this, Ui(), LeftView);
	LeftView.HSplitTop(SECTION_SPACING, nullptr, &LeftView);
	RenderModernUiSection(this, Ui(), LeftView);

	RenderHideEffectsSection(this, Ui(), RightView);
	RightView.HSplitTop(SECTION_SPACING, nullptr, &RightView);

	RenderChatAnimationsSection(this, Ui(), RightView);
}
