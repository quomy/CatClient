#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_CATCLIENT_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_CATCLIENT_H

#include "catclient_nametags.h"

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/http.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <chrono>
#include <string>
#include <vector>

class CCatClient : public CComponent
{
	int m_AutoTeamLockTeam = 0;
	std::chrono::nanoseconds m_AutoTeamLockStart = std::chrono::nanoseconds::zero();
	bool m_AutoTeamLockIssued = false;
	int m_AntiKillTeam = 0;
	std::chrono::nanoseconds m_AntiKillStart = std::chrono::nanoseconds::zero();
	CCatClientNameTags m_NameTags;
	std::shared_ptr<CHttpRequest> m_pCursorDownloadTask;

	IGraphics::CTextureHandle m_CursorTexture;
	bool m_HasCustomCursor = false;
	bool m_StreamerWordsLoaded = false;
	std::vector<std::string> m_vIgnoredPlayers;
	std::vector<std::string> m_vStreamerBlockedWords;

	static void ConfigSaveCallback(class IConfigManager *pConfigManager, void *pUserData);
	static void ConIgnorePlayer(class IConsole::IResult *pResult, void *pUserData);
	static void ConUnignorePlayer(class IConsole::IResult *pResult, void *pUserData);

	void AbortTask(std::shared_ptr<CHttpRequest> &pTask);
	void ResetAutoTeamLock();
	void ResetAntiKill();
	bool IsLocalTeamLocked() const;
	bool IsLocalPlayerInTeam() const;
	bool HasActiveTeammateInLocalTeam() const;
	bool IsLocalClientId(int ClientId) const;
	bool IsLikelyLocalHammerHit(vec2 Pos) const;
	void UpdateAspectRatioOverride();
	void UpdateAntiKillState();
	void UpdateIgnoredPlayers();
	bool LoadAutomaticCursorAsset();
	void StartAutomaticCursorDownload();
	void FinishAutomaticCursorDownload();
	void EnsureStreamerWordsLoaded();
	void SaveStreamerWords() const;
	bool SetPlayerIgnoredInternal(const char *pPlayerName, bool Ignored, bool SaveConfig);

public:
	enum EMuteSoundFlags
	{
		MUTE_SOUND_OTHERS_HOOK = 1 << 0,
		MUTE_SOUND_OTHERS_HAMMER = 1 << 1,
		MUTE_SOUND_LOCAL_HAMMER = 1 << 2,
		MUTE_SOUND_WEAPON_SWITCH = 1 << 3,
		MUTE_SOUND_JUMP = 1 << 4,
	};

	enum EHideEffectFlags
	{
		HIDE_EFFECT_FREEZE_FLAKES = 1 << 0,
		HIDE_EFFECT_HAMMER_HITS = 1 << 1,
		HIDE_EFFECT_JUMPS = 1 << 2,
	};

	enum EChatAnimationFlags
	{
		CHAT_ANIM_OPEN_CLOSE = 1 << 0,
		CHAT_ANIM_TYPING = 1 << 1,
	};

	enum EModernUiFlags
	{
		MODERN_UI_FPS_PING = 1 << 0,
		MODERN_UI_RACE_TIMER = 1 << 1,
		MODERN_UI_LOCAL_TIMER = 1 << 2,
	};

	enum EStreamerFlags
	{
		STREAMER_HIDE_SERVER_IP = 1 << 0,
		STREAMER_HIDE_CHAT = 1 << 1,
		STREAMER_HIDE_FRIEND_WHISPER = 1 << 2,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnUpdate() override;
	void OnReset() override;
	void OnRender() override;
	void OnNewSnapshot() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	void LoadCursorAsset(const char *pPath);
	const IGraphics::CTextureHandle &CursorTexture() const;
	const char *ResolveAudioFile(const char *pDefaultPath, char *pBuffer, size_t BufferSize) const;
	bool HasMuteSoundFlag(int Flag) const;
	bool HasHideEffectFlag(int Flag) const;
	bool IsStreamerModeEnabled() const;
	bool HasStreamerFlag(int Flag) const;
	void AddStreamerBlockedWord(const char *pWord);
	void RemoveStreamerBlockedWord(int Index);
	const std::vector<std::string> &StreamerBlockedWords();
	void SanitizeText(const char *pInput, char *pOutput, size_t OutputSize);
	void BuildStreamerBlockedWordsPreview(char *pOutput, size_t OutputSize);
	int StreamerBlockedWordCount();
	const char *MaskServerAddress(const char *pAddress, char *pOutput, size_t OutputSize);
	bool HasCatTag(int ClientId) const;
	bool HasCatServer(const char *pAddress) const;
	int KnownCatServerCount() const;
	bool HasCatIconTexture() const;
	const IGraphics::CTextureHandle &CatIconTexture() const;
	void RenderCatIcon(const CUIRect &Rect, float Alpha = 1.0f) const;
	bool IsPlayerIgnored(const char *pPlayerName) const;
	bool IsPlayerIgnored(int ClientId) const;
	bool IgnorePlayer(const char *pPlayerName);
	bool UnignorePlayer(const char *pPlayerName);
	bool InvitePlayer(int ClientId);
	bool ShouldBlockKill();
	bool ShouldMuteSound(int SoundId, int OwnerId, const vec2 *pSoundPos = nullptr) const;
};

#endif
