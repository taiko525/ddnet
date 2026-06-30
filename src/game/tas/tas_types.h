#ifndef GAME_TAS_TAS_TYPES_H
#define GAME_TAS_TAS_TYPES_H

#include <engine/shared/tick.h>
#include <game/generated/protocol.h>
#include <vector>
#include <cstdint>

// TAS 输入帧数据结构
// 存储单帧的所有玩家输入信息
struct STasInputFrame
{
    int64_t m_Tick;              // 帧对应的 tick 数
    int m_Direction;             // 移动方向 (-1, 0, 1)
    int m_Jump;                  // 跳跃输入 (0 或 1)
    int m_Fire;                  // 开火/钩子输入 (0 或 1)
    int m_Hook;                  // 钩子状态 (0 或 1)
    int m_PlayerAngle;           // 玩家角度 (0-255, 对应 0-360 度)
    bool m_WantHook;             // 是否希望使用钩子
    int m_Force;                 // 力度控制 (用于武器)
    
    // 从游戏输入转换到 TAS 帧
    static STasInputFrame FromGameInput(int64_t Tick, const CNetObj_PlayerInput *pInput)
    {
        STasInputFrame Frame;
        Frame.m_Tick = Tick;
        Frame.m_Direction = pInput->m_Direction;
        Frame.m_Jump = pInput->m_Jump;
        Frame.m_Fire = pInput->m_Fire;
        Frame.m_Hook = pInput->m_Hook;
        Frame.m_PlayerAngle = pInput->m_TargetAngle;
        Frame.m_WantHook = (pInput->m_Hook != 0);
        Frame.m_Force = pInput->m_Force;
        return Frame;
    }
    
    // 转换回游戏输入
    void ToGameInput(CNetObj_PlayerInput *pInput) const
    {
        pInput->m_Direction = m_Direction;
        pInput->m_Jump = m_Jump;
        pInput->m_Fire = m_Fire;
        pInput->m_Hook = m_Hook;
        pInput->m_TargetAngle = m_PlayerAngle;
        pInput->m_Force = m_Force;
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
    
    // 从角色状态创建快照
    static STasStateSnapshot FromCharacter(class CCharacter *pChar, int64_t Tick)
    {
        STasStateSnapshot Snapshot;
        Snapshot.m_Tick = Tick;
        
        if(pChar)
        {
            Snapshot.m_PosX = pChar->m_Pos.x;
            Snapshot.m_PosY = pChar->m_Pos.y;
            Snapshot.m_VelX = pChar->m_Vel.x;
            Snapshot.m_VelY = pChar->m_Vel.y;
            Snapshot.m_Direction = pChar->m_Direction;
            Snapshot.m_Jumped = pChar->m_Jumped;
            Snapshot.m_JumpedTotal = pChar->m_JumpedTotal;
            Snapshot.m_HookState = pChar->m_Core.m_Hook.m_State;
            Snapshot.m_HookX = pChar->m_Core.m_Hook.m_Pos.x;
            Snapshot.m_HookY = pChar->m_Core.m_Hook.m_Pos.y;
            Snapshot.m_HookTick = pChar->m_Core.m_Hook.m_Tick;
            Snapshot.m_Weapon = pChar->m_Weapon;
            Snapshot.m_Ammo = pChar->m_AmmoCount;
            Snapshot.m_Health = pChar->m_Health;
            Snapshot.m_Armor = pChar->m_Armor;
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
    CTasReplayBuffer() : m_CurrentTick(0), m_IsRecording(false), m_IsPlaying(false) {}
    
    // 添加输入帧
    void AddFrame(const STasInputFrame &Frame)
    {
        m_Frames.push_back(Frame);
    }
    
    // 获取指定 tick 的输入帧
    const STasInputFrame* GetFrame(int64_t Tick) const
    {
        if(Tick < 0 || Tick >= (int64_t)m_Frames.size())
            return nullptr;
        return &m_Frames[Tick];
    }
    
    // 获取帧数
    size_t Size() const { return m_Frames.size(); }
    
    // 清除所有帧
    void Clear() 
    { 
        m_Frames.clear(); 
        m_CurrentTick = 0;
        m_IsRecording = false;
        m_IsPlaying = false;
    }
    
    // 设置当前回放位置
    void SetCurrentTick(int64_t Tick)
    {
        if(Tick >= 0 && Tick <= (int64_t)m_Frames.size())
            m_CurrentTick = Tick;
    }
    
    int64_t GetCurrentTick() const { return m_CurrentTick; }
    
    void SetRecording(bool Recording) { m_IsRecording = Recording; }
    void SetPlaying(bool Playing) { m_IsPlaying = Playing; }
    
    bool IsRecording() const { return m_IsRecording; }
    bool IsPlaying() const { return m_IsPlaying; }
    
private:
    std::vector<STasInputFrame> m_Frames;
    int64_t m_CurrentTick;
    bool m_IsRecording;
    bool m_IsPlaying;
};

// TAS 系统配置
struct STasConfig
{
    int m_TPS;                   // 每秒 tick 数 (默认 50)
    bool m_Paused;               // 是否暂停
    int m_StepSize;              // 逐帧导航的步长 (默认 1 tick)
    
    STasConfig() : m_TPS(50), m_Paused(false), m_StepSize(1) {}
};

#endif // GAME_TAS_TAS_TYPES_H
