#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_AUDIO_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_GENERAL_MENUS_CATCLIENT_GENERAL_AUDIO_H

static const SCatClientMenuFlagOption gs_aMuteSoundOptions[] = {
	{CCLocalizable("Others hook sound"), CCatClient::MUTE_SOUND_OTHERS_HOOK},
	{CCLocalizable("Others hammer sound"), CCatClient::MUTE_SOUND_OTHERS_HAMMER},
	{CCLocalizable("Local hammer sound"), CCatClient::MUTE_SOUND_LOCAL_HAMMER},
	{CCLocalizable("Weapon switch sound"), CCatClient::MUTE_SOUND_WEAPON_SWITCH},
	{CCLocalizable("Jump sound"), CCatClient::MUTE_SOUND_JUMP},
};

struct SMuteSoundsPopupContext : public SPopupMenuId
{
	CUi *m_pUi = nullptr;
	int *m_pFlags = nullptr;
	SPopupMenuProperties m_Props;
	float m_Width = 0.0f;
	float m_AlignmentHeight = 0.0f;
	CButtonContainer m_aButtons[std::size(gs_aMuteSoundOptions)];
};

static void BuildMuteSoundsSummary(char *pBuf, size_t BufSize, int Flags)
{
	if(Flags == 0)
	{
		str_copy(pBuf, CCLocalize("Off"), BufSize);
		return;
	}

	pBuf[0] = '\0';
	bool First = true;
	for(const auto &Option : gs_aMuteSoundOptions)
	{
		if((Flags & Option.m_Flag) == 0)
			continue;

		if(!First)
			str_append(pBuf, ", ", BufSize);
		str_append(pBuf, CCLocalize(Option.m_pLabel), BufSize);
		First = false;
	}
}

static CUi::EPopupMenuFunctionResult PopupMuteSounds(void *pContext, CUIRect View, bool Active)
{
	auto *pPopup = static_cast<SMuteSoundsPopupContext *>(pContext);
	if(!Active || pPopup->m_pFlags == nullptr)
		return CUi::POPUP_KEEP_OPEN;

	for(size_t i = 0; i < std::size(gs_aMuteSoundOptions); ++i)
	{
		const auto &Option = gs_aMuteSoundOptions[i];
		const bool Selected = (*pPopup->m_pFlags & Option.m_Flag) != 0;
		CUIRect Button;
		View.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &View);

		const std::optional<ColorRGBA> ButtonColor = Selected ? std::optional<ColorRGBA>(ColorRGBA(0.55f, 0.68f, 1.0f, 0.30f)) : std::nullopt;
		if(pPopup->m_pUi->DoButton_PopupMenu(&pPopup->m_aButtons[i], CCLocalize(Option.m_pLabel), &Button, CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_ML, 6.0f, true, true, ButtonColor))
			*pPopup->m_pFlags ^= Option.m_Flag;

		if(i + 1 < std::size(gs_aMuteSoundOptions))
			View.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &View);
	}

	return CUi::POPUP_KEEP_OPEN;
}

static void RenderMuteSoundsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	(void)pMenus;
	static CUIElement s_DropDownUiElement;
	static CButtonContainer s_DropDownButton;
	static SMuteSoundsPopupContext s_PopupContext;
	static bool s_DropDownInitialized = false;

	if(!s_DropDownInitialized)
	{
		s_DropDownUiElement.Init(pUi, -1);
		s_DropDownInitialized = true;
	}

	CUIRect Section, Content, Label, DropDown;
	CatClientMenuBeginSection(View, Section, Content, 69.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Mute Sounds"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &DropDown, &Content);

	char aSummary[256] = {};
	BuildMuteSoundsSummary(aSummary, sizeof(aSummary), g_Config.m_CcMuteSounds);

	SMenuButtonProperties Props;
	Props.m_HintRequiresStringCheck = true;
	Props.m_HintCanChangePositionOrSize = true;
	Props.m_ShowDropDownIcon = true;
	if(pUi->IsPopupOpen(&s_PopupContext))
		Props.m_Corners = IGraphics::CORNER_ALL & (~s_PopupContext.m_Props.m_Corners);

	if(pUi->DoButton_Menu(s_DropDownUiElement, &s_DropDownButton, [&aSummary]() { return aSummary; }, &DropDown, Props))
	{
		if(pUi->IsPopupOpen(&s_PopupContext))
			pUi->ClosePopupMenu(&s_PopupContext);
		else
		{
			constexpr float PopupBorderAndMargin = 6.0f;
			const float PopupHeight = std::size(gs_aMuteSoundOptions) * CATCLIENT_MENU_LINE_SIZE + (std::size(gs_aMuteSoundOptions) - 1) * CATCLIENT_MENU_MARGIN_SMALL + PopupBorderAndMargin * 2.0f;
			constexpr float ScreenMargin = 6.0f;

			s_PopupContext.m_pUi = pUi;
			s_PopupContext.m_pFlags = &g_Config.m_CcMuteSounds;
			s_PopupContext.m_Width = DropDown.w;
			s_PopupContext.m_AlignmentHeight = DropDown.h;
			s_PopupContext.m_Props.m_BorderColor = ColorRGBA(0.7f, 0.7f, 0.7f, 0.9f);
			s_PopupContext.m_Props.m_BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

			float PopupX = DropDown.x;
			float PopupY = DropDown.y;
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

			pUi->DoPopupMenu(&s_PopupContext, PopupX, PopupY, s_PopupContext.m_Width, PopupHeight, &s_PopupContext, PopupMuteSounds, s_PopupContext.m_Props);
		}
	}
}

#endif
