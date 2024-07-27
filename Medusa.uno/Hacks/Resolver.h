#pragma once

#include "Animations.h"

#include "../SDK/GameEvent.h"
#include "../SDK/Entity.h"

namespace Resolver
{
	int missed_shots[65];
	bool updating_animation;

	void reset() noexcept;

	void processMissedShots() noexcept;
	void saveRecord(int playerIndex, float playerSimulationTime) noexcept;
	void getEvent(GameEvent* event) noexcept;

	void runPreUpdate(Animations::Players player, Entity* entity) noexcept;
	void runPostUpdate(Animations::Players player, Entity* entity) noexcept;

	void updateEventListeners(bool forceRemove = false) noexcept;

	float BruteForce(Entity* entity, bool roll);
	void ResolveAir(Animations::Players player, Entity* entity) noexcept;
	void detect_jitter(Animations::Players player, Entity* entity) noexcept;
	void anim_layers(Animations::Players player, Entity* entity) noexcept;
	void apply(Animations::Players player, Entity* entity) noexcept;
	
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