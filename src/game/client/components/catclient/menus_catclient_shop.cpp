#include "menus_catclient_common.h"

#include <base/math.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

static constexpr const char *CATCLIENT_SHOP_HOST = "https://data.teeworlds.xyz";
static constexpr const char *CATCLIENT_SHOP_API_URL = "https://data.teeworlds.xyz/api/skins?page=%d&limit=100&type=%s";
static constexpr CTimeout CATCLIENT_SHOP_TIMEOUT{8000, 0, 1024, 8};
static constexpr int64_t CATCLIENT_SHOP_PAGE_MAX_RESPONSE_SIZE = 2 * 1024 * 1024;
static constexpr int64_t CATCLIENT_SHOP_IMAGE_MAX_RESPONSE_SIZE = 32 * 1024 * 1024;
static constexpr int64_t CATCLIENT_SHOP_ARCHIVE_MAX_RESPONSE_SIZE = 128 * 1024 * 1024;
static constexpr int CATCLIENT_SHOP_PREVIEW_BACKGROUND_COLOR_DEFAULT = 0;

enum
{
	CATCLIENT_SHOP_ENTITIES = 0,
	CATCLIENT_SHOP_GAME,
	CATCLIENT_SHOP_EMOTICONS,
	CATCLIENT_SHOP_PARTICLES,
	CATCLIENT_SHOP_HUD,
	CATCLIENT_SHOP_AUDIO,
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
	int m_AssetsTab;
	bool m_IsArchive;
};

static const SCatClientShopTypeInfo gs_aCatClientShopTypeInfos[NUM_CATCLIENT_SHOP_TABS] = {
	{"Entities", "entity", "assets/entities", FontIcon::IMAGE, CMenus::ASSETS_TAB_ENTITIES, false},
	{"Game", "gameskin", "assets/game", FontIcon::IMAGE, CMenus::ASSETS_TAB_GAME, false},
	{"Emoticons", "emoticon", "assets/emoticons", FontIcon::IMAGE, CMenus::ASSETS_TAB_EMOTICONS, false},
	{"Particles", "particle", "assets/particles", FontIcon::IMAGE, CMenus::ASSETS_TAB_PARTICLES, false},
	{"HUD", "hud", "assets/hud", FontIcon::IMAGE, CMenus::ASSETS_TAB_HUD, false},
	{"Audio", "audio", "assets/audio", FontIcon::MUSIC, CMenus::ASSETS_TAB_AUDIO, true},
	{"Cursors", "cursor", "assets/cursors", FontIcon::IMAGE, CMenus::ASSETS_TAB_CURSORS, false},
};

struct SCatClientShopItem
{
	char m_aId[64]{};
	char m_aName[128]{};
	char m_aFilename[128]{};
	char m_aUsername[64]{};
	char m_aImageUrl[256]{};
	char m_aDownloadUrl[256]{};
	char m_aFileUrl[256]{};
	int m_Downloads = 0;
	int m_Likes = 0;
	int m_Dislikes = 0;
	bool m_PreviewFailed = false;
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
	if(gs_aCatClientShopTypeInfos[Tab].m_IsArchive || Item.m_PreviewTexture.IsValid() || Item.m_aId[0] == '\0')
	{
		return Item.m_PreviewTexture.IsValid();
	}

	char aPreviewPath[IO_MAX_PATH_LENGTH];
	CatClientShopBuildPreviewPath(Tab, Item.m_aId, aPreviewPath, sizeof(aPreviewPath));
	if(!pMenus->MenuStorage()->FileExists(aPreviewPath, IStorage::TYPE_SAVE))
	{
		return false;
	}

	Item.m_PreviewTexture = pMenus->MenuGraphics()->LoadTexture(aPreviewPath, IStorage::TYPE_SAVE);
	return Item.m_PreviewTexture.IsValid() && !Item.m_PreviewTexture.IsNullTexture();
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

static const SCatClientShopTypeInfo &CatClientShopCurrentTypeInfo()
{
	return gs_aCatClientShopTypeInfos[gs_CatClientShopState.m_Tab];
}

static bool CatClientShopIsArchive(int Tab)
{
	return gs_aCatClientShopTypeInfos[Tab].m_IsArchive;
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

static void CatClientShopBuildAssetPath(int Tab, const char *pAssetName, char *pOutput, size_t OutputSize)
{
	if(CatClientShopIsArchive(Tab))
	{
		str_format(pOutput, OutputSize, "%s/%s", gs_aCatClientShopTypeInfos[Tab].m_pAssetDirectory, pAssetName);
	}
	else
	{
		str_format(pOutput, OutputSize, "%s/%s.png", gs_aCatClientShopTypeInfos[Tab].m_pAssetDirectory, pAssetName);
	}
}

static bool CatClientShopAssetExists(CMenus *pMenus, int Tab, const char *pAssetName)
{
	char aPath[IO_MAX_PATH_LENGTH];
	CatClientShopBuildAssetPath(Tab, pAssetName, aPath, sizeof(aPath));
	if(CatClientShopIsArchive(Tab))
	{
		return pMenus->MenuStorage()->FolderExists(aPath, IStorage::TYPE_ALL);
	}
	return pMenus->MenuStorage()->FileExists(aPath, IStorage::TYPE_ALL);
}

static bool CatClientShopAssetSelected(int Tab, const char *pAssetName)
{
	switch(Tab)
	{
	case CATCLIENT_SHOP_ENTITIES:
		return str_comp(g_Config.m_ClAssetsEntities, pAssetName) == 0;
	case CATCLIENT_SHOP_GAME:
		return str_comp(g_Config.m_ClAssetGame, pAssetName) == 0;
	case CATCLIENT_SHOP_EMOTICONS:
		return str_comp(g_Config.m_ClAssetEmoticons, pAssetName) == 0;
	case CATCLIENT_SHOP_PARTICLES:
		return str_comp(g_Config.m_ClAssetParticles, pAssetName) == 0;
	case CATCLIENT_SHOP_HUD:
		return str_comp(g_Config.m_ClAssetHud, pAssetName) == 0;
	case CATCLIENT_SHOP_AUDIO:
		return str_comp(g_Config.m_ClAssetAudio, pAssetName) == 0;
	case CATCLIENT_SHOP_CURSORS:
		return str_comp(g_Config.m_ClAssetCursor, pAssetName) == 0;
	default:
		return false;
	}
}

static void CatClientShopApplyAsset(CMenus *pMenus, int Tab, const char *pAssetName, bool RefreshAssetsList)
{
	switch(Tab)
	{
	case CATCLIENT_SHOP_ENTITIES:
		str_copy(g_Config.m_ClAssetsEntities, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_ENTITIES);
		}
		else
		{
			pMenus->MenuGameClient()->m_MapImages.ChangeEntitiesPath(g_Config.m_ClAssetsEntities);
		}
		break;
	case CATCLIENT_SHOP_GAME:
		str_copy(g_Config.m_ClAssetGame, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_GAME);
		}
		else
		{
			pMenus->MenuGameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
		}
		break;
	case CATCLIENT_SHOP_EMOTICONS:
		str_copy(g_Config.m_ClAssetEmoticons, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_EMOTICONS);
		}
		else
		{
			pMenus->MenuGameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
		}
		break;
	case CATCLIENT_SHOP_PARTICLES:
		str_copy(g_Config.m_ClAssetParticles, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_PARTICLES);
		}
		else
		{
			pMenus->MenuGameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
		}
		break;
	case CATCLIENT_SHOP_HUD:
		str_copy(g_Config.m_ClAssetHud, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_HUD);
		}
		else
		{
			pMenus->MenuGameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
		}
		break;
	case CATCLIENT_SHOP_AUDIO:
		str_copy(g_Config.m_ClAssetAudio, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_AUDIO);
		}
		else
		{
			pMenus->MenuGameClient()->m_Sounds.ReloadSamples();
		}
		break;
	case CATCLIENT_SHOP_CURSORS:
		str_copy(g_Config.m_ClAssetCursor, pAssetName);
		if(RefreshAssetsList)
		{
			pMenus->RefreshCustomAssetsTab(CMenus::ASSETS_TAB_CURSORS);
		}
		else
		{
			pMenus->MenuGameClient()->m_CatClient.LoadCursorAsset(g_Config.m_ClAssetCursor);
		}
		break;
	}
}

static void CatClientShopOpenAssetDirectory(CMenus *pMenus, int Tab)
{
	char aRelativePath[IO_MAX_PATH_LENGTH];
	str_copy(aRelativePath, gs_aCatClientShopTypeInfos[Tab].m_pAssetDirectory, sizeof(aRelativePath));

	pMenus->MenuStorage()->CreateFolder("assets", IStorage::TYPE_SAVE);
	if(str_comp(aRelativePath, "assets/audio") == 0)
	{
		pMenus->MenuStorage()->CreateFolder("assets/audio", IStorage::TYPE_SAVE);
	}
	else
	{
		pMenus->MenuStorage()->CreateFolder(aRelativePath, IStorage::TYPE_SAVE);
	}

	char aAbsolutePath[IO_MAX_PATH_LENGTH];
	pMenus->MenuStorage()->GetCompletePath(IStorage::TYPE_SAVE, aRelativePath, aAbsolutePath, sizeof(aAbsolutePath));
	pMenus->MenuClient()->ViewFile(aAbsolutePath);
}

static bool CatClientShopWriteFile(CMenus *pMenus, const char *pRelativePath, const void *pData, size_t DataSize)
{
	char aAbsolutePath[IO_MAX_PATH_LENGTH];
	pMenus->MenuStorage()->GetCompletePath(IStorage::TYPE_SAVE, pRelativePath, aAbsolutePath, sizeof(aAbsolutePath));
	if(fs_makedir_rec_for(aAbsolutePath) != 0)
	{
		return false;
	}

	IOHANDLE File = pMenus->MenuStorage()->OpenFile(pRelativePath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(File == nullptr)
	{
		return false;
	}

	const bool Success = io_write(File, pData, DataSize) == DataSize && io_error(File) == 0 && io_close(File) == 0;
	return Success;
}

static bool CatClientShopIsPngBuffer(const unsigned char *pData, size_t DataSize)
{
	static const unsigned char s_aSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	return DataSize >= sizeof(s_aSignature) && mem_comp(pData, s_aSignature, sizeof(s_aSignature)) == 0;
}

static bool CatClientShopIsZipBuffer(const unsigned char *pData, size_t DataSize)
{
	if(DataSize < 4)
	{
		return false;
	}

	return pData[0] == 'P' && pData[1] == 'K' &&
	       ((pData[2] == 3 && pData[3] == 4) || (pData[2] == 5 && pData[3] == 6) || (pData[2] == 7 && pData[3] == 8));
}

static uint16_t CatClientShopReadLe16(const unsigned char *pData)
{
	return (uint16_t)pData[0] | ((uint16_t)pData[1] << 8);
}

static uint32_t CatClientShopReadLe32(const unsigned char *pData)
{
	return (uint32_t)pData[0] | ((uint32_t)pData[1] << 8) | ((uint32_t)pData[2] << 16) | ((uint32_t)pData[3] << 24);
}

struct SCatClientZipEntry
{
	std::string m_Name;
	uint16_t m_CompressionMethod = 0;
	uint32_t m_CompressedSize = 0;
	uint32_t m_UncompressedSize = 0;
	uint32_t m_LocalHeaderOffset = 0;
	bool m_IsDirectory = false;
};

static bool CatClientShopSanitizeZipPath(const std::string &Path)
{
	if(Path.empty() || Path[0] == '/' || Path[0] == '\\')
	{
		return false;
	}

	if(Path.size() >= 2 && Path[1] == ':')
	{
		return false;
	}

	if(Path.find("..") != std::string::npos)
	{
		return false;
	}

	return true;
}

static bool CatClientShopInflateRaw(const unsigned char *pCompressedData, size_t CompressedSize, std::vector<unsigned char> &vOutput, size_t UncompressedSize)
{
	vOutput.resize(UncompressedSize);

	z_stream Stream{};
	Stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(pCompressedData));
	Stream.avail_in = CompressedSize;
	Stream.next_out = reinterpret_cast<Bytef *>(vOutput.data());
	Stream.avail_out = vOutput.size();

	if(inflateInit2(&Stream, -MAX_WBITS) != Z_OK)
	{
		return false;
	}

	const int Result = inflate(&Stream, Z_FINISH);
	inflateEnd(&Stream);
	return Result == Z_STREAM_END && Stream.total_out == UncompressedSize;
}

static bool CatClientShopExtractAudioArchive(CMenus *pMenus, const unsigned char *pData, size_t DataSize, const char *pAssetName)
{
	if(DataSize < 22)
	{
		return false;
	}

	size_t EndRecordOffset = DataSize;
	const size_t SearchStart = DataSize > 0x10000 + 22 ? DataSize - (0x10000 + 22) : 0;
	for(size_t Index = DataSize - 22 + 1; Index-- > SearchStart;)
	{
		if(pData[Index] == 'P' && pData[Index + 1] == 'K' && pData[Index + 2] == 5 && pData[Index + 3] == 6)
		{
			EndRecordOffset = Index;
			break;
		}
	}

	if(EndRecordOffset == DataSize)
	{
		return false;
	}

	const unsigned char *pEndRecord = pData + EndRecordOffset;
	const uint16_t EntriesInDirectory = CatClientShopReadLe16(pEndRecord + 10);
	const uint32_t CentralDirectorySize = CatClientShopReadLe32(pEndRecord + 12);
	const uint32_t CentralDirectoryOffset = CatClientShopReadLe32(pEndRecord + 16);

	if(CentralDirectoryOffset > DataSize || CentralDirectorySize > DataSize - CentralDirectoryOffset)
	{
		return false;
	}

	std::vector<SCatClientZipEntry> vEntries;
	vEntries.reserve(EntriesInDirectory);

	size_t DirectoryOffset = CentralDirectoryOffset;
	for(uint16_t EntryIndex = 0; EntryIndex < EntriesInDirectory; ++EntryIndex)
	{
		if(DirectoryOffset + 46 > DataSize || CatClientShopReadLe32(pData + DirectoryOffset) != 0x02014b50)
		{
			return false;
		}

		const uint16_t CompressionMethod = CatClientShopReadLe16(pData + DirectoryOffset + 10);
		const uint32_t CompressedSize = CatClientShopReadLe32(pData + DirectoryOffset + 20);
		const uint32_t UncompressedSize = CatClientShopReadLe32(pData + DirectoryOffset + 24);
		const uint16_t FilenameLength = CatClientShopReadLe16(pData + DirectoryOffset + 28);
		const uint16_t ExtraLength = CatClientShopReadLe16(pData + DirectoryOffset + 30);
		const uint16_t CommentLength = CatClientShopReadLe16(pData + DirectoryOffset + 32);
		const uint32_t LocalHeaderOffset = CatClientShopReadLe32(pData + DirectoryOffset + 42);

		const size_t FilenameOffset = DirectoryOffset + 46;
		const size_t NextOffset = FilenameOffset + FilenameLength + ExtraLength + CommentLength;
		if(FilenameOffset + FilenameLength > DataSize || NextOffset > DataSize)
		{
			return false;
		}

		std::string Name(reinterpret_cast<const char *>(pData + FilenameOffset), FilenameLength);
		std::replace(Name.begin(), Name.end(), '\\', '/');
		while(!Name.empty() && Name.back() == '/')
		{
			Name.pop_back();
		}

		if(Name.empty() || !CatClientShopSanitizeZipPath(Name))
		{
			DirectoryOffset = NextOffset;
			continue;
		}

		SCatClientZipEntry Entry;
		Entry.m_Name = Name;
		Entry.m_CompressionMethod = CompressionMethod;
		Entry.m_CompressedSize = CompressedSize;
		Entry.m_UncompressedSize = UncompressedSize;
		Entry.m_LocalHeaderOffset = LocalHeaderOffset;
		Entry.m_IsDirectory = FilenameLength > 0 && (reinterpret_cast<const char *>(pData + FilenameOffset))[FilenameLength - 1] == '/';
		vEntries.push_back(std::move(Entry));

		DirectoryOffset = NextOffset;
	}

	if(vEntries.empty())
	{
		return false;
	}

	std::string CommonRoot;
	bool HasCommonRoot = true;
	for(const SCatClientZipEntry &Entry : vEntries)
	{
		const size_t Slash = Entry.m_Name.find('/');
		if(Slash == std::string::npos)
		{
			HasCommonRoot = false;
			break;
		}

		const std::string Root = Entry.m_Name.substr(0, Slash);
		if(CommonRoot.empty())
		{
			CommonRoot = Root;
		}
		else if(CommonRoot != Root)
		{
			HasCommonRoot = false;
			break;
		}
	}

	char aBasePath[IO_MAX_PATH_LENGTH];
	str_format(aBasePath, sizeof(aBasePath), "%s/%s", gs_aCatClientShopTypeInfos[CATCLIENT_SHOP_AUDIO].m_pAssetDirectory, pAssetName);
	pMenus->MenuStorage()->CreateFolder("assets", IStorage::TYPE_SAVE);
	pMenus->MenuStorage()->CreateFolder("assets/audio", IStorage::TYPE_SAVE);
	pMenus->MenuStorage()->CreateFolder(aBasePath, IStorage::TYPE_SAVE);

	int FilesWritten = 0;
	for(const SCatClientZipEntry &Entry : vEntries)
	{
		if(Entry.m_IsDirectory)
		{
			continue;
		}

		std::string RelativeName = Entry.m_Name;
		if(HasCommonRoot)
		{
			const size_t StripLength = CommonRoot.size() + 1;
			if(RelativeName.size() <= StripLength)
			{
				continue;
			}
			RelativeName = RelativeName.substr(StripLength);
		}

		if(RelativeName.empty() || !CatClientShopSanitizeZipPath(RelativeName))
		{
			continue;
		}

		if(Entry.m_LocalHeaderOffset + 30 > DataSize || CatClientShopReadLe32(pData + Entry.m_LocalHeaderOffset) != 0x04034b50)
		{
			return false;
		}

		const uint16_t LocalFilenameLength = CatClientShopReadLe16(pData + Entry.m_LocalHeaderOffset + 26);
		const uint16_t LocalExtraLength = CatClientShopReadLe16(pData + Entry.m_LocalHeaderOffset + 28);
		const size_t DataOffset = Entry.m_LocalHeaderOffset + 30 + LocalFilenameLength + LocalExtraLength;
		if(DataOffset > DataSize || Entry.m_CompressedSize > DataSize - DataOffset)
		{
			return false;
		}

		std::vector<unsigned char> vOutput;
		if(Entry.m_CompressionMethod == 0)
		{
			vOutput.assign(pData + DataOffset, pData + DataOffset + Entry.m_CompressedSize);
			if(vOutput.size() != Entry.m_UncompressedSize)
			{
				return false;
			}
		}
		else if(Entry.m_CompressionMethod == 8)
		{
			if(!CatClientShopInflateRaw(pData + DataOffset, Entry.m_CompressedSize, vOutput, Entry.m_UncompressedSize))
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		char aRelativePath[IO_MAX_PATH_LENGTH];
		str_format(aRelativePath, sizeof(aRelativePath), "%s/%s", aBasePath, RelativeName.c_str());
		if(!CatClientShopWriteFile(pMenus, aRelativePath, vOutput.data(), vOutput.size()))
		{
			return false;
		}
		++FilesWritten;
	}

	return FilesWritten > 0;
}

static void CatClientShopBuildInstallUrls(const SCatClientShopItem &Item, int Tab, std::vector<std::string> &vUrls)
{
	vUrls.clear();

	auto AddUrl = [&vUrls](const char *pUrl) {
		if(pUrl == nullptr || pUrl[0] == '\0')
		{
			return;
		}

		char aResolvedUrl[256];
		CatClientShopResolveUrl(pUrl, aResolvedUrl, sizeof(aResolvedUrl));
		if(aResolvedUrl[0] == '\0')
		{
			return;
		}

		const std::string Url = aResolvedUrl;
		if(std::find(vUrls.begin(), vUrls.end(), Url) == vUrls.end())
		{
			vUrls.push_back(Url);
		}
	};

	if(CatClientShopIsArchive(Tab))
	{
		AddUrl(Item.m_aFileUrl);
		AddUrl(Item.m_aDownloadUrl);

		char aUrl[256];
		str_format(aUrl, sizeof(aUrl), "%s/api/skins/%s?download=true", CATCLIENT_SHOP_HOST, Item.m_aId);
		AddUrl(aUrl);
		str_format(aUrl, sizeof(aUrl), "%s/api/skins/%s?file=true", CATCLIENT_SHOP_HOST, Item.m_aId);
		AddUrl(aUrl);
		str_format(aUrl, sizeof(aUrl), "%s/api/skins/%s?archive=true", CATCLIENT_SHOP_HOST, Item.m_aId);
		AddUrl(aUrl);
	}
	else
	{
		AddUrl(Item.m_aImageUrl);
		if(vUrls.empty())
		{
			char aUrl[256];
			str_format(aUrl, sizeof(aUrl), "%s/api/skins/%s?image=true", CATCLIENT_SHOP_HOST, Item.m_aId);
			AddUrl(aUrl);
		}
	}
}

static void CatClientShopStartFetch(CMenus *pMenus)
{
	const int Page = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), CATCLIENT_SHOP_API_URL, Page, CatClientShopCurrentTypeInfo().m_pApiType);

	gs_CatClientShopState.m_pFetchTask = HttpGet(aUrl);
	gs_CatClientShopState.m_pFetchTask->Timeout(CATCLIENT_SHOP_TIMEOUT);
	gs_CatClientShopState.m_pFetchTask->IpResolve(IPRESOLVE::V4);
	gs_CatClientShopState.m_pFetchTask->MaxResponseSize(CATCLIENT_SHOP_PAGE_MAX_RESPONSE_SIZE);
	gs_CatClientShopState.m_pFetchTask->LogProgress(HTTPLOG::NONE);
	gs_CatClientShopState.m_pFetchTask->FailOnErrorStatus(false);
	gs_CatClientShopState.m_FetchTab = gs_CatClientShopState.m_Tab;
	gs_CatClientShopState.m_FetchPage = Page;
	CatClientShopSetStatus(Localize("Loading shop..."));
	pMenus->MenuHttp()->Run(gs_CatClientShopState.m_pFetchTask);
}

static void CatClientShopEnsureFetch(CMenus *pMenus)
{
	if(gs_CatClientShopState.m_pFetchTask)
	{
		return;
	}

	const int Page = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	if(gs_CatClientShopState.m_LoadedTab != gs_CatClientShopState.m_Tab || gs_CatClientShopState.m_LoadedPage != Page)
	{
		CatClientShopStartFetch(pMenus);
	}
}

static void CatClientShopStartPreviewFetch(CMenus *pMenus)
{
	if(CatClientShopIsArchive(gs_CatClientShopState.m_Tab) || gs_CatClientShopState.m_pPreviewTask != nullptr)
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

	if(gs_CatClientShopState.m_pFetchTask->StatusCode() >= 400)
	{
		str_format(gs_CatClientShopState.m_aStatus, sizeof(gs_CatClientShopState.m_aStatus), "%s (%d)", Localize("Shop request failed"), gs_CatClientShopState.m_pFetchTask->StatusCode());
		return;
	}

	json_value *pJson = gs_CatClientShopState.m_pFetchTask->ResultJson();
	if(pJson == nullptr || pJson->type != json_object)
	{
		if(pJson != nullptr)
		{
			json_value_free(pJson);
		}
		CatClientShopSetStatus(Localize("Shop response is invalid"));
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
			if(const char *pDownloadUrl = json_string_get(json_object_get(pSkin, "downloadUrl")); pDownloadUrl != nullptr)
			{
				str_copy(Item.m_aDownloadUrl, pDownloadUrl, sizeof(Item.m_aDownloadUrl));
			}
			if(const char *pFileUrl = json_string_get(json_object_get(pSkin, "fileUrl")); pFileUrl != nullptr)
			{
				str_copy(Item.m_aFileUrl, pFileUrl, sizeof(Item.m_aFileUrl));
			}
			else if(const char *pArchiveUrl = json_string_get(json_object_get(pSkin, "archiveUrl")); pArchiveUrl != nullptr)
			{
				str_copy(Item.m_aFileUrl, pArchiveUrl, sizeof(Item.m_aFileUrl));
			}
			else if(const char *pUrl = json_string_get(json_object_get(pSkin, "url")); pUrl != nullptr)
			{
				str_copy(Item.m_aFileUrl, pUrl, sizeof(Item.m_aFileUrl));
			}

			const json_value *pDownloads = json_object_get(pSkin, "downloads");
			if(pDownloads != &json_value_none && pDownloads->type == json_integer)
			{
				Item.m_Downloads = json_int_get(pDownloads);
			}

			const json_value *pLikes = json_object_get(pSkin, "likes_count");
			if(pLikes != &json_value_none && pLikes->type == json_integer)
			{
				Item.m_Likes = json_int_get(pLikes);
			}

			const json_value *pDislikes = json_object_get(pSkin, "dislikes_count");
			if(pDislikes != &json_value_none && pDislikes->type == json_integer)
			{
				Item.m_Dislikes = json_int_get(pDislikes);
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
		CatClientShopSetStatus(Localize("No assets found on this page"));
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

	if(CatClientShopIsArchive(gs_CatClientShopState.m_InstallTab))
	{
		if(!CatClientShopIsZipBuffer(pData, DataSize))
		{
			return false;
		}

		return CatClientShopExtractAudioArchive(pMenus, pData, DataSize, gs_CatClientShopState.m_aInstallAssetName);
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
		CatClientShopSetStatus(Localize("Failed to build a download URL"));
		return;
	}

	const std::string &Url = gs_CatClientShopState.m_vInstallUrls[gs_CatClientShopState.m_InstallUrlIndex];
	gs_CatClientShopState.m_pInstallTask = HttpGet(Url.c_str());
	gs_CatClientShopState.m_pInstallTask->Timeout(CATCLIENT_SHOP_TIMEOUT);
	gs_CatClientShopState.m_pInstallTask->IpResolve(IPRESOLVE::V4);
	gs_CatClientShopState.m_pInstallTask->MaxResponseSize(CatClientShopIsArchive(gs_CatClientShopState.m_InstallTab) ? CATCLIENT_SHOP_ARCHIVE_MAX_RESPONSE_SIZE : CATCLIENT_SHOP_IMAGE_MAX_RESPONSE_SIZE);
	gs_CatClientShopState.m_pInstallTask->LogProgress(HTTPLOG::NONE);
	gs_CatClientShopState.m_pInstallTask->FailOnErrorStatus(false);

	char aMessage[256];
	str_format(aMessage, sizeof(aMessage), "%s: %s", Localize("Installing"), gs_CatClientShopState.m_aInstallAssetName);
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
		str_format(aMessage, sizeof(aMessage), "%s: %s", Localize("Unable to install asset"), gs_CatClientShopState.m_aInstallAssetName);
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

	CatClientShopApplyAsset(pMenus, gs_CatClientShopState.m_InstallTab, gs_CatClientShopState.m_aInstallAssetName, true);
	char aMessage[256];
	str_format(aMessage, sizeof(aMessage), "%s: %s", Localize("Installed"), gs_CatClientShopState.m_aInstallAssetName);
	CatClientShopSetStatus(aMessage);

	CatClientShopAbortTask(gs_CatClientShopState.m_pInstallTask);
	gs_CatClientShopState.m_vInstallUrls.clear();
	gs_CatClientShopState.m_InstallUrlIndex = 0;
}

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
	CatClientMenuConstrainWidth(Overlay, Panel, 860.0f);
	const float VerticalMargin = Overlay.h > 620.0f ? (Overlay.h - 620.0f) / 2.0f : 24.0f;
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
	HeaderRow.VSplitRight(110.0f, &TitleLabel, &CloseButton);
	pMenus->MenuUi()->DoLabel(&TitleLabel, pItem->m_aName, CATCLIENT_MENU_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	if(pMenus->DoButton_Menu(&gs_CatClientShopState.m_PreviewCloseButton, Localize("Close"), 0, &CloseButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.30f)))
	{
		CatClientShopCloseTexturePreview();
		return;
	}

	pMenus->MenuUi()->DoScrollbarOption(&gs_CatClientShopState.m_PreviewBackgroundColor, &gs_CatClientShopState.m_PreviewBackgroundColor, &SliderRow, Localize("Background color"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

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
	PreviewArea.Margin(CATCLIENT_MENU_MARGIN, &TextureRect);
	pMenus->MenuGraphics()->WrapClamp();
	pMenus->MenuGraphics()->TextureSet(pItem->m_PreviewTexture);
	pMenus->MenuGraphics()->QuadsBegin();
	pMenus->MenuGraphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(TextureRect.x, TextureRect.y, TextureRect.w, TextureRect.h);
	pMenus->MenuGraphics()->QuadsDrawTL(&QuadItem, 1);
	pMenus->MenuGraphics()->QuadsEnd();
	pMenus->MenuGraphics()->WrapNormal();
}

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
	MainView.HSplitTop(CATCLIENT_MENU_LINE_SIZE, &ControlsRow, &MainView);
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
		if(DoButton_MenuTab(&s_aShopTabs[Tab], Localize(gs_aCatClientShopTypeInfos[Tab].m_pLabel), gs_CatClientShopState.m_Tab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			CatClientShopSetTab(this, Tab);
		}
	}

	CUIRect PrevButton, PageLabel, NextButton, RefreshButton;
	ControlsRow.VSplitLeft(90.0f, &PrevButton, &ControlsRow);
	ControlsRow.VSplitLeft(150.0f, &PageLabel, &ControlsRow);
	ControlsRow.VSplitLeft(90.0f, &NextButton, &ControlsRow);
	ControlsRow.VSplitLeft(CATCLIENT_MENU_MARGIN_SMALL, nullptr, &ControlsRow);
	ControlsRow.VSplitLeft(120.0f, &RefreshButton, &ControlsRow);

	static CButtonContainer s_PrevButton;
	static CButtonContainer s_NextButton;
	static CButtonContainer s_RefreshButton;
	const int CurrentPage = gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab];
	if(DoButton_Menu(&s_PrevButton, Localize("Previous"), 0, &PrevButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, CurrentPage > 1 ? 0.25f : 0.15f)) && CurrentPage > 1)
	{
		CatClientShopSetPage(this, CurrentPage - 1);
	}

	char aPageLabel[128];
	str_format(aPageLabel, sizeof(aPageLabel), "%s %d / %d", Localize("Page"), CurrentPage, maximum(1, gs_CatClientShopState.m_TotalPages));
	Ui()->DoLabel(&PageLabel, aPageLabel, CATCLIENT_MENU_FONT_SIZE, TEXTALIGN_MC);

	if(DoButton_Menu(&s_NextButton, Localize("Next"), 0, &NextButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, CurrentPage < gs_CatClientShopState.m_TotalPages ? 0.25f : 0.15f)) && CurrentPage < gs_CatClientShopState.m_TotalPages)
	{
		CatClientShopSetPage(this, CurrentPage + 1);
	}

	if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		CatClientShopRefreshCurrentPage(this);
	}

	if(gs_CatClientShopState.m_LoadedTab != gs_CatClientShopState.m_Tab || gs_CatClientShopState.m_LoadedPage != gs_CatClientShopState.m_aPages[gs_CatClientShopState.m_Tab])
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
	else if(CatClientShopIsArchive(gs_CatClientShopState.m_Tab))
	{
		str_format(aStatusText, sizeof(aStatusText), "%s: %d   %s", Localize("Items"), gs_CatClientShopState.m_TotalItems, Localize("Audio assets install from archive and use an icon preview"));
	}
	else
	{
		str_format(aStatusText, sizeof(aStatusText), "%s: %d", Localize("Items"), gs_CatClientShopState.m_TotalItems);
	}
	Ui()->DoLabel(&StatusRow, aStatusText, CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML);

	static CListBox s_ListBox;
	const int NumItems = gs_CatClientShopState.m_vItems.size();
	s_ListBox.DoStart(72.0f, NumItems, 1, 7, gs_CatClientShopState.m_SelectedIndex, &ListView, true);

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

		CUIRect IconRect, TextRect, ButtonRect;
		Row.VSplitLeft(62.0f, &IconRect, &TextRect);
		TextRect.VSplitRight(88.0f, &TextRect, &ButtonRect);
		TextRect.VSplitRight(CATCLIENT_MENU_MARGIN, &TextRect, nullptr);

		const bool CanOpenPreview = !CatClientShopIsArchive(gs_CatClientShopState.m_Tab) && Item.m_PreviewTexture.IsValid() && !Item.m_PreviewTexture.IsNullTexture();
		const int IconButtonResult = Ui()->DoButtonLogic(&Item.m_PreviewButton, 0, &IconRect, BUTTONFLAG_LEFT);
		if(IconButtonResult)
		{
			if(CanOpenPreview)
			{
				gs_CatClientShopState.m_PreviewOpen = true;
				str_copy(gs_CatClientShopState.m_aOpenPreviewItemId, Item.m_aId, sizeof(gs_CatClientShopState.m_aOpenPreviewItemId));
			}
			else if(!CatClientShopIsArchive(gs_CatClientShopState.m_Tab))
			{
				CatClientShopSetStatus(Item.m_PreviewFailed ? Localize("Preview unavailable") : Localize("Preview is still loading"));
			}
		}

		IconRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_ALL, 8.0f);
		CUIRect PreviewRect;
		IconRect.Margin(5.0f, &PreviewRect);
		if(CanOpenPreview)
		{
			Graphics()->WrapClamp();
			Graphics()->TextureSet(Item.m_PreviewTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(PreviewRect.x, PreviewRect.y, PreviewRect.w, PreviewRect.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
		else
		{
			RenderFontIcon(IconRect, gs_aCatClientShopTypeInfos[gs_CatClientShopState.m_Tab].m_pIcon, gs_CatClientShopState.m_Tab == CATCLIENT_SHOP_AUDIO ? 24.0f : 22.0f, TEXTALIGN_MC);
		}
		if(CanOpenPreview && Ui()->HotItem() == &Item.m_PreviewButton)
		{
			IconRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f), IGraphics::CORNER_ALL, 8.0f);
		}

		CUIRect NameLabel, MetaLabel, ExtraLabel;
		TextRect.HSplitTop(22.0f, &NameLabel, &TextRect);
		TextRect.HSplitTop(18.0f, &MetaLabel, &TextRect);
		TextRect.HSplitTop(18.0f, &ExtraLabel, &TextRect);

		Ui()->DoLabel(&NameLabel, Item.m_aName, 15.0f, TEXTALIGN_ML);

		char aAuthorLabel[160];
		if(Item.m_aUsername[0] != '\0')
		{
			str_format(aAuthorLabel, sizeof(aAuthorLabel), "%s: %s", Localize("Author"), Item.m_aUsername);
		}
		else
		{
			str_copy(aAuthorLabel, Localize("Unknown author"), sizeof(aAuthorLabel));
		}
		Ui()->DoLabel(&MetaLabel, aAuthorLabel, 13.0f, TEXTALIGN_ML);

		char aExtraLabel[256];
		str_format(aExtraLabel, sizeof(aExtraLabel), "%s: %d   %s: %d", Localize("Downloads"), Item.m_Downloads, Localize("Likes"), Item.m_Likes);
		Ui()->DoLabel(&ExtraLabel, aExtraLabel, 13.0f, TEXTALIGN_ML);

		char aAssetName[128];
		CatClientShopNormalizeAssetName(Item.m_aName, Item.m_aFilename, aAssetName, sizeof(aAssetName));
		const bool Installed = CatClientShopAssetExists(this, gs_CatClientShopState.m_Tab, aAssetName);
		const bool Selected = CatClientShopAssetSelected(gs_CatClientShopState.m_Tab, aAssetName);
		const bool InstallingThisItem = gs_CatClientShopState.m_pInstallTask != nullptr && str_comp(gs_CatClientShopState.m_aInstallItemId, Item.m_aId) == 0;

		const char *pButtonLabel = Localize("Install");
		if(InstallingThisItem)
		{
			pButtonLabel = Localize("Installing...");
		}
		else if(Selected)
		{
			pButtonLabel = Localize("Applied");
		}
		else if(Installed)
		{
			pButtonLabel = Localize("Apply");
		}

		if(DoButton_Menu(&Item.m_InstallButton, pButtonLabel, Selected, &ButtonRect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			if(!InstallingThisItem)
			{
				if(Installed)
				{
					CatClientShopApplyAsset(this, gs_CatClientShopState.m_Tab, aAssetName, false);
					char aMessage[256];
					str_format(aMessage, sizeof(aMessage), "%s: %s", Localize("Applied"), aAssetName);
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
	FooterRow.VSplitLeft(170.0f, &OpenFolderButton, &HintLabel);
	static CButtonContainer s_OpenFolderButton;
	if(DoButton_Menu(&s_OpenFolderButton, Localize("Assets directory"), 0, &OpenFolderButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		CatClientShopOpenAssetDirectory(this, gs_CatClientShopState.m_Tab);
	}

	char aHint[256];
	str_format(aHint, sizeof(aHint), "%s: %s", Localize("Target"), gs_aCatClientShopTypeInfos[gs_CatClientShopState.m_Tab].m_pAssetDirectory);
	Ui()->DoLabel(&HintLabel, aHint, CATCLIENT_MENU_SMALL_FONT_SIZE, TEXTALIGN_ML);

	if(CatClientShopHasTexturePreview())
	{
		CatClientShopRenderTexturePreview(this, FullView);
	}
}
