#include "AimbotFunctions.h"
#include "Animations.h"
#include "Resolver.h"

#include "../Logger.h"
#include "../Math.h"

#include "../SDK/GameEvent.h"
#include "../SDK/Entity.h"

std::deque<Resolver::SnapShot> snapshots;

// smexy runs u

struct resolver_info_t
{
    int side{};
    bool is_jittering{};
    int mode{};
    float foot_yaw{};
};

resolver_info_t resolver_info[65];

bool resolver = true;

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
            players->at(i).misses = 0;
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
        Animations::setPlayer(index)->misses = 0;
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
        std::string sideHit = info.side == 1 ? "1" : info.side == -1 ? "-1" : "0"; // 1 = right  -1 = left    0 = middle
        std::string jittering = info.is_jittering ? "true" : "false"; // check if they were jittering
        Logger::addLog("Hit player | " + std::string(entity->getPlayerName()) + "  | anim_layers | side: " + sideHit + " | Jitter: " + jittering);


        snapshots.pop_front(); //Hit somebody so dont calculate
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
            std::string missed = "missed shot on " + entity->getPlayerName() + " due to resolver";
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
}

void Resolver::detect_jitter(Animations::Players player, Entity* entity) noexcept
{
    auto& info = resolver_info[entity->index()];

    const auto m_layer_delta1 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
    const auto m_layer_delta2 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
    const auto m_layer_delta3 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);

    if (m_layer_delta1 == m_layer_delta2 && m_layer_delta2 == m_layer_delta3)
    {
        info.foot_yaw = 90.f;
        info.is_jittering = true;

        // only update fake when  not choking packets
        info.side = player.chokedPackets == 0;
    }
    else
    {
        info.is_jittering = false;
    }
}

void Resolver::anim_layers(Animations::Players player, Entity* entity) noexcept
{
    auto& info = resolver_info[entity->index()];
    const float eye_yaw = entity->getAnimstate()->eyeYaw;
    float max_rotation = entity->getMaxDesyncAngle();

    if (entity->velocity().length2D() <= 2.5f) {
        const float angle_difference = Helpers::angleDiff(eye_yaw, entity->getAnimstate()->footYaw);
        info.side = 2 * angle_difference <= 0.0f ? 1 : -1;
    }
    else
    {
        if (!static_cast<int>(player.layers[12].weight * 1000.f) && entity->velocity().length2D() > 3.f) {
            const auto m_layer_delta1 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
            const auto m_layer_delta2 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);

            if (const auto m_layer_delta3 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate); m_layer_delta1 < m_layer_delta2
                || m_layer_delta3 <= m_layer_delta2
                || static_cast<signed int>((m_layer_delta2 * 1000.0f)))
            {
                if (m_layer_delta1 >= m_layer_delta3
                    && m_layer_delta2 > m_layer_delta3
                    && !static_cast<signed int>((m_layer_delta3 * 1000.0f)))
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



void Resolver::apply(Animations::Players player, Entity* entity) noexcept
{
    if (!entity || !entity->isAlive())
        return;

    auto& info = resolver_info[entity->index()];

        detect_jitter(player, entity);
        anim_layers(player, entity);

    entity->getAnimstate()->footYaw = entity->eyeAngles().y + resolver_info[entity->index()].foot_yaw;
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