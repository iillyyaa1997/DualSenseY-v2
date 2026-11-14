#ifndef BATTERYLEDS_H
#define BATTERYLEDS_H

#include <cstdint>

// Shows a DualSense battery level on the five player LEDs under the touchpad, one step per full
// 20%, drawn like the PlayStation player numbers (newer controller revisions only light the LEDs
// in symmetric groups: centre, inner pair, outer pair):
//   1 = centre, 2 = inner pair, 3 = centre + outer pair, 4 = inner + outer pairs, 5 = all
// Unplugged: the level stays lit; at 20% or less the centre LED blinks.
// Charging: the level and the next one take turns. Charged: all five blink while the cable stays in.
// (The pad fades LEDs in by itself and switches them off at once; it has no smooth way out.)
class BatteryLeds {
public:
	// duaLib PowerState values
	static constexpr uint8_t POWER_DISCHARGING = 0x00;
	static constexpr uint8_t POWER_CHARGING = 0x01;
	static constexpr uint8_t POWER_COMPLETE = 0x02;

	static constexpr uint8_t LOW_PERCENT = 20;
	// A freshly connected pad reads as discharging at 0% until its first input report arrives
	static constexpr int64_t CONFIRM_MS = 3000;
	// Each picture of a blink or of the charging turns lasts this long
	static constexpr int64_t HALF_PERIOD_MS = 1000;

	struct Show {
		int level;  // -1 = leave the player number, 0 = all off, 1-5 = steps
		int dim;    // brightness steps below the user's choice (kept 0)
	};

	Show Update(bool connected, uint8_t powerState, uint8_t percent, int64_t nowMs) {
		if (!connected || powerState > POWER_COMPLETE) {
			m_ConnectedAt = -1;
			return {-1, 0};
		}
		if (m_ConnectedAt < 0) m_ConnectedAt = nowMs;
		if (nowMs - m_ConnectedAt < CONFIRM_MS) return {-1, 0};

		int full = FullSteps(percent);
		if (powerState == POWER_COMPLETE || (powerState == POWER_CHARGING && full == 5))
			return {Alternate(5, 0, nowMs), 0};
		if (powerState == POWER_CHARGING)
			return {Alternate(full, full + 1, nowMs), 0};
		if (percent <= LOW_PERCENT)
			return {Alternate(1, 0, nowMs), 0};
		return {full, 0};
	}

	// Full 20% steps: 0-10% -> 0, 20-30% -> 1, 40-50% -> 2, 60-70% -> 3, 80-90% -> 4, 100% -> 5
	static int FullSteps(uint8_t percent) {
		int steps = percent / 20;
		return steps > 5 ? 5 : steps;
	}

	static int Alternate(int first, int second, int64_t nowMs) {
		return (nowMs / HALF_PERIOD_MS) % 2 == 0 ? first : second;
	}

private:
	int64_t m_ConnectedAt = -1;
};

#endif
