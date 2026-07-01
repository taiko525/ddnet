/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_TAS_CONTROLLER_H
#define GAME_CLIENT_COMPONENTS_TAS_CONTROLLER_H

#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include "tas_types.h"

class CTasController : public CComponentInterfaces
{
public:
	CTasController();
	~CTasController();

	void Init(CGameClient *pGameClient);
	void Update();

	// Core Control Functions
	void SetTPS(int TPS);
	int GetTPS() const { return m_Config.m_CurrentTPS; }
	
	bool IsActive() const { return m_Recording || m_Playing || !m_ReplayBuffer.IsEmpty(); }
	
	void TogglePause();
	bool IsPaused() const { return m_Config.m_Paused; }
	
	void ToggleRecording();
	void TogglePlayback();
	
	void StartRecording();
	void StopRecording();
	bool IsRecording() const { return m_Recording; }
	
	void StartPlayback();
	void StopPlayback();
	bool IsPlayingBack() const { return m_Playing; }
	
	void Clear();
	
	void Rewind(int Frames = 1);
	void Forward(int Frames = 1);
	
	int GetCurrentFrame() const { return m_ReplayBuffer.GetCurrentFrame(); }
	int GetTotalFrames() const { return (int)m_ReplayBuffer.Size(); }
	int GetCurrentTps() const { return m_Config.m_CurrentTPS; }
	int GetMaxTps() const { return 50; }
	
	// Input Interception
	void ProcessPlayerInput(int ClientID, CNetObj_PlayerInput *pInput);
	CNetObj_PlayerInput GetPlaybackInput(int ClientID);
	
	// State Management
	void CaptureStateSnapshot();
	void RestoreStateSnapshot(int FrameIndex);
	
	// File I/O
	bool SaveToFile(const char *pFilename);
	bool LoadFromFile(const char *pFilename);
	
private:
	CGameClient *m_pGameClient;
	
	CTasReplayBuffer m_ReplayBuffer;
	STasConfig m_Config;
	
	bool m_Recording;
	bool m_Playing;
	
	// State snapshots for rewinding
	std::vector<STasStateSnapshot> m_StateSnapshots;
	
	// Helper functions
	void ApplyTPSDelay();
	int GetGameTick();
	void ResetCharacter(int ClientID);
};

#endif
