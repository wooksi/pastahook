#pragma once

#include "../SDK/FrameStage.h"
#include "../SDK/Vector.h"

#include <array>

struct UserCmd;
struct Vector;

static const float epsilon = 0.03125f;

class StoredData_t
{
public:
	int    m_tickbase;
	Vector  m_punch;
	Vector  m_punch_vel;
	Vector m_view_offset;
	float  m_velocity_modifier;

public:
	__forceinline StoredData_t() : m_tickbase{ }, m_punch{ }, m_punch_vel{ }, m_view_offset{ }, m_velocity_modifier{ } {};
};

enum EThinkMethods : int
{
	THINK_FIRE_ALL_FUNCTIONS = 0,
	THINK_FIRE_BASE_ONLY,
	THINK_FIRE_ALL_BUT_BASE,
};

namespace EnginePrediction
{
	void reset() noexcept;

	void update() noexcept;
	void run(UserCmd* cmd) noexcept;

	UserCmd* pLastCmd = nullptr;

	void save() noexcept;

	void restore() noexcept;

	void store() noexcept;
	void apply(FrameStage) noexcept;

	int getFlags() noexcept;
	Vector getVelocity() noexcept;
	bool isInPrediction() noexcept;

	struct NetvarData
	{
		int tickbase = -1;
		float spawn_time{};

		Vector aimPunchAngle{ };
		Vector aimPunchAngleVelocity{ };
		Vector m_vecVelocity = {};
		Vector m_vecOrigin = {};
		Vector baseVelocity{ };
		float duckAmount{ 0.0f };
		float duckSpeed{ 0.0f };
		float fallVelocity{ 0.0f };
		float thirdPersonRecoil{ 0.0f };
		Vector velocity{ };
		float velocityModifier{ 0.0f };
		Vector viewPunchAngle{ };
		Vector viewOffset{ };
		Vector network_origin{};
		Vector m_vecViewOffset = ZERO;
		float m_flThirdpersonRecoil = FLT_MIN;
		float m_flFallVelocity = FLT_MIN;

		float m_bIsFilled = false;


		int command_number = INT_MAX;

		static float checkDifference(float predicted, float original) noexcept
		{
			float delta = predicted - original;
			if (std::abs(delta) < 0.03125)
				return original;
			return predicted;
		}

		static Vector checkDifference(Vector predicted, Vector original) noexcept {
			Vector delta = predicted - original;
			if (std::abs(delta.x) < 0.03125 && std::abs(delta.y) < 0.03125f &&
				std::abs(delta.z) < 0.031254)
			{
				return original;
			}
			return predicted;
		}
	};

	float old_recoil_index{};
	float old_accuracy_penalty{};
	float predicted_inaccuracy{};
	float predicted_spread{};
	float spread{};

}

#define MULTIPLAYER_BACKUP 150
std::array< StoredData_t, MULTIPLAYER_BACKUP > m_data;