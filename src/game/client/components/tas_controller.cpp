/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/io.h>
#include <engine/storage.h>
#include <game/client/gameclient.h>
#include <game/client/components/players.h>
#include "tas_controller.h"

CTasController::CTasController()
	: m_pGameClient(nullptr)
	, m_Recording(false)
	, m_Playing(false)
{
	m_Config.m_CurrentTPS = 50; // Default Teeworlds TPS
	m_Config.m_Paused = false;
	m_Config.m_FrameStep = 1;
}

CTasController::~CTasController()
{
	// Cleanup handled by replay buffer destructor
}

void CTasController::Init(CGameClient *pGameClient)
{
	m_pGameClient = pGameClient;
	m_ReplayBuffer.Clear();
	m_StateSnapshots.clear();
}

void CTasController::Update()
{
	if(m_Config.m_Paused)
		return;
	
	ApplyTPSDelay();
	
	// Capture state snapshot during recording
	if(m_Recording)
	{
		CaptureStateSnapshot();
	}
	
	// Handle playback
	if(m_Playing)
	{
		if(m_ReplayBuffer.GetCurrentFrame() >= (int)m_ReplayBuffer.Size())
		{
			StopPlayback();
		}
	}
}

void CTasController::SetTPS(int TPS)
{
	if(TPS < 1)
		TPS = 1;
	if(TPS > 50)
		TPS = 50;
	
	m_Config.m_CurrentTPS = TPS;
}

void CTasController::TogglePause()
{
	m_Config.m_Paused = !m_Config.m_Paused;
}

void CTasController::ToggleRecording()
{
	if(m_Recording)
		StopRecording();
	else
		StartRecording();
}

void CTasController::TogglePlayback()
{
	if(m_Playing)
		StopPlayback();
	else
		StartPlayback();
}

void CTasController::StartRecording()
{
	if(m_Playing)
		StopPlayback();
	
	m_Recording = true;
	m_ReplayBuffer.Clear();
	m_StateSnapshots.clear();
	
	// Capture initial state
	CaptureStateSnapshot();
}

void CTasController::StopRecording()
{
	m_Recording = false;
}

void CTasController::StartPlayback()
{
	if(m_Recording)
		StopRecording();
	
	if(m_ReplayBuffer.Size() == 0)
	{
		return;
	}
	
	m_Playing = true;
	m_ReplayBuffer.SetCurrentFrame(0);
	
	// Restore initial state
	if(!m_StateSnapshots.empty())
		RestoreStateSnapshot(0);
}

void CTasController::StopPlayback()
{
	m_Playing = false;
}

void CTasController::Clear()
{
	StopRecording();
	StopPlayback();
	
	m_ReplayBuffer.Clear();
	m_StateSnapshots.clear();
	
	// Reset character position
	if(m_pGameClient)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_pGameClient->m_Snap.m_aCharacters[i].m_Active)
			{
				ResetCharacter(i);
			}
		}
	}
}

void CTasController::Rewind(int Frames)
{
	if(!m_Config.m_Paused)
		TogglePause();
	
	int NewFrame = m_ReplayBuffer.GetCurrentFrame() - Frames;
	if(NewFrame < 0)
		NewFrame = 0;
	
	m_ReplayBuffer.SetCurrentFrame(NewFrame);
	
	if(NewFrame < (int)m_StateSnapshots.size())
		RestoreStateSnapshot(NewFrame);
}

void CTasController::Forward(int Frames)
{
	if(!m_Config.m_Paused)
		TogglePause();
	
	int NewFrame = m_ReplayBuffer.GetCurrentFrame() + Frames;
	if(NewFrame >= (int)m_ReplayBuffer.Size())
		NewFrame = (int)m_ReplayBuffer.Size() - 1;
	
	m_ReplayBuffer.SetCurrentFrame(NewFrame);
	
	if(NewFrame < (int)m_StateSnapshots.size())
		RestoreStateSnapshot(NewFrame);
}

void CTasController::ProcessPlayerInput(int ClientID, CNetObj_PlayerInput *pInput)
{
	if(!m_pGameClient || !m_Recording)
		return;
	
	// Convert game input to TAS input frame
	STasInputFrame Frame = STasInputFrame::FromGameInput(GetGameTick(), pInput);
	Frame.m_ClientID = ClientID;
	
	m_ReplayBuffer.AddFrame(Frame);
}

CNetObj_PlayerInput CTasController::GetPlaybackInput(int ClientID)
{
	CNetObj_PlayerInput Input = {0};
	
	if(!m_Playing || m_ReplayBuffer.GetCurrentFrame() >= (int)m_ReplayBuffer.Size())
		return Input;
	
	const STasInputFrame *pFrame = m_ReplayBuffer.GetFrame(m_ReplayBuffer.GetCurrentFrame());
	if(pFrame && pFrame->m_ClientID == ClientID)
	{
		pFrame->ToGameInput(&Input);
	}
	
	return Input;
}

void CTasController::CaptureStateSnapshot()
{
	if(!m_pGameClient || !m_pGameClient->Client())
		return;
	
	STasStateSnapshot Snapshot;
	Snapshot.m_Tick = GetGameTick();
	
	// Capture state for local player
	int LocalClientID = m_pGameClient->m_Snap.m_LocalClientId;
	if(LocalClientID >= 0 && LocalClientID < MAX_CLIENTS && m_pGameClient->m_Snap.m_aCharacters[LocalClientID].m_Active)
	{
		const CNetObj_Character &CharData = m_pGameClient->m_Snap.m_aCharacters[LocalClientID].m_Cur;
		Snapshot.m_PosX = CharData.m_X / 256.0f;
		Snapshot.m_PosY = CharData.m_Y / 256.0f;
		Snapshot.m_VelX = CharData.m_VelX / 256.0f;
		Snapshot.m_VelY = CharData.m_VelY / 256.0f;
		Snapshot.m_Direction = CharData.m_Direction;
		Snapshot.m_Jumped = CharData.m_Jumped;
		Snapshot.m_JumpedTotal = 0; // Not available in snap
		Snapshot.m_HookState = CharData.m_HookState;
		Snapshot.m_HookX = CharData.m_HookX / 256.0f;
		Snapshot.m_HookY = CharData.m_HookY / 256.0f;
		Snapshot.m_HookTick = CharData.m_HookTick;
		Snapshot.m_Weapon = CharData.m_Weapon;
		Snapshot.m_Ammo = CharData.m_AmmoCount;
		Snapshot.m_Health = CharData.m_Health;
		Snapshot.m_Armor = CharData.m_Armor;
	}
	else
	{
		// Default values when character data not available
		Snapshot = STasStateSnapshot::FromCharacter(nullptr, Snapshot.m_Tick);
	}
	
	m_StateSnapshots.push_back(Snapshot);
}

void CTasController::RestoreStateSnapshot(int FrameIndex)
{
	// State restoration on client side is limited since we can't directly modify character state
	// This would require server support or prediction system integration
	// For now, this is a placeholder for future implementation
}

bool CTasController::SaveToFile(const char *pFilename)
{
	if(!m_pGameClient || !m_pGameClient->Storage())
		return false;
	
	IOHANDLE File = m_pGameClient->Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	
	if(!File)
	{
		return false;
	}
	
	// Write header
	int Version = 1;
	io_write(File, &Version, sizeof(Version));
	
	int FrameCount = (int)m_ReplayBuffer.Size();
	io_write(File, &FrameCount, sizeof(FrameCount));
	
	// Write frames
	for(int i = 0; i < FrameCount; i++)
	{
		const STasInputFrame *pFrame = m_ReplayBuffer.GetFrame(i);
		if(pFrame)
		{
			io_write(File, pFrame, sizeof(STasInputFrame));
		}
	}
	
	io_close(File);
	
	return true;
}

bool CTasController::LoadFromFile(const char *pFilename)
{
	if(!m_pGameClient || !m_pGameClient->Storage())
		return false;
	
	IOHANDLE File = m_pGameClient->Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	
	if(!File)
	{
		return false;
	}
	
	// Read header
	int Version;
	if(io_read(File, &Version, sizeof(Version)) != sizeof(Version))
	{
		io_close(File);
		return false;
	}
	
	if(Version != 1)
	{
		io_close(File);
		return false;
	}
	
	int FrameCount;
	if(io_read(File, &FrameCount, sizeof(FrameCount)) != sizeof(FrameCount))
	{
		io_close(File);
		return false;
	}
	
	// Clear existing replay
	m_ReplayBuffer.Clear();
	m_StateSnapshots.clear();
	
	// Read frames
	for(int i = 0; i < FrameCount; i++)
	{
		STasInputFrame Frame;
		if(io_read(File, &Frame, sizeof(STasInputFrame)) == sizeof(STasInputFrame))
		{
			m_ReplayBuffer.AddFrame(Frame);
		}
	}
	
	io_close(File);
	
	return true;
}

void CTasController::ApplyTPSDelay()
{
	// This would integrate with the game's main loop to control tick rate
	// Implementation depends on Teeworlds engine architecture
	if(m_Config.m_CurrentTPS < 50)
	{
		// Add delay between ticks to achieve lower TPS
		// This is a simplified representation
		int DelayMs = (1000 / m_Config.m_CurrentTPS) - (1000 / 50);
		if(DelayMs > 0)
		{
			// In real implementation, this would use platform-specific sleep
			// thread_sleep(DelayMs);
		}
	}
}

int CTasController::GetGameTick()
{
	if(!m_pGameClient || !m_pGameClient->Client())
		return 0;
	return m_pGameClient->Client()->GameTick(0);
}

void CTasController::ResetCharacter(int ClientID)
{
	// Client-side character reset is limited
	// This would typically require a server command or respawn mechanism
	// For now, this is a placeholder
}
