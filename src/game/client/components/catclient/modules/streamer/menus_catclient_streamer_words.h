#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_STREAMER_MENUS_CATCLIENT_STREAMER_WORDS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_STREAMER_MENUS_CATCLIENT_STREAMER_WORDS_H

enum EBlockedWordsAction
{
	BLOCKED_WORDS_ACTION_NONE = 0,
	BLOCKED_WORDS_ACTION_OPEN,
	BLOCKED_WORDS_ACTION_HIDE,
};

static bool gs_ShowBlockedWordsList = false;

static EBlockedWordsAction RenderBlockedWordsSection(CMenus *pMenus, CUi *pUi, CCatClient *pCatClient, CUIRect &View)
{
	static CLineInputBuffered<128> s_WordInput;
	static CButtonContainer s_AddWordButton;
	static CButtonContainer s_OpenBlockedWordsButton;
	static CButtonContainer s_HideBlockedWordsButton;
	static CScrollRegion s_BlockedWordsScrollRegion;
	static std::vector<CButtonContainer> s_vDeleteWordButtons;

	CUIRect Section, Content, HeaderRow, Label, ToggleButton, InputRow, InputBox, AddButton, ListBox, Description;
	const float SectionHeight = gs_ShowBlockedWordsList ? 292.0f : 96.0f;
	CatClientMenuBeginSection(View, Section, Content, SectionHeight);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &HeaderRow, &Content);
	HeaderRow.VSplitRight(92.0f, &Label, &ToggleButton);
	char aTitle[64];
	str_format(aTitle, sizeof(aTitle), CCLocalize("Blocked Words (%d)"), pCatClient->StreamerBlockedWordCount());
	pUi->DoLabel(&Label, aTitle, CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);

	if(gs_ShowBlockedWordsList)
	{
		if(pMenus->DoButton_Menu(&s_HideBlockedWordsButton, CCLocalize("Hide"), 0, &ToggleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f)))
		{
			return BLOCKED_WORDS_ACTION_HIDE;
		}
	}
	else if(pMenus->DoButton_Menu(&s_OpenBlockedWordsButton, CCLocalize("Open"), 0, &ToggleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f)))
	{
		return BLOCKED_WORDS_ACTION_OPEN;
	}

	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	if(!gs_ShowBlockedWordsList)
	{
		Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE * 2.0f, &Description, &Content);
		SLabelProperties Props;
		Props.m_MaxWidth = Description.w;
		pUi->DoLabel(&Description, CCLocalize("The list is hidden by default for stream safety."), CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML, Props);
		return BLOCKED_WORDS_ACTION_NONE;
	}

	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &InputRow, &Content);
	InputRow.VSplitRight(CATCLIENT_MENU_LINE_SIZE + 4.0f, &InputBox, &AddButton);
	s_WordInput.SetEmptyText(CCLocalize("Add blocked word"));
	pUi->DoEditBox(&s_WordInput, &InputBox, 12.0f);

	if(pMenus->DoButton_Menu(&s_AddWordButton, "+", 0, &AddButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.3f)))
	{
		pCatClient->AddStreamerBlockedWord(s_WordInput.GetString());
		s_WordInput.Clear();
	}

	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	ListBox = Content;

	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 14.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = CATCLIENT_MENU_LINE_SIZE * 4.0f;
	ScrollParams.m_ClipBgColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.03f);
	ScrollParams.m_RailBgColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f);
	ScrollParams.m_SliderColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.28f);
	ScrollParams.m_SliderColorHover = ColorRGBA(1.0f, 1.0f, 1.0f, 0.42f);
	ScrollParams.m_SliderColorGrabbed = ColorRGBA(1.0f, 1.0f, 1.0f, 0.55f);

	const auto &vWords = pCatClient->StreamerBlockedWords();
	s_vDeleteWordButtons.resize(vWords.size());

	vec2 ScrollOffset(0.0f, 0.0f);
	s_BlockedWordsScrollRegion.Begin(&ListBox, &ScrollOffset, &ScrollParams);

	CUIRect ListContent = ListBox;
	ListContent.y += ScrollOffset.y;
	int RemoveIndex = -1;

	if(vWords.empty())
	{
		CUIRect EmptyRow;
		ListContent.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &EmptyRow, &ListContent);
		s_BlockedWordsScrollRegion.AddRect(EmptyRow);
		if(!s_BlockedWordsScrollRegion.RectClipped(EmptyRow))
		{
			pUi->DoLabel(&EmptyRow, CCLocalize("No blocked words yet"), CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_MC);
		}
	}
	else
	{
		for(size_t i = 0; i < vWords.size(); ++i)
		{
			if(i != 0)
			{
				ListContent.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &ListContent);
			}

			CUIRect Row, Inner, WordLabel, DeleteButton;
			ListContent.HSplitTop(CATCLIENT_MENU_LINE_SIZE + 4.0f, &Row, &ListContent);
			s_BlockedWordsScrollRegion.AddRect(Row);
			if(s_BlockedWordsScrollRegion.RectClipped(Row))
			{
				continue;
			}

			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, i % 2 == 0 ? 0.05f : 0.08f), IGraphics::CORNER_ALL, 6.0f);
			Row.Margin(4.0f, &Inner);
			Inner.VSplitRight(CATCLIENT_MENU_LINE_SIZE, &WordLabel, &DeleteButton);

			SLabelProperties Props;
			Props.m_MaxWidth = WordLabel.w;
			pUi->DoLabel(&WordLabel, vWords[i].c_str(), CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML, Props);

			if(pUi->DoButton_FontIcon(&s_vDeleteWordButtons[i], FontIcon::TRASH, 0, &DeleteButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, true, ColorRGBA(0.85f, 0.25f, 0.25f, 0.35f)))
			{
				RemoveIndex = (int)i;
			}
		}
	}

	s_BlockedWordsScrollRegion.End();

	if(RemoveIndex >= 0)
	{
		pCatClient->RemoveStreamerBlockedWord(RemoveIndex);
	}

	return BLOCKED_WORDS_ACTION_NONE;
}

void CMenus::PopupConfirmOpenBlockedWords()
{
	gs_ShowBlockedWordsList = true;
}

#endif
