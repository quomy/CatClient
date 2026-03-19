#include "menus_catclient_common.h"

#include <base/math.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/image.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

static constexpr const char *CATCLIENT_SHOP_HOST = "https://data.teeworlds.xyz";
static constexpr const char *CATCLIENT_SHOP_API_URL = "https://data.teeworlds.xyz/api/skins?page=%d&limit=10&type=%s";
static constexpr const char *CATCLIENT_SHOP_API_SEARCH_URL = "https://data.teeworlds.xyz/api/skins?page=%d&limit=10&type=%s&search=%s";
static constexpr CTimeout CATCLIENT_SHOP_TIMEOUT{8000, 0, 1024, 8};
static constexpr int64_t CATCLIENT_SHOP_PAGE_MAX_RESPONSE_SIZE = 2 * 1024 * 1024;
static constexpr int64_t CATCLIENT_SHOP_IMAGE_MAX_RESPONSE_SIZE = 32 * 1024 * 1024;
static constexpr int CATCLIENT_SHOP_PREVIEW_BACKGROUND_COLOR_DEFAULT = 0;

enum
{
	CATCLIENT_SHOP_ENTITIES = 0,
	CATCLIENT_SHOP_GAME,
	CATCLIENT_SHOP_EMOTICONS,
	CATCLIENT_SHOP_PARTICLES,
	CATCLIENT_SHOP_HUD,
	CATCLIENT_SHOP_CURSORS,
	NUM_CATCLIENT_SHOP_TABS,
};

static constexpr int gs_aVisibleCatClientShopTabs[] = {
	CATCLIENT_SHOP_ENTITIES,
	CATCLIENT_SHOP_GAME,
	CATCLIENT_SHOP_EMOTICONS,
	CATCLIENT_SHOP_PARTICLES,
	CATCLIENT_SHOP_HUD,
	CATCLIENT_SHOP_CURSORS,
};

struct SCatClientShopTypeInfo
{
	const char *m_pLabel;
	const char *m_pApiType;
	const char *m_pAssetDirectory;
	const char *m_pIcon;
};

static const SCatClientShopTypeInfo gs_aCatClientShopTypeInfos[NUM_CATCLIENT_SHOP_TABS] = {
	{CCLocalizable("Entities"), "entity", "assets/entities", FontIcon::IMAGE},
	{CCLocalizable("Game"), "gameskin", "assets/game", FontIcon::IMAGE},
	{CCLocalizable("Emoticons"), "emoticon", "assets/emoticons", FontIcon::IMAGE},
	{CCLocalizable("Particles"), "particle", "assets/particles", FontIcon::IMAGE},
	{CCLocalizable("HUD"), "hud", "assets/hud", FontIcon::IMAGE},
	{CCLocalizable("Cursors"), "cursor", "assets/cursors", FontIcon::IMAGE},
};

struct SCatClientShopItem
{
	char m_aId[64]{};
	char m_aName[128]{};
	char m_aFilename[128]{};
	char m_aUsername[64]{};
	char m_aImageUrl[256]{};
	bool m_PreviewFailed = false;
	float m_PreviewWidth = 0.0f;
	float m_PreviewHeight = 0.0f;
	IGraphics::CTextureHandle m_PreviewTexture;
	CButtonContainer m_PreviewButton;
	CButtonContainer m_InstallButton;
};

struct SCatClientShopState
{
	bool m_Initialized = false;
	int m_Tab = CATCLIENT_SHOP_ENTITIES;
	int m_SelectedIndex = -1;
	int m_TotalPages = 1;
	int m_TotalItems = 0;
	int m_LoadedTab = -1;
	int m_LoadedPage = 0;
	int m_FetchTab = -1;
	int m_FetchPage = 0;
	char m_aSearch[128]{};
	char m_aLoadedSearch[128]{};
	char m_aFetchSearch[128]{};
	int m_PreviewTab = -1;
	int m_PreviewPage = 0;
	std::array<int, NUM_CATCLIENT_SHOP_TABS> m_aPages{};
	std::shared_ptr<CHttpRequest> m_pFetchTask;
	std::shared_ptr<CHttpRequest> m_pInstallTask;
	std::shared_ptr<CHttpRequest> m_pPreviewTask;
	std::vector<SCatClientShopItem> m_vItems;
	std::vector<std::string> m_vInstallUrls;
	int m_InstallUrlIndex = 0;
	int m_InstallTab = -1;
	char m_aInstallAssetName[128]{};
	char m_aInstallItemId[64]{};
	char m_aPreviewItemId[64]{};
	char m_aPreviewPath[IO_MAX_PATH_LENGTH]{};
	char m_aStatus[256]{};
	char m_aOpenPreviewItemId[64]{};
	int m_PreviewBackgroundColor = CATCLIENT_SHOP_PREVIEW_BACKGROUND_COLOR_DEFAULT;
	bool m_PreviewOpen = false;
	CButtonContainer m_PreviewCloseButton;
};

static SCatClientShopState gs_CatClientShopState;

#include "modules/shop/menus_catclient_shop_core.h"
#include "modules/shop/menus_catclient_shop_assets.h"
#include "modules/shop/menus_catclient_shop_requests.h"
#include "modules/shop/menus_catclient_shop_preview.h"
#include "modules/shop/menus_catclient_shop_render.h"
