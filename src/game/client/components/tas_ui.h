/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_TAS_UI_H
#define GAME_CLIENT_COMPONENTS_TAS_UI_H

#include <game/client/component.h>
#include "tas_controller.h"

class CTasUI : public CComponent
{
	friend class CTasController;

private:
	CTasController *m_pTasController;
	
	// UI State
	bool m_ShowTasPanel;
	float m_PanelPositionX;
	float m_PanelPositionY;
	
	// Display info
	char m_StatusMessage[256];
	float m_StatusMessageTime;

public:
	CTasUI();
	
	virtual void OnStateInit();
	virtual void OnReset();
	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual bool OnInput(IInput::CEvent Event);
	
	void SetController(CTasController *pController) { m_pTasController = pController; }
	void ShowPanel(bool Show) { m_ShowTasPanel = Show; }
	bool IsPanelVisible() const { return m_ShowTasPanel; }
	
	void RenderTasPanel();
	void RenderStatusMessage();
	void UpdateStatusMessage(const char *pMessage);
	
	// Shortcut handlers
	bool HandleTasShortcuts(IInput::CEvent Event);
};

#endif
