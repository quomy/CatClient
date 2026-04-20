#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_MENUS_CATCLIENT_SHOP_RENDER_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_MENUS_CATCLIENT_SHOP_RENDER_H

void CMenus::RenderSettingsCatClientShop(CUIRect MainView)
{
	const CUIRect FullView = MainView;
	CatClientShopInitState();

	if(gs_CatClientShopState.m_pFetchTask && gs_CatClientShopState.m_pFetchTask->Done())
	{
		CatClientShopFinishFetch(this);
		gs_CatClientShopState.m_pFetchTask = nullptr;
		gs_CatClientShopState.m_FetchTab = -1;
		gs_CatClientShopState.m_FetchPage = 0;
	}

	if(gs_CatClientShopState.m_pPreviewTask && gs_CatClientShopState.m_pPreviewTask->Done())
	{
		CatClientShopFinishPreviewFetch(this);
	}

	if(gs_CatClientShopState.m_pInstallTask && gs_CatClientShopState.m_pInstallTask->Done())
	{
		CatClientShopFinishInstall(this);
	}

	if(CatClientShopHasTexturePreview())
	{
		CatClientShopRenderTexturePreview(this, FullView);
		return;
	}

	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	CatClientMenuConstrainWidth(MainView, MainView, 860.0f);

	CUIRect TabsRow, ControlsRow, StatusRow, ListView, FooterRow;
	MainView.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &TabsRow, &MainView);
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	MainView.HSplitTop(CATCLIENT_MENU_LINE_SIZE * 2.0f + CATCLIENT_MENU_MARGIN_SMALL, &ControlsRow, &MainView);
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	MainView.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &StatusRow, &MainView);
	MainView.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &MainView);
	MainView.HSplitBottom(CATCLIENT_MENU_LINE_SIZE, &ListView, &FooterRow);

	static CButtonContainer s_aShopTabs[NUM_CATCLIENT_SHOP_TABS] = {};
	CUIRect Tabs;
	CatClientMenuConstrainWidth(TabsRow, Tabs, CATCLIENT_MENU_TAB_WIDTH * (float)(sizeof(gs_aVisibleCatClientShopTabs) / sizeof(gs_aVisibleCatClientShopTabs[0])));
	for(int VisibleIndex = 0; VisibleIndex < (int)(sizeof(gs_aVisibleCatClientShopTabs) / sizeof(gs_aVisibleCatClientShopTabs[0])); ++VisibleIndex)
	{
		const int Tab = gs_aVisibleCatClientShopTabs[VisibleIndex];
		CUIRect Button;
		Tabs.VSplitLeft(CATCLIENT_MENU_TAB_WIDTH, &Button, &Tabs);
		const int Corners = VisibleIndex == 0 ? IGraphics::CORNER_L : (VisibleIndex == (int)(sizeof(gs_aVisibleCatClientShopTabs) / sizeof(gs_aVisibleCatClientShopTabs[0])) - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aShopTabs[Tab], CCLocalize(gs_aCatClientShopTypeInfos[Tab].m_pLabel), gs_CatClientShopState.m_Tab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			CatClientShopSetTab(this, Tab);
		}
	}

	CUIRect SearchRow, NavRow;
	ControlsRow.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &SearchRow, &ControlsRow);
	ControlsRow.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &ControlsRow);
	ControlsRow.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &NavRow, nullptr);

	CUIRect SearchRowCenter = SearchRow;
	CUIRect AutoSetButton, SearchArea;
	SearchRow.VSplitLeft(170.0f, &AutoSetButton, &SearchArea);
	SearchArea.VSplitLeft(CATCLIENT_MENU_MARGIN, nullptr, &SearchArea);
	CUIRect SearchBox;
	CatClientMenuConstrainWidth(SearchRowCenter, SearchBox, 360.0f);

	if(DoButton_CheckBox(&g_Config.m_CcShopAutoSet, CCLocalize("Auto set"), g_Config.m_CcShopAutoSet, &AutoSetButton))
	{
		g_Config.m_CcShopAutoSet ^= 1;
	}

	static CLineInputBuffered<128> s_SearchInput;
	if(!s_SearchInput.IsActive() && str_comp(s_SearchInput.GetString(), gs_CatClientShopState.m_aSearch) != 0)
	{
		s_SearchInput.Set(gs_CatClientShopState.m_aSearch);
	}
	if(Ui()->DoEditBox_Search(&s_SearchInput, &SearchBox, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive()))
	{
		CatClientShopSetSearch(this, s_SearchInput.GetString());
	}

	CUIRect PrevButton, PageLabel, NextButton, RefreshButton;
	CUIRect CenterNav;
	CatClientMenuConstrainWidth(NavRow, CenterNav, 274.0f);
	CenterNav.x = minimum(CenterNav.x + 34.0f, NavRow.x + NavRow.w - CenterNav.w);
	CenterNav.VSplitLeft(CATCLIENT_MENU_LINE_SIZE, &PrevButton, &CenterNav);
	CenterNav.VSplitLeft(132.0f, &PageLabel, &CenterNav);
	CenterNav.VSplitLeft(CATCLIENT_MENU_LINE_SIZE, &NextButton, &CenterNav);
	CenterNav.VSplitLeft(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &CenterNav);
	CenterNav.VSplitLeft(CATCLIENT_MENU_LINE_SIZE, &RefreshButton, &CenterNav);

	static CButtonContainer s_PrevButton;
	static CButtonContainer s_NextButton;
	static CButtonContainer s_RefreshButton;
	const int CurrentPage = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	if(Ui()->DoButton_FontIcon(&s_PrevButton, FontIcon::CHEVRON_LEFT, 0, &PrevButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, CurrentPage > 1, ColorRGBA(0.0f, 0.0f, 0.0f, CurrentPage > 1 ? 0.25f : 0.15f)) && CurrentPage > 1)
	{
		CatClientShopSetPage(this, CurrentPage - 1);
	}

	char aPageLabel[128];
	str_format(aPageLabel, sizeof(aPageLabel), "%s %d / %d", CCLocalize("Page"), CurrentPage, maximum(1, gs_CatClientShopState.m_TotalPages));
	Ui()->DoLabel(&PageLabel, aPageLabel, CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_MC);

	if(Ui()->DoButton_FontIcon(&s_NextButton, FontIcon::CHEVRON_RIGHT, 0, &NextButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, CurrentPage < gs_CatClientShopState.m_TotalPages, ColorRGBA(0.0f, 0.0f, 0.0f, CurrentPage < gs_CatClientShopState.m_TotalPages ? 0.25f : 0.15f)) && CurrentPage < gs_CatClientShopState.m_TotalPages)
	{
		CatClientShopSetPage(this, CurrentPage + 1);
	}

	if(Ui()->DoButton_FontIcon(&s_RefreshButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &RefreshButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, true, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		CatClientShopRefreshCurrentPage(this);
	}

	if(gs_CatClientShopState.m_LoadedTab != gs_CatClientShopState.m_Tab ||
		gs_CatClientShopState.m_LoadedPage != gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab] ||
		str_comp(gs_CatClientShopState.m_aLoadedSearch, gs_CatClientShopState.m_aSearch) != 0)
	{
		CatClientShopEnsureFetch(this);
	}
	else
	{
		CatClientShopStartPreviewFetch(this);
	}

	char aStatusText[256];
	if(gs_CatClientShopState.m_aStatus[0] != '\0')
	{
		str_copy(aStatusText, gs_CatClientShopState.m_aStatus, sizeof(aStatusText));
	}
	else
	{
		str_format(aStatusText, sizeof(aStatusText), "%s: %d", CCLocalize("Items"), gs_CatClientShopState.m_TotalItems);
	}

	static CButtonContainer s_PoweredByButton;
	const char *pPoweredByText = CCLocalize("Powered by Cat Data");
	const float PoweredByWidth = TextRender()->TextWidth(CATCLIENT_MENU_SMALL_FONT_SIZE, pPoweredByText, -1, -1.0f) + 10.0f;
	CUIRect StatusLabel, PoweredByRect;
	StatusRow.VSplitRight(PoweredByWidth, &StatusLabel, &PoweredByRect);
	Ui()->DoLabel(&StatusLabel, aStatusText, CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML);

	const int PoweredByResult = Ui()->DoButtonLogic(&s_PoweredByButton, 0, &PoweredByRect, BUTTONFLAG_LEFT);
	const bool PoweredByHovered = Ui()->HotItem() == &s_PoweredByButton;
	if(PoweredByResult)
	{
		Client()->ViewLink("https://catdata.pages.dev");
	}

	const ColorRGBA OldTextColor = TextRender()->GetTextColor();
	const ColorRGBA OldOutlineColor = TextRender()->GetTextOutlineColor();
	TextRender()->TextColor(PoweredByHovered ? ColorRGBA(0.82f, 0.91f, 1.0f, 1.0f) : ColorRGBA(0.68f, 0.84f, 1.0f, 1.0f));
	TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.95f));
	Ui()->DoLabel(&PoweredByRect, pPoweredByText, CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_MR);
	TextRender()->TextColor(OldTextColor);
	TextRender()->TextOutlineColor(OldOutlineColor);

	static CListBox s_ListBox;
	const int NumItems = gs_CatClientShopState.m_vItems.size();
	s_ListBox.DoStart(106.0f, NumItems, 1, 2, gs_CatClientShopState.m_SelectedIndex, &ListView, true);

	for(int Index = 0; Index < NumItems; ++Index)
	{
		SCatClientShopItem &Item = gs_CatClientShopState.m_vItems[Index];
		const CListboxItem ListItem = s_ListBox.DoNextItem(&Item, Index == gs_CatClientShopState.m_SelectedIndex);
		if(!ListItem.m_Visible)
		{
			continue;
		}

		CUIRect Row = ListItem.m_Rect;
		Row.Margin(8.0f, &Row);

		CUIRect IconRect, ContentRect;
		Row.VSplitLeft(112.0f, &IconRect, &ContentRect);
		ContentRect.VSplitLeft(CATCLIENT_MENU_MARGIN, nullptr, &ContentRect);
		const bool CanOpenPreview = Item.m_PreviewTexture.IsValid() && !Item.m_PreviewTexture.IsNullTexture();
		const int IconButtonResult = Ui()->DoButtonLogic(&Item.m_PreviewButton, 0, &IconRect, BUTTONFLAG_LEFT);
		if(IconButtonResult)
		{
			if(CanOpenPreview)
			{
				gs_CatClientShopState.m_PreviewOpen = true;
				str_copy(gs_CatClientShopState.m_aOpenPreviewItemId, Item.m_aId, sizeof(gs_CatClientShopState.m_aOpenPreviewItemId));
			}
			else
			{
				CatClientShopSetStatus(Item.m_PreviewFailed ? CCLocalize("Preview unavailable") : CCLocalize("Preview is still loading"));
			}
		}

		IconRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_ALL, 8.0f);
		CUIRect PreviewRect;
		IconRect.Margin(5.0f, &PreviewRect);
		if(CanOpenPreview)
		{
			CatClientShopRenderTextureFit(Graphics(), PreviewRect, Item.m_PreviewTexture, Item.m_PreviewWidth, Item.m_PreviewHeight);
		}
		else
		{
			RenderFontIcon(IconRect, gs_aCatClientShopTypeInfos[gs_CatClientShopState.m_Tab].m_pIcon, 26.0f, TEXTALIGN_MC);
		}
		if(CanOpenPreview && Ui()->HotItem() == &Item.m_PreviewButton)
		{
			IconRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f), IGraphics::CORNER_ALL, 8.0f);
			CUIRect OpenPreviewIcon = IconRect;
			OpenPreviewIcon.Margin(8.0f, &OpenPreviewIcon);
			RenderFontIcon(OpenPreviewIcon, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 18.0f, TEXTALIGN_TR);
		}

		char aAuthorLabel[160];
		if(Item.m_aUsername[0] != '\0')
		{
			str_format(aAuthorLabel, sizeof(aAuthorLabel), "%s: %s", CCLocalize("Author"), Item.m_aUsername);
		}
		else
		{
			str_copy(aAuthorLabel, CCLocalize("Unknown author"), sizeof(aAuthorLabel));
		}
		char aAssetName[128];
		CatClientShopNormalizeAssetName(Item.m_aName, Item.m_aFilename, aAssetName, sizeof(aAssetName));
		const bool Installed = CatClientShopAssetExists(this, gs_CatClientShopState.m_Tab, aAssetName);
		const bool Selected = CatClientShopAssetSelected(gs_CatClientShopState.m_Tab, aAssetName);
		const bool InstallingThisItem = gs_CatClientShopState.m_pInstallTask != nullptr && str_comp(gs_CatClientShopState.m_aInstallItemId, Item.m_aId) == 0;

		const char *pActionIcon = FontIcon::SQUARE_PLUS;
		ColorRGBA ActionColor(0.0f, 0.0f, 0.0f, 0.25f);
		if(InstallingThisItem)
		{
			pActionIcon = FontIcon::ARROWS_ROTATE;
			ActionColor = ColorRGBA(0.22f, 0.36f, 0.60f, 0.38f);
		}
		else if(Selected)
		{
			pActionIcon = FontIcon::CHECK;
			ActionColor = ColorRGBA(0.18f, 0.46f, 0.26f, 0.38f);
		}
		else if(Installed)
		{
			pActionIcon = FontIcon::PLAY;
			ActionColor = ColorRGBA(0.52f, 0.38f, 0.14f, 0.34f);
		}

		CUIRect InfoRect, ActionArea;
		ContentRect.VSplitRight(40.0f, &InfoRect, &ActionArea);

		CUIRect NameLabel, AuthorLabel;
		InfoRect.HSplitTop(22.0f, &NameLabel, &InfoRect);
		InfoRect.HSplitTop(16.0f, &AuthorLabel, &InfoRect);

		Ui()->DoLabel(&NameLabel, Item.m_aName, 15.0f, TEXTALIGN_ML);
		Ui()->DoLabel(&AuthorLabel, aAuthorLabel, 12.0f, TEXTALIGN_ML);

		CUIRect ButtonRect = ActionArea;
		ButtonRect.HMargin((ButtonRect.h - 30.0f) / 2.0f, &ButtonRect);

		if(Ui()->DoButton_FontIcon(&Item.m_InstallButton, pActionIcon, Selected || InstallingThisItem, &ButtonRect, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, !InstallingThisItem, ActionColor))
		{
			if(!InstallingThisItem)
			{
				if(Installed)
				{
					CatClientShopApplyAsset(this, gs_CatClientShopState.m_Tab, aAssetName, false);
					char aMessage[256];
					str_format(aMessage, sizeof(aMessage), "%s: %s", CCLocalize("Applied"), aAssetName);
					CatClientShopSetStatus(aMessage);
				}
				else
				{
					CatClientShopStartInstall(this, gs_CatClientShopState.m_Tab, Item);
				}
			}
		}
	}

	gs_CatClientShopState.m_SelectedIndex = s_ListBox.DoEnd();

	FooterRow.HSplitTop(2.0f, nullptr, &FooterRow);
	CUIRect OpenFolderButton, HintLabel;
	FooterRow.VSplitLeft(CATCLIENT_MENU_LINE_SIZE, &OpenFolderButton, &HintLabel);
	HintLabel.VSplitLeft(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &HintLabel);
	static CButtonContainer s_OpenFolderButton;
	if(Ui()->DoButton_FontIcon(&s_OpenFolderButton, FontIcon::FOLDER, 0, &OpenFolderButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, true, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		CatClientShopOpenAssetDirectory(this, gs_CatClientShopState.m_Tab);
	}

	char aHint[256];
	str_format(aHint, sizeof(aHint), "%s: %s", CCLocalize("Target"), gs_aCatClientShopTypeInfos[gs_CatClientShopState.m_Tab].m_pAssetDirectory);
	Ui()->DoLabel(&HintLabel, aHint, CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML);

	if(CatClientShopHasTexturePreview())
	{
		CatClientShopRenderTexturePreview(this, FullView);
	}
}

#endif
