#pragma once

#include "Animations.h"

#include "../SDK/GameEvent.h"
#include "../SDK/Entity.h"

struct resolver_info_t
{
    int side{};
    int delta{};
    int CurSide{};
    bool islowdelta{};
    bool is_jittering{};
    int resolve_yaw{};
    float bruteforce_angle{};
    int mode{};
    float velyaw_info;
    float foot_yaw{};
    int misses{};
    bool in_air = false;

    struct jitter_t
    {
        bool is_jitter{};

        float delta_cache[2]{};
        int cache_offset{};

        float yaw_cache[20]{};
        int yaw_cache_offset{};

        int jitter_ticks{};
        int static_ticks{};

        int jitter_tick{};

        __forceinline void reset()
        {
            is_jitter = false;

            cache_offset = 0;
            yaw_cache_offset = 0;

            jitter_ticks = 0;
            static_ticks = 0;

            jitter_tick = 0;

            std::memset(delta_cache, 0, sizeof(delta_cache));
            std::memset(yaw_cache, 0, sizeof(yaw_cache));
        }
    } jitter;


    __forceinline void reset()
    {
        side = 0;
        is_jittering = false;
        islowdelta = false;
        mode = 0;
        foot_yaw = 0.f;
        misses = 0;
        in_air = false;
        jitter.reset();
    }
};

namespace Resolver
{
    int FreestandSide[65];
    bool updating_animation;
    void reset() noexcept;

    void processMissedShots() noexcept;
    void saveRecord(int playerIndex, float playerSimulationTime) noexcept;
    void getEvent(GameEvent* event) noexcept;

    void runPreUpdate(Animations::Players player, Entity* entity) noexcept;
    void runPostUpdate(Animations::Players player, Entity* entity) noexcept;

    void updateEventListeners(bool forceRemove = false) noexcept;

    float resolve_shot(const Animations::Players& player, Entity* entity);
    bool GetLowDeltaState(Entity* entity);
    float BruteForce(Entity* entity, bool roll);
    void delta_side_detect(Entity* entity);
    void ResolveAir(Animations::Players player, Entity* entity) noexcept;
    void detect_side(Entity* entity, int* side);
    void setup_detect(Animations::Players& player, Entity* entity);
    void detect_jitter(Animations::Players player, Entity* entity) noexcept;
    void anim_layers(Animations::Players player, Entity* entity) noexcept;
    void apply(Animations::Players player, Entity* entity) noexcept;

    void resolve_backtrack(Animations::Players player, Entity* entity, int bestrecord);
    struct SnapShot
    {
        Animations::Players player;
        const Model* model{ };
        Vector eyePosition{};
        Vector bulletImpact{};
        bool gotImpact{ false };
        float time{ -1 };
        int playerIndex{ -1 };
        int backtrackRecord{ -1 };
    };
}