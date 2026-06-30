/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/storage.h>
#include <game/server/entities/character.h>
#include "tas_controller.h"

CTasController::CTasController()
	: m_pGameContext(nullptr)
	, m_Recording(false)
	, m_Playing(false)
	, m_CurrentFrame(0)
{
	m_Config.m_CurrentTPS = 50; // Default Teeworlds TPS
	m_Config.m_Paused = false;
	m_Config.m_FrameStep = 1;
}

CTasController::~CTasController()
{
	// Cleanup handled by replay buffer destructor
}

void CTasController::Init(CGameContext *pGameContext)
{
	m_pGameContext = pGameContext;
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
		if(m_CurrentFrame >= (int)m_ReplayBuffer.Size())
		{
			StopPlayback();
		}
		else
		{
			m_CurrentFrame++;
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
	
	if(!m_Config.m_Paused && m_pGameContext)
	{
		// Reset timer when unpausing to prevent large delta time
		m_pGameContext->ResetTASDelta();
	}
}

void CTasController::StartRecording()
{
	if(m_Playing)
		StopPlayback();
	
	m_Recording = true;
	m_CurrentFrame = 0;
	m_ReplayBuffer.Clear();
	m_StateSnapshots.clear();
	
	// Capture initial state
	CaptureStateSnapshot();
	
	if(m_pGameContext)
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Recording started");
}

void CTasController::StopRecording()
{
	m_Recording = false;
	
	if(m_pGameContext)
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Recording stopped");
}

void CTasController::StartPlayback()
{
	if(m_Recording)
		StopRecording();
	
	if(m_ReplayBuffer.Size() == 0)
	{
		if(m_pGameContext)
			m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "No replay to play back");
		return;
	}
	
	m_Playing = true;
	m_CurrentFrame = 0;
	
	// Restore initial state
	if(!m_StateSnapshots.empty())
		RestoreStateSnapshot(0);
	
	if(m_pGameContext)
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Playback started");
}

void CTasController::StopPlayback()
{
	m_Playing = false;
	
	if(m_pGameContext)
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Playback stopped");
}

void CTasController::ClearReplay()
{
	StopRecording();
	StopPlayback();
	
	m_ReplayBuffer.Clear();
	m_StateSnapshots.clear();
	m_CurrentFrame = 0;
	
	// Reset character position
	if(m_pGameContext)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_pGameContext->m_apPlayers[i] && m_pGameContext->m_apPlayers[i]->GetCharacter())
			{
				ResetCharacter(i);
			}
		}
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Replay cleared and characters reset");
	}
}

void CTasController::RewindFrame()
{
	if(!m_Config.m_Paused)
		TogglePause();
	
	if(m_CurrentFrame > 0)
	{
		m_CurrentFrame -= m_Config.m_FrameStep;
		if(m_CurrentFrame < 0)
			m_CurrentFrame = 0;
		
		RestoreStateSnapshot(m_CurrentFrame);
		
		if(m_pGameContext)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Rewound to frame %d", m_CurrentFrame);
			m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
		}
	}
}

void CTasController::ForwardFrame()
{
	if(!m_Config.m_Paused)
		TogglePause();
	
	if(m_CurrentFrame < (int)m_ReplayBuffer.Size() - 1)
	{
		m_CurrentFrame += m_Config.m_FrameStep;
		if(m_CurrentFrame >= (int)m_ReplayBuffer.Size())
			m_CurrentFrame = (int)m_ReplayBuffer.Size() - 1;
		
		RestoreStateSnapshot(m_CurrentFrame);
		
		if(m_pGameContext)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Forwarded to frame %d", m_CurrentFrame);
			m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
		}
	}
}

void CTasController::ProcessPlayerInput(int ClientID, CNetObj_PlayerInput *pInput)
{
	if(!m_pGameContext || !m_pGameContext->m_apPlayers[ClientID])
		return;
	
	if(m_Recording)
	{
		// Convert game input to TAS input frame
		STasInputFrame Frame = STasInputFrame::FromGameInput(GetGameTick(), pInput);
		Frame.m_ClientID = ClientID;
		
		m_ReplayBuffer.AddFrame(Frame);
	}
}

CNetObj_PlayerInput CTasController::GetPlaybackInput(int ClientID)
{
	CNetObj_PlayerInput Input = {0};
	
	if(!m_Playing || m_CurrentFrame >= (int)m_ReplayBuffer.Size())
		return Input;
	
	const STasInputFrame *pFrame = m_ReplayBuffer.GetFrame(m_CurrentFrame);
	if(pFrame && pFrame->m_ClientID == ClientID)
	{
		pFrame->ToGameInput(&Input);
	}
	
	return Input;
}

void CTasController::CaptureStateSnapshot()
{
	if(!m_pGameContext)
		return;
	
	STasStateSnapshot Snapshot;
	Snapshot.m_Tick = GetGameTick();
	
	// Capture state for first active player (simplified for single-player TAS)
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pGameContext->m_apPlayers[i] && m_pGameContext->m_apPlayers[i]->GetCharacter())
		{
			CCharacter *pChar = m_pGameContext->m_apPlayers[i]->GetCharacter();
			Snapshot = STasStateSnapshot::FromCharacter(pChar, GetGameTick());
			break;
		}
	}
	
	m_StateSnapshots.push_back(Snapshot);
}

void CTasController::RestoreStateSnapshot(int FrameIndex)
{
	if(!m_pGameContext || FrameIndex < 0 || FrameIndex >= (int)m_StateSnapshots.size())
		return;
	
	const STasStateSnapshot &Snapshot = m_StateSnapshots[FrameIndex];
	
	// Find and restore the player character
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pGameContext->m_apPlayers[i] && m_pGameContext->m_apPlayers[i]->GetCharacter())
		{
			CCharacter *pChar = m_pGameContext->m_apPlayers[i]->GetCharacter();
			
			vec2 Pos(Snapshot.m_PosX, Snapshot.m_PosY);
			vec2 Vel(Snapshot.m_VelX, Snapshot.m_VelY);
			
			pChar->SetPos(Pos);
			pChar->SetVel(Vel);
			pChar->m_Direction = Snapshot.m_Direction;
			pChar->m_Jumped = Snapshot.m_Jumped;
			pChar->m_JumpedTotal = Snapshot.m_JumpedTotal;
			pChar->m_Core.m_Hook.m_State = Snapshot.m_HookState;
			pChar->m_Core.m_Hook.m_Pos = vec2(Snapshot.m_HookX, Snapshot.m_HookY);
			pChar->m_Core.m_Hook.m_Tick = Snapshot.m_HookTick;
			pChar->m_Weapon = Snapshot.m_Weapon;
			pChar->m_AmmoCount = Snapshot.m_Ammo;
			pChar->m_Health = Snapshot.m_Health;
			pChar->m_Armor = Snapshot.m_Armor;
			
			break; // Only restore first player for now
		}
	}
}

bool CTasController::SaveReplay(const char *pFilename)
{
	if(!m_pGameContext)
		return false;
	
	IStorage *pStorage = m_pGameContext->Storage();
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	
	if(!File)
	{
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Failed to open file for writing");
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
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Replay saved to %s with %d frames", pFilename, FrameCount);
	m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	
	return true;
}

bool CTasController::LoadReplay(const char *pFilename)
{
	if(!m_pGameContext)
		return false;
	
	IStorage *pStorage = m_pGameContext->Storage();
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	
	if(!File)
	{
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Failed to open file for reading");
		return false;
	}
	
	// Read header
	int Version;
	if(io_read(File, &Version, sizeof(Version)) != sizeof(Version))
	{
		io_close(File);
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Invalid replay file format");
		return false;
	}
	
	if(Version != 1)
	{
		io_close(File);
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Unsupported replay version");
		return false;
	}
	
	int FrameCount;
	if(io_read(File, &FrameCount, sizeof(FrameCount)) != sizeof(FrameCount))
	{
		io_close(File);
		m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "Failed to read frame count");
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
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Replay loaded from %s with %d frames", pFilename, FrameCount);
	m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	
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
	if(m_pGameContext)
		return m_pGameContext->Server()->Tick();
	return 0;
}

void CTasController::ResetCharacter(int ClientID)
{
	if(!m_pGameContext || !m_pGameContext->m_apPlayers[ClientID])
		return;
	
	CCharacter *pChar = m_pGameContext->m_apPlayers[ClientID]->GetCharacter();
	if(pChar)
	{
		// Kill and respawn the character
		pChar->Die(0, WEAPON_SELF);
		m_pGameContext->m_apPlayers[ClientID]->Respawn();
	}
}
