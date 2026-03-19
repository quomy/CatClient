#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_PREVIEW_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_PREVIEW_H

static void CatClientShopRenderTexturePreview(CMenus *pMenus, const CUIRect &MainView)
{
	SCatClientShopItem *pItem = CatClientShopFindItem(gs_CatClientShopState.m_aOpenPreviewItemId);
	if(pItem == nullptr || !pItem->m_PreviewTexture.IsValid() || pItem->m_PreviewTexture.IsNullTexture())
	{
		CatClientShopCloseTexturePreview();
		return;
	}

	CUIRect Overlay = MainView;
	Overlay.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.8f), IGraphics::CORNER_ALL, 0.0f);

	CUIRect Panel;
	CatClientMenuConstrainWidth(Overlay, Panel, 1040.0f);
	const float VerticalMargin = Overlay.h > 760.0f ? (Overlay.h - 760.0f) / 2.0f : 20.0f;
	Panel.HMargin(VerticalMargin, &Panel);
	Panel.Draw(ColorRGBA(0.02f, 0.02f, 0.02f, 0.92f), IGraphics::CORNER_ALL, CATCLIENT_MENU_SECTION_ROUNDING + 2.0f);

	CUIRect Content;
	Panel.Margin(CATCLIENT_MENU_SECTION_PADDING + 4.0f, &Content);

	CUIRect HeaderRow, SliderRow, PreviewRow;
	Content.HSplitTop(28.0f, &HeaderRow, &Content);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &Content);
	Content.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &SliderRow, &Content);
	Content.HSplitTop(CATCLIENT_MENU_MARGIN, nullptr, &Content);
	PreviewRow = Content;

	CUIRect TitleLabel, CloseButton;
	HeaderRow.VSplitRight(32.0f, &TitleLabel, &CloseButton);
	pMenus->MenuUi()->DoLabel(&TitleLabel, pItem->m_aName, CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	if(pMenus->MenuUi()->DoButton_FontIcon(&gs_CatClientShopState.m_PreviewCloseButton, FontIcon::XMARK, 0, &CloseButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, true, ColorRGBA(0.0f, 0.0f, 0.0f, 0.30f)))
	{
		CatClientShopCloseTexturePreview();
		return;
	}

	pMenus->MenuUi()->DoScrollbarOption(&gs_CatClientShopState.m_PreviewBackgroundColor, &gs_CatClientShopState.m_PreviewBackgroundColor, &SliderRow, CCLocalize("Background color"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

	CUIRect PreviewArea = PreviewRow;
	PreviewArea.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_ALL, CATCLIENT_MENU_SECTION_ROUNDING);
	PreviewArea.Margin(CATCLIENT_MENU_MARGIN, &PreviewArea);
	PreviewArea.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f), IGraphics::CORNER_ALL, CATCLIENT_MENU_SECTION_ROUNDING - 2.0f);

	const float BackgroundColor = gs_CatClientShopState.m_PreviewBackgroundColor / 100.0f;
	pMenus->MenuGraphics()->DrawRect(
		PreviewArea.x,
		PreviewArea.y,
		PreviewArea.w,
		PreviewArea.h,
		ColorRGBA(BackgroundColor, BackgroundColor, BackgroundColor, 1.0f),
		IGraphics::CORNER_ALL,
		CATCLIENT_MENU_SECTION_ROUNDING - 2.0f);

	CUIRect TextureRect;
	PreviewArea.Margin(CATCLIENT_MENU_MARGIN * 1.5f, &TextureRect);
	CatClientShopRenderTextureFit(pMenus->MenuGraphics(), TextureRect, pItem->m_PreviewTexture, pItem->m_PreviewWidth, pItem->m_PreviewHeight);
}

#endif
