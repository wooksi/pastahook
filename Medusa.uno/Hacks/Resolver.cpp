#include "AimbotFunctions.h"
#include "Animations.h"
#include "Resolver.h"

#include "../Logger.h"
#include "../Math.h"

#include "../SDK/GameEvent.h"
#include "../SDK/Entity.h"

std::deque<Resolver::SnapShot> snapshots;

// smexy runs u

bool resolver = true;
resolver_info_t resolver_info[65];
#define M_RADPI 57.295779513082f
void Resolver::reset() noexcept
{
    snapshots.clear();
}

void Resolver::saveRecord(int playerIndex, float playerSimulationTime) noexcept
{
    const auto entity = interfaces->entityList->getEntity(playerIndex);
    const auto player = Animations::getPlayer(playerIndex);
    if (!player.gotMatrix || !entity)
        return;

    SnapShot snapshot;
    snapshot.player = player;
    snapshot.playerIndex = playerIndex;
    snapshot.eyePosition = localPlayer->getEyePosition();
    snapshot.model = entity->getModel();

    if (player.simulationTime == playerSimulationTime)
    {
        snapshots.push_back(snapshot);
        return;
    }

    for (int i = 0; i < static_cast<int>(player.backtrackRecords.size()); i++)
    {
        if (player.backtrackRecords.at(i).simulationTime == playerSimulationTime)
        {
            snapshot.backtrackRecord = i;
            snapshots.push_back(snapshot);
            return;
        }
    }
}

void Resolver::getEvent(GameEvent* event) noexcept
{
    if (!event || !localPlayer || interfaces->engine->isHLTV())
        return;

    switch (fnv::hashRuntime(event->getName())) {
    case fnv::hash("round_start"):
    {
        // Reset all
        auto players = Animations::setPlayers();
        if (players->empty())
            break;

        for (int i = 0; i < static_cast<int>(players->size()); i++)
        {
            auto& res_info = resolver_info[i];
            res_info.reset();
        }
        snapshots.clear();
        break;
    }
    case fnv::hash("player_death"):
    {
        // Reset player
        const auto playerId = event->getInt("userid");
        if (playerId == localPlayer->getUserId())
            break;

        const auto index = interfaces->engine->getPlayerFromUserID(playerId);
        auto& res_info = resolver_info[index];
        res_info.reset();
        break;
    }
    case fnv::hash("player_hurt"):
    {
        if (snapshots.empty())
            break;

        if (event->getInt("attacker") != localPlayer->getUserId())
            break;

        const auto hitgroup = event->getInt("hitgroup");
        if (hitgroup < HitGroup::Head || hitgroup > HitGroup::RightLeg)
            break;

        // Get the player entity
        const auto playerId = event->getInt("userid");
        const auto index = interfaces->engine->getPlayerFromUserID(playerId);
        const auto entity = interfaces->entityList->getEntity(index);
        if (!entity)
            break;

        // Log the side hit when a player is hurt
        const auto& info = resolver_info[index];
        std::string sideHit = info.side == 1 ? "1" : info.side == -1 ? "-1" : "0"; // 1 = right, -1 = left, 0 = middle
        std::string jittering = info.is_jittering ? "true" : "false"; // check if they were jittering
        std::string safety = info.is_jittering ? "not really" : "kinda"; /* @note by JannesBonk: ah yes, safety check $$$; @note by Drip: it's either YES or NO */
        std::string playerName = entity->getPlayerName(); // Get player name
        std::string footyaw = std::to_string(info.foot_yaw);
        std::string v39 = GetLowDeltaState(entity) ? "true" : "false"; // reversed
        
        /* @note by JannesBonk: ah yes, totally using AnimLayers 24/7 */
        Logger::addLog("hit player | " + playerName + " | AnimLayers | Side: " + sideHit + " | body_yaw: " + footyaw + "°" + " | History: " + " | Delta: " + v39 + " | Jitter: " + jittering + " | Safe: " + safety);

        snapshots.pop_front(); // Hit somebody so don't calculate       
        break;
    }
    case fnv::hash("bullet_impact"):
    {
        if (snapshots.empty())
            break;

        if (event->getInt("userid") != localPlayer->getUserId())
            break;

        auto& snapshot = snapshots.front();

        if (!snapshot.gotImpact)
        {
            snapshot.time = memory->globalVars->serverTime();
            snapshot.bulletImpact = Vector{ event->getFloat("x"), event->getFloat("y"), event->getFloat("z") };
            snapshot.gotImpact = true;
        }
        break;
    }
    default:
        break;
    }
    if (!resolver)
        snapshots.clear();
}

void Resolver::processMissedShots() noexcept
{
    if (!resolver)
    {
        snapshots.clear();
        return;
    }

    if (!localPlayer || !localPlayer->isAlive())
    {
        snapshots.clear();
        return;
    }

    if (snapshots.empty())
        return;

    if (snapshots.front().time == -1) //Didnt get data yet
        return;

    auto snapshot = snapshots.front();
    snapshots.pop_front(); //got the info no need for this
    if (!snapshot.player.gotMatrix)
        return;

    const auto entity = interfaces->entityList->getEntity(snapshot.playerIndex);
    if (!entity)
        return;

    const Model* model = snapshot.model;
    if (!model)
        return;

    StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
    if (!hdr)
        return;

    StudioHitboxSet* set = hdr->getHitboxSet(0);
    if (!set)
        return;

    const auto angle = AimbotFunction::calculateRelativeAngle(snapshot.eyePosition, snapshot.bulletImpact, Vector{ });
    const auto end = snapshot.bulletImpact + Vector::fromAngle(angle) * 2000.f;

    const auto matrix = snapshot.backtrackRecord == -1 ? snapshot.player.matrix.data() : snapshot.player.backtrackRecords.at(snapshot.backtrackRecord).matrix;

    bool resolverMissed = false;

    for (int hitbox = 0; hitbox < Hitboxes::Max; hitbox++)
    {
        if (AimbotFunction::hitboxIntersection(matrix, hitbox, set, snapshot.eyePosition, end))
        {
            resolverMissed = true;
            std::string missed = "missed shot on " + entity->getPlayerName() + " due to ?"; // @note by JannesBonk: amazing resolver miss log $$$
            if (snapshot.backtrackRecord > 0)
                missed += " ( bt: " + std::to_string(snapshot.backtrackRecord) + ", ";
            if (resolver_info[entity->index()].side < 2)
                missed += " side: " + std::to_string(resolver_info[entity->index()].side) + " )";
            Logger::addLog(missed);
            Animations::setPlayer(snapshot.playerIndex)->misses++;
            break;
        }
    }
    if (!resolverMissed)
        Logger::addLog(std::string("missed shot on " + entity->getPlayerName() + " due to spread"));

}

void Resolver::runPreUpdate(Animations::Players player, Entity* entity) noexcept
{
    if (!resolver)
        return;

    const auto misses = player.misses;
    if (!entity || !entity->isAlive())
        return;

    if (player.chokedPackets <= 0)
        return;

    ResolveAir(player, entity);
    anim_layers(player, entity);
}

void Resolver::runPostUpdate(Animations::Players player, Entity* entity) noexcept
{
    if (!resolver)
        return;

    const auto misses = player.misses;
    if (!entity || !entity->isAlive())
        return;

    if (player.chokedPackets <= 0)
        return;

    /* start calling ur shit */
    ResolveAir(player, entity);
    detect_jitter(player, entity);
    anim_layers(player, entity);    
}

float flAngleMod(float flAngle)
{
    return((360.0f / 65536.0f) * ((int32_t)(flAngle * (65536.0f / 360.0f)) & 65535));
}

float ApproachAngle(float flTarget, float flValue, float flSpeed)
{
    flTarget = flAngleMod(flTarget);
    flValue = flAngleMod(flValue);

    float delta = flTarget - flValue;

    if (flSpeed < 0)
        flSpeed = -flSpeed;

    if (delta < -180)
        delta += 360;
    else if (delta > 180)
        delta -= 360;

    if (delta > flSpeed)
        flValue += flSpeed;
    else if (delta < -flSpeed)
        flValue -= flSpeed;
    else
        flValue = flTarget;

    return flValue;
}

void SideCheck(int* side, Entity* entity)
{
    if (!entity || !entity->isAlive())
        return;
    PlayerInfo player_info;

    if (!interfaces->engine->getPlayerInfo(entity->index(), player_info))
        return;

    Vector src3D, dst3D, forward, right, up, src, dst;
    float back_two, right_two, left_two;
    Trace tr;
    TraceFilter filter = entity;

    Math::angle_vectors(Vector(0, Math::calculate_angle(localPlayer->getAbsOrigin(), entity->getAbsOrigin()).y, 0), &forward, &right, &up);

    filter.skip = entity;
    src3D = entity->eyeAngles();
    dst3D = src3D + (forward * 384);

    interfaces->engineTrace->traceRay({ src3D, dst3D }, MASK_SHOT, filter, tr);
    back_two = (tr.endpos - tr.startpos).length();


    interfaces->engineTrace->traceRay({ src3D + right * 35, dst3D + right * 35 }, MASK_SHOT, filter, tr);
    right_two = (tr.endpos - tr.startpos).length();

    interfaces->engineTrace->traceRay({ src3D - right * 35, dst3D - right * 35 }, MASK_SHOT, filter, tr);
    left_two = (tr.endpos - tr.startpos).length();

    if (left_two > right_two) {
        *side = 1;
    }
    else if (right_two > left_two) {
        *side = -1;
    }
}

float build_server_abs_yaw(Entity* entity, const float angle)
{
    Vector velocity = entity->velocity();
    const auto& anim_state = entity->getAnimstate();
    const float m_fl_eye_yaw = angle;
    float m_fl_goal_feet_yaw = 0.f;

    const float eye_feet_delta = Helpers::angleDiff(m_fl_eye_yaw, m_fl_goal_feet_yaw);

    static auto get_smoothed_velocity = [](const float min_delta, const Vector a, const Vector b) {
        const Vector delta = a - b;
        const float delta_length = delta.length();

        if (delta_length <= min_delta)
        {
            if (-min_delta <= delta_length)
                return a;
            const float i_radius = 1.0f / (delta_length + FLT_EPSILON);
            return b - delta * i_radius * min_delta;
        }
        const float i_radius = 1.0f / (delta_length + FLT_EPSILON);
        return b + delta * i_radius * min_delta;
        };

    if (const float spd = velocity.squareLength(); spd > std::powf(1.2f * 260.0f, 2.f))
    {
        const Vector velocity_normalized = velocity.normalized();
        velocity = velocity_normalized * (1.2f * 260.0f);
    }

    const float m_fl_choked_time = anim_state->lastUpdateTime;
    const float duck_additional = std::clamp(entity->duckAmount() + anim_state->duckAdditional, 0.0f, 1.0f);
    const float duck_amount = anim_state->animDuckAmount;
    const float choked_time = m_fl_choked_time * 6.0f;
    float v28;

    // clamp
    if (duck_additional - duck_amount <= choked_time)
        if (duck_additional - duck_amount >= -choked_time)
            v28 = duck_additional;
        else
            v28 = duck_amount - choked_time;
    else
        v28 = duck_amount + choked_time;

    const float fl_duck_amount = std::clamp(v28, 0.0f, 1.0f);

    const Vector animation_velocity = get_smoothed_velocity(m_fl_choked_time * 2000.0f, velocity, entity->velocity());
    const float speed = std::fminf(animation_velocity.length(), 260.0f);

    float fl_max_movement_speed = 260.0f;

    if (Entity* p_weapon = entity->getActiveWeapon(); p_weapon && p_weapon->getWeaponData())
        fl_max_movement_speed = std::fmaxf(p_weapon->getWeaponData()->maxSpeed, 0.001f);

    float fl_running_speed = speed / (fl_max_movement_speed * 0.520f);
    float fl_ducking_speed = speed / (fl_max_movement_speed * 0.340f);
    fl_running_speed = std::clamp(fl_running_speed, 0.0f, 1.0f);

    float fl_yaw_modifier = (anim_state->walkToRunTransition * -0.3f - 0.2f) * fl_running_speed + 1.0f;

    if (fl_duck_amount > 0.0f)
    {
        float fl_ducking_speed2 = std::clamp(fl_ducking_speed, 0.0f, 1.0f);
        fl_yaw_modifier += (fl_ducking_speed2 * fl_duck_amount) * (0.5f - fl_yaw_modifier);
    }

    constexpr float v60 = -58.f;
    constexpr float v61 = 58.f;

    const float fl_min_yaw_modifier = v60 * fl_yaw_modifier;

    if (const float fl_max_yaw_modifier = v61 * fl_yaw_modifier; eye_feet_delta <= fl_max_yaw_modifier)
    {
        if (fl_min_yaw_modifier > eye_feet_delta)
            m_fl_goal_feet_yaw = fabs(fl_min_yaw_modifier) + m_fl_eye_yaw;
    }
    else
    {
        m_fl_goal_feet_yaw = m_fl_eye_yaw - fabs(fl_max_yaw_modifier);
    }

    Helpers::normalizeYaw(m_fl_goal_feet_yaw);

    if (speed > 0.1f || fabs(velocity.z) > 100.0f)
    {
        m_fl_goal_feet_yaw = Helpers::approachAngle(
            m_fl_eye_yaw,
            m_fl_goal_feet_yaw,
            (anim_state->walkToRunTransition * 20.0f + 30.0f)
            * m_fl_choked_time);
    }
    else
    {
        m_fl_goal_feet_yaw = Helpers::approachAngle(
            entity->lby(),
            m_fl_goal_feet_yaw,
            m_fl_choked_time * 100.0f);
    }

    return m_fl_goal_feet_yaw;
}

float Resolver::resolve_shot(const Animations::Players& player, Entity* entity) {
    /* fix unrestricted shot */
    const float fl_pseudo_fire_yaw = Helpers::angleNormalize(Helpers::angleDiff(localPlayer->origin().y, player.matrix[8].origin().y));
    if (player.extended) {
        const float fl_left_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y + 58.f)));
        const float fl_right_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y - 58.f)));
        return fl_left_fire_yaw_delta > fl_right_fire_yaw_delta ? -58.f : 58.f;
    }
    const float fl_left_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y + 28.f)));
    const float fl_right_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y - 28.f)));

    return fl_left_fire_yaw_delta > fl_right_fire_yaw_delta ? -28.f : 28.f;
}

void Resolver::ResolveAir(Animations::Players player, Entity* entity) noexcept
{
    auto& info = resolver_info[entity->index()];
    auto& state = entity->getAnimstate();
    if (!state || state->onGround)
    {
        resolver_info[entity->index()].in_air = false;
        return;
    }

    resolver_info[entity->index()].in_air = true;
    float velyaw = Helpers::rad2deg(std::atan2(entity->velocity().y, entity->velocity().x));
    auto& misses = resolver_info[entity->index()].misses;
    float max_rotation = entity->getMaxDesyncAngle();
    float angle1 = 0;

    switch (misses % 1) // @note by JannesBonk & Drip: ???
    {
    case 0:
        entity->eyeAngles().y = info.foot_yaw;
        break;
    }
}

float get_backward_side(Entity* entity) {
    if (!entity->isAlive())
        return -1.f;
    const float result = Helpers::angleDiff(localPlayer->origin().y, entity->origin().y);
    return result;
}

float get_angle(Entity* entity) {
    return Helpers::angleNormalize(entity->eyeAngles().y);
}

float get_foword_yaw(Entity* entity) {
    return Helpers::angleNormalize(get_backward_side(entity) - 180.f);
}

void Resolver::detect_side(Entity* entity, int* side) {
    /* externs */
    Vector src3D, dst3D, forward, right, up;
    float back_two, right_two, left_two;
    Trace tr;
    Helpers::AngleVectors(Vector(0, get_backward_side(entity), 0), &forward, &right, &up);
    /* filtering */

    src3D = entity->getEyePosition();
    dst3D = src3D + (forward * 384);

    /* back engine tracers */
    interfaces->engineTrace->traceRay({ src3D, dst3D }, 0x200400B, { entity }, tr);
    back_two = (tr.endpos - tr.startpos).length();

    /* right engine tracers */
    interfaces->engineTrace->traceRay(Ray(src3D + right * 35, dst3D + right * 35), 0x200400B, { entity }, tr);
    right_two = (tr.endpos - tr.startpos).length();

    /* left engine tracers */
    interfaces->engineTrace->traceRay(Ray(src3D - right * 35, dst3D - right * 35), 0x200400B, { entity }, tr);
    left_two = (tr.endpos - tr.startpos).length();

    /* fix side */
    if (left_two > right_two) {
        *side = -1;
    }
    else if (right_two > left_two) {
        *side = 1;
    }
}

void Resolver::setup_detect(Animations::Players& player, Entity* entity) {

    // detect if player is using maximum desync.
    if (player.layers[3].cycle == 0.f)
    {
        if (player.layers[3].weight = 0.f)
        {
            player.extended = true;
        }
    }
    /* calling detect side */
    Resolver::detect_side(entity, &player.side);
    int side = player.side;
    /* bruting vars */
    float resolve_value = 50.f;
    static float brute = 0.f;
    float fl_max_rotation = entity->getMaxDesyncAngle();
    float fl_eye_yaw = entity->getAnimstate()->eyeYaw;
    float perfect_resolve_yaw = resolve_value;
    bool fl_foword = fabsf(Helpers::angleNormalize(get_angle(entity) - get_foword_yaw(entity))) < 90.f;
    int fl_shots = player.misses;

    /* clamp angle */
    if (fl_max_rotation < resolve_value) {
        resolve_value = fl_max_rotation;
    }

    /* detect if entity is using max desync angle */
    if (player.extended) {
        resolve_value = fl_max_rotation;
    }
    /* setup brting */
    if (fl_shots == 0) {
        brute = perfect_resolve_yaw * (fl_foword ? -side : side);
    }
    else {
        switch (fl_shots % 3) {
        case 0: {
            brute = perfect_resolve_yaw * (fl_foword ? -side : side);
        } break;
        case 1: {
            brute = perfect_resolve_yaw * (fl_foword ? side : -side);
        } break;
        case 2: {
            brute = 0;
        } break;
        }
    }

    /* fix goalfeet yaw */
    entity->getAnimstate()->footYaw = fl_eye_yaw + brute;
}

bool freestand_target(Entity* target, float* yaw)
{
    float dmg_left = 0.f;
    float dmg_right = 0.f;

    static auto get_rotated_pos = [](Vector start, float rotation, float distance)
        {
            float rad = DEG2RAD(rotation);
            start.x += cos(rad) * distance;
            start.y += sin(rad) * distance;

            return start;
        };

    if (!localPlayer || !target || !localPlayer->isAlive())
        return false;

    Vector local_eye_pos = target->getEyePosition();
    Vector eye_pos = localPlayer->getEyePosition();
    Vector angle = (local_eye_pos, eye_pos);

    auto backwards = target->eyeAngles().y; // angle.y;

    Vector pos_left = get_rotated_pos(eye_pos, angle.y + 90.f, 60.f);
    Vector pos_right = get_rotated_pos(eye_pos, angle.y - 90.f, -60.f);

    const auto wall_left = (local_eye_pos, pos_left,
        nullptr, nullptr, localPlayer);

    const auto wall_right = (local_eye_pos, pos_right,
        nullptr, nullptr, localPlayer);

    if (dmg_left == 0.f && dmg_right == 0.f)
    {
        *yaw = backwards;
        return false;
    }

    // we can hit both sides, lets force backwards
    if (fabsf(dmg_left - dmg_right) < 5.f)
    {
        *yaw = backwards;
        return false;
    }

    bool direction = dmg_left > dmg_right;
    *yaw = direction ? angle.y - 90.f : angle.y + 90.f;

    return true;
}

void Resolver::detect_jitter(Animations::Players player, Entity* entity) noexcept
{
    auto& info = resolver_info[entity->index()];
    auto& jitter = info.jitter;
    float current_yaw = entity->eyeAngles().y;

    // Update yaw cache with the current yaw
    jitter.yaw_cache[jitter.yaw_cache_offset % 16] = current_yaw;
    jitter.yaw_cache_offset = (jitter.yaw_cache_offset + 1) % 16;

    int static_ticks = 0;
    int jitter_ticks = 0;

    // Evaluate the differences in yaw cache
    for (int i = 0; i < 14; ++i) {
        float diff = std::fabsf(jitter.yaw_cache[i] - jitter.yaw_cache[i + 1]);

        if (diff <= 0.f) {
            static_ticks++;
        }
        else if (diff >= 15.f) {
            jitter_ticks++;
        }
    }

    // Check if the player is jittering based on ticks
    jitter.is_jitter = jitter_ticks > static_ticks;
    info.is_jittering = jitter.is_jitter;

}

void Resolver::anim_layers(Animations::Players player, Entity* entity) noexcept
{
    auto& info = resolver_info[entity->index()];
    const float eye_yaw = entity->getAnimstate()->eyeYaw;
    float max_rotation = entity->getMaxDesyncAngle();
    bool stand = entity->velocity().length2D() <= 0.1f; // @note by JannesBonk: ah yes, standing with micromovement $$$

    Resolver::setup_detect(player, entity);

    if (const bool extended = player.extended; !extended && fabs(max_rotation) > 58.f) {
        max_rotation = max_rotation / 1.8f;
    }
    if (entity->velocity().length2D() <= 2.5f) {
        const float angle_difference = Helpers::angleDiff(eye_yaw, entity->getAnimstate()->footYaw);
        info.side = 2 * angle_difference <= 0.0f ? 1 : -1;
        info.mode = 0; // stand
    }
    if (stand) /* stand fix */
    {


        const auto feet_delta = Helpers::angleNormalize(Helpers::angleDiff(Helpers::angleNormalize(entity->getAnimstate()->timeToAlignLowerBody), Helpers::angleNormalize((entity->eyeAngles().y))));

        const auto eye_diff = Helpers::angleDiff(entity->eyeAngles().y, (entity->getAnimstate()->eyeYaw));


        auto stopped_moving = (player.layers[ANIMATION_LAYER_ADJUST].sequence) == ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING;


        auto balance_adjust = (player.layers[ANIMATION_LAYER_ADJUST].sequence) == ACT_CSGO_IDLE_TURN_BALANCEADJUST;


        bool stopped_moving_this_frame = false;
        if (entity->getAnimstate()->velocityLengthXY <= 0.1f)
        {
            auto  m_duration_moving = ROTATION_MOVE;
            auto m_duration_still = entity->getAnimstate()->lastUpdateIncrement;

            auto stopped_moving_this_frame = entity->getAnimstate()->velocityLengthXY < 0;
            m_duration_moving == 0.0f;
            m_duration_still += (entity->getAnimstate()->lastUpdateIncrement);

        }


        if (entity->getAnimstate()->velocityLengthXY == 0.0f && entity->getAnimstate()->landAnimMultiplier == 0.0f && entity->getAnimstate()->lastUpdateIncrement > 0.0f)
        {
            const auto current_feet_yaw = entity->getAnimstate()->eyeYaw;
            const auto goal_feet_yaw = entity->getAnimstate()->eyeYaw;
            auto eye_delta = current_feet_yaw - goal_feet_yaw;

            if (goal_feet_yaw < current_feet_yaw)
            {
                if (eye_delta >= 180.0f)
                    eye_delta -= 360.0f;
            }
            else if (eye_delta <= -180.0f)
                eye_delta += 360.0f;

            if (eye_delta / entity->getAnimstate()->lastUpdateIncrement > 120.f)
            {
                (player.layers[ANIMATION_LAYER_ADJUST].sequence) == 0.0f;
                (player.layers[ANIMATION_LAYER_ADJUST].sequence) == 0.0f;

            }
        }

    }
    else
    {
        if (!((int)player.layers[12].weight * 1000.f) && entity->velocity().length2D() > 0.1) {

            auto m_layer_delta1 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
            auto m_layer_delta2 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
            auto m_layer_delta3 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);

            if (m_layer_delta1 < m_layer_delta2
                || m_layer_delta3 <= m_layer_delta2
                || (signed int)(float)(m_layer_delta2 * 1000.0))
            {
                if (m_layer_delta1 >= m_layer_delta3
                    && m_layer_delta2 > m_layer_delta3
                    && !(signed int)(float)(m_layer_delta3 * 1000.0))
                {
                    info.side = 1;
                }
            }
            else
            {
                info.side = -1;
            }
        }
    }
    info.foot_yaw = info.side * max_rotation;
}

void Resolver::delta_side_detect(Entity* entity)
{
    auto& info = resolver_info[entity->index()];
    float EyeDelta = Helpers::normalizeYaw(entity->eyeAngles().y);

    if (fabs(EyeDelta) > 5)
    {
        if (EyeDelta > 5)
            info.delta = -1;
        else if (EyeDelta < -5)
            info.delta = 1;
    }
    else
        info.delta = 0;
}

bool Resolver::GetLowDeltaState(Entity* entity) {
    auto animstate = entity->getAnimstate();

    float fl_eye_yaw = entity->eyeAngles().y;
    float fl_desync_delta = remainderf(fl_eye_yaw, animstate->footYaw);
    fl_desync_delta = std::clamp(fl_desync_delta, -60.f, 60.f);

    if (fabs(fl_desync_delta) < 35.f)
        return true;

    return false;
}

void Resolver::apply(Animations::Players player, Entity* entity) noexcept
{
    // @note from JannesBonk: C6262: Function uses '18340' bytes of stack. Consider moving some data to heap"
    if (!entity->isAlive())
        return;

    AnimationLayer ResolverLayers[3][13];
    float orig_rate = player.oldlayers[6].playbackRate;
    float_t CenterYawPlaybackRate = ResolverLayers[0][6].playbackRate;
    float_t RightYawPlaybackRate = ResolverLayers[1][6].playbackRate;
    float_t LeftYawPlaybackRate = ResolverLayers[2][6].playbackRate;

    float_t LeftYawDelta = fabsf(LeftYawPlaybackRate - orig_rate);
    float_t RightYawDelta = fabsf(RightYawPlaybackRate - orig_rate);
    float_t CenterYawDelta = fabsf(LeftYawDelta - RightYawDelta);

    auto& info = resolver_info[entity->index()];

    int side;
    SideCheck(&side, entity);

    if (LeftYawDelta > RightYawDelta)
        info.CurSide = -1;
    else if (LeftYawDelta < RightYawDelta)
        info.CurSide = 1;

    auto& misses = info.misses;
    float max_rotation = entity->getMaxDesyncAngle();
    float fl_eye_yaw = entity->getAnimstate()->eyeYaw;
    float fl_first_low_delta = (fl_eye_yaw + 35.0f);
    float fl_second_low_delta = (fl_eye_yaw + 29.0f);
    Resolver::delta_side_detect(entity); // low delta
    Resolver::setup_detect(player, entity); // detections

    if (player.shot) {
        entity->getAnimstate()->footYaw = fl_eye_yaw + resolve_shot(player, entity);
        return;
    } // onshot logic
    if (!player.extended && fabs(max_rotation) > 58.f)
    {
        max_rotation = max_rotation / 1.8f;
    } // max_rotation
    if (GetLowDeltaState(entity)) {
        switch (misses % 3) {
            // low delta
        case 0:
            entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y + fl_first_low_delta * info.delta); // low delta brute 1st
            break;
        case 1:
            entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y + fl_second_low_delta * info.delta); // low delta brute 2cd
            break;
        case 2:
            entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y); // zero
            break;
        }
    }
    switch (misses % 2) {
    case 0: //default
        entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y + info.foot_yaw); // animlayers 
        break;
    case 1: // backup
        entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y + max_rotation * info.CurSide); // animlayers
        break;
    }
}

void Resolver::resolve_backtrack(Animations::Players player, Entity* entity, int bestrecord) // [8] bt head only on 16 ticks
{
    detect_jitter(player, entity);
    ResolveAir(player, entity);
    anim_layers(player, entity);
}

void Resolver::updateEventListeners(bool forceRemove) noexcept
{
    class ImpactEventListener : public GameEventListener {
    public:
        void fireGameEvent(GameEvent* event) {
            getEvent(event);
        }
    };

    static ImpactEventListener listener;
    static bool listenerRegistered = false;

    if (resolver && !listenerRegistered) {
        interfaces->gameEventManager->addListener(&listener, "bullet_impact");
        listenerRegistered = true;
    }
    else if ((!resolver || forceRemove) && listenerRegistered) {
        interfaces->gameEventManager->removeListener(&listener);
        listenerRegistered = false;
    }
}
