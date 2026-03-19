#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_REQUESTS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_REQUESTS_H

static void CatClientShopStartFetch(CMenus *pMenus)
{
	const int Page = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	char aUrl[512];
	if(gs_CatClientShopState.m_aSearch[0] != '\0')
	{
		char aEscapedSearch[256];
		EscapeUrl(aEscapedSearch, gs_CatClientShopState.m_aSearch);
		str_format(aUrl, sizeof(aUrl), CATCLIENT_SHOP_API_SEARCH_URL, Page, CatClientShopCurrentTypeInfo().m_pApiType, aEscapedSearch);
	}
	else
	{
		str_format(aUrl, sizeof(aUrl), CATCLIENT_SHOP_API_URL, Page, CatClientShopCurrentTypeInfo().m_pApiType);
	}

	gs_CatClientShopState.m_pFetchTask = HttpGet(aUrl);
	gs_CatClientShopState.m_pFetchTask->Timeout(CATCLIENT_SHOP_TIMEOUT);
	gs_CatClientShopState.m_pFetchTask->IpResolve(IPRESOLVE::V4);
	gs_CatClientShopState.m_pFetchTask->MaxResponseSize(CATCLIENT_SHOP_PAGE_MAX_RESPONSE_SIZE);
	gs_CatClientShopState.m_pFetchTask->LogProgress(HTTPLOG::NONE);
	gs_CatClientShopState.m_pFetchTask->FailOnErrorStatus(false);
	gs_CatClientShopState.m_FetchTab = gs_CatClientShopState.m_Tab;
	gs_CatClientShopState.m_FetchPage = Page;
	str_copy(gs_CatClientShopState.m_aFetchSearch, gs_CatClientShopState.m_aSearch, sizeof(gs_CatClientShopState.m_aFetchSearch));
	CatClientShopSetStatus(CCLocalize("Loading shop..."));
	pMenus->MenuHttp()->Run(gs_CatClientShopState.m_pFetchTask);
}

static void CatClientShopEnsureFetch(CMenus *pMenus)
{
	if(gs_CatClientShopState.m_pFetchTask)
	{
		return;
	}

	const int Page = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	if(gs_CatClientShopState.m_LoadedTab != gs_CatClientShopState.m_Tab ||
		gs_CatClientShopState.m_LoadedPage != Page ||
		str_comp(gs_CatClientShopState.m_aLoadedSearch, gs_CatClientShopState.m_aSearch) != 0)
	{
		CatClientShopStartFetch(pMenus);
	}
}

static void CatClientShopStartPreviewFetch(CMenus *pMenus)
{
	if(gs_CatClientShopState.m_pPreviewTask != nullptr)
	{
		return;
	}

	for(SCatClientShopItem &Item : gs_CatClientShopState.m_vItems)
	{
		if(Item.m_PreviewFailed || Item.m_aImageUrl[0] == '\0')
		{
			continue;
		}

		if(CatClientShopLoadPreviewTexture(pMenus, Item, gs_CatClientShopState.m_Tab))
		{
			continue;
		}

		char aPreviewUrl[256];
		CatClientShopResolveUrl(Item.m_aImageUrl, aPreviewUrl, sizeof(aPreviewUrl));
		if(aPreviewUrl[0] == '\0')
		{
			Item.m_PreviewFailed = true;
			continue;
		}

		CatClientShopBuildPreviewPath(gs_CatClientShopState.m_Tab, Item.m_aId, gs_CatClientShopState.m_aPreviewPath, sizeof(gs_CatClientShopState.m_aPreviewPath));
		str_copy(gs_CatClientShopState.m_aPreviewItemId, Item.m_aId, sizeof(gs_CatClientShopState.m_aPreviewItemId));
		gs_CatClientShopState.m_PreviewTab = gs_CatClientShopState.m_Tab;
		gs_CatClientShopState.m_PreviewPage = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];

		gs_CatClientShopState.m_pPreviewTask = HttpGet(aPreviewUrl);
		gs_CatClientShopState.m_pPreviewTask->Timeout(CATCLIENT_SHOP_TIMEOUT);
		gs_CatClientShopState.m_pPreviewTask->IpResolve(IPRESOLVE::V4);
		gs_CatClientShopState.m_pPreviewTask->MaxResponseSize(CATCLIENT_SHOP_IMAGE_MAX_RESPONSE_SIZE);
		gs_CatClientShopState.m_pPreviewTask->LogProgress(HTTPLOG::NONE);
		gs_CatClientShopState.m_pPreviewTask->FailOnErrorStatus(false);
		pMenus->MenuHttp()->Run(gs_CatClientShopState.m_pPreviewTask);
		return;
	}
}

static void CatClientShopFinishPreviewFetch(CMenus *pMenus)
{
	if(!gs_CatClientShopState.m_pPreviewTask || gs_CatClientShopState.m_pPreviewTask->State() != EHttpState::DONE)
	{
		return;
	}

	SCatClientShopItem *pItem = CatClientShopFindItem(gs_CatClientShopState.m_aPreviewItemId);
	const bool SamePage = gs_CatClientShopState.m_PreviewTab == gs_CatClientShopState.m_Tab &&
		gs_CatClientShopState.m_PreviewPage == gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];

	if(pItem != nullptr)
	{
		if(!SamePage || gs_CatClientShopState.m_pPreviewTask->StatusCode() >= 400)
		{
			pItem->m_PreviewFailed = true;
		}
		else
		{
			unsigned char *pResult = nullptr;
			size_t ResultLength = 0;
			gs_CatClientShopState.m_pPreviewTask->Result(&pResult, &ResultLength);
			if(pResult != nullptr && ResultLength > 0 && CatClientShopIsPngBuffer(pResult, ResultLength) &&
				CatClientShopWriteFile(pMenus, gs_CatClientShopState.m_aPreviewPath, pResult, ResultLength) &&
				CatClientShopLoadPreviewTexture(pMenus, *pItem, gs_CatClientShopState.m_Tab))
			{
				pItem->m_PreviewFailed = false;
			}
			else
			{
				pItem->m_PreviewFailed = true;
			}
		}
	}

	CatClientShopAbortPreviewTask();
}

static void CatClientShopFinishFetch(CMenus *pMenus)
{
	if(!gs_CatClientShopState.m_pFetchTask || gs_CatClientShopState.m_pFetchTask->State() != EHttpState::DONE)
	{
		return;
	}

	gs_CatClientShopState.m_LoadedTab = gs_CatClientShopState.m_FetchTab;
	gs_CatClientShopState.m_LoadedPage = gs_CatClientShopState.m_FetchPage;
	str_copy(gs_CatClientShopState.m_aLoadedSearch, gs_CatClientShopState.m_aFetchSearch, sizeof(gs_CatClientShopState.m_aLoadedSearch));

	if(gs_CatClientShopState.m_pFetchTask->StatusCode() >= 400)
	{
		str_format(gs_CatClientShopState.m_aStatus, sizeof(gs_CatClientShopState.m_aStatus), "%s (%d)", CCLocalize("Shop request failed"), gs_CatClientShopState.m_pFetchTask->StatusCode());
		return;
	}

	json_value *pJson = gs_CatClientShopState.m_pFetchTask->ResultJson();
	if(pJson == nullptr || pJson->type != json_object)
	{
		if(pJson != nullptr)
		{
			json_value_free(pJson);
		}
		CatClientShopSetStatus(CCLocalize("Shop response is invalid"));
		return;
	}

	std::vector<SCatClientShopItem> vItems;
	const json_value *pSkins = json_object_get(pJson, "skins");
	if(pSkins != &json_value_none && pSkins->type == json_array)
	{
		for(int Index = 0; Index < json_array_length(pSkins); ++Index)
		{
			const json_value *pSkin = json_array_get(pSkins, Index);
			if(pSkin == &json_value_none || pSkin->type != json_object)
			{
				continue;
			}

			const char *pStatus = json_string_get(json_object_get(pSkin, "status"));
			if(pStatus != nullptr && pStatus[0] != '\0' && str_comp(pStatus, "approved") != 0)
			{
				continue;
			}

			SCatClientShopItem Item;
			if(const char *pId = json_string_get(json_object_get(pSkin, "id")); pId != nullptr)
			{
				str_copy(Item.m_aId, pId, sizeof(Item.m_aId));
			}
			if(const char *pName = json_string_get(json_object_get(pSkin, "name")); pName != nullptr)
			{
				str_copy(Item.m_aName, pName, sizeof(Item.m_aName));
			}
			if(const char *pFilename = json_string_get(json_object_get(pSkin, "filename")); pFilename != nullptr)
			{
				str_copy(Item.m_aFilename, pFilename, sizeof(Item.m_aFilename));
			}
			if(const char *pUsername = json_string_get(json_object_get(pSkin, "username")); pUsername != nullptr)
			{
				str_copy(Item.m_aUsername, pUsername, sizeof(Item.m_aUsername));
			}
			if(const char *pImageUrl = json_string_get(json_object_get(pSkin, "imageUrl")); pImageUrl != nullptr)
			{
				str_copy(Item.m_aImageUrl, pImageUrl, sizeof(Item.m_aImageUrl));
			}
			if(Item.m_aId[0] == '\0')
			{
				continue;
			}

			if(Item.m_aName[0] == '\0')
			{
				str_copy(Item.m_aName, Item.m_aFilename, sizeof(Item.m_aName));
			}

			vItems.push_back(Item);
		}
	}

	int TotalPages = 1;
	const json_value *pTotalPages = json_object_get(pJson, "totalPages");
	if(pTotalPages != &json_value_none && pTotalPages->type == json_integer)
	{
		TotalPages = maximum(1, json_int_get(pTotalPages));
	}

	int TotalItems = (int)vItems.size();
	const json_value *pTotal = json_object_get(pJson, "total");
	if(pTotal != &json_value_none && pTotal->type == json_integer)
	{
		TotalItems = maximum(0, json_int_get(pTotal));
	}

	CatClientShopClearItems(pMenus);
	gs_CatClientShopState.m_vItems = std::move(vItems);
	gs_CatClientShopState.m_SelectedIndex = gs_CatClientShopState.m_vItems.empty() ? -1 : 0;
	gs_CatClientShopState.m_TotalPages = TotalPages;
	gs_CatClientShopState.m_TotalItems = TotalItems;
	for(SCatClientShopItem &Item : gs_CatClientShopState.m_vItems)
	{
		CatClientShopLoadPreviewTexture(pMenus, Item, gs_CatClientShopState.m_Tab);
	}

	if(gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab] > gs_CatClientShopState.m_TotalPages)
	{
		gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab] = gs_CatClientShopState.m_TotalPages;
	}

	if(gs_CatClientShopState.m_vItems.empty())
	{
		CatClientShopSetStatus(CCLocalize("No assets found on this page"));
	}
	else
	{
		gs_CatClientShopState.m_aStatus[0] = '\0';
	}

	json_value_free(pJson);
}

static bool CatClientShopInstallDownloadedData(CMenus *pMenus, const unsigned char *pData, size_t DataSize)
{
	if(gs_CatClientShopState.m_InstallTab < 0 || gs_CatClientShopState.m_InstallTab >= NUM_CATCLIENT_SHOP_TABS)
	{
		return false;
	}

	if(!CatClientShopIsPngBuffer(pData, DataSize))
	{
		return false;
	}

	char aAssetPath[IO_MAX_PATH_LENGTH];
	CatClientShopBuildAssetPath(gs_CatClientShopState.m_InstallTab, gs_CatClientShopState.m_aInstallAssetName, aAssetPath, sizeof(aAssetPath));
	return CatClientShopWriteFile(pMenus, aAssetPath, pData, DataSize);
}

static void CatClientShopStartInstallRequest(CMenus *pMenus)
{
	if(gs_CatClientShopState.m_InstallUrlIndex < 0 || gs_CatClientShopState.m_InstallUrlIndex >= (int)gs_CatClientShopState.m_vInstallUrls.size())
	{
		CatClientShopSetStatus(CCLocalize("Failed to build a download URL"));
		return;
	}

	const std::string &Url = gs_CatClientShopState.m_vInstallUrls[gs_CatClientShopState.m_InstallUrlIndex];
	gs_CatClientShopState.m_pInstallTask = HttpGet(Url.c_str());
	gs_CatClientShopState.m_pInstallTask->Timeout(CATCLIENT_SHOP_TIMEOUT);
	gs_CatClientShopState.m_pInstallTask->IpResolve(IPRESOLVE::V4);
	gs_CatClientShopState.m_pInstallTask->MaxResponseSize(CATCLIENT_SHOP_IMAGE_MAX_RESPONSE_SIZE);
	gs_CatClientShopState.m_pInstallTask->LogProgress(HTTPLOG::NONE);
	gs_CatClientShopState.m_pInstallTask->FailOnErrorStatus(false);

	char aMessage[256];
	str_format(aMessage, sizeof(aMessage), "%s: %s", CCLocalize("Installing"), gs_CatClientShopState.m_aInstallAssetName);
	CatClientShopSetStatus(aMessage);
	pMenus->MenuHttp()->Run(gs_CatClientShopState.m_pInstallTask);
}

static void CatClientShopStartInstall(CMenus *pMenus, int Tab, const SCatClientShopItem &Item)
{
	CatClientShopAbortTask(gs_CatClientShopState.m_pInstallTask);
	gs_CatClientShopState.m_vInstallUrls.clear();
	gs_CatClientShopState.m_InstallUrlIndex = 0;
	gs_CatClientShopState.m_InstallTab = Tab;
	str_copy(gs_CatClientShopState.m_aInstallItemId, Item.m_aId, sizeof(gs_CatClientShopState.m_aInstallItemId));
	CatClientShopNormalizeAssetName(Item.m_aName, Item.m_aFilename, gs_CatClientShopState.m_aInstallAssetName, sizeof(gs_CatClientShopState.m_aInstallAssetName));
	CatClientShopBuildInstallUrls(Item, Tab, gs_CatClientShopState.m_vInstallUrls);
	CatClientShopStartInstallRequest(pMenus);
}

static void CatClientShopRetryInstall(CMenus *pMenus)
{
	CatClientShopAbortTask(gs_CatClientShopState.m_pInstallTask);
	++gs_CatClientShopState.m_InstallUrlIndex;
	if(gs_CatClientShopState.m_InstallUrlIndex >= (int)gs_CatClientShopState.m_vInstallUrls.size())
	{
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage), "%s: %s", CCLocalize("Unable to install asset"), gs_CatClientShopState.m_aInstallAssetName);
		CatClientShopSetStatus(aMessage);
		return;
	}

	CatClientShopStartInstallRequest(pMenus);
}

static void CatClientShopFinishInstall(CMenus *pMenus)
{
	if(!gs_CatClientShopState.m_pInstallTask || gs_CatClientShopState.m_pInstallTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(gs_CatClientShopState.m_pInstallTask->StatusCode() >= 400)
	{
		CatClientShopRetryInstall(pMenus);
		return;
	}

	unsigned char *pResult = nullptr;
	size_t ResultLength = 0;
	gs_CatClientShopState.m_pInstallTask->Result(&pResult, &ResultLength);
	if(pResult == nullptr || ResultLength == 0 || !CatClientShopInstallDownloadedData(pMenus, pResult, ResultLength))
	{
		CatClientShopRetryInstall(pMenus);
		return;
	}

	char aMessage[256];
	if(g_Config.m_CcShopAutoSet)
	{
		CatClientShopApplyAsset(pMenus, gs_CatClientShopState.m_InstallTab, gs_CatClientShopState.m_aInstallAssetName, true);
		str_format(aMessage, sizeof(aMessage), "%s: %s", CCLocalize("Installed"), gs_CatClientShopState.m_aInstallAssetName);
	}
	else
	{
		str_format(aMessage, sizeof(aMessage), "%s: %s", CCLocalize("Downloaded"), gs_CatClientShopState.m_aInstallAssetName);
	}
	CatClientShopSetStatus(aMessage);

	CatClientShopAbortTask(gs_CatClientShopState.m_pInstallTask);
	gs_CatClientShopState.m_vInstallUrls.clear();
	gs_CatClientShopState.m_InstallUrlIndex = 0;
}

#endif
