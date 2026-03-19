#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_ASSETS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_SHOP_ASSETS_H

static void CatClientShopBuildAssetPath(int Tab, const char *pAssetName, char *pOutput, size_t OutputSize)
{
	str_format(pOutput, OutputSize, "%s/%s.png", gs_aCatClientShopTypeInfos[Tab].m_pAssetDirectory, pAssetName);
}

static bool CatClientShopAssetExists(CMenus *pMenus, int Tab, const char *pAssetName)
{
	char aPath[IO_MAX_PATH_LENGTH];
	CatClientShopBuildAssetPath(Tab, pAssetName, aPath, sizeof(aPath));
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
	pMenus->MenuStorage()->CreateFolder(aRelativePath, IStorage::TYPE_SAVE);

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

static void CatClientShopBuildInstallUrls(const SCatClientShopItem &Item, int Tab, std::vector<std::string> &vUrls)
{
	(void)Tab;
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

	AddUrl(Item.m_aImageUrl);
	if(vUrls.empty())
	{
		char aUrl[256];
		str_format(aUrl, sizeof(aUrl), "%s/api/skins/%s?image=true", CATCLIENT_SHOP_HOST, Item.m_aId);
		AddUrl(aUrl);
	}
}

#endif
