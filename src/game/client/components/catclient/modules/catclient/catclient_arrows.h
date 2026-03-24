#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_ARROWS_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_ARROWS_H

void CCatClient::LoadArrowAsset(const char *pPath)
{
	if(m_HasCustomArrow)
	{
		Graphics()->UnloadTexture(&m_ArrowTexture);
		m_HasCustomArrow = false;
	}

	if(str_comp(pPath, "default") == 0)
	{
		return;
	}

	char aAssetPath[IO_MAX_PATH_LENGTH];
	str_format(aAssetPath, sizeof(aAssetPath), "assets/arrows/%s.png", pPath);
	m_ArrowTexture = Graphics()->LoadTexture(aAssetPath, IStorage::TYPE_ALL);
	if(m_ArrowTexture.IsNullTexture())
	{
		str_format(aAssetPath, sizeof(aAssetPath), "assets/arrows/%s/arrow.png", pPath);
		m_ArrowTexture = Graphics()->LoadTexture(aAssetPath, IStorage::TYPE_ALL);
	}

	if(!m_ArrowTexture.IsNullTexture())
	{
		m_HasCustomArrow = true;
	}
}

const IGraphics::CTextureHandle &CCatClient::ArrowTexture() const
{
	if(m_HasCustomArrow)
	{
		return m_ArrowTexture;
	}
	return g_pData->m_aImages[IMAGE_ARROW].m_Id;
}

#endif
