#include "menus.h"

#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <chrono>

using namespace std::chrono_literals;

typedef std::function<void()> TMenuAssetScanLoadedFunc;

struct SMenuAssetScanUser
{
	void *m_pUser;
	TMenuAssetScanLoadedFunc m_LoadedFunc;
};

void CMenus::LoadEntities(SCustomEntities *pEntitiesItem, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;

	char aPath[IO_MAX_PATH_LENGTH];
	if(str_comp(pEntitiesItem->m_aName, "default") == 0)
	{
		for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT; ++i)
		{
			str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", gs_apModEntitiesNames[i]);
			pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			if(!pEntitiesItem->m_RenderTexture.IsValid() || pEntitiesItem->m_RenderTexture.IsNullTexture())
				pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
		}
	}
	else
	{
		for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT; ++i)
		{
			str_format(aPath, sizeof(aPath), "assets/entities/%s/%s.png", pEntitiesItem->m_aName, gs_apModEntitiesNames[i]);
			pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			if(pEntitiesItem->m_aImages[i].m_Texture.IsNullTexture())
			{
				str_format(aPath, sizeof(aPath), "assets/entities/%s.png", pEntitiesItem->m_aName);
				pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			}
			if(!pEntitiesItem->m_RenderTexture.IsValid() || pEntitiesItem->m_RenderTexture.IsNullTexture())
				pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
		}
	}
}

int CMenus::EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	if(IsDir)
	{
		if(pName[0] == '.')
			return 0;

		// default is reserved
		if(str_comp(pName, "default") == 0)
			return 0;

		SCustomEntities EntitiesItem;
		str_copy(EntitiesItem.m_aName, pName);
		CMenus::LoadEntities(&EntitiesItem, pUser);
		pThis->m_vEntitiesList.push_back(EntitiesItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[IO_MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, aName);
			CMenus::LoadEntities(&EntitiesItem, pUser);
			pThis->m_vEntitiesList.push_back(EntitiesItem);
		}
	}

	pRealUser->m_LoadedFunc();

	return 0;
}

template<typename TName>
static void LoadAsset(TName *pAssetItem, const char *pAssetName, IGraphics *pGraphics)
{
	char aPath[IO_MAX_PATH_LENGTH];
	if(str_comp(pAssetItem->m_aName, "default") == 0)
	{
		str_format(aPath, sizeof(aPath), "%s.png", pAssetName);
		pAssetItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
	}
	else
	{
		str_format(aPath, sizeof(aPath), "assets/%s/%s.png", pAssetName, pAssetItem->m_aName);
		pAssetItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
		if(pAssetItem->m_RenderTexture.IsNullTexture())
		{
			str_format(aPath, sizeof(aPath), "assets/%s/%s/%s.png", pAssetName, pAssetItem->m_aName, pAssetName);
			pAssetItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
		}
	}
}

template<typename TName>
static int AssetScan(const char *pName, int IsDir, int DirType, std::vector<TName> &vAssetList, const char *pAssetName, IGraphics *pGraphics, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	if(IsDir)
	{
		if(pName[0] == '.')
			return 0;

		// default is reserved
		if(str_comp(pName, "default") == 0)
			return 0;

		TName AssetItem;
		str_copy(AssetItem.m_aName, pName);
		LoadAsset(&AssetItem, pAssetName, pGraphics);
		vAssetList.push_back(AssetItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[IO_MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			TName AssetItem;
			str_copy(AssetItem.m_aName, aName);
			LoadAsset(&AssetItem, pAssetName, pGraphics);
			vAssetList.push_back(AssetItem);
		}
	}

	pRealUser->m_LoadedFunc();

	return 0;
}

int CMenus::GameScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vGameList, "game", pGraphics, pUser);
}

int CMenus::EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vEmoticonList, "emoticons", pGraphics, pUser);
}

int CMenus::ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vParticlesList, "particles", pGraphics, pUser);
}

int CMenus::HudScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vHudList, "hud", pGraphics, pUser);
}

int CMenus::ExtrasScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vExtrasList, "extras", pGraphics, pUser);
}

static void LoadCursor(CMenus::SCustomCursor *pCursorItem, IGraphics *pGraphics)
{
	char aPath[IO_MAX_PATH_LENGTH];
	if(str_comp(pCursorItem->m_aName, "default") == 0)
	{
		str_copy(aPath, "gui_cursor.png");
		pCursorItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
	}
	else
	{
		str_format(aPath, sizeof(aPath), "assets/cursors/%s.png", pCursorItem->m_aName);
		pCursorItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
		if(pCursorItem->m_RenderTexture.IsNullTexture())
		{
			str_format(aPath, sizeof(aPath), "assets/cursors/%s/gui_cursor.png", pCursorItem->m_aName);
			pCursorItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
		}
	}
}

int CMenus::AudioScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	if(!IsDir || pName[0] == '.' || str_comp(pName, "default") == 0)
	{
		return 0;
	}

	for(const SCustomAudio &ExistingItem : pThis->m_vAudioList)
	{
		if(str_comp(ExistingItem.m_aName, pName) == 0)
		{
			return 0;
		}
	}

	SCustomAudio AudioItem;
	str_copy(AudioItem.m_aName, pName);
	pThis->m_vAudioList.push_back(AudioItem);

	pRealUser->m_LoadedFunc();
	return 0;
}

int CMenus::CursorScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	if(IsDir)
	{
		if(pName[0] == '.' || str_comp(pName, "default") == 0)
		{
			return 0;
		}

		SCustomCursor CursorItem;
		str_copy(CursorItem.m_aName, pName);
		LoadCursor(&CursorItem, pThis->Graphics());
		pThis->m_vCursorList.push_back(CursorItem);
	}
	else if(str_endswith(pName, ".png"))
	{
		char aName[IO_MAX_PATH_LENGTH];
		str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
		if(str_comp(aName, "default") == 0)
		{
			return 0;
		}

		SCustomCursor CursorItem;
		str_copy(CursorItem.m_aName, aName);
		LoadCursor(&CursorItem, pThis->Graphics());
		pThis->m_vCursorList.push_back(CursorItem);
	}

	pRealUser->m_LoadedFunc();
	return 0;
}

static std::vector<const CMenus::SCustomEntities *> gs_vpSearchEntitiesList;
static std::vector<const CMenus::SCustomGame *> gs_vpSearchGamesList;
static std::vector<const CMenus::SCustomEmoticon *> gs_vpSearchEmoticonsList;
static std::vector<const CMenus::SCustomParticle *> gs_vpSearchParticlesList;
static std::vector<const CMenus::SCustomHud *> gs_vpSearchHudList;
static std::vector<const CMenus::SCustomExtras *> gs_vpSearchExtrasList;
static std::vector<const CMenus::SCustomAudio *> gs_vpSearchAudioList;
static std::vector<const CMenus::SCustomCursor *> gs_vpSearchCursorList;

static bool gs_aInitCustomList[CMenus::NUM_ASSETS_TABS] = {
	true,
};

static size_t gs_aCustomListSize[CMenus::NUM_ASSETS_TABS] = {
	0,
};

static CLineInputBuffered<64> s_aFilterInputs[CMenus::NUM_ASSETS_TABS];

static int gs_CurCustomTab = CMenus::ASSETS_TAB_ENTITIES;

static const CMenus::SCustomItem *GetCustomItem(int CurTab, size_t Index)
{
	if(CurTab == CMenus::ASSETS_TAB_ENTITIES)
		return gs_vpSearchEntitiesList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_GAME)
		return gs_vpSearchGamesList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_EMOTICONS)
		return gs_vpSearchEmoticonsList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_PARTICLES)
		return gs_vpSearchParticlesList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_HUD)
		return gs_vpSearchHudList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_EXTRAS)
		return gs_vpSearchExtrasList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_AUDIO)
		return gs_vpSearchAudioList[Index];
	else if(CurTab == CMenus::ASSETS_TAB_CURSORS)
		return gs_vpSearchCursorList[Index];
	dbg_assert_failed("Invalid CurTab: %d", CurTab);
}

template<typename TName>
static void ClearAssetList(std::vector<TName> &vList, IGraphics *pGraphics)
{
	for(TName &Asset : vList)
	{
		pGraphics->UnloadTexture(&Asset.m_RenderTexture);
	}
	vList.clear();
}

void CMenus::ClearCustomItems(int CurTab)
{
	if(CurTab == CMenus::ASSETS_TAB_ENTITIES)
	{
		for(auto &Entity : m_vEntitiesList)
		{
			for(auto &Image : Entity.m_aImages)
			{
				Graphics()->UnloadTexture(&Image.m_Texture);
			}
		}
		m_vEntitiesList.clear();

		// reload current entities
		GameClient()->m_MapImages.ChangeEntitiesPath(g_Config.m_ClAssetsEntities);
	}
	else if(CurTab == CMenus::ASSETS_TAB_GAME)
	{
		ClearAssetList(m_vGameList, Graphics());

		// reload current game skin
		GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
	}
	else if(CurTab == CMenus::ASSETS_TAB_EMOTICONS)
	{
		ClearAssetList(m_vEmoticonList, Graphics());

		// reload current emoticons skin
		GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
	}
	else if(CurTab == CMenus::ASSETS_TAB_PARTICLES)
	{
		ClearAssetList(m_vParticlesList, Graphics());

		// reload current particles skin
		GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
	}
	else if(CurTab == CMenus::ASSETS_TAB_HUD)
	{
		ClearAssetList(m_vHudList, Graphics());

		// reload current hud skin
		GameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
	}
	else if(CurTab == CMenus::ASSETS_TAB_EXTRAS)
	{
		ClearAssetList(m_vExtrasList, Graphics());

		// reload current DDNet particles skin
		GameClient()->LoadExtrasSkin(g_Config.m_ClAssetExtras);
	}
	else if(CurTab == CMenus::ASSETS_TAB_AUDIO)
	{
		m_vAudioList.clear();
		GameClient()->m_Sounds.ReloadSamples();
	}
	else if(CurTab == CMenus::ASSETS_TAB_CURSORS)
	{
		ClearAssetList(m_vCursorList, Graphics());
		GameClient()->m_CatClient.LoadCursorAsset(g_Config.m_ClAssetCursor);
	}
	else
	{
		dbg_assert_failed("Invalid CurTab: %d", CurTab);
	}
	gs_aInitCustomList[CurTab] = true;
}

void CMenus::UnloadCustomItems()
{
	for(auto &Entity : m_vEntitiesList)
	{
		for(auto &Image : Entity.m_aImages)
		{
			Graphics()->UnloadTexture(&Image.m_Texture);
		}
	}
	m_vEntitiesList.clear();

	ClearAssetList(m_vGameList, Graphics());
	ClearAssetList(m_vEmoticonList, Graphics());
	ClearAssetList(m_vParticlesList, Graphics());
	ClearAssetList(m_vHudList, Graphics());
	ClearAssetList(m_vExtrasList, Graphics());
	ClearAssetList(m_vCursorList, Graphics());
	m_vAudioList.clear();
}

template<typename TName, typename TCaller>
static void InitAssetList(std::vector<TName> &vAssetList, const char *pAssetPath, const char *pAssetName, FS_LISTDIR_CALLBACK pfnCallback, IGraphics *pGraphics, IStorage *pStorage, TCaller Caller)
{
	if(vAssetList.empty())
	{
		TName AssetItem;
		str_copy(AssetItem.m_aName, "default");
		LoadAsset(&AssetItem, pAssetName, pGraphics);
		vAssetList.push_back(AssetItem);

		// load assets
		pStorage->ListDirectory(IStorage::TYPE_ALL, pAssetPath, pfnCallback, Caller);
		std::sort(vAssetList.begin(), vAssetList.end());
	}
	if(vAssetList.size() != gs_aCustomListSize[gs_CurCustomTab])
		gs_aInitCustomList[gs_CurCustomTab] = true;
}

template<typename TName>
static int InitSearchList(std::vector<const TName *> &vpSearchList, std::vector<TName> &vAssetList)
{
	vpSearchList.clear();
	int ListSize = vAssetList.size();
	for(int i = 0; i < ListSize; ++i)
	{
		const TName *pAsset = &vAssetList[i];

		// filter quick search
		if(!s_aFilterInputs[gs_CurCustomTab].IsEmpty() && !str_utf8_find_nocase(pAsset->m_aName, s_aFilterInputs[gs_CurCustomTab].GetString()))
			continue;

		vpSearchList.push_back(pAsset);
	}
	return vAssetList.size();
}

void CMenus::RenderSettingsCustom(CUIRect MainView)
{
	CUIRect TabBar, CustomList, QuickSearch, DirectoryButton, ReloadButton;

	gs_CurCustomTab = m_AssetsTab;
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / (float)CMenus::NUM_ASSETS_TABS;
	static CButtonContainer s_aPageTabs[CMenus::NUM_ASSETS_TABS] = {};
	const char *apTabNames[CMenus::NUM_ASSETS_TABS] = {
		Localize("Entities"),
		Localize("Game"),
		Localize("Emoticons"),
		Localize("Particles"),
		Localize("HUD"),
		Localize("Extras"),
		Localize("Audio"),
		Localize("Cursors")};

	for(int Tab = CMenus::ASSETS_TAB_ENTITIES; Tab < CMenus::NUM_ASSETS_TABS; ++Tab)
	{
		CUIRect Button;
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == CMenus::ASSETS_TAB_ENTITIES ? IGraphics::CORNER_L : (Tab == CMenus::NUM_ASSETS_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], gs_CurCustomTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			gs_CurCustomTab = Tab;
			m_AssetsTab = Tab;
		}
	}

	auto LoadStartTime = time_get_nanoseconds();
	SMenuAssetScanUser User;
	User.m_pUser = this;
	User.m_LoadedFunc = [&]() {
		if(time_get_nanoseconds() - LoadStartTime > 500ms)
			RenderLoading(Localize("Loading assets"), "", 0);
	};
	if(gs_CurCustomTab == CMenus::ASSETS_TAB_ENTITIES)
	{
		if(m_vEntitiesList.empty())
		{
			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, "default");
			LoadEntities(&EntitiesItem, &User);
			m_vEntitiesList.push_back(EntitiesItem);

			// load entities
			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/entities", EntitiesScan, &User);
			std::sort(m_vEntitiesList.begin(), m_vEntitiesList.end());
		}
		if(m_vEntitiesList.size() != gs_aCustomListSize[gs_CurCustomTab])
			gs_aInitCustomList[gs_CurCustomTab] = true;
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_GAME)
	{
		InitAssetList(m_vGameList, "assets/game", "game", GameScan, Graphics(), Storage(), &User);
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EMOTICONS)
	{
		InitAssetList(m_vEmoticonList, "assets/emoticons", "emoticons", EmoticonsScan, Graphics(), Storage(), &User);
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_PARTICLES)
	{
		InitAssetList(m_vParticlesList, "assets/particles", "particles", ParticlesScan, Graphics(), Storage(), &User);
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_HUD)
	{
		InitAssetList(m_vHudList, "assets/hud", "hud", HudScan, Graphics(), Storage(), &User);
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EXTRAS)
	{
		InitAssetList(m_vExtrasList, "assets/extras", "extras", ExtrasScan, Graphics(), Storage(), &User);
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
	{
		if(m_vAudioList.empty())
		{
			SCustomAudio AudioItem;
			str_copy(AudioItem.m_aName, "default");
			m_vAudioList.push_back(AudioItem);

			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/audio", AudioScan, &User);
			Storage()->ListDirectory(IStorage::TYPE_ALL, "audio", AudioScan, &User);
			std::sort(m_vAudioList.begin(), m_vAudioList.end());
		}
		if(m_vAudioList.size() != gs_aCustomListSize[gs_CurCustomTab])
		{
			gs_aInitCustomList[gs_CurCustomTab] = true;
		}
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
	{
		if(m_vCursorList.empty())
		{
			SCustomCursor CursorItem;
			str_copy(CursorItem.m_aName, "default");
			LoadCursor(&CursorItem, Graphics());
			m_vCursorList.push_back(CursorItem);

			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/cursors", CursorScan, &User);
			std::sort(m_vCursorList.begin(), m_vCursorList.end());
		}
		if(m_vCursorList.size() != gs_aCustomListSize[gs_CurCustomTab])
		{
			gs_aInitCustomList[gs_CurCustomTab] = true;
		}
	}
	else
	{
		dbg_assert_failed("Invalid gs_CurCustomTab: %d", gs_CurCustomTab);
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	CUIRect AnimatedMainView;
	BeginPageTransition(m_AssetsTransition, gs_CurCustomTab, MainView, AnimatedMainView);
	MainView = AnimatedMainView;

	// skin selector
	MainView.HSplitTop(MainView.h - 10.0f - ms_ButtonHeight, &CustomList, &MainView);
	if(IsFirstRunSetupStepActive(FIRST_RUN_SETUP_CURSORS) && gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
		RegisterFirstRunFocus(FIRST_RUN_SETUP_CURSORS, CustomList);
	if(IsFirstRunSetupStepActive(FIRST_RUN_SETUP_AUDIO) && gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
		RegisterFirstRunFocus(FIRST_RUN_SETUP_AUDIO, CustomList);
	if(gs_aInitCustomList[gs_CurCustomTab])
	{
		int ListSize = 0;
		if(gs_CurCustomTab == CMenus::ASSETS_TAB_ENTITIES)
		{
			gs_vpSearchEntitiesList.clear();
			ListSize = m_vEntitiesList.size();
			for(int i = 0; i < ListSize; ++i)
			{
				const SCustomEntities *pEntity = &m_vEntitiesList[i];

				// filter quick search
				if(!s_aFilterInputs[gs_CurCustomTab].IsEmpty() && !str_utf8_find_nocase(pEntity->m_aName, s_aFilterInputs[gs_CurCustomTab].GetString()))
					continue;

				gs_vpSearchEntitiesList.push_back(pEntity);
			}
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_GAME)
		{
			ListSize = InitSearchList(gs_vpSearchGamesList, m_vGameList);
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EMOTICONS)
		{
			ListSize = InitSearchList(gs_vpSearchEmoticonsList, m_vEmoticonList);
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_PARTICLES)
		{
			ListSize = InitSearchList(gs_vpSearchParticlesList, m_vParticlesList);
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_HUD)
		{
			ListSize = InitSearchList(gs_vpSearchHudList, m_vHudList);
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EXTRAS)
		{
			ListSize = InitSearchList(gs_vpSearchExtrasList, m_vExtrasList);
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
		{
			ListSize = InitSearchList(gs_vpSearchAudioList, m_vAudioList);
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
		{
			ListSize = InitSearchList(gs_vpSearchCursorList, m_vCursorList);
		}
		gs_aInitCustomList[gs_CurCustomTab] = false;
		gs_aCustomListSize[gs_CurCustomTab] = ListSize;
	}

	int OldSelected = -1;
	float Margin = 10;
	float TextureWidth = 150;
	float TextureHeight = 150;

	size_t SearchListSize = 0;

	if(gs_CurCustomTab == CMenus::ASSETS_TAB_ENTITIES)
	{
		SearchListSize = gs_vpSearchEntitiesList.size();
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_GAME)
	{
		SearchListSize = gs_vpSearchGamesList.size();
		TextureHeight = 75;
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EMOTICONS)
	{
		SearchListSize = gs_vpSearchEmoticonsList.size();
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_PARTICLES)
	{
		SearchListSize = gs_vpSearchParticlesList.size();
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_HUD)
	{
		SearchListSize = gs_vpSearchHudList.size();
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EXTRAS)
	{
		SearchListSize = gs_vpSearchExtrasList.size();
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
	{
		SearchListSize = gs_vpSearchAudioList.size();
		TextureHeight = 60;
	}
	else if(gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
	{
		SearchListSize = gs_vpSearchCursorList.size();
		TextureWidth = 96;
		TextureHeight = 96;
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(TextureHeight + 15.0f + 10.0f + Margin, SearchListSize, CustomList.w / (Margin + TextureWidth), 1, OldSelected, &CustomList, false);
	for(size_t i = 0; i < SearchListSize; ++i)
	{
		const SCustomItem *pItem = GetCustomItem(gs_CurCustomTab, i);
		if(pItem == nullptr)
			continue;

		if(gs_CurCustomTab == CMenus::ASSETS_TAB_ENTITIES)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetsEntities) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_GAME)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetGame) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EMOTICONS)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetEmoticons) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_PARTICLES)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetParticles) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_HUD)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetHud) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EXTRAS)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetExtras) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetAudio) == 0)
				OldSelected = i;
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetCursor) == 0)
				OldSelected = i;
		}

		const CListboxItem Item = s_ListBox.DoNextItem(pItem, OldSelected >= 0 && (size_t)OldSelected == i);
		CUIRect ItemRect = Item.m_Rect;
		ItemRect.Margin(Margin / 2, &ItemRect);
		if(!Item.m_Visible)
			continue;

		CUIRect TextureRect;
		ItemRect.HSplitTop(15, &ItemRect, &TextureRect);
		TextureRect.HSplitTop(10, nullptr, &TextureRect);
		Ui()->DoLabel(&ItemRect, pItem->m_aName, ItemRect.h - 2, TEXTALIGN_MC);
		if(pItem->m_RenderTexture.IsValid())
		{
			Graphics()->WrapClamp();
			Graphics()->TextureSet(pItem->m_RenderTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1, 1, 1, 1);
			IGraphics::CQuadItem QuadItem(TextureRect.x + (TextureRect.w - TextureWidth) / 2, TextureRect.y + (TextureRect.h - TextureHeight) / 2, TextureWidth, TextureHeight);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO || gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
		{
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
			Ui()->DoLabel(&TextureRect, gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO ? FontIcon::MUSIC : FontIcon::IMAGE, TextureHeight * 0.5f, TEXTALIGN_MC);
			TextRender()->SetRenderFlags(0);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected)
	{
		if(GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName[0] != '\0')
		{
			if(gs_CurCustomTab == CMenus::ASSETS_TAB_ENTITIES)
			{
				str_copy(g_Config.m_ClAssetsEntities, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->m_MapImages.ChangeEntitiesPath(GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_GAME)
			{
				str_copy(g_Config.m_ClAssetGame, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EMOTICONS)
			{
				str_copy(g_Config.m_ClAssetEmoticons, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_PARTICLES)
			{
				str_copy(g_Config.m_ClAssetParticles, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_HUD)
			{
				str_copy(g_Config.m_ClAssetHud, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EXTRAS)
			{
				str_copy(g_Config.m_ClAssetExtras, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadExtrasSkin(g_Config.m_ClAssetExtras);
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
			{
				str_copy(g_Config.m_ClAssetAudio, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->m_Sounds.ReloadSamples();
			}
			else if(gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
			{
				str_copy(g_Config.m_ClAssetCursor, GetCustomItem(gs_CurCustomTab, NewSelected)->m_aName);
				GameClient()->m_CatClient.LoadCursorAsset(g_Config.m_ClAssetCursor);
			}
		}
	}

	// Quick search
	MainView.HSplitBottom(ms_ButtonHeight, &MainView, &QuickSearch);
	QuickSearch.VSplitLeft(220.0f, &QuickSearch, &DirectoryButton);
	QuickSearch.HSplitTop(5.0f, nullptr, &QuickSearch);
	if(Ui()->DoEditBox_Search(&s_aFilterInputs[gs_CurCustomTab], &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive()))
	{
		gs_aInitCustomList[gs_CurCustomTab] = true;
	}

	DirectoryButton.HSplitTop(5.0f, nullptr, &DirectoryButton);
	DirectoryButton.VSplitRight(175.0f, nullptr, &DirectoryButton);
	DirectoryButton.VSplitRight(25.0f, &DirectoryButton, &ReloadButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, nullptr);
	static CButtonContainer s_AssetsDirId;
	if(DoButton_Menu(&s_AssetsDirId, Localize("Assets directory"), 0, &DirectoryButton))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		char aBufFull[IO_MAX_PATH_LENGTH + 7];
		if(gs_CurCustomTab == CMenus::ASSETS_TAB_ENTITIES)
			str_copy(aBufFull, "assets/entities");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_GAME)
			str_copy(aBufFull, "assets/game");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EMOTICONS)
			str_copy(aBufFull, "assets/emoticons");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_PARTICLES)
			str_copy(aBufFull, "assets/particles");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_HUD)
			str_copy(aBufFull, "assets/hud");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_EXTRAS)
			str_copy(aBufFull, "assets/extras");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
			str_copy(aBufFull, "assets/audio");
		else if(gs_CurCustomTab == CMenus::ASSETS_TAB_CURSORS)
			str_copy(aBufFull, "assets/cursors");
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, aBufFull, aBuf, sizeof(aBuf));
		if(gs_CurCustomTab == CMenus::ASSETS_TAB_AUDIO)
		{
			Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
			Storage()->CreateFolder("assets/audio", IStorage::TYPE_SAVE);
		}
		else
		{
			Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
			Storage()->CreateFolder(aBufFull, IStorage::TYPE_SAVE);
		}
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_AssetsDirId, &DirectoryButton, Localize("Open the directory to add custom assets"));

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	static CButtonContainer s_AssetsReloadBtnId;
	if(DoButton_Menu(&s_AssetsReloadBtnId, FontIcon::ARROW_ROTATE_RIGHT, 0, &ReloadButton) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		ClearCustomItems(gs_CurCustomTab);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	EndPageTransition();
}

void CMenus::ConchainAssetsEntities(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetsEntities) != 0)
		{
			pThis->GameClient()->m_MapImages.ChangeEntitiesPath(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetGame(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetGame) != 0)
		{
			pThis->GameClient()->LoadGameSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetParticles(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetParticles) != 0)
		{
			pThis->GameClient()->LoadParticlesSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetEmoticons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetEmoticons) != 0)
		{
			pThis->GameClient()->LoadEmoticonsSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetHud) != 0)
		{
			pThis->GameClient()->LoadHudSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetExtras(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetExtras) != 0)
		{
			pThis->GameClient()->LoadExtrasSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetAudio(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetAudio) != 0)
		{
			pThis->GameClient()->m_Sounds.ReloadSamples();
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetCursor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetCursor) != 0)
		{
			pThis->GameClient()->m_CatClient.LoadCursorAsset(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}
