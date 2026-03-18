#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) ;
#endif

MACRO_CONFIG_INT(CcAutoTeamLock, cc_auto_team_lock, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically lock your team after joining it")
MACRO_CONFIG_INT(CcAutoTeamLockDelay, cc_auto_team_lock_delay, 0, 0, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many seconds to wait before automatically locking your team")
MACRO_CONFIG_INT(CcAntiKill, cc_anti_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Prevent killing while staying with other players in a team")
MACRO_CONFIG_INT(CcAntiKillDelay, cc_anti_kill_delay, 5, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many minutes to wait before blocking kill in an active team")
MACRO_CONFIG_INT(CcAntiQuit, cc_anti_quit, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Always show a confirmation popup before quitting the client")
MACRO_CONFIG_STR(CcFirstRun, first_run, 16, "true", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether the CatClient first-run setup should be shown")
MACRO_CONFIG_INT(CcServerBrowserAutoRefresh, cc_server_browser_auto_refresh, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically refresh the current server browser tab")
MACRO_CONFIG_INT(CcServerBrowserRefreshInterval, cc_server_browser_refresh_interval, 5, 1, 30, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How often to automatically refresh the current server browser tab, in seconds")
MACRO_CONFIG_INT(CcBrFilterCatClient, cc_br_filter_catclient, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show only servers where CatClient users are currently online")
MACRO_CONFIG_INT(CcAspectRatioEnabled, cc_aspect_ratio_enabled, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply a custom stretched aspect ratio to ingame rendering")
MACRO_CONFIG_INT(CcAspectRatio, cc_aspect_ratio, 177, 100, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom ingame aspect ratio multiplied by 100, e.g. 177 means 1.77")
MACRO_CONFIG_INT(CcUiScale, cc_ui_scale, 100, 50, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Scale the menu UI in percent, where 100 is the default size")
MACRO_CONFIG_INT(CcNewMainMenu, cc_new_main_menu, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the start menu in the compact CatClient layout")
MACRO_CONFIG_INT(CcCustomBackgroundMainMenu, cc_custom_background_main_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the selected custom background image in the main menu")
MACRO_CONFIG_INT(CcCustomBackgroundGame, cc_custom_background_game, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the selected custom background image ingame behind the world")
MACRO_CONFIG_STR(CcCustomBackgroundImage, cc_custom_background_image, 128, "firstbg.jpg", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Selected custom background image filename inside catclient/backgrounds")
MACRO_CONFIG_INT(CcMuteSounds, cc_mute_sounds, 0, 0, 31, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bitmask of CatClient sound muting options")
MACRO_CONFIG_INT(CcHideEffects, cc_hide_effects, 0, 0, 7, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bitmask of CatClient effect hiding options")
MACRO_CONFIG_INT(CcChatAnimations, cc_chat_animations, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bitmask of CatClient chat animation options")
MACRO_CONFIG_INT(CcModernUi, cc_modern_ui, 0, 0, 7, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bitmask of CatClient modern UI widgets")
MACRO_CONFIG_INT(CcHorizontalSettingsTabs, cc_horizontal_settings_tabs, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the main settings tabs in a horizontal top bar")
MACRO_CONFIG_INT(CcStreamerMode, cc_streamer_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable CatClient streamer mode")
MACRO_CONFIG_INT(CcStreamerFlags, cc_streamer_flags, 0, 0, 7, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bitmask of CatClient streamer mode options")
