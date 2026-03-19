#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_VISUALS_MENUS_CATCLIENT_VISUALS_ASPECT_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_VISUALS_MENUS_CATCLIENT_VISUALS_ASPECT_H

static void UpdateAspectRatioFromCustomInputs()
{
	const int CustomX = std::max(g_Config.m_CcAspectRatioCustomX, 1);
	const int CustomY = std::max(g_Config.m_CcAspectRatioCustomY, 1);
	const int64_t RatioValue = ((int64_t)CustomX * 100 + CustomY / 2) / CustomY;
	g_Config.m_CcAspectRatio = std::clamp((int)RatioValue, 100, 400);
}

struct SAspectRatioExcludeOption
{
	const char *m_pSummaryLabel;
	const char *m_pPopupLabel;
	int m_Flag;
};

static const SAspectRatioExcludeOption gs_aAspectRatioExcludeOptions[] = {
	{"Interface", "Interface", CCatClient::ASPECT_RATIO_EXCLUDE_INTERFACE},
	{"Bind Wheel", "Bind Wheel", CCatClient::ASPECT_RATIO_EXCLUDE_BIND_WHEEL},
	{"Emote Wheel", "Emote Wheel", CCatClient::ASPECT_RATIO_EXCLUDE_EMOTE_WHEEL},
};

struct SAspectRatioExcludePopupContext : public SPopupMenuId
{
	CUi *m_pUi = nullptr;
	int *m_pFlags = nullptr;
	SPopupMenuProperties m_Props;
	float m_Width = 0.0f;
	float m_AlignmentHeight = 0.0f;
	CButtonContainer m_aButtons[std::size(gs_aAspectRatioExcludeOptions)];
};

static void BuildAspectRatioExcludeSummary(char *pBuf, size_t BufSize, int Flags)
{
	if(Flags == 0)
	{
		str_copy(pBuf, CCLocalize("Off"), BufSize);
		return;
	}

	pBuf[0] = '\0';
	bool First = true;
	for(const auto &Option : gs_aAspectRatioExcludeOptions)
	{
		if((Flags & Option.m_Flag) == 0)
			continue;

		if(!First)
			str_append(pBuf, ", ", BufSize);
		str_append(pBuf, CCLocalize(Option.m_pSummaryLabel), BufSize);
		First = false;
	}
}

static CUi::EPopupMenuFunctionResult PopupAspectRatioExclude(void *pContext, CUIRect View, bool Active)
{
	auto *pPopup = static_cast<SAspectRatioExcludePopupContext *>(pContext);
	if(!Active || pPopup->m_pFlags == nullptr)
		return CUi::POPUP_KEEP_OPEN;

	for(size_t i = 0; i < std::size(gs_aAspectRatioExcludeOptions); ++i)
	{
		const auto &Option = gs_aAspectRatioExcludeOptions[i];
		const bool Selected = (*pPopup->m_pFlags & Option.m_Flag) != 0;
		CUIRect Button;
		View.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &View);

		const std::optional<ColorRGBA> ButtonColor = Selected ? std::optional<ColorRGBA>(ColorRGBA(0.55f, 0.68f, 1.0f, 0.30f)) : std::nullopt;
		if(pPopup->m_pUi->DoButton_PopupMenu(&pPopup->m_aButtons[i], CCLocalize(Option.m_pPopupLabel), &Button, CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_ML, 6.0f, true, true, ButtonColor))
			*pPopup->m_pFlags ^= Option.m_Flag;

		if(i + 1 < std::size(gs_aAspectRatioExcludeOptions))
			View.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &View);
	}

	return CUi::POPUP_KEEP_OPEN;
}

static void RenderAspectRatioExcludeSelector(CUi *pUi, CUIRect &Content)
{
	static CUIElement s_DropDownUiElement;
	static CButtonContainer s_DropDownButton;
	static SAspectRatioExcludePopupContext s_PopupContext;
	static bool s_DropDownInitialized = false;

	if(!s_DropDownInitialized)
	{
		s_DropDownUiElement.Init(pUi, -1);
		s_DropDownInitialized = true;
	}

	CUIRect Label, Button;
	Content.HSplitTop(CATCLIENT_MENU_SMALL_FONT_SIZE, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Exclude"), CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);

	char aSummary[128] = {};
	const auto SummaryLabel = [&aSummary]() {
		BuildAspectRatioExcludeSummary(aSummary, sizeof(aSummary), g_Config.m_CcAspectRatioExclude);
		return aSummary;
	};

	SMenuButtonProperties Props;
	Props.m_HintRequiresStringCheck = true;
	Props.m_HintCanChangePositionOrSize = true;
	Props.m_ShowDropDownIcon = true;
	if(pUi->IsPopupOpen(&s_PopupContext))
		Props.m_Corners = IGraphics::CORNER_ALL & (~s_PopupContext.m_Props.m_Corners);
	if(pUi->DoButton_Menu(s_DropDownUiElement, &s_DropDownButton, SummaryLabel, &Button, Props))
	{
		if(pUi->IsPopupOpen(&s_PopupContext))
			pUi->ClosePopupMenu(&s_PopupContext);
		else
		{
			constexpr float PopupBorderAndMargin = 6.0f;
			const float PopupHeight = std::size(gs_aAspectRatioExcludeOptions) * CATCLIENT_MENU_LINE_SIZE + (std::size(gs_aAspectRatioExcludeOptions) - 1) * CATCLIENT_MENU_MARGIN_SMALL + PopupBorderAndMargin * 2.0f;
			constexpr float ScreenMargin = 6.0f;

			s_PopupContext.m_pUi = pUi;
			s_PopupContext.m_pFlags = &g_Config.m_CcAspectRatioExclude;
			s_PopupContext.m_Width = Button.w;
			s_PopupContext.m_AlignmentHeight = Button.h;
			s_PopupContext.m_Props.m_BorderColor = ColorRGBA(0.7f, 0.7f, 0.7f, 0.9f);
			s_PopupContext.m_Props.m_BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

			float PopupX = Button.x;
			float PopupY = Button.y;
			if(PopupX + s_PopupContext.m_Width > pUi->Screen()->w - ScreenMargin)
				PopupX = maximum<float>(PopupX - s_PopupContext.m_Width, ScreenMargin);
			if(PopupY + s_PopupContext.m_AlignmentHeight + PopupHeight > pUi->Screen()->h - ScreenMargin)
			{
				PopupY -= PopupHeight;
				s_PopupContext.m_Props.m_Corners = IGraphics::CORNER_T;
			}
			else
			{
				PopupY += s_PopupContext.m_AlignmentHeight;
				s_PopupContext.m_Props.m_Corners = IGraphics::CORNER_B;
			}

			pUi->DoPopupMenu(&s_PopupContext, PopupX, PopupY, s_PopupContext.m_Width, PopupHeight, &s_PopupContext, PopupAspectRatioExclude, s_PopupContext.m_Props);
		}
	}
}

static void RenderAspectRatioSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CButtonContainer s_Aspect43Button, s_Aspect1610Button, s_Aspect169Button, s_Aspect54Button, s_AspectCustomButton;
	static CCatClientMenuSliderState s_AspectRatioSlider;
	static CLineInputNumber s_CustomAspectXInput(g_Config.m_CcAspectRatioCustomX);
	static CLineInputNumber s_CustomAspectYInput(g_Config.m_CcAspectRatioCustomY);

	CUIRect Section, Content, Label, Button;
	CatClientMenuBeginSection(View, Section, Content, 175.0f);
	if(pMenus->IsFirstRunSetupStepActive(CMenus::FIRST_RUN_SETUP_ASPECT_RATIO))
		pMenus->RegisterFirstRunFocus(CMenus::FIRST_RUN_SETUP_ASPECT_RATIO, Section);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Aspect Ratio"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcAspectRatioEnabled, CCLocalize("Stretch Ingame Aspect"), &g_Config.m_CcAspectRatioEnabled, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
	CUIRect Aspect43, Aspect1610, Aspect169, Aspect54, AspectCustom, Remaining = Button;
	const float ButtonSpacing = CATCLIENT_MENU_MARGIN_SMALL;
	const float ButtonWidth = (Button.w - ButtonSpacing * 4.0f) / 5.0f;
	Remaining.VSplitLeft(ButtonWidth, &Aspect43, &Remaining);
	Remaining.VSplitLeft(ButtonSpacing, nullptr, &Remaining);
	Remaining.VSplitLeft(ButtonWidth, &Aspect1610, &Remaining);
	Remaining.VSplitLeft(ButtonSpacing, nullptr, &Remaining);
	Remaining.VSplitLeft(ButtonWidth, &Aspect169, &Remaining);
	Remaining.VSplitLeft(ButtonSpacing, nullptr, &Remaining);
	Remaining.VSplitLeft(ButtonWidth, &Aspect54, &Remaining);
	Remaining.VSplitLeft(ButtonSpacing, nullptr, &Remaining);
	Remaining.VSplitLeft(ButtonWidth, &AspectCustom, nullptr);

	if(pMenus->DoButton_Menu(&s_Aspect43Button, "4:3", 0, &Aspect43, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatioCustom = 0;
		g_Config.m_CcAspectRatio = 130;
	}
	if(pMenus->DoButton_Menu(&s_Aspect1610Button, "16:10", 0, &Aspect1610, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatioCustom = 0;
		g_Config.m_CcAspectRatio = 160;
	}
	if(pMenus->DoButton_Menu(&s_Aspect169Button, "16:9", 0, &Aspect169, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatioCustom = 0;
		g_Config.m_CcAspectRatio = 177;
	}
	if(pMenus->DoButton_Menu(&s_Aspect54Button, "5:4", 0, &Aspect54, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatioCustom = 0;
		g_Config.m_CcAspectRatio = 115;
	}
	if(pMenus->DoButton_Menu(&s_AspectCustomButton, CCLocalize("Custom"), 0, &AspectCustom, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		g_Config.m_CcAspectRatioEnabled = 1;
		g_Config.m_CcAspectRatioCustom = 1;
		UpdateAspectRatioFromCustomInputs();
	}

	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);

	if(g_Config.m_CcAspectRatioCustom == 0)
	{
		CatClientMenuDoSliderOption(pUi, &g_Config.m_CcAspectRatio, &s_AspectRatioSlider, &g_Config.m_CcAspectRatio, Button, 100, 250, &CUi::ms_LinearScrollbarScale, false, [](char *pBuf, size_t BufSize, int Value) {
			str_format(pBuf, BufSize, "%s: %d.%02d", CCLocalize("Ratio"), Value / 100, Value % 100);
		});
	}
	else
	{
		s_CustomAspectXInput.SetEmptyText("1920");
		s_CustomAspectYInput.SetEmptyText("1080");
		if(!s_CustomAspectXInput.IsActive() && s_CustomAspectXInput.GetInteger() != g_Config.m_CcAspectRatioCustomX)
			s_CustomAspectXInput.SetInteger(g_Config.m_CcAspectRatioCustomX);
		if(!s_CustomAspectYInput.IsActive() && s_CustomAspectYInput.GetInteger() != g_Config.m_CcAspectRatioCustomY)
			s_CustomAspectYInput.SetInteger(g_Config.m_CcAspectRatioCustomY);

		CUIRect InputX, InputLabel, InputY;
		Button.VSplitLeft((Button.w - CATCLIENT_MENU_MARGIN_SMALL * 2.0f - 12.0f) * 0.5f, &InputX, &Button);
		Button.VSplitLeft(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Button);
		Button.VSplitLeft(12.0f, &InputLabel, &Button);
		Button.VSplitLeft(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Button);
		InputY = Button;

		bool Updated = false;
		if(pUi->DoEditBox(&s_CustomAspectXInput, &InputX, CATCLIENT_MENU_FONT_SIZE))
		{
			const int Value = s_CustomAspectXInput.GetInteger();
			if(Value > 0)
			{
				g_Config.m_CcAspectRatioCustomX = std::clamp(Value, 1, 100000);
				Updated = true;
			}
		}
		pUi->DoLabel(&InputLabel, "x", CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_MC);
		if(pUi->DoEditBox(&s_CustomAspectYInput, &InputY, CATCLIENT_MENU_FONT_SIZE))
		{
			const int Value = s_CustomAspectYInput.GetInteger();
			if(Value > 0)
			{
				g_Config.m_CcAspectRatioCustomY = std::clamp(Value, 1, 100000);
				Updated = true;
			}
		}

		if(Updated)
		{
			UpdateAspectRatioFromCustomInputs();
		}
	}

	Content.HSplitTop(CATCLIENT_MENU_MARGIN, nullptr, &Content);
	RenderAspectRatioExcludeSelector(pUi, Content);
}

#endif
