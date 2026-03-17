#include "catclient.h"
#include "menus_catclient_common.h"

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

namespace
{
	using namespace CatClientMenu;
	bool s_ShowBlockedWordsList = false;
	enum EBlockedWordsAction
	{
		BLOCKED_WORDS_ACTION_NONE = 0,
		BLOCKED_WORDS_ACTION_OPEN,
		BLOCKED_WORDS_ACTION_HIDE,
	};

	bool DoStreamerFlagCheckBox(CMenus *pMenus, const void *pId, const char *pLabel, int Flag, int &Flags, CUIRect &Content)
	{
		CUIRect Button;
		Content.HSplitTop(LINE_SIZE, &Button, &Content);
		const int Checked = (Flags & Flag) != 0;
		if(pMenus->DoButton_CheckBox(pId, pLabel, Checked, &Button))
		{
			Flags ^= Flag;
			return true;
		}
		return false;
	}

	void RenderStreamerSettingsSection(CMenus *pMenus, CUi *pUi, CUIRect &View)
	{
		CUIRect Section, Content, Label;
		BeginSection(View, Section, Content, 145.0f);
		Content.HSplitTop(HEADLINE_HEIGHT, &Label, &Content);
		pUi->DoLabel(&Label, Localize("Streamer Mode"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);

		pMenus->DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_CcStreamerMode, Localize("Enable Streamer Mode"), &g_Config.m_CcStreamerMode, &Content, LINE_SIZE);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
		DoStreamerFlagCheckBox(pMenus, (void *)(intptr_t)1001, Localize("Hide Server IP"), CCatClient::STREAMER_HIDE_SERVER_IP, g_Config.m_CcStreamerFlags, Content);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
		DoStreamerFlagCheckBox(pMenus, (void *)(intptr_t)1002, Localize("Hide Chat"), CCatClient::STREAMER_HIDE_CHAT, g_Config.m_CcStreamerFlags, Content);
		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
		DoStreamerFlagCheckBox(pMenus, (void *)(intptr_t)1003, Localize("Hide Friend/Whisper Info"), CCatClient::STREAMER_HIDE_FRIEND_WHISPER, g_Config.m_CcStreamerFlags, Content);
	}

	EBlockedWordsAction RenderBlockedWordsSection(CMenus *pMenus, CUi *pUi, CCatClient *pCatClient, CUIRect &View)
	{
		static CLineInputBuffered<128> s_WordInput;
		static CButtonContainer s_AddWordButton;
		static CButtonContainer s_OpenBlockedWordsButton;
		static CButtonContainer s_HideBlockedWordsButton;
		static CScrollRegion s_BlockedWordsScrollRegion;
		static std::vector<CButtonContainer> s_vDeleteWordButtons;

		CUIRect Section, Content, HeaderRow, Label, ToggleButton, InputRow, InputBox, AddButton, ListBox, Description;
		const float SectionHeight = s_ShowBlockedWordsList ? 292.0f : 96.0f;
		BeginSection(View, Section, Content, SectionHeight);
		Content.HSplitTop(HEADLINE_HEIGHT, &HeaderRow, &Content);
		HeaderRow.VSplitRight(92.0f, &Label, &ToggleButton);
		char aTitle[64];
		str_format(aTitle, sizeof(aTitle), "Blocked Words (%d)", pCatClient->StreamerBlockedWordCount());
		pUi->DoLabel(&Label, aTitle, HEADLINE_FONT_SIZE, TEXTALIGN_ML);

		if(s_ShowBlockedWordsList)
		{
			if(pMenus->DoButton_Menu(&s_HideBlockedWordsButton, "Hide", 0, &ToggleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f)))
			{
				return BLOCKED_WORDS_ACTION_HIDE;
			}
		}
		else if(pMenus->DoButton_Menu(&s_OpenBlockedWordsButton, "Open", 0, &ToggleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f)))
		{
			return BLOCKED_WORDS_ACTION_OPEN;
		}

		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
		if(!s_ShowBlockedWordsList)
		{
			Content.HSplitTop(LINE_SIZE * 2.0f, &Description, &Content);
			SLabelProperties Props;
			Props.m_MaxWidth = Description.w;
			pUi->DoLabel(&Description, "The list is hidden by default for stream safety.", SMALL_FONT_SIZE, TEXTALIGN_ML, Props);
			return BLOCKED_WORDS_ACTION_NONE;
		}

		Content.HSplitTop(LINE_SIZE, &InputRow, &Content);
		InputRow.VSplitRight(LINE_SIZE + 4.0f, &InputBox, &AddButton);
		s_WordInput.SetEmptyText("Add blocked word");
		pUi->DoEditBox(&s_WordInput, &InputBox, 12.0f);

		if(pMenus->DoButton_Menu(&s_AddWordButton, "+", 0, &AddButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.3f)))
		{
			pCatClient->AddStreamerBlockedWord(s_WordInput.GetString());
			s_WordInput.Clear();
		}

		Content.HSplitTop(MARGIN_SMALL, nullptr, &Content);
		ListBox = Content;

		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollbarWidth = 14.0f;
		ScrollParams.m_ScrollbarMargin = 3.0f;
		ScrollParams.m_ScrollUnit = LINE_SIZE * 4.0f;
		ScrollParams.m_ClipBgColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.03f);
		ScrollParams.m_RailBgColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f);
		ScrollParams.m_SliderColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.28f);
		ScrollParams.m_SliderColorHover = ColorRGBA(1.0f, 1.0f, 1.0f, 0.42f);
		ScrollParams.m_SliderColorGrabbed = ColorRGBA(1.0f, 1.0f, 1.0f, 0.55f);

		const auto &vWords = pCatClient->StreamerBlockedWords();
		s_vDeleteWordButtons.resize(vWords.size());

		vec2 ScrollOffset(0.0f, 0.0f);
		s_BlockedWordsScrollRegion.Begin(&ListBox, &ScrollOffset, &ScrollParams);

		CUIRect ListContent = ListBox;
		ListContent.y += ScrollOffset.y;
		int RemoveIndex = -1;

		if(vWords.empty())
		{
			CUIRect EmptyRow;
			ListContent.HSplitTop(LINE_SIZE, &EmptyRow, &ListContent);
			s_BlockedWordsScrollRegion.AddRect(EmptyRow);
			if(!s_BlockedWordsScrollRegion.RectClipped(EmptyRow))
			{
				pUi->DoLabel(&EmptyRow, "No blocked words yet", SMALL_FONT_SIZE, TEXTALIGN_MC);
			}
		}
		else
		{
			for(size_t i = 0; i < vWords.size(); ++i)
			{
				if(i != 0)
				{
					ListContent.HSplitTop(MARGIN_SMALL, nullptr, &ListContent);
				}

				CUIRect Row, Inner, WordLabel, DeleteButton;
				ListContent.HSplitTop(LINE_SIZE + 4.0f, &Row, &ListContent);
				s_BlockedWordsScrollRegion.AddRect(Row);
				if(s_BlockedWordsScrollRegion.RectClipped(Row))
				{
					continue;
				}

				Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, i % 2 == 0 ? 0.05f : 0.08f), IGraphics::CORNER_ALL, 6.0f);
				Row.Margin(4.0f, &Inner);
				Inner.VSplitRight(LINE_SIZE, &WordLabel, &DeleteButton);

				SLabelProperties Props;
				Props.m_MaxWidth = WordLabel.w;
				pUi->DoLabel(&WordLabel, vWords[i].c_str(), SMALL_FONT_SIZE, TEXTALIGN_ML, Props);

				if(pUi->DoButton_FontIcon(&s_vDeleteWordButtons[i], FontIcon::TRASH, 0, &DeleteButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, true, ColorRGBA(0.85f, 0.25f, 0.25f, 0.35f)))
				{
					RemoveIndex = (int)i;
				}
			}
		}

		s_BlockedWordsScrollRegion.End();

		if(RemoveIndex >= 0)
		{
			pCatClient->RemoveStreamerBlockedWord(RemoveIndex);
		}

		return BLOCKED_WORDS_ACTION_NONE;
	}
}

void CMenus::PopupConfirmOpenBlockedWords()
{
	s_ShowBlockedWordsList = true;
}

void CMenus::RenderSettingsCatClientStreamer(CUIRect MainView)
{
	using namespace CatClientMenu;

	CUIRect LeftView, RightView;
	MainView.HSplitTop(MARGIN_SMALL, nullptr, &MainView);
	ConstrainWidth(MainView, MainView, 760.0f);
	MainView.VSplitMid(&LeftView, &RightView, MARGIN_BETWEEN_VIEWS);

	RenderStreamerSettingsSection(this, Ui(), LeftView);
	const EBlockedWordsAction Action = RenderBlockedWordsSection(this, Ui(), &GameClient()->m_CatClient, RightView);
	if(Action == BLOCKED_WORDS_ACTION_OPEN)
	{
		PopupConfirm(
			"Open blocked words?",
			"Are you sure you want to open the blocked words list, and did you hide your screen on stream?",
			"Open",
			"Cancel",
			&CMenus::PopupConfirmOpenBlockedWords);
	}
	else if(Action == BLOCKED_WORDS_ACTION_HIDE)
	{
		s_ShowBlockedWordsList = false;
	}
}
