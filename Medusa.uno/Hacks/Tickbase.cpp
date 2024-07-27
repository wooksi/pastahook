
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../includes.hpp"
#include "Tickbase.h"
#include "AntiAim.h"
#include "../SDK/ClientState.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/NetworkChannel.h"
#include "../Hacks/EnginePrediction.h"
#include "../SDK/Input.h"
#include "../SDK/Prediction.h"
UserCmd* command;
static bool doDefensive{ false };
EnginePrediction::NetvarData netvars{ };

void Tickbase::getCmd(UserCmd* cmd)
{
    command = cmd;
}

void Tickbase::start(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
    {
        hasHadTickbaseActive = false;
        return;
    }
    if (cmd->buttons & UserCmd::IN_ATTACK)
    {
        config->tickbase.lastFireShiftTick = memory->globalVars->tickCount + config->tickbase.onshotFlAmount;
        config->tickbase.readyFire = true;
    }

    if (!config->tickbase.doubletap.isActive() && !config->tickbase.hideshots.isActive())
        config->tickbase.DisabledTickbase = true;
    else
        config->tickbase.DisabledTickbase = false;
    if (const auto netChannel = interfaces->engine->getNetworkChannel(); netChannel)
        if (netChannel->chokedPackets > chokedPackets)
            chokedPackets = netChannel->chokedPackets;

    auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    if (activeWeapon->isGrenade() || activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver || activeWeapon->itemDefinitionIndex2() == WeaponId::C4)
        return;

    auto weaponData = activeWeapon->getWeaponData();
    if (!weaponData)
        return;

    auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    if (!config->tickbase.doubletap.isActive() && !config->tickbase.hideshots.isActive())
    {
        if (hasHadTickbaseActive)
            shiftOffensive(cmd, ticksAllowedForProcessing, true);
        hasHadTickbaseActive = false;
        return;
    }

    if (config->tickbase.doubletap.isActive())
    {
        targetTickShift = 16;
        breakLagComp(cmd);
    }
    else if (config->tickbase.hideshots.isActive())
        targetTickShift = 10;

    targetTickShift = std::clamp(targetTickShift, 0, maxUserCmdProcessTicks - 1);
    hasHadTickbaseActive = true;
}

void Tickbase::end(UserCmd* cmd, bool sendPacket) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    auto weapon = localPlayer->getActiveWeapon();
    if (!weapon)
        return;

    if (!config->tickbase.doubletap.isActive() && !config->tickbase.hideshots.isActive())
    {
        targetTickShift = 0;
        return;
    }

    if (weapon->isKnife() && cmd->buttons & UserCmd::IN_ATTACK2 && config->tickbase.doubletap.isActive())
        shiftOffensive(cmd, targetTickShift);
    if (cmd->buttons & UserCmd::IN_ATTACK && config->tickbase.doubletap.isActive())
        shiftOffensive(cmd, targetTickShift);
    else if (cmd->buttons & UserCmd::IN_ATTACK && config->tickbase.hideshots.isActive())
        shiftOffensive(cmd, targetTickShift);
}

bool Tickbase::shiftOffensive(UserCmd* cmd, int shiftAmount, bool forceShift) noexcept
{
    if (!canShiftDT(shiftAmount, forceShift))
        return false;

    auto weapon = localPlayer->getActiveWeapon();
    if (weapon->itemDefinitionIndex2() == WeaponId::Revolver)
        return false;

    realTime = memory->globalVars->realtime;
    shiftedTickbase = shiftAmount;
    shiftCommand = cmd->commandNumber;
    tickShift = shiftAmount;
    return true;
}

void Tickbase::breakLagComp(UserCmd* cmd)
{
    static int defensive_tick = memory->clientState->lastServerTickTime;
    static bool can_defensive = false;

    if (abs(memory->clientState->m_clock_drift_mgr.m_server_tick - defensive_tick) > 1)
    {
        defensive_tick = memory->clientState->m_clock_drift_mgr.m_server_tick;

        if (isPeeking)
        {
            if (!can_defensive)
            {
                can_defensive = true;
                sendPacket = true;

                /* set shift amnt. */
                targetTickShift = 16;
            }
        }
        else
        {
            can_defensive = false;
            isPeeking = false;

            /* set shift amnt. */
            targetTickShift = 16;
        }
    }
    else
        targetTickShift = 16;
}

bool Tickbase::shiftDefensive(UserCmd* cmd, int shiftAmount, bool forceShift) noexcept
{
    if (!canShiftDT(shiftAmount, forceShift))
        return false;

    auto weapon = localPlayer->getActiveWeapon();
    if (weapon->itemDefinitionIndex2() == WeaponId::Revolver)
        return false;

    realTime = memory->globalVars->realtime;
    shiftedTickbase = shiftAmount;
    shiftCommand = cmd->commandNumber;
    tickShift = shiftAmount;
    for (int i = 0; i < shiftAmount; i++)
    {
        ++memory->clientState->netChannel->chokedPackets;
    }
    return true;
}

bool Tickbase::shiftHideShots(UserCmd* cmd, int shiftAmount, bool forceShift) noexcept
{
    if (!canShiftHS(shiftAmount, forceShift))
        return false;

    //tickShift = shiftAmount;
    shiftedTickbase = shiftAmount;
    return true;
}

bool Tickbase::canRun() noexcept
{
    static float spawnTime = 0.f;
    if (!interfaces->engine->isInGame() || !interfaces->engine->isConnected())
    {
        ticksAllowedForProcessing = 0;
        chokedPackets = 0;
        pauseTicks = 0;
        return true;
    }

    if (!localPlayer || !localPlayer->isAlive() || !targetTickShift)
    {
        ticksAllowedForProcessing = 0;
        return true;
    }

    if ((*memory->gameRules)->freezePeriod())
    {
        realTime = memory->globalVars->realtime;
        return true;
    }

    if (spawnTime != localPlayer->spawnTime())
    {
        spawnTime = localPlayer->spawnTime();
        ticksAllowedForProcessing = 0;
        pauseTicks = 0;
    }

    if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
    {
        realTime = memory->globalVars->realtime;
        shiftedTickbase = 0;
        shiftCommand = 0;
        tickShift = 0;
        return true;
    }
    auto data = localPlayer.get()->getActiveWeapon();
    float recharge_time = 0;
    recharge_time = 0.24825f;

    static ConVar* sv_maxusrcmdprocessticks = interfaces->cvar->findVar(skCrypt("sv_maxusrcmdprocessticks"));
    int iMaxShiftAmmount = sv_maxusrcmdprocessticks->getInt() - 1;
    static int iRechargeDelay = TIME_TO_TICKS(0.5);

    if ((*memory->gameRules)->isValveDS())
        iMaxShiftAmmount = min(6, iMaxShiftAmmount);

    if ((ticksAllowedForProcessing < targetTickShift || chokedPackets > maxUserCmdProcessTicks - targetTickShift) && memory->globalVars->realtime - realTime > recharge_time && (memory->globalVars->realtime - AntiAim::getLastShotTime() > 1.0f || hasHadTickbaseActive == false))
    {       
        ticksAllowedForProcessing = min(ticksAllowedForProcessing++, maxUserCmdProcessTicks);
        chokedPackets = max(chokedPackets--, 0);
        pauseTicks++;
        bShouldRecharge = true;
        return false;
    }
    else if (chokedPackets >= iMaxShiftAmmount)
        bShouldRecharge = false;

    return true;
}


bool Tickbase::canShiftDT(int shiftAmount, bool forceShift) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return false;

    float recharge_time = 0;
    recharge_time = 0.24825f;
    if (!shiftAmount || shiftAmount > ticksAllowedForProcessing || memory->globalVars->realtime - realTime <= recharge_time)
        return false;

    if (forceShift)
        return true;

    if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return false;

    if (activeWeapon->isGrenade() || activeWeapon->isBomb()
        || activeWeapon->itemDefinitionIndex2() == WeaponId::Healthshot)
        return false;

    const float shiftTime = (localPlayer->tickBase() - shiftAmount) * memory->globalVars->intervalPerTick;
    if (localPlayer->nextAttack() > shiftTime)
        return false;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return false;

    return activeWeapon->nextPrimaryAttack() <= shiftTime;
}

bool Tickbase::canShiftHS(int shiftAmount, bool forceShift) noexcept
{
    if (config->tickbase.doubletap.isActive())
        return false;

    if (!localPlayer || !localPlayer->isAlive())
        return false;

    if (!shiftAmount || shiftAmount > ticksAllowedForProcessing)
        return false;

    if (forceShift)
        return true;

    if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return false;

    if (activeWeapon->isKnife() ||
        activeWeapon->isGrenade() || activeWeapon->isBomb()
        || activeWeapon->itemDefinitionIndex2() == WeaponId::Healthshot)
        return false;

    const float shiftTime = (localPlayer->tickBase() - shiftAmount) * memory->globalVars->intervalPerTick;
    if (localPlayer->nextAttack() > shiftTime)
        return false;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return false;

    return activeWeapon->nextPrimaryAttack() <= shiftTime;
}

int calc_correction_ticks()
{
    //return valve::g_global_vars->m_max_clients <= 1
    //	? -1 : valve::to_ticks(std::clamp(g_context->cvars().m_sv_clockcorrection_msecs->get_float() / 1000.f, 0.f, 1.f));

    float correction_ms; // st7
    float v4; // xmm0_4
    float v6; // [esp+0h] [ebp-10h]

    auto clock = interfaces->cvar->findVar(skCrypt("sv_clockcorrection_msecs"));

    if (!clock || memory->globalVars->maxClients <= 1)
        return -1;

    correction_ms = clock->getFloat();
    v4 = 1.0f;
    v6 = correction_ms / 1000.0f;

    if (v6 <= 1.0f)
    {
        v4 = 0.0f;
        if (v6 >= 0.0f)
            v4 = correction_ms / 1000.0f;
    }

    return ((v4 / memory->globalVars->intervalPerTick) + 0.5f);
}

int Tickbase::adjust_tick_base(const int old_new_cmds, const int total_new_cmds, const int delta) noexcept
{
    // russian autists can't paste from src leak.
    // https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/server/player.cpp#L3475

    // fixed antipaste
    auto ret = 0;

    //const auto flCorrectionSeconds = std::clamp(g_context->cvars().m_sv_clockcorrection_msecs->get_float() / 1000.0f, 0.0f, 1.0f);
    const auto correction_ticks = calc_correction_ticks();

    if (correction_ticks != -1)
    {
        // const auto& prev_local_data = g_eng_pred->local_data().at(valve::g_client_state->m_last_cmd_out % 150);

        if (netvars.spawn_time == localPlayer->spawnTime())
        {
            ret = netvars.tickbase + 1;

            const auto tick_count = ret + old_new_cmds - targetTickShift;
            const auto ideal_final_tick = tick_count + correction_ticks;

            const auto too_fast_limit = ideal_final_tick + correction_ticks;
            const auto too_slow_limit = ideal_final_tick - correction_ticks;

            const auto adjusted_final_tick = ret + total_new_cmds;

            if (adjusted_final_tick > too_fast_limit || adjusted_final_tick < too_slow_limit)
                ret = ideal_final_tick - total_new_cmds;
        }
    }

    if (ret != 0)
        return ret;

    return (netvars.spawn_time != localPlayer->spawnTime() ? netvars.tickbase : localPlayer->tickBase()) - (delta + 3); // fixed antipaste (was adding 3 cause meme russian code)
}

void Tickbase::getCorrectTickbase(int commandNumber) noexcept
{
    if (commandNumber <= 0)
        return;

    if (commandNumber == shiftCommand)
        localPlayer->tickBase() -= shiftedTickbase/* + memory->globalVars->simTicksThisFrame*/;
    else if (commandNumber == shiftCommand + 1)
        localPlayer->tickBase() += shiftedTickbase/* - memory->globalVars->simTicksThisFrame*/;
}

int& Tickbase::pausedTicks() noexcept
{
    return pauseTicks;
}

//If you have dt enabled, you need to shift 13 ticks, so it will return 13 ticks
//If you have hs enabled, you need to shift 9 ticks, so it will return 7 ticks
int Tickbase::getTargetTickShift() noexcept
{
    return targetTickShift;
}

int Tickbase::getTickshift() noexcept
{
    return tickShift;
}

void Tickbase::resetTickshift() noexcept
{
    shiftedTickbase = tickShift;
    if (config->tickbase.teleport && config->tickbase.doubletap.isActive())
    {
        ticksAllowedForProcessing = max(ticksAllowedForProcessing - tickShift, 0);
    }
    tickShift = 0;
}

bool& Tickbase::isFinalTick() noexcept
{
    return finalTick;
}

bool& Tickbase::isShifting() noexcept
{
    return shifting;
}

void Tickbase::updateInput() noexcept
{
    config->tickbase.doubletap.handleToggle();
    config->tickbase.hideshots.handleToggle();
}

void Tickbase::reset() noexcept
{
    isRecharging = false;
    shifting = false;
    finalTick = false;
    hasHadTickbaseActive = false;
    sendPacket = false;

    pauseTicks = 0;
    chokedPackets = 0;
    tickShift = 0;
    shiftCommand = 0;
    shiftedTickbase = 0;
    ticksAllowedForProcessing = 0;
    realTime = 0.0f;
}