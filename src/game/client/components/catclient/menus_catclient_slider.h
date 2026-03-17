#ifndef GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_SLIDER_H
#define GAME_CLIENT_COMPONENTS_CATCLIENT_MENUS_CATCLIENT_SLIDER_H

#include <base/math.h>

#include <engine/graphics.h>

#include <game/client/ui.h>
#include <game/client/ui_rect.h>

struct CCatClientMenuSliderState
{
	bool m_Initialized = false;
	bool m_WasActive = false;
	int m_VisualValue = 0;
};

inline float CatClientMenuDoRoundScrollbarH(CUi *pUi, const void *pId, const CUIRect &Rect, float Current)
{
	Current = std::clamp(Current, 0.0f, 1.0f);

	CUIRect Rail = Rect;
	const float RailHeight = minimum(4.0f, Rect.h);
	Rail.y = Rect.y + (Rect.h - RailHeight) / 2.0f;
	Rail.h = RailHeight;

	const float HandleSize = std::clamp(Rect.h - 6.0f, 10.0f, 14.0f);
	CUIRect Handle;
	Handle.w = HandleSize;
	Handle.h = HandleSize;
	Handle.x = Rail.x + (Rail.w - Handle.w) * Current;
	Handle.y = Rect.y + (Rect.h - Handle.h) / 2.0f;

	CUIRect HandleArea = Handle;
	HandleArea.x -= 3.0f;
	HandleArea.y -= 3.0f;
	HandleArea.w += 6.0f;
	HandleArea.h += 6.0f;

	const bool InsideRail = pUi->MouseHovered(&Rect);
	const bool InsideHandle = pUi->MouseHovered(&HandleArea);

	if(pUi->CheckActiveItem(pId))
	{
		if(!pUi->MouseButton(0))
		{
			pUi->SetActiveItem(nullptr);
		}
	}
	else if((InsideRail || InsideHandle) && pUi->MouseButtonClicked(0))
	{
		pUi->SetActiveItem(pId);
	}

	if(InsideRail && !pUi->MouseButton(0))
	{
		pUi->SetHotItem(pId);
	}

	float ReturnValue = Current;
	if(pUi->CheckActiveItem(pId) && pUi->MouseButton(0))
	{
		const float SliderMax = maximum(Rail.w - Handle.w, 1.0f);
		ReturnValue = std::clamp((pUi->MouseX() - Rail.x - Handle.w / 2.0f) / SliderMax, 0.0f, 1.0f);
	}

	CUIRect Fill = Rail;
	Fill.w = maximum(Handle.x + Handle.w / 2.0f - Rail.x, Rail.h);

	const bool Hovered = pUi->HotItem() == pId;
	const bool Active = pUi->CheckActiveItem(pId);
	const ColorRGBA RailColor(1.0f, 1.0f, 1.0f, 0.10f);
	const ColorRGBA FillColor = Active ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.34f) : Hovered ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.28f) :
												ColorRGBA(1.0f, 1.0f, 1.0f, 0.22f);
	const ColorRGBA HandleColor = Active ? ColorRGBA(0.96f, 0.96f, 0.96f, 1.0f) : Hovered ? ColorRGBA(0.93f, 0.93f, 0.93f, 0.98f) :
												ColorRGBA(0.88f, 0.88f, 0.88f, 0.96f);

	Rail.Draw(RailColor, IGraphics::CORNER_ALL, Rail.h / 2.0f);
	Fill.Draw(FillColor, IGraphics::CORNER_ALL, Fill.h / 2.0f);
	Handle.Draw(HandleColor, IGraphics::CORNER_ALL, Handle.w / 2.0f);

	return ReturnValue;
}

template<typename TFormatFn>
inline bool CatClientMenuDoSliderOption(CUi *pUi, const void *pId, CCatClientMenuSliderState *pState, int *pValue, const CUIRect &Rect, int Min, int Max, const IScrollbarScale *pScale, bool DelayApply, TFormatFn &&FormatValue)
{
	if(!pState->m_Initialized)
	{
		pState->m_VisualValue = *pValue;
		pState->m_Initialized = true;
	}

	int Value = (DelayApply && (pState->m_WasActive || pUi->CheckActiveItem(pId))) ? pState->m_VisualValue : *pValue;
	Value = std::clamp(Value, Min, Max);

	const int Increment = std::max(1, (Max - Min) / 35);
	if(pUi->Input()->ModifierIsPressed() && pUi->Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && pUi->MouseInside(&Rect))
	{
		Value = minimum(Value + Increment, Max);
	}
	if(pUi->Input()->ModifierIsPressed() && pUi->Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && pUi->MouseInside(&Rect))
	{
		Value = maximum(Value - Increment, Min);
	}

	CUIRect Label, ScrollBar;
	Rect.VSplitMid(&Label, &ScrollBar, minimum(10.0f, Rect.w * 0.05f));

	int NewValue = pScale->ToAbsolute(CatClientMenuDoRoundScrollbarH(pUi, pId, ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	NewValue = std::clamp(NewValue, Min, Max);

	const bool ActiveNow = pUi->CheckActiveItem(pId);
	const int DisplayValue = DelayApply && ActiveNow ? NewValue : (DelayApply && pState->m_WasActive ? pState->m_VisualValue : NewValue);

	char aBuf[256];
	FormatValue(aBuf, sizeof(aBuf), DisplayValue);
	pUi->DoLabel(&Label, aBuf, Label.h * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_ML);

	bool Changed = false;
	if(DelayApply)
	{
		if(ActiveNow)
		{
			pState->m_VisualValue = NewValue;
		}
		else if(pState->m_WasActive)
		{
			if(*pValue != pState->m_VisualValue)
			{
				*pValue = pState->m_VisualValue;
				Changed = true;
			}
		}
		else if(*pValue != NewValue)
		{
			*pValue = NewValue;
			pState->m_VisualValue = NewValue;
			Changed = true;
		}
		else
		{
			pState->m_VisualValue = *pValue;
		}
	}
	else if(*pValue != NewValue)
	{
		*pValue = NewValue;
		pState->m_VisualValue = NewValue;
		Changed = true;
	}
	else
	{
		pState->m_VisualValue = *pValue;
	}

	pState->m_WasActive = ActiveNow;
	return Changed;
}

#endif
