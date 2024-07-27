#pragma once

struct UserCmd;

namespace Tickbase
{
	bool isPeeking{ false };
	bool sendPacket{ false };
	float realTime{ 0.0f };
	int targetTickShift{ 0 };
	int tickShift{ 0 };
	int shiftCommand{ 0 };
	int shiftedTickbase{ 0 };
	int ticksAllowedForProcessing{ 0 };
	int chokedPackets{ 0 };
	int pauseTicks{ 0 };
	bool warmup = true;
	bool shifting{ false };
	int tickbase_shift = 0;
	int shift_time = 0;
	bool finalTick{ false };
	bool hasHadTickbaseActive{ false };
	int nextShiftAmount = 0;
	bool isRecharging{ false };
	int DeftargetTickShift{ 0 };
	bool m_bIsPeeking = false;
	bool m_bIsPrevPeek = false;
	int fixed_tickbase = 0;
	int next_tickbase_shift = 0;
	bool revolver_working = false;
	bool send_packet = false;
	bool dotelepeek = false;
	bool bShouldRecharge = false;

	void getCmd(UserCmd* cmd);
	void start(UserCmd* cmd) noexcept;
	void end(UserCmd* cmd, bool sendPacket) noexcept;
	bool shiftOffensive(UserCmd* cmd, int amount, bool forceShift = false) noexcept;
	void breakLagComp(UserCmd* cmd);
	bool shiftDefensive(UserCmd* cmd, int amount, bool forceShift = false) noexcept;
	bool shiftHideShots(UserCmd* cmd, int shiftAmount, bool forceShift = false) noexcept;
	bool canRun() noexcept;
	bool DTWeapon();
	void Telepeek();
	bool CanFireWeapon(float curtime);
	bool CanHideWeapon(float curtime);
	float Firerate();
	bool CanFireWithExploit(int m_iShiftedTick);
	bool CanHideWithExploit(int m_iShiftedTick);
	bool canShiftDT(int shiftAmount, bool forceShift = false) noexcept;
	bool canShiftHS(int shiftAmount, bool forceShift = false) noexcept;
	int adjust_tick_base(const int old_new_cmds, const int total_new_cmds, const int delta) noexcept;
	void getCorrectTickbase(int commandNumber) noexcept;
	int& pausedTicks() noexcept;
	int getTargetTickShift() noexcept;
	int getTickshift() noexcept;
	void resetTickshift() noexcept;
	bool& isFinalTick() noexcept;
	bool& isShifting() noexcept;
	void updateInput() noexcept;
	void reset() noexcept;

	Vector eye_pos = ZERO;
	Vector velocity = ZERO;

	int doubletapCharge = 0,
		ticksToShift = 0,
		commandNumber = 0,
		lastShift = 0,
		shiftAmount = 0,
		maxTicks = 17;
	bool isShifting1 = false;
	bool charged = false;
}