#ifndef BATTERYLEDS_H
#define BATTERYLEDS_H

#include <cstdint>

// Shows a DualSense battery level on the five player LEDs under the touchpad, one step per full
// 20%, drawn like the PlayStation player numbers (newer controller revisions only light the LEDs
// in symmetric groups: centre, inner pair, outer pair):
//   1 = centre, 2 = inner pair, 3 = centre + outer pair, 4 = inner + outer pairs, 5 = all
// Unplugged: the level stays lit; at 20% or less the centre LED pulses.
// Charging: the level and the next one take turns. Charged: all five pulse while the cable stays in.
// The pad switches LEDs off hard and has only three brightness levels, so nothing blinks off:
// pictures pulse between bright and dim, and while charging they change over at dim.
class BatteryLeds {
public:
	// duaLib PowerState values
	static constexpr uint8_t POWER_DISCHARGING = 0x00;
	static constexpr uint8_t POWER_CHARGING = 0x01;
	static constexpr uint8_t POWER_COMPLETE = 0x02;

	static constexpr uint8_t LOW_PERCENT = 20;
	// A freshly connected pad reads as discharging at 0% until its first input report arrives
	static constexpr int64_t CONFIRM_MS = 3000;
	// One pulse: bright, mid, dim, mid
	static constexpr int64_t PULSE_MS = 1500;
	// While charging each picture rises from dim to bright and back down before the other one
	static constexpr int64_t PICTURE_MS = 1500;

	struct Show {
		int level;  // -1 = leave the player number, 0 = all off, 1-5 = steps
		int dim;    // 0 = full brightness, 1 = one step dimmer, 2 = two steps dimmer
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
			return {5, PulseDim(nowMs)};
		if (powerState == POWER_CHARGING) {
			bool first = (nowMs / PICTURE_MS) % 2 == 0;
			return {first ? full : full + 1, PictureDim(nowMs)};
		}
		if (percent <= LOW_PERCENT)
			return {1, PulseDim(nowMs)};
		return {full, 0};
	}

	// Full 20% steps: 0-10% -> 0, 20-30% -> 1, 40-50% -> 2, 60-70% -> 3, 80-90% -> 4, 100% -> 5
	static int FullSteps(uint8_t percent) {
		int steps = percent / 20;
		return steps > 5 ? 5 : steps;
	}

	// bright 500 ms, mid 250, dim 500, mid 250
	static int PulseDim(int64_t nowMs) {
		int64_t p = nowMs % PULSE_MS;
		if (p < 500) return 0;
		if (p < 750) return 1;
		if (p < 1250) return 2;
		return 1;
	}

	// dim 300 ms, mid 250, bright 400, mid 250, dim 300: pictures change over while dim
	static int PictureDim(int64_t nowMs) {
		int64_t p = nowMs % PICTURE_MS;
		if (p < 300) return 2;
		if (p < 550) return 1;
		if (p < 950) return 0;
		if (p < 1200) return 1;
		return 2;
	}

private:
	int64_t m_ConnectedAt = -1;
};

#endif
