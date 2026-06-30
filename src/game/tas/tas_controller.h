/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_TAS_TAS_CONTROLLER_H
#define GAME_SERVER_TAS_TAS_CONTROLLER_H

#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "tas_types.h"

class CTasController
{
public:
	CTasController();
	~CTasController();

	void Init(CGameContext *pGameContext);
	void Update();

	// Core Control Functions
	void SetTPS(int TPS);
	int GetTPS() const { return m_Config.m_CurrentTPS; }
	
	void TogglePause();
	bool IsPaused() const { return m_Config.m_Paused; }
	
	void StartRecording();
	void StopRecording();
	bool IsRecording() const { return m_Recording; }
	
	void StartPlayback();
	void StopPlayback();
	bool IsPlaying() const { return m_Playing; }
	
	void ClearReplay();
	
	void RewindFrame();
	void ForwardFrame();
	
	// Input Interception
	void ProcessPlayerInput(int ClientID, CNetObj_PlayerInput *pInput);
	CNetObj_PlayerInput GetPlaybackInput(int ClientID);
	
	// State Management
	void CaptureStateSnapshot();
	void RestoreStateSnapshot(int FrameIndex);
	
	// File I/O
	bool SaveReplay(const char *pFilename);
	bool LoadReplay(const char *pFilename);
	
private:
	CGameContext *m_pGameContext;
	
	CTasReplayBuffer m_ReplayBuffer;
	STasConfig m_Config;
	
	bool m_Recording;
	bool m_Playing;
	int m_CurrentFrame;
	
	// State snapshots for rewinding
	std::vector<STasStateSnapshot> m_StateSnapshots;
	
	// Helper functions
	void ApplyTPSDelay();
	int GetGameTick();
	void ResetCharacter(int ClientID);
};

#endif
