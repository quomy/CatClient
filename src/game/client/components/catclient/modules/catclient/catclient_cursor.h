#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_CURSOR_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_CURSOR_H

void CCatClient::LoadCursorAsset(const char *pPath)
{
	if(m_HasCustomCursor)
	{
		Graphics()->UnloadTexture(&m_CursorTexture);
		m_HasCustomCursor = false;
	}

	if(str_comp(pPath, "default") == 0)
	{
		return;
	}

	char aAssetPath[IO_MAX_PATH_LENGTH];
	str_format(aAssetPath, sizeof(aAssetPath), "assets/cursors/%s.png", pPath);
	m_CursorTexture = Graphics()->LoadTexture(aAssetPath, IStorage::TYPE_ALL);
	if(m_CursorTexture.IsNullTexture())
	{
		str_format(aAssetPath, sizeof(aAssetPath), "assets/cursors/%s/gui_cursor.png", pPath);
		m_CursorTexture = Graphics()->LoadTexture(aAssetPath, IStorage::TYPE_ALL);
	}

	if(!m_CursorTexture.IsNullTexture())
	{
		m_HasCustomCursor = true;
	}
}

const IGraphics::CTextureHandle &CCatClient::CursorTexture() const
{
	if(m_HasCustomCursor)
	{
		return m_CursorTexture;
	}
	return g_pData->m_aImages[IMAGE_CURSOR].m_Id;
}

#endif
