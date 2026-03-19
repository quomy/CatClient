#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_VISUALS_MENUS_CATCLIENT_VISUALS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_VISUALS_MENUS_CATCLIENT_VISUALS_BACKGROUND_H

static bool IsSupportedBackgroundImage(const char *pName)
{
	return str_endswith_nocase(pName, ".png") != nullptr || str_endswith_nocase(pName, ".jpg") != nullptr || str_endswith_nocase(pName, ".jpeg") != nullptr
#if defined(CONF_VIDEORECORDER)
		|| str_endswith_nocase(pName, ".gif") != nullptr
#endif
		;
}

static int CollectCustomBackgroundImage(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	if(IsDir || !IsSupportedBackgroundImage(pInfo->m_pName))
	{
		return 0;
	}

	auto *pEntries = static_cast<std::vector<std::string> *>(pUser);
	pEntries->emplace_back(pInfo->m_pName);
	return 0;
}

static std::vector<std::string> GetCustomBackgroundImages(CMenus *pMenus)
{
	pMenus->MenuStorage()->CreateFolder("catclient", IStorage::TYPE_SAVE);
	pMenus->MenuStorage()->CreateFolder("catclient/backgrounds", IStorage::TYPE_SAVE);

	std::vector<std::string> vEntries;
	pMenus->MenuStorage()->ListDirectoryInfo(IStorage::TYPE_SAVE, "catclient/backgrounds", CollectCustomBackgroundImage, &vEntries);
	std::sort(vEntries.begin(), vEntries.end(), [](const std::string &Left, const std::string &Right) {
		return str_comp_nocase(Left.c_str(), Right.c_str()) < 0;
	});
	return vEntries;
}

static void RenderCustomBackgroundSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
{
	static CUi::SDropDownState s_BackgroundDropDownState;
	static CScrollRegion s_BackgroundDropDownScrollRegion;
	static CButtonContainer s_OpenFolderButton;
	static CButtonContainer s_RefreshButton;
	s_BackgroundDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_BackgroundDropDownScrollRegion;

	CUIRect Section, Content, Label, Button, DropDownRect, Buttons, FolderButton, RefreshButton, Hint;
	CatClientMenuBeginSection(View, Section, Content, 142.0f);
	Content.HSplitTop(CATCLIENT_MENU_HEADLINE_HEIGHT, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Custom Background"), CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcCustomBackgroundMainMenu, CCLocalize("Main Menu"), &g_Config.m_CcCustomBackgroundMainMenu, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcCustomBackgroundGame, CCLocalize("Game BG"), &g_Config.m_CcCustomBackgroundGame, &Content, CATCLIENT_MENU_LINE_SIZE);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN, nullptr, &Content);

	Content.HSplitTop(CATCLIENT_MENU_SMALL_FONT_SIZE, &Label, &Content);
	pUi->DoLabel(&Label, CCLocalize("Image"), CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);

	const std::vector<std::string> vEntries = GetCustomBackgroundImages(pMenus);
	if(!vEntries.empty())
	{
		std::vector<const char *> vNames;
		vNames.reserve(vEntries.size());
		int SelectedOld = -1;
		for(size_t i = 0; i < vEntries.size(); ++i)
		{
			vNames.push_back(vEntries[i].c_str());
			if(str_comp(g_Config.m_CcCustomBackgroundImage, vEntries[i].c_str()) == 0)
			{
				SelectedOld = (int)i;
			}
		}

		if(SelectedOld == -1)
		{
			SelectedOld = 0;
			str_copy(g_Config.m_CcCustomBackgroundImage, vEntries[0].c_str(), sizeof(g_Config.m_CcCustomBackgroundImage));
			pMenus->MenuGameClient()->m_CatClient.ReloadCustomBackground();
		}

		Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
		Button.VSplitRight(CATCLIENT_MENU_LINE_SIZE * 2.0f + CATCLIENT_MENU_MARGIN_SMALL, &DropDownRect, &Buttons);
		Buttons.VSplitMid(&FolderButton, &RefreshButton, CATCLIENT_MENU_MARGIN_SMALL);

		const int SelectedNew = pUi->DoDropDown(&DropDownRect, SelectedOld, vNames.data(), vNames.size(), s_BackgroundDropDownState);
		if(SelectedNew >= 0 && SelectedNew != SelectedOld)
		{
			str_copy(g_Config.m_CcCustomBackgroundImage, vEntries[SelectedNew].c_str(), sizeof(g_Config.m_CcCustomBackgroundImage));
			pMenus->MenuGameClient()->m_CatClient.ReloadCustomBackground();
		}
		if(pUi->DoButton_FontIcon(&s_OpenFolderButton, FontIcon::FOLDER, 0, &FolderButton, IGraphics::CORNER_ALL))
		{
			char aPath[IO_MAX_PATH_LENGTH];
			pMenus->MenuStorage()->CreateFolder("catclient", IStorage::TYPE_SAVE);
			pMenus->MenuStorage()->CreateFolder("catclient/backgrounds", IStorage::TYPE_SAVE);
			pMenus->MenuStorage()->GetCompletePath(IStorage::TYPE_SAVE, "catclient/backgrounds", aPath, sizeof(aPath));
			pMenus->MenuClient()->ViewFile(aPath);
		}
		if(pUi->DoButton_FontIcon(&s_RefreshButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &RefreshButton, IGraphics::CORNER_ALL))
		{
			pMenus->MenuGameClient()->m_CatClient.ReloadCustomBackground();
		}
	}
	else
	{
		Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &Button, &Content);
		Button.VSplitRight(CATCLIENT_MENU_LINE_SIZE * 2.0f + CATCLIENT_MENU_MARGIN_SMALL, &Label, &Buttons);
		pUi->DoLabel(&Label,
			pMenus->MenuGameClient()->m_CatClient.IsDefaultBackgroundDownloading() ? CCLocalize("Downloading default background…") : CCLocalize("No background images found"),
			CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_ML);
		Buttons.VSplitMid(&FolderButton, &RefreshButton, CATCLIENT_MENU_MARGIN_SMALL);
		if(pUi->DoButton_FontIcon(&s_OpenFolderButton, FontIcon::FOLDER, 0, &FolderButton, IGraphics::CORNER_ALL))
		{
			char aPath[IO_MAX_PATH_LENGTH];
			pMenus->MenuStorage()->CreateFolder("catclient", IStorage::TYPE_SAVE);
			pMenus->MenuStorage()->CreateFolder("catclient/backgrounds", IStorage::TYPE_SAVE);
			pMenus->MenuStorage()->GetCompletePath(IStorage::TYPE_SAVE, "catclient/backgrounds", aPath, sizeof(aPath));
			pMenus->MenuClient()->ViewFile(aPath);
		}
		if(pUi->DoButton_FontIcon(&s_RefreshButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &RefreshButton, IGraphics::CORNER_ALL))
		{
			pMenus->MenuGameClient()->m_CatClient.ReloadCustomBackground();
		}
	}

	Content.HSplitTop(CATCLIENT_MENU_MARGIN, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE * 2.0f, &Hint, &Content);
}

#endif
