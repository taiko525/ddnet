/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <engine/console.h>
#include <game/client/gameclient.h>
#include <game/client/components/menus.h>
#include <generated/client_data.h>
#include <game/client/lineinput.h>

#include "tas_ui.h"
#include "tas_controller.h"

CTasUI::CTasUI()
{
	m_pTasController = 0;
	m_ShowTasPanel = false;
	m_PanelPositionX = 10.0f;
	m_PanelPositionY = 100.0f;
	m_StatusMessage[0] = '\0';
	m_StatusMessageTime = 0.0f;
}

void CTasUI::OnStateInit()
{
	// Initialize UI state
}

void CTasUI::OnReset()
{
	// Reset UI state
	m_ShowTasPanel = false;
	m_StatusMessage[0] = '\0';
	m_StatusMessageTime = 0.0f;
}

void CTasUI::OnRender()
{
	if(m_pTasController && m_pTasController->IsActive())
	{
		RenderTasPanel();
		RenderStatusMessage();
	}
}

void CTasUI::OnConsoleInit()
{
	if(!m_pTasController)
		return;
	
	IConsole *pConsole = GameClient()->Console();
	
	// TAS Control Commands
	pConsole->Register("tas_pause", "", CFGFLAG_CLIENT, ConPause, this, "Toggle TAS pause");
	pConsole->Register("tas_record", "", CFGFLAG_CLIENT, ConRecord, this, "Start/stop recording TAS");
	pConsole->Register("tas_playback", "", CFGFLAG_CLIENT, ConPlayback, this, "Start/stop TAS playback");
	pConsole->Register("tas_clear", "", CFGFLAG_CLIENT, ConClear, this, "Clear TAS buffer and reset position");
	pConsole->Register("tas_rewind", "i", CFGFLAG_CLIENT, ConRewind, this, "Rewind TAS by specified frames");
	pConsole->Register("tas_forward", "i", CFGFLAG_CLIENT, ConForward, this, "Forward TAS by specified frames");
	pConsole->Register("tas_tps", "i", CFGFLAG_CLIENT, ConSetTps, this, "Set TAS TPS (1-50)");
	pConsole->Register("tas_panel", "", CFGFLAG_CLIENT, ConTogglePanel, this, "Toggle TAS UI panel");
	pConsole->Register("tas_save", "s", CFGFLAG_CLIENT, ConSave, this, "Save TAS to file");
	pConsole->Register("tas_load", "s", CFGFLAG_CLIENT, ConLoad, this, "Load TAS from file");
	pConsole->Register("tas_status", "", CFGFLAG_CLIENT, ConStatus, this, "Show TAS status");
}

bool CTasUI::OnInput(IInput::CEvent Event)
{
	if(!m_pTasController || !m_pTasController->IsActive())
		return false;
	
	return HandleTasShortcuts(Event);
}

bool CTasUI::HandleTasShortcuts(IInput::CEvent Event)
{
	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		switch(Event.m_Key)
		{
			case KEY_PAUSE:
				m_pTasController->TogglePause();
				UpdateStatusMessage(m_pTasController->IsPaused() ? "TAS Paused" : "TAS Resumed");
				return true;
				
			case KEY_L: // Load/Playback
				m_pTasController->TogglePlayback();
				UpdateStatusMessage(m_pTasController->IsPlayingBack() ? "Playback Started" : "Playback Stopped");
				return true;
				
			case KEY_R: // Record
				m_pTasController->ToggleRecording();
				UpdateStatusMessage(m_pTasController->IsRecording() ? "Recording Started" : "Recording Stopped");
				return true;
				
			case KEY_C: // Clear
				m_pTasController->Clear();
				UpdateStatusMessage("TAS Buffer Cleared");
				return true;
				
			case KEY_COMMA: // Rewind (,)
				m_pTasController->Rewind(1);
				UpdateStatusMessage("Rewound 1 frame");
				return true;
				
			case KEY_PERIOD: // Forward (.)
				m_pTasController->Forward(1);
				UpdateStatusMessage("Advanced 1 frame");
				return true;
				
			case KEY_SHIFT:
				// Shift + , or . for larger steps
				break;
		}
	}
	
	// Handle shift combinations
	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		if(Input()->KeyIsPressed(KEY_SHIFT))
		{
			if(Event.m_Key == KEY_COMMA)
			{
				m_pTasController->Rewind(10);
				UpdateStatusMessage("Rewound 10 frames");
				return true;
			}
			else if(Event.m_Key == KEY_PERIOD)
			{
				m_pTasController->Forward(10);
				UpdateStatusMessage("Advanced 10 frames");
				return true;
			}
		}
	}
	
	return false;
}

void CTasUI::RenderTasPanel()
{
	if(!m_ShowTasPanel || !m_pTasController)
		return;
	
	CRenderTools *pRT = &GameClient()->m_RenderTools;
	CTextRender *pTextRender = TextRender();
	
	float ScreenX0, ScreenX1, ScreenY0, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	
	float PanelWidth = 250.0f;
	float PanelHeight = 180.0f;
	float X = m_PanelPositionX;
	float Y = m_PanelPositionY;
	
	// Draw background
	CColorRGBA BgColor(0.0f, 0.0f, 0.0f, 0.7f);
	pRT->DrawRoundRect(X, Y, PanelWidth, PanelHeight, BgColor, 5.0f);
	
	// Title
	char aTitle[64];
	str_format(aTitle, sizeof(aTitle), "TAS Controller - %s", 
		m_pTasController->IsRecording() ? "RECORDING" : 
		(m_pTasController->IsPlayingBack() ? "PLAYBACK" : "IDLE"));
	
	pTextRender->TextColor(1.0f, 1.0f, 0.0f, 1.0f);
	pTextRender->Text(X + 10.0f, Y + 15.0f, 12.0f, aTitle);
	
	// Status info
	float TextY = Y + 35.0f;
	float LineHeight = 15.0f;
	
	pTextRender->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	
	char aLine[128];
	
	// TPS
	str_format(aLine, sizeof(aLine), "TPS: %d/%d", m_pTasController->GetCurrentTps(), m_pTasController->GetMaxTps());
	pTextRender->Text(X + 10.0f, TextY, 10.0f, aLine);
	TextY += LineHeight;
	
	// Pause state
	str_format(aLine, sizeof(aLine), "State: %s", m_pTasController->IsPaused() ? "PAUSED" : "RUNNING");
	pTextRender->Text(X + 10.0f, TextY, 10.0f, aLine);
	TextY += LineHeight;
	
	// Frame info
	str_format(aLine, sizeof(aLine), "Frame: %d / %d", 
		m_pTasController->GetCurrentFrame(), 
		m_pTasController->GetTotalFrames());
	pTextRender->Text(X + 10.0f, TextY, 10.0f, aLine);
	TextY += LineHeight;
	
	// Recording/Playback indicator
	if(m_pTasController->IsRecording())
	{
		pTextRender->TextColor(1.0f, 0.0f, 0.0f, 1.0f);
		pTextRender->Text(X + 10.0f, TextY, 10.0f, "[REC]");
		TextY += LineHeight;
		pTextRender->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(m_pTasController->IsPlayingBack())
	{
		pTextRender->TextColor(0.0f, 1.0f, 0.0f, 1.0f);
		pTextRender->Text(X + 10.0f, TextY, 10.0f, "[PLAY]");
		TextY += LineHeight;
		pTextRender->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	
	// Controls hint
	TextY += 5.0f;
	pTextRender->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
	pTextRender->Text(X + 10.0f, TextY, 8.0f, "Controls:");
	TextY += 12.0f;
	pTextRender->Text(X + 10.0f, TextY, 8.0f, "Pause: PAUSE key");
	TextY += 10.0f;
	pTextRender->Text(X + 10.0f, TextY, 8.0f, "Record: R | Playback: L");
	TextY += 10.0f;
	pTextRender->Text(X + 10.0f, TextY, 8.0f, "Clear: C");
	TextY += 10.0f;
	pTextRender->Text(X + 10.0f, TextY, 8.0f, "Frame Step: , / .");
	TextY += 10.0f;
	pTextRender->Text(X + 10.0f, TextY, 8.0f, "Shift+: 10 frames");
}

void CTasUI::RenderStatusMessage()
{
	if(m_StatusMessageTime <= 0.0f || m_StatusMessage[0] == '\0')
		return;
	
	CRenderTools *pRT = &GameClient()->m_RenderTools;
	CTextRender *pTextRender = TextRender();
	
	float ScreenX0, ScreenX1, ScreenY0, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	
	float MsgWidth = TextRender()->TextWidth(12.0f, m_StatusMessage) + 20.0f;
	float MsgHeight = 25.0f;
	float X = (ScreenX0 + ScreenX1) / 2.0f - MsgWidth / 2.0f;
	float Y = ScreenY1 - 50.0f;
	
	// Background
	CColorRGBA BgColor(0.0f, 0.0f, 0.0f, 0.8f);
	pRT->DrawRoundRect(X, Y, MsgWidth, MsgHeight, BgColor, 5.0f);
	
	// Text
	pTextRender->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	pTextRender->Text(X + 10.0f, Y + 7.0f, 12.0f, m_StatusMessage);
	
	// Update fade time
	m_StatusMessageTime -= 1.0f / 60.0f; // Assume 60 FPS for UI
	if(m_StatusMessageTime < 0.0f)
		m_StatusMessageTime = 0.0f;
}

void CTasUI::UpdateStatusMessage(const char *pMessage)
{
	str_copy(m_StatusMessage, pMessage, sizeof(m_StatusMessage));
	m_StatusMessageTime = 3.0f; // Show for 3 seconds
}

// Console command callbacks
void CTasUI::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		pSelf->m_pTasController->TogglePause();
		pSelf->UpdateStatusMessage(pSelf->m_pTasController->IsPaused() ? "TAS Paused" : "TAS Resumed");
	}
}

void CTasUI::ConRecord(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		pSelf->m_pTasController->ToggleRecording();
		pSelf->UpdateStatusMessage(pSelf->m_pTasController->IsRecording() ? "Recording Started" : "Recording Stopped");
	}
}

void CTasUI::ConPlayback(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		pSelf->m_pTasController->TogglePlayback();
		pSelf->UpdateStatusMessage(pSelf->m_pTasController->IsPlayingBack() ? "Playback Started" : "Playback Stopped");
	}
}

void CTasUI::ConClear(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		pSelf->m_pTasController->Clear();
		pSelf->UpdateStatusMessage("TAS Buffer Cleared");
	}
}

void CTasUI::ConRewind(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		int Frames = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 1;
		pSelf->m_pTasController->Rewind(Frames);
		
		char aMsg[64];
		str_format(aMsg, sizeof(aMsg), "Rewound %d frame(s)", Frames);
		pSelf->UpdateStatusMessage(aMsg);
	}
}

void CTasUI::ConForward(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		int Frames = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 1;
		pSelf->m_pTasController->Forward(Frames);
		
		char aMsg[64];
		str_format(aMsg, sizeof(aMsg), "Advanced %d frame(s)", Frames);
		pSelf->UpdateStatusMessage(aMsg);
	}
}

void CTasUI::ConSetTps(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController && pResult->NumArguments() > 0)
	{
		int Tps = pResult->GetInteger(0);
		pSelf->m_pTasController->SetTps(Tps);
		
		char aMsg[64];
		str_format(aMsg, sizeof(aMsg), "TPS set to %d", Tps);
		pSelf->UpdateStatusMessage(aMsg);
	}
}

void CTasUI::ConTogglePanel(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	pSelf->ShowPanel(!pSelf->IsPanelVisible());
}

void CTasUI::ConSave(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController && pResult->NumArguments() > 0)
	{
		const char *pFilename = pResult->GetString(0);
		if(pSelf->m_pTasController->SaveToFile(pFilename))
		{
			char aMsg[128];
			str_format(aMsg, sizeof(aMsg), "Saved to %s", pFilename);
			pSelf->UpdateStatusMessage(aMsg);
		}
		else
		{
			pSelf->UpdateStatusMessage("Failed to save TAS");
		}
	}
}

void CTasUI::ConLoad(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController && pResult->NumArguments() > 0)
	{
		const char *pFilename = pResult->GetString(0);
		if(pSelf->m_pTasController->LoadFromFile(pFilename))
		{
			char aMsg[128];
			str_format(aMsg, sizeof(aMsg), "Loaded %s", pFilename);
			pSelf->UpdateStatusMessage(aMsg);
		}
		else
		{
			pSelf->UpdateStatusMessage("Failed to load TAS");
		}
	}
}

void CTasUI::ConStatus(IConsole::IResult *pResult, void *pUserData)
{
	CTasUI *pSelf = (CTasUI *)pUserData;
	if(pSelf->m_pTasController)
	{
		char aStatus[256];
		str_format(aStatus, sizeof(aStatus), 
			"TAS Status: %s | Frames: %d/%d | TPS: %d | %s",
			pSelf->m_pTasController->IsRecording() ? "REC" : 
				(pSelf->m_pTasController->IsPlayingBack() ? "PLAY" : "IDLE"),
			pSelf->m_pTasController->GetCurrentFrame(),
			pSelf->m_pTasController->GetTotalFrames(),
			pSelf->m_pTasController->GetCurrentTps(),
			pSelf->m_pTasController->IsPaused() ? "PAUSED" : "RUNNING");
		
		pSelf->UpdateStatusMessage(aStatus);
	}
}
