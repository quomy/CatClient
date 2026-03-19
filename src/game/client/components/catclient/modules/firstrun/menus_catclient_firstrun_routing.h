#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_FIRSTRUN_MENUS_CATCLIENT_FIRSTRUN_ROUTING_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MODULES_FIRSTRUN_MENUS_CATCLIENT_FIRSTRUN_ROUTING_H

void CMenus::UpdateFirstRunSetupRouting()
{
	m_FirstRunSetupStep = std::clamp(m_FirstRunSetupStep, 0, (int)NUM_FIRST_RUN_SETUP_STEPS - 1);

	switch((EFirstRunSetupStep)m_FirstRunSetupStep)
	{
	case FIRST_RUN_SETUP_UI_SCALE:
	case FIRST_RUN_SETUP_ASPECT_RATIO:
		g_Config.m_UiSettingsPage = SETTINGS_CATCLIENT;
		m_CatClientTab = CATCLIENT_TAB_VISUALS;
		break;
	case FIRST_RUN_SETUP_FAST_INPUTS:
		g_Config.m_UiSettingsPage = SETTINGS_CATCLIENT;
		m_CatClientTab = CATCLIENT_TAB_GENERAL;
		break;
	case FIRST_RUN_SETUP_CURSORS:
		g_Config.m_UiSettingsPage = SETTINGS_ASSETS;
		m_AssetsTab = ASSETS_TAB_CURSORS;
		break;
	case FIRST_RUN_SETUP_AUDIO:
		g_Config.m_UiSettingsPage = SETTINGS_ASSETS;
		m_AssetsTab = ASSETS_TAB_AUDIO;
		break;
	default:
		break;
	}
}

void CMenus::FinishFirstRunSetup(bool Skip)
{
	str_copy(g_Config.m_CcFirstRun, "false");
	m_FirstRunSetupStep = FIRST_RUN_SETUP_UI_SCALE;
	ResetFirstRunFocus();
	ConfigManager()->Save();

	if(!Skip)
	{
		g_Config.m_UiSettingsPage = SETTINGS_CATCLIENT;
		m_CatClientTab = CATCLIENT_TAB_GENERAL;
	}
}

#endif
