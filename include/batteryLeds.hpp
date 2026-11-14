#ifndef BATTERYLEDS_H
#define BATTERYLEDS_H

#include <cstdint>

// Shows a DualSense battery level on the five player LEDs under the touchpad, one step per full
// 20%, drawn like the PlayStation player numbers (newer controller revisions only light the LEDs
// in symmetric groups: centre, inner pair, outer pair):
//   1 = centre, 2 = inner pair, 3 = centre + outer pair, 4 = inner + outer pairs, 5 = all
// Unplugged: the level stays lit; at 20% or less the centre LED blinks.
// Charging: the level and the next one take turns. Charged: all five blink while the cable stays in.
// The pad itself fades LEDs in but switches them off hard and has only three brightness levels,
// so each picture fades out in software: the brightness glides from bright to dim, then the dim
// picture is switched on less and less often until it is gone (see FadeDither).
class BatteryLeds {
public:
	// duaLib PowerState values
	static constexpr uint8_t POWER_DISCHARGING = 0x00;
	static constexpr uint8_t POWER_CHARGING = 0x01;
	static constexpr uint8_t POWER_COMPLETE = 0x02;

	static constexpr uint8_t LOW_PERCENT = 20;
	// A freshly connected pad reads as discharging at 0% until its first input report arrives
	static constexpr int64_t CONFIRM_MS = 3000;
	// One picture is shown per half period: it holds, glides down to dim, then fades away
	static constexpr int64_t HALF_PERIOD_MS = 1500;
	static constexpr int64_t HOLD_MS = 500;
	static constexpr int64_t GLIDE_MS = 700;  // bright -> dim
	static constexpr int64_t VANISH_MS = HALF_PERIOD_MS - HOLD_MS - GLIDE_MS;  // dim -> off

	struct Show {
		int level;    // -1 = leave the player number, 0 = all off, 1-5 = steps
		float fade;   // 0-2 = brightness from bright to dim, 2-3 = from dim to gone
	};

	Show Update(bool connected, uint8_t powerState, uint8_t percent, int64_t nowMs) {
		if (!connected || powerState > POWER_COMPLETE) {
			m_ConnectedAt = -1;
			return {-1, 0.0f};
		}
		if (m_ConnectedAt < 0) m_ConnectedAt = nowMs;
		if (nowMs - m_ConnectedAt < CONFIRM_MS) return {-1, 0.0f};

		int full = FullSteps(percent);
		if (powerState == POWER_COMPLETE || (powerState == POWER_CHARGING && full == 5))
			return Alternate(5, 0, nowMs);
		if (powerState == POWER_CHARGING)
			return Alternate(full, full + 1, nowMs);
		if (percent <= LOW_PERCENT)
			return Alternate(1, 0, nowMs);
		return {full, 0.0f};
	}

	// Full 20% steps: 0-10% -> 0, 20-30% -> 1, 40-50% -> 2, 60-70% -> 3, 80-90% -> 4, 100% -> 5
	static int FullSteps(uint8_t percent) {
		int steps = percent / 20;
		return steps > 5 ? 5 : steps;
	}

	// Fade within a half period: 0 while holding (the pad fades the picture in by itself),
	// then 0 -> 2 while gliding to dim, then 2 -> 3 while vanishing
	static float Fade(int64_t nowMs) {
		int64_t p = nowMs % HALF_PERIOD_MS;
		if (p < HOLD_MS) return 0.0f;
		if (p < HOLD_MS + GLIDE_MS) return 2.0f * (float)(p - HOLD_MS) / (float)GLIDE_MS;
		return 2.0f + (float)(p - HOLD_MS - GLIDE_MS) / (float)VANISH_MS;
	}

private:
	static Show Alternate(int first, int second, int64_t nowMs) {
		bool firstHalf = (nowMs / HALF_PERIOD_MS) % 2 == 0;
		return {firstHalf ? first : second, Fade(nowMs)};
	}

	int64_t m_ConnectedAt = -1;
};

// Turns a fade value into what the pad can do, a tick at a time: one of its three brightness
// levels, and whether the picture is lit at all. Each tick picks the nearer level and carries
// the rounding error to the next tick, so the average over a few ticks is the wanted value.
class FadeDither {
public:
	struct Out {
		int brightnessSteps;  // 0-2 steps dimmer than the user's brightness
		bool lit;
		bool instant;         // switch the pad's own fade-in off while vanishing
	};

	Out Next(float fade) {
		if (fade <= 0.0f) {
			m_LevelError = m_LitError = 0.0f;
			return {0, true, false};
		}
		if (fade <= 2.0f) {
			m_LitError = 0.0f;
			float want = fade + m_LevelError;
			int level = want < 0.5f ? 0 : want < 1.5f ? 1 : 2;
			m_LevelError = want - (float)level;
			return {level, true, false};
		}
		float duty = 3.0f - (fade > 3.0f ? 3.0f : fade);  // share of ticks the dim picture is lit
		float want = duty + m_LitError;
		bool lit = want >= 0.5f;
		m_LitError = want - (lit ? 1.0f : 0.0f);
		return {2, lit, true};
	}

private:
	float m_LevelError = 0.0f;
	float m_LitError = 0.0f;
};

#endif
