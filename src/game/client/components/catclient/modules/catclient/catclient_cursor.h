#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_CURSOR_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_CATCLIENT_CATCLIENT_CURSOR_H

bool CCatClient::LoadAutomaticCursorAsset()
{
	if(!Storage()->FileExists(CATCLIENT_AUTO_CURSOR_PATH, IStorage::TYPE_SAVE))
	{
		return false;
	}

	m_CursorTexture = Graphics()->LoadTexture(CATCLIENT_AUTO_CURSOR_PATH, IStorage::TYPE_SAVE);
	if(m_CursorTexture.IsNullTexture())
	{
		return false;
	}

	m_HasCustomCursor = true;
	return true;
}

void CCatClient::StartAutomaticCursorDownload()
{
	if(m_pCursorDownloadTask)
	{
		return;
	}

	m_pCursorDownloadTask = HttpGetBoth(CATCLIENT_AUTO_CURSOR_URL, Storage(), CATCLIENT_AUTO_CURSOR_PATH, IStorage::TYPE_SAVE);
	m_pCursorDownloadTask->Timeout(AUTO_CURSOR_REQUEST_TIMEOUT);
	m_pCursorDownloadTask->IpResolve(IPRESOLVE::V4);
	m_pCursorDownloadTask->MaxResponseSize(256 * 1024);
	m_pCursorDownloadTask->LogProgress(HTTPLOG::NONE);
	m_pCursorDownloadTask->FailOnErrorStatus(false);
	m_pCursorDownloadTask->SkipByFileTime(false);
	Http()->Run(m_pCursorDownloadTask);
}

void CCatClient::FinishAutomaticCursorDownload()
{
	if(!m_pCursorDownloadTask || m_pCursorDownloadTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pCursorDownloadTask->StatusCode() < 400 && str_comp(g_Config.m_ClAssetCursor, "default") == 0)
	{
		LoadCursorAsset(g_Config.m_ClAssetCursor);
	}
}

void CCatClient::LoadCursorAsset(const char *pPath)
{
	if(m_HasCustomCursor)
	{
		Graphics()->UnloadTexture(&m_CursorTexture);
		m_HasCustomCursor = false;
	}

	if(str_comp(pPath, "default") == 0)
	{
		LoadAutomaticCursorAsset();
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
