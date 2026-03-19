#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_ICON_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_NAMETAGS_CATCLIENT_NAMETAGS_ICON_H

bool CCatClientNameTags::LoadCatIconFromFile()
{
	if(m_pGameClient == nullptr || !m_pGameClient->Storage()->FileExists(CATCLIENT_ICON_PATH, IStorage::TYPE_SAVE))
	{
		return false;
	}

	const IGraphics::CTextureHandle Texture = m_pGameClient->Graphics()->LoadTexture(CATCLIENT_ICON_PATH, IStorage::TYPE_SAVE);
	if(Texture.IsNullTexture())
	{
		return false;
	}

	if(m_HasCatIconTexture)
	{
		m_pGameClient->Graphics()->UnloadTexture(&m_CatIconTexture);
	}

	m_CatIconTexture = Texture;
	m_HasCatIconTexture = true;
	return true;
}

void CCatClientNameTags::StartIconDownload()
{
	if(m_pGameClient == nullptr || m_pIconTask || m_HasCatIconTexture)
	{
		return;
	}

	m_pIconTask = HttpGetBoth(CATCLIENT_ICON_URL, m_pGameClient->Storage(), CATCLIENT_ICON_PATH, IStorage::TYPE_SAVE);
	m_pIconTask->Timeout(REQUEST_TIMEOUT);
	m_pIconTask->IpResolve(IPRESOLVE::V4);
	m_pIconTask->MaxResponseSize(256 * 1024);
	m_pIconTask->LogProgress(HTTPLOG::NONE);
	m_pIconTask->FailOnErrorStatus(false);
	m_LastIconAttempt = time_get_nanoseconds();
	m_pGameClient->Http()->Run(m_pIconTask);
}

void CCatClientNameTags::FinishIconDownload()
{
	if(!m_pIconTask || m_pIconTask->State() != EHttpState::DONE)
	{
		return;
	}

	if(m_pIconTask->StatusCode() < 400)
	{
		LoadCatIconFromFile();
	}
}

void CCatClientNameTags::UnloadCatIcon()
{
	if(m_pGameClient != nullptr && m_HasCatIconTexture)
	{
		m_pGameClient->Graphics()->UnloadTexture(&m_CatIconTexture);
	}
	m_CatIconTexture = IGraphics::CTextureHandle();
	m_HasCatIconTexture = false;
}

#endif
