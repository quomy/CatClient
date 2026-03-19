#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_CORE_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_CORE_H

static bool CatClientShopIsVisibleTab(int Tab)
{
	for(int VisibleTab : gs_aVisibleCatClientShopTabs)
	{
		if(VisibleTab == Tab)
		{
			return true;
		}
	}
	return false;
}

static void CatClientShopAbortTask(std::shared_ptr<CHttpRequest> &pTask)
{
	if(pTask)
	{
		pTask->Abort();
		pTask = nullptr;
	}
}

static void CatClientShopInitState()
{
	if(gs_CatClientShopState.m_Initialized)
	{
		return;
	}

	gs_CatClientShopState.m_Initialized = true;
	gs_CatClientShopState.m_aPages.fill(1);
	gs_CatClientShopState.m_TotalPages = 1;
	if(!CatClientShopIsVisibleTab(gs_CatClientShopState.m_Tab))
	{
		gs_CatClientShopState.m_Tab = gs_aVisibleCatClientShopTabs[0];
	}
}

static void CatClientShopSetStatus(const char *pText)
{
	str_copy(gs_CatClientShopState.m_aStatus, pText, sizeof(gs_CatClientShopState.m_aStatus));
}

static void CatClientShopCloseTexturePreview()
{
	gs_CatClientShopState.m_PreviewOpen = false;
	gs_CatClientShopState.m_aOpenPreviewItemId[0] = '\0';
}

static bool CatClientShopHasTexturePreview()
{
	return gs_CatClientShopState.m_PreviewOpen && gs_CatClientShopState.m_aOpenPreviewItemId[0] != '\0';
}

static void CatClientShopClearItems(CMenus *pMenus)
{
	for(SCatClientShopItem &Item : gs_CatClientShopState.m_vItems)
	{
		if(Item.m_PreviewTexture.IsValid())
		{
			pMenus->MenuGraphics()->UnloadTexture(&Item.m_PreviewTexture);
		}
	}
	gs_CatClientShopState.m_vItems.clear();
	gs_CatClientShopState.m_SelectedIndex = -1;
}

static void CatClientShopBuildPreviewPath(int Tab, const char *pItemId, char *pOutput, size_t OutputSize)
{
	str_format(pOutput, OutputSize, "catclient/shop_previews/%s/%s.png", gs_aCatClientShopTypeInfos[Tab].m_pApiType, pItemId);
}

static SCatClientShopItem *CatClientShopFindItem(const char *pItemId)
{
	for(SCatClientShopItem &Item : gs_CatClientShopState.m_vItems)
	{
		if(str_comp(Item.m_aId, pItemId) == 0)
		{
			return &Item;
		}
	}
	return nullptr;
}

static bool CatClientShopLoadPreviewTexture(CMenus *pMenus, SCatClientShopItem &Item, int Tab)
{
	if(Item.m_PreviewTexture.IsValid() || Item.m_aId[0] == '\0')
	{
		return Item.m_PreviewTexture.IsValid();
	}

	char aPreviewPath[IO_MAX_PATH_LENGTH];
	CatClientShopBuildPreviewPath(Tab, Item.m_aId, aPreviewPath, sizeof(aPreviewPath));
	if(!pMenus->MenuStorage()->FileExists(aPreviewPath, IStorage::TYPE_SAVE))
	{
		return false;
	}

	CImageInfo ImageInfo;
	if(pMenus->MenuGraphics()->LoadPng(ImageInfo, aPreviewPath, IStorage::TYPE_SAVE))
	{
		Item.m_PreviewWidth = (float)ImageInfo.m_Width;
		Item.m_PreviewHeight = (float)ImageInfo.m_Height;
		ImageInfo.Free();
	}

	Item.m_PreviewTexture = pMenus->MenuGraphics()->LoadTexture(aPreviewPath, IStorage::TYPE_SAVE);
	return Item.m_PreviewTexture.IsValid() && !Item.m_PreviewTexture.IsNullTexture();
}

static void CatClientShopRenderTextureFit(IGraphics *pGraphics, const CUIRect &Rect, const IGraphics::CTextureHandle &Texture, float TextureWidth, float TextureHeight)
{
	if(!Texture.IsValid() || Texture.IsNullTexture())
	{
		return;
	}

	CUIRect DrawRect = Rect;
	if(TextureWidth > 0.0f && TextureHeight > 0.0f)
	{
		const float Scale = minimum(Rect.w / TextureWidth, Rect.h / TextureHeight);
		DrawRect.w = maximum(1.0f, TextureWidth * Scale);
		DrawRect.h = maximum(1.0f, TextureHeight * Scale);
		DrawRect.x += (Rect.w - DrawRect.w) / 2.0f;
		DrawRect.y += (Rect.h - DrawRect.h) / 2.0f;
	}

	pGraphics->WrapClamp();
	pGraphics->TextureSet(Texture);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(DrawRect.x, DrawRect.y, DrawRect.w, DrawRect.h);
	pGraphics->QuadsDrawTL(&QuadItem, 1);
	pGraphics->QuadsEnd();
	pGraphics->WrapNormal();
}

static void CatClientShopAbortPreviewTask()
{
	CatClientShopAbortTask(gs_CatClientShopState.m_pPreviewTask);
	gs_CatClientShopState.m_PreviewTab = -1;
	gs_CatClientShopState.m_PreviewPage = 0;
	gs_CatClientShopState.m_aPreviewItemId[0] = '\0';
	gs_CatClientShopState.m_aPreviewPath[0] = '\0';
}

static void CatClientShopInvalidatePage(CMenus *pMenus)
{
	CatClientShopAbortTask(gs_CatClientShopState.m_pFetchTask);
	CatClientShopAbortPreviewTask();
	CatClientShopCloseTexturePreview();
	CatClientShopClearItems(pMenus);
	gs_CatClientShopState.m_LoadedTab = -1;
	gs_CatClientShopState.m_LoadedPage = 0;
	gs_CatClientShopState.m_aLoadedSearch[0] = '\0';
	gs_CatClientShopState.m_aFetchSearch[0] = '\0';
}

static void CatClientShopSetTab(CMenus *pMenus, int Tab)
{
	CatClientShopInitState();
	Tab = std::clamp(Tab, 0, NUM_CATCLIENT_SHOP_TABS - 1);
	if(!CatClientShopIsVisibleTab(Tab))
	{
		Tab = gs_aVisibleCatClientShopTabs[0];
	}
	if(gs_CatClientShopState.m_Tab == Tab)
	{
		return;
	}

	gs_CatClientShopState.m_Tab = Tab;
	CatClientShopInvalidatePage(pMenus);
}

static void CatClientShopSetPage(CMenus *pMenus, int Page)
{
	CatClientShopInitState();
	Page = maximum(1, Page);
	int &CurrentPage = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	if(CurrentPage == Page)
	{
		return;
	}

	CurrentPage = Page;
	CatClientShopInvalidatePage(pMenus);
}

static void CatClientShopRefreshCurrentPage(CMenus *pMenus)
{
	CatClientShopInvalidatePage(pMenus);
}

static void CatClientShopSetSearch(CMenus *pMenus, const char *pSearch)
{
	CatClientShopInitState();

	char aSearch[sizeof(gs_CatClientShopState.m_aSearch)];
	str_copy(aSearch, pSearch != nullptr ? pSearch : "", sizeof(aSearch));
	str_utf8_trim_right(aSearch);
	char aTrimmedSearch[sizeof(gs_CatClientShopState.m_aSearch)];
	str_copy(aTrimmedSearch, str_utf8_skip_whitespaces(aSearch), sizeof(aTrimmedSearch));
	if(str_comp(gs_CatClientShopState.m_aSearch, aTrimmedSearch) == 0)
	{
		return;
	}

	str_copy(gs_CatClientShopState.m_aSearch, aTrimmedSearch, sizeof(gs_CatClientShopState.m_aSearch));
	gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab] = 1;
	CatClientShopInvalidatePage(pMenus);
}

static const SCatClientShopTypeInfo &CatClientShopCurrentTypeInfo()
{
	return gs_aCatClientShopTypeInfos[gs_CatClientShopState.m_Tab];
}

static void CatClientShopResolveUrl(const char *pInput, char *pOutput, size_t OutputSize)
{
	pOutput[0] = '\0';
	if(pInput == nullptr || pInput[0] == '\0')
	{
		return;
	}

	if(str_startswith(pInput, "https://") != nullptr || str_startswith(pInput, "http://") != nullptr)
	{
		str_copy(pOutput, pInput, OutputSize);
	}
	else if(pInput[0] == '/')
	{
		str_format(pOutput, OutputSize, "%s%s", CATCLIENT_SHOP_HOST, pInput);
	}
	else
	{
		str_format(pOutput, OutputSize, "%s/%s", CATCLIENT_SHOP_HOST, pInput);
	}
}

static void CatClientShopNormalizeAssetName(const char *pName, const char *pFilename, char *pOutput, size_t OutputSize)
{
	char aRawName[128];
	if(pFilename != nullptr && pFilename[0] != '\0')
	{
		IStorage::StripPathAndExtension(pFilename, aRawName, sizeof(aRawName));
	}
	else if(pName != nullptr && pName[0] != '\0')
	{
		str_copy(aRawName, pName, sizeof(aRawName));
	}
	else
	{
		str_copy(aRawName, "asset", sizeof(aRawName));
	}

	str_sanitize_filename(aRawName);

	char aSanitized[128];
	int WritePos = 0;
	bool LastWasSeparator = true;
	for(int ReadPos = 0; aRawName[ReadPos] != '\0' && WritePos < (int)sizeof(aSanitized) - 1; ++ReadPos)
	{
		unsigned char Character = (unsigned char)aRawName[ReadPos];
		if(Character <= 32)
		{
			if(!LastWasSeparator && WritePos < (int)sizeof(aSanitized) - 1)
			{
				aSanitized[WritePos++] = '_';
				LastWasSeparator = true;
			}
			continue;
		}

		aSanitized[WritePos++] = Character;
		LastWasSeparator = false;
	}
	aSanitized[WritePos] = '\0';

	while(WritePos > 0 && aSanitized[WritePos - 1] == '_')
	{
		aSanitized[--WritePos] = '\0';
	}

	if(aSanitized[0] == '\0')
	{
		str_copy(aSanitized, "asset", sizeof(aSanitized));
	}

	str_copy(pOutput, aSanitized, OutputSize);
}

#endif
