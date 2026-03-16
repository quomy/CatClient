#include <base/math.h>
#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

namespace
{
	static void ExpandAndClampRect(CUIRect &Rect, const CUIRect &Bounds, float Padding)
	{
		Rect.x -= Padding;
		Rect.y -= Padding;
		Rect.w += Padding * 2.0f;
		Rect.h += Padding * 2.0f;

		if(Rect.x < Bounds.x)
			Rect.x = Bounds.x;
		if(Rect.y < Bounds.y)
			Rect.y = Bounds.y;
		if(Rect.x + Rect.w > Bounds.x + Bounds.w)
			Rect.w = Bounds.x + Bounds.w - Rect.x;
		if(Rect.y + Rect.h > Bounds.y + Bounds.h)
			Rect.h = Bounds.y + Bounds.h - Rect.y;
	}

	static const char *FirstRunTitle(CMenus::EFirstRunSetupStep Step)
	{
		switch(Step)
		{
		case CMenus::FIRST_RUN_SETUP_UI_SCALE:
			return Localize("Choose UI Scale");
		case CMenus::FIRST_RUN_SETUP_ASPECT_RATIO:
			return Localize("Choose Aspect Ratio");
		case CMenus::FIRST_RUN_SETUP_CURSORS:
			return Localize("Choose Cursor Preset");
		case CMenus::FIRST_RUN_SETUP_AUDIO:
			return Localize("Choose Audio Preset");
		default:
			return "";
		}
	}

	static const char *FirstRunDescription(CMenus::EFirstRunSetupStep Step)
	{
		switch(Step)
		{
		case CMenus::FIRST_RUN_SETUP_UI_SCALE:
			return Localize("Set the menu size that feels comfortable. You can keep the default and continue.");
		case CMenus::FIRST_RUN_SETUP_ASPECT_RATIO:
			return Localize("Pick the ingame stretch ratio you want to use. The menu itself stays unaffected.");
		case CMenus::FIRST_RUN_SETUP_CURSORS:
			return Localize("Choose a custom cursor preset or leave Default to keep the original game cursor.");
		case CMenus::FIRST_RUN_SETUP_AUDIO:
			return Localize("Choose an audio preset or leave Default to use sounds from the original game data.");
		default:
			return "";
		}
	}
}

bool CMenus::IsFirstRunSetupActive() const
{
	return str_comp(g_Config.m_CcFirstRun, "false") != 0;
}

bool CMenus::IsFirstRunSetupStepActive(EFirstRunSetupStep Step) const
{
	return IsFirstRunSetupActive() && m_FirstRunSetupStep == Step;
}

void CMenus::RegisterFirstRunFocus(EFirstRunSetupStep Step, const CUIRect &Rect)
{
	if(!IsFirstRunSetupStepActive(Step))
		return;

	m_aFirstRunFocus[Step].m_Valid = true;
	m_aFirstRunFocus[Step].m_Rect = Rect;
}

void CMenus::ResetFirstRunFocus()
{
	for(auto &Focus : m_aFirstRunFocus)
		Focus.m_Valid = false;
}

void CMenus::UpdateFirstRunSetupRouting()
{
	m_FirstRunSetupStep = std::clamp(m_FirstRunSetupStep, 0, (int)NUM_FIRST_RUN_SETUP_STEPS - 1);

	switch((EFirstRunSetupStep)m_FirstRunSetupStep)
	{
	case FIRST_RUN_SETUP_UI_SCALE:
	case FIRST_RUN_SETUP_ASPECT_RATIO:
		g_Config.m_UiSettingsPage = SETTINGS_CATCLIENT;
		m_CatClientTab = CATCLIENT_TAB_VISUALS;
		break;
	case FIRST_RUN_SETUP_CURSORS:
		g_Config.m_UiSettingsPage = SETTINGS_ASSETS;
		m_AssetsTab = ASSETS_TAB_CURSORS;
		break;
	case FIRST_RUN_SETUP_AUDIO:
		g_Config.m_UiSettingsPage = SETTINGS_ASSETS;
		m_AssetsTab = ASSETS_TAB_AUDIO;
		break;
	default:
		break;
	}
}

void CMenus::FinishFirstRunSetup(bool Skip)
{
	str_copy(g_Config.m_CcFirstRun, "false");
	m_FirstRunSetupStep = FIRST_RUN_SETUP_UI_SCALE;
	ResetFirstRunFocus();
	ConfigManager()->Save();

	if(!Skip)
	{
		g_Config.m_UiSettingsPage = SETTINGS_CATCLIENT;
		m_CatClientTab = CATCLIENT_TAB_GENERAL;
	}
}

void CMenus::RenderFirstRunSetupOverlay(const CUIRect &Screen)
{
	if(!IsFirstRunSetupActive())
		return;

	const auto Now = time_get_nanoseconds();
	if(m_FirstRunSetupAnimatedStep != m_FirstRunSetupStep)
	{
		m_FirstRunSetupAnimatedStep = m_FirstRunSetupStep;
		m_FirstRunSetupAnimationStart = Now;
	}

	constexpr auto AnimationDuration = std::chrono::milliseconds(140);
	const float RawAnimProgress = m_FirstRunSetupAnimationStart == std::chrono::nanoseconds::zero() ? 1.0f :
		std::clamp(std::chrono::duration<float>(Now - m_FirstRunSetupAnimationStart).count() / std::chrono::duration<float>(AnimationDuration).count(), 0.0f, 1.0f);
	const float AnimProgress = RawAnimProgress * RawAnimProgress * (3.0f - 2.0f * RawAnimProgress);

	const EFirstRunSetupStep Step = (EFirstRunSetupStep)m_FirstRunSetupStep;
	CUIRect FocusRect = Screen;
	if(m_aFirstRunFocus[Step].m_Valid)
	{
		FocusRect = m_aFirstRunFocus[Step].m_Rect;
		ExpandAndClampRect(FocusRect, Screen, 10.0f);
	}

	CUIRect Top = Screen;
	Top.h = maximum(0.0f, FocusRect.y - Screen.y);

	CUIRect Bottom = Screen;
	Bottom.y = FocusRect.y + FocusRect.h;
	Bottom.h = maximum(0.0f, Screen.y + Screen.h - Bottom.y);

	CUIRect Left = Screen;
	Left.y = FocusRect.y;
	Left.h = FocusRect.h;
	Left.w = maximum(0.0f, FocusRect.x - Screen.x);

	CUIRect Right = Screen;
	Right.x = FocusRect.x + FocusRect.w;
	Right.y = FocusRect.y;
	Right.w = maximum(0.0f, Screen.x + Screen.w - Right.x);
	Right.h = FocusRect.h;

	const ColorRGBA OverlayColor(0.0f, 0.0f, 0.0f, 0.78f * AnimProgress);
	static int s_aBlockIds[4];
	const CUIRect aOverlayRects[4] = {Top, Bottom, Left, Right};
	for(int i = 0; i < 4; ++i)
	{
		if(aOverlayRects[i].w <= 0.0f || aOverlayRects[i].h <= 0.0f)
			continue;
		aOverlayRects[i].Draw(OverlayColor, IGraphics::CORNER_NONE, 0.0f);
		Ui()->DoButtonLogic(&s_aBlockIds[i], 0, &aOverlayRects[i], BUTTONFLAG_LEFT);
	}

	if(m_aFirstRunFocus[Step].m_Valid)
	{
		FocusRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f * AnimProgress), IGraphics::CORNER_ALL, 10.0f);
		FocusRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f * AnimProgress), IGraphics::CORNER_ALL, 11.0f);
	}

	CUIRect Panel;
	Panel.w = minimum(540.0f, Screen.w - 40.0f);
	Panel.h = 118.0f;
	Panel.x = Screen.x + (Screen.w - Panel.w) / 2.0f;
	Panel.y = FocusRect.y + FocusRect.h > Screen.y + Screen.h * 0.58f ? Screen.y + 24.0f : Screen.y + Screen.h - Panel.h - 24.0f;
	Panel.y += (1.0f - AnimProgress) * 10.0f;
	Panel.Draw(ColorRGBA(0.07f, 0.07f, 0.09f, 0.96f * AnimProgress), IGraphics::CORNER_ALL, 12.0f);

	CUIRect Content, Header, Description, Footer;
	Panel.Margin(14.0f, &Content);
	Content.HSplitTop(26.0f, &Header, &Content);
	Content.HSplitTop(40.0f, &Description, &Footer);

	char aStepLabel[64];
	str_format(aStepLabel, sizeof(aStepLabel), "%s %d/%d", Localize("Step"), m_FirstRunSetupStep + 1, (int)NUM_FIRST_RUN_SETUP_STEPS);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.6f * AnimProgress);
	Ui()->DoLabel(&Header, aStepLabel, 12.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Ui()->DoLabel(&Header, FirstRunTitle(Step), 18.0f, TEXTALIGN_MC);

	CUIRect ProgressDots = Header;
	ProgressDots.VSplitRight(90.0f, &Header, &ProgressDots);
	ProgressDots.HMargin(8.0f, &ProgressDots);
	for(int i = 0; i < NUM_FIRST_RUN_SETUP_STEPS; ++i)
	{
		CUIRect Dot;
		ProgressDots.VSplitLeft(16.0f, &Dot, &ProgressDots);
		ProgressDots.VSplitLeft(6.0f, nullptr, &ProgressDots);
		const bool Active = i == m_FirstRunSetupStep;
		Dot.Draw(Active ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f * AnimProgress) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.16f * AnimProgress), IGraphics::CORNER_ALL, 4.0f);
	}

	SLabelProperties Props;
	Props.m_MaxWidth = Description.w;
	Props.m_EnableWidthCheck = true;
	Ui()->DoLabel(&Description, FirstRunDescription(Step), 13.0f, TEXTALIGN_ML, Props);

	CUIRect SkipButton, BackButton, NextButton;
	Footer.HSplitBottom(32.0f, nullptr, &Footer);
	Footer.VSplitLeft(110.0f, &SkipButton, &Footer);
	Footer.VSplitRight(120.0f, &Footer, &NextButton);
	Footer.VSplitRight(10.0f, &Footer, nullptr);
	Footer.VSplitRight(110.0f, &Footer, &BackButton);

	static CButtonContainer s_SkipButton;
	if(DoButton_Menu(&s_SkipButton, Localize("Skip setup"), 0, &SkipButton))
	{
		FinishFirstRunSetup(true);
		return;
	}

	static CButtonContainer s_BackButton;
	if(m_FirstRunSetupStep > 0)
	{
		if(DoButton_Menu(&s_BackButton, Localize("Back"), 0, &BackButton))
		{
			m_FirstRunSetupStep--;
			UpdateFirstRunSetupRouting();
			return;
		}
	}
	else
	{
		BackButton.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), IGraphics::CORNER_ALL, 5.0f);
		Ui()->DoLabel(&BackButton, Localize("Back"), 12.0f, TEXTALIGN_MC);
	}

	static CButtonContainer s_NextButton;
	const bool LastStep = m_FirstRunSetupStep == NUM_FIRST_RUN_SETUP_STEPS - 1;
	if(DoButton_Menu(&s_NextButton, LastStep ? Localize("Finish") : Localize("Next"), 0, &NextButton))
	{
		if(LastStep)
			FinishFirstRunSetup(false);
		else
		{
			m_FirstRunSetupStep++;
			UpdateFirstRunSetupRouting();
		}
	}
}
