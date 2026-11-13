#ifndef BATTERYLEDS_H
#define BATTERYLEDS_H

#include <cstdint>

// Shows a DualSense battery level on the five player LEDs under the touchpad.
// The level grows from the middle outwards (1 = centre LED ... 5 = all five), because newer
// controller revisions only light the LEDs in symmetric pairs. While charging the next step
// blinks, and at 10% or less the single centre LED blinks.
class BatteryLeds {
public:
	// duaLib PowerState values
	static constexpr uint8_t POWER_DISCHARGING = 0x00;
	static constexpr uint8_t POWER_CHARGING = 0x01;
	static constexpr uint8_t POWER_COMPLETE = 0x02;

	static constexpr uint8_t LOW_PERCENT = 10;
	// A freshly connected pad reads as discharging at 0% until its first input report arrives
	static constexpr int64_t CONFIRM_MS = 3000;
	static constexpr int64_t BLINK_HALF_PERIOD_MS = 700;

	// Level the LEDs should show: -1 = leave the player number pattern, 0 = all off, 1-5 = steps
	int Update(bool connected, uint8_t powerState, uint8_t percent, int64_t nowMs) {
		if (!connected || powerState > POWER_COMPLETE) {
			m_ConnectedAt = -1;
			return -1;
		}
		if (m_ConnectedAt < 0) m_ConnectedAt = nowMs;
		if (nowMs - m_ConnectedAt < CONFIRM_MS) return -1;

		bool blinkOn = (nowMs / BLINK_HALF_PERIOD_MS) % 2 == 0;
		if (powerState == POWER_COMPLETE) return 5;

		int steps = Steps(percent);
		if (powerState == POWER_CHARGING) {
			if (steps == 5) return blinkOn ? 5 : 4;
			return blinkOn ? steps + 1 : steps;
		}

		if (percent <= LOW_PERCENT) return blinkOn ? 1 : 0;
		return steps;
	}

	// 10-20% -> 1, 30-40% -> 2, 50-60% -> 3, 70-80% -> 4, 90-100% -> 5
	static int Steps(uint8_t percent) {
		int steps = (percent + 19) / 20;
		return steps < 1 ? 1 : steps > 5 ? 5 : steps;
	}

private:
	int64_t m_ConnectedAt = -1;
};

#endif
