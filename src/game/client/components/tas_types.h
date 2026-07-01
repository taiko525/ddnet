#ifndef GAME_TAS_TAS_TYPES_H
#define GAME_TAS_TAS_TYPES_H

#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <vector>
#include <cstdint>

// TAS 输入帧数据结构
// 存储单帧的所有玩家输入信息
struct STasInputFrame
{
    int m_ClientID;              // 客户端 ID
    int m_Direction;             // 移动方向 (-1, 0, 1)
    int m_Jump;                  // 跳跃输入 (0 或 1)
    int m_Fire;                  // 开火/钩子输入 (0 或 1)
    int m_Hook;                  // 钩子状态 (0 或 1)
    int m_TargetX;               // 目标 X 坐标
    int m_TargetY;               // 目标 Y 坐标
    int m_WantedWeapon;          // 期望武器
    
    // 从游戏输入转换到 TAS 帧
    static STasInputFrame FromGameInput(int64_t Tick, const CNetObj_PlayerInput *pInput)
    {
        STasInputFrame Frame;
        Frame.m_ClientID = -1;
        Frame.m_Direction = pInput->m_Direction;
        Frame.m_Jump = pInput->m_Jump;
        Frame.m_Fire = pInput->m_Fire;
        Frame.m_Hook = pInput->m_Hook;
        Frame.m_TargetX = pInput->m_TargetX;
        Frame.m_TargetY = pInput->m_TargetY;
        Frame.m_WantedWeapon = pInput->m_WantedWeapon;
        return Frame;
    }
    
    // 转换回游戏输入
    void ToGameInput(CNetObj_PlayerInput *pInput) const
    {
        pInput->m_Direction = m_Direction;
        pInput->m_Jump = m_Jump;
        pInput->m_Fire = m_Fire;
        pInput->m_Hook = m_Hook;
        pInput->m_TargetX = m_TargetX;
        pInput->m_TargetY = m_TargetY;
        pInput->m_WantedWeapon = m_WantedWeapon;
    }
};

// 游戏状态快照 - 用于倒带/前进功能
struct STasStateSnapshot
{
    int64_t m_Tick;              // 快照对应的 tick
    float m_PosX;                // 角色 X 坐标
    float m_PosY;                // 角色 Y 坐标
    float m_VelX;                // X 方向速度
    float m_VelY;                // Y 方向速度
    int m_Direction;             // 面向方向
    int m_Jumped;                // 跳跃状态
    int m_JumpedTotal;           // 总跳跃次数
    int m_HookState;             // 钩子状态
    float m_HookX;               // 钩子 X 坐标
    float m_HookY;               // 钩子 Y 坐标
    int m_HookTick;              // 钩子 tick
    int m_Weapon;                // 当前武器
    int m_Ammo;                  // 弹药数量
    int m_Health;                // 生命值
    int m_Armor;                 // 护甲值
    
    // 从角色状态创建快照 (using CNetObj_Character instead of CCharacter)
    static STasStateSnapshot FromCharacter(const CNetObj_Character *pCharData, int64_t Tick)
    {
        STasStateSnapshot Snapshot;
        Snapshot.m_Tick = Tick;
        
        if(pCharData)
        {
            Snapshot.m_PosX = pCharData->m_X / 256.0f;
            Snapshot.m_PosY = pCharData->m_Y / 256.0f;
            Snapshot.m_VelX = pCharData->m_VelX / 256.0f;
            Snapshot.m_VelY = pCharData->m_VelY / 256.0f;
            Snapshot.m_Direction = pCharData->m_Direction;
            Snapshot.m_Jumped = pCharData->m_Jumped;
            Snapshot.m_JumpedTotal = 0; // Not available in snap
            Snapshot.m_HookState = pCharData->m_HookState;
            Snapshot.m_HookX = pCharData->m_HookX / 256.0f;
            Snapshot.m_HookY = pCharData->m_HookY / 256.0f;
            Snapshot.m_HookTick = pCharData->m_HookTick;
            Snapshot.m_Weapon = pCharData->m_Weapon;
            Snapshot.m_Ammo = pCharData->m_AmmoCount;
            Snapshot.m_Health = pCharData->m_Health;
            Snapshot.m_Armor = pCharData->m_Armor;
        }
        else
        {
            // 默认值（角色不存在时）
            Snapshot.m_PosX = 0;
            Snapshot.m_PosY = 0;
            Snapshot.m_VelX = 0;
            Snapshot.m_VelY = 0;
            Snapshot.m_Direction = 0;
            Snapshot.m_Jumped = 0;
            Snapshot.m_JumpedTotal = 0;
            Snapshot.m_HookState = HOOK_IDLE;
            Snapshot.m_HookX = 0;
            Snapshot.m_HookY = 0;
            Snapshot.m_HookTick = 0;
            Snapshot.m_Weapon = WEAPON_GUN;
            Snapshot.m_Ammo = -1;
            Snapshot.m_Health = 10;
            Snapshot.m_Armor = 0;
        }
        
        return Snapshot;
    }
};

// 回放缓冲区 - 存储输入帧序列
class CTasReplayBuffer
{
public:
    CTasReplayBuffer() : m_CurrentFrame(0), m_IsRecording(false), m_IsPlaying(false) {}
    
    // 添加输入帧
    void AddFrame(const STasInputFrame &Frame)
    {
        m_Frames.push_back(Frame);
    }
    
    // 获取指定索引的输入帧
    const STasInputFrame* GetFrame(int Index) const
    {
        if(Index < 0 || Index >= (int)m_Frames.size())
            return nullptr;
        return &m_Frames[Index];
    }
    
    // 获取帧数
    size_t Size() const { return m_Frames.size(); }
    
    // 清除所有帧
    void Clear() 
    { 
        m_Frames.clear(); 
        m_CurrentFrame = 0;
        m_IsRecording = false;
        m_IsPlaying = false;
    }
    
    // 设置当前回放位置
    void SetCurrentFrame(int Frame)
    {
        if(Frame >= 0 && Frame <= (int)m_Frames.size())
            m_CurrentFrame = Frame;
    }
    
    int GetCurrentFrame() const { return m_CurrentFrame; }
    
    void SetRecording(bool Recording) { m_IsRecording = Recording; }
    void SetPlaying(bool Playing) { m_IsPlaying = Playing; }
    
    bool IsRecording() const { return m_IsRecording; }
    bool IsPlaying() const { return m_IsPlaying; }
    bool IsEmpty() const { return m_Frames.empty(); }
    
private:
    std::vector<STasInputFrame> m_Frames;
    int m_CurrentFrame;
    bool m_IsRecording;
    bool m_IsPlaying;
};

// TAS 系统配置
struct STasConfig
{
    int m_CurrentTPS;            // 当前每秒 tick 数 (默认 50)
    bool m_Paused;               // 是否暂停
    int m_FrameStep;             // 逐帧导航的步长 (默认 1 tick)
    
    STasConfig() : m_CurrentTPS(50), m_Paused(false), m_FrameStep(1) {}
};

#endif // GAME_TAS_TAS_TYPES_H
