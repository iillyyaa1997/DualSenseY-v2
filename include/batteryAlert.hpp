#ifndef BATTERYALERT_H
#define BATTERYALERT_H

#include <cstdint>

// When a controller's battery runs low or finishes charging the controller itself says so:
// the lightbar blinks every few minutes, and it vibrates once when the battery runs low.
class BatteryAlert {
public:
	enum class Signal : uint8_t { None, Low, Full };

	// duaLib PowerState values
	static constexpr uint8_t POWER_DISCHARGING = 0x00;
	static constexpr uint8_t POWER_CHARGING = 0x01;
	static constexpr uint8_t POWER_COMPLETE = 0x02;

	static constexpr uint8_t LOW_PERCENT = 10;
	// A freshly connected pad reads as discharging at 0% until its first input report arrives
	static constexpr int64_t CONFIRM_MS = 3000;
	static constexpr int64_t REPEAT_MS = 5 * 60 * 1000;
	static constexpr int64_t BLINK_HALF_PERIOD_MS = 300;
	static constexpr int64_t BLINK_COUNT = 3;
	static constexpr int64_t RUMBLE_MS = 600;

	void Update(bool connected, uint8_t powerState, uint8_t percent, int64_t nowMs) {
		if (!connected) {
			m_Signal = Signal::None;
			m_Pending = Signal::None;
			return;
		}

		// Only a charger re-arms the vibration, so a pad hovering around the limit vibrates once
		if (powerState == POWER_CHARGING || powerState == POWER_COMPLETE) m_LowRumbled = false;

		Signal wanted = Wanted(powerState, percent);
		if (wanted == Signal::None) {
			m_Signal = Signal::None;
			m_Pending = Signal::None;
			return;
		}
		if (wanted == m_Signal) {
			m_Pending = Signal::None;
			return;
		}
		if (m_Pending != wanted) {
			m_Pending = wanted;
			m_PendingSinceMs = nowMs;
			return;
		}
		if (nowMs - m_PendingSinceMs < CONFIRM_MS) return;

		m_Signal = wanted;
		m_Pending = Signal::None;
		m_SignalStartMs = nowMs;
		if (wanted == Signal::Low && !m_LowRumbled) {
			m_LowRumbled = true;
			m_RumbleUntilMs = nowMs + RUMBLE_MS;
		}
	}

	// The battery level at which the low warning starts (LOW_PERCENT unless changed)
	void SetLowPercent(uint8_t percent) { m_LowPercent = percent; }

	Signal CurrentSignal() const { return m_Signal; }

	bool IsBlinking(int64_t nowMs) const {
		return m_Signal != Signal::None && BurstPosition(nowMs) < 2 * BLINK_COUNT * BLINK_HALF_PERIOD_MS;
	}

	// Each blink is a dark half followed by a lit half
	bool IsDarkPhase(int64_t nowMs) const {
		return IsBlinking(nowMs) && (BurstPosition(nowMs) / BLINK_HALF_PERIOD_MS) % 2 == 0;
	}

	bool IsRumbling(int64_t nowMs) const { return nowMs < m_RumbleUntilMs; }

private:
	Signal Wanted(uint8_t powerState, uint8_t percent) const {
		switch (powerState) {
			case POWER_COMPLETE: return Signal::Full;
			// A full pad may top itself up: it stays full until the cable is removed
			case POWER_CHARGING: return m_Signal == Signal::Full ? Signal::Full : Signal::None;
			// Latched, so a reading that bounces back above the limit does not end the warning
			case POWER_DISCHARGING: return percent <= m_LowPercent || m_Signal == Signal::Low ? Signal::Low : Signal::None;
			default: return Signal::None;
		}
	}

	int64_t BurstPosition(int64_t nowMs) const { return (nowMs - m_SignalStartMs) % REPEAT_MS; }

	Signal m_Signal = Signal::None;
	Signal m_Pending = Signal::None;
	int64_t m_PendingSinceMs = 0;
	int64_t m_SignalStartMs = 0;
	int64_t m_RumbleUntilMs = 0;
	bool m_LowRumbled = false;
	uint8_t m_LowPercent = LOW_PERCENT;
};

#endif
