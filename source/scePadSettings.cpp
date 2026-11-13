#include "scePadSettings.hpp"
#include "led.hpp"
#include "batteryAlert.hpp"
#include "batteryLeds.hpp"
#include <atomic>
#include <algorithm>
#include <fstream>
#include <imgui.h>
#include <platform_folders.h>

static std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
static BatteryAlert g_BatteryAlerts[4];
static std::atomic<int> g_BatteryAlertTest[4] = {};
static bool g_BatteryAlertRumbled[4] = {};
static BatteryLeds g_BatteryLeds[4];
static int g_BatteryLedLevel[4] = {-1, -1, -1, -1};
static std::atomic<bool> g_PlayerLedBattery = false;

// <Mac address, was loaded>
static std::unordered_map<std::string, bool> loadList;

void SaveSettingsToFile(const s_scePadSettings &s, const std::string &filepath)
{

	std::filesystem::path filePath = std::filesystem::u8path(filepath);
	if (!std::filesystem::exists(filePath.parent_path()))
		std::filesystem::create_directory(filePath.parent_path());
	nlohmann::json j = s;
	std::ofstream(filePath) << j.dump(4);
}

bool LoadSettingsFromFile(s_scePadSettings *s, const std::string &filepath)
{
	try
	{
		std::ifstream ifs(std::filesystem::u8path(filepath));
		if (!ifs)
			return false;
		nlohmann::json j;
		ifs >> j;
		*s = j.get<s_scePadSettings>();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool GetDefaultConfigFromMac(const std::string &mac, s_scePadSettings *s)
{
	std::string cleanMac = mac;
	cleanMac.erase(std::remove(cleanMac.begin(), cleanMac.end(), ':'), cleanMac.end());
	std::filesystem::path filePath = std::filesystem::u8path(sago::getDocumentsFolder() + "/DSY/DefaultConfigs/" + cleanMac);

	if (std::filesystem::exists(filePath))
	{
		std::ifstream file(filePath);
		std::string configPath = "";
		std::getline(file, configPath); // the stored path may contain spaces
		file.close();
		LoadSettingsFromFile(s, configPath);
		return true;
	}

	return false;
}

bool RemoveDefaultConfigByMac(const std::string &mac)
{
	std::string cleanMac = mac;
	cleanMac.erase(std::remove(cleanMac.begin(), cleanMac.end(), ':'), cleanMac.end());
	std::filesystem::path filePath = std::filesystem::u8path(sago::getDocumentsFolder() + "/DSY/DefaultConfigs/" + cleanMac);

	if (std::filesystem::exists(filePath))
	{
		std::filesystem::remove(filePath);
		return true;
	}

	return false;
}

void LoadDefaultConfig(int currentController, s_scePadSettings *s)
{
	std::string macAddress = scePadGetMacAddress(g_ScePad[currentController]);

	if (macAddress != "" && !loadList[macAddress])
	{
		if (GetDefaultConfigFromMac(macAddress, s))
		{
			s->WasHidHideRanAfterLoad = false;
			loadList[macAddress] = true;
		}
	}
}

void ForceControllerToNotLoadDefault(int controller)
{
	std::string macAddress = scePadGetMacAddress(g_ScePad[controller]);
	loadList[macAddress] = true;
}

std::string ScePadSettingsToString(s_scePadSettings *s)
{
	nlohmann::json j = *s;
	return j.dump();
}

bool LoadSettingsFromString(s_scePadSettings *s, const std::string &String)
{
	try
	{
		nlohmann::json j = nlohmann::json::parse(String);
		*s = j.get<s_scePadSettings>();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool SaveSettingsFromString(const std::string &String, const std::string &Path)
{
	try
	{
		s_scePadSettings settings = {};
		nlohmann::json j = nlohmann::json::parse(String);
		settings = j.get<s_scePadSettings>();
		SaveSettingsToFile(settings, Path);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void SetPlayerLedBattery(bool enabled)
{
	g_PlayerLedBattery = enabled;
}

void SetBatteryAlertTest(int padNumber, int mode)
{
	for (int i = 0; i < 4; i++)
	{
		if (padNumber == 0 || padNumber == i + 1)
			g_BatteryAlertTest[i] = mode;
	}
}

static void UpdateBatteryAlert(uint32_t index, int64_t nowMs)
{
	uint8_t percent = 0;
	uint8_t powerState = 0;
	bool connected = scePadGetBatteryInfo(g_ScePad[index], &percent, &powerState) == SCE_OK;

	int test = g_BatteryAlertTest[index];
	if (test == 1)
	{
		percent = BatteryAlert::LOW_PERCENT;
		powerState = BatteryAlert::POWER_DISCHARGING;
	}
	else if (test == 2)
	{
		percent = 100;
		powerState = BatteryAlert::POWER_COMPLETE;
	}
	else if (test == 3)
	{
		percent = 50;
		powerState = BatteryAlert::POWER_CHARGING;
	}
	else if (test == 4)
	{
		percent = 60;
		powerState = BatteryAlert::POWER_DISCHARGING;
	}

	g_BatteryAlerts[index].Update(connected, powerState, percent, nowMs);
	g_BatteryLedLevel[index] = g_BatteryLeds[index].Update(connected, powerState, percent, nowMs);
}

// The pad's own colour blinks; a pad with no colour blinks red for low battery and green for full
static void ApplyBatteryAlertToLightbar(const BatteryAlert &alert, s_SceLightBar &lightbar, int64_t nowMs)
{
	if (!alert.IsBlinking(nowMs))
		return;

	if (alert.IsDarkPhase(nowMs))
	{
		lightbar.r = lightbar.g = lightbar.b = 0;
	}
	else if (lightbar.r == 0 && lightbar.g == 0 && lightbar.b == 0)
	{
		bool low = alert.CurrentSignal() == BatteryAlert::Signal::Low;
		lightbar.r = low ? 255 : 0;
		lightbar.g = low ? 0 : 255;
	}
}

void applySettings(uint32_t index, s_scePadSettings settings, AudioPassthrough &audio)
{
	auto now = std::chrono::steady_clock::now();
	auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
	float elapsed = std::chrono::duration<float>(now - startTime).count();

	float discoModeTime = fmod(elapsed * settings.discoModeSpeed, 1.0f);
	s_SceLightBar rainbowLightbar = {};
	GetRainbowColor(discoModeTime, rainbowLightbar);

	float audioPeak = audio.GetCurrentCapturePeak();
	uint8_t audioPeakUint8 = (uint8_t)scaleFloatToInt(audioPeak, 1.0);

	int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
	UpdateBatteryAlert(index, nowMs);
	const BatteryAlert &batteryAlert = g_BatteryAlerts[index];

	s_SceLightBar finalLightbar = {};
	if (settings.useLightbarFromEmulatedController && (settings.emulatedController == (int)EmulatedController::DUALSHOCK4 || settings.usingPeerController))
	{
		finalLightbar = settings.lightbarFromEmulatedController;
	}
	else if (settings.audioToLed && !settings.discoMode)
	{
		s_SceLightBar lightbar = {audioPeakUint8, audioPeakUint8, audioPeakUint8};
		finalLightbar = lightbar;
	}
	else if (settings.audioToLed && settings.discoMode)
	{
		s_SceLightBar lightbar = {std::min(audioPeakUint8, rainbowLightbar.r), std::min(audioPeakUint8, rainbowLightbar.g), std::min(audioPeakUint8, rainbowLightbar.b)};
		finalLightbar = lightbar;
	}
	else if (settings.discoMode)
	{
		finalLightbar = rainbowLightbar;
	}
	else
	{
		s_SceLightBar lightbar = {(uint8_t)scaleFloatToInt(settings.led[0], 1.0f), (uint8_t)scaleFloatToInt(settings.led[1], 1.0f), (uint8_t)scaleFloatToInt(settings.led[2], 1.0f)};
		finalLightbar = lightbar;
	}
	ApplyBatteryAlertToLightbar(batteryAlert, finalLightbar, nowMs);
	scePadSetLightBar(g_ScePad[index], &finalLightbar);

	scePadSetPlayerLedBrightness(g_ScePad[index], settings.brightness);
	scePadSetPlayerLed(g_ScePad[index], settings.disablePlayerLed ? false : true);
	scePadSetPlayerLedLevel(g_ScePad[index], g_PlayerLedBattery ? g_BatteryLedLevel[index] : -1);
	bool micLedState = settings.gyroToRightStick && settings.gyroToRightStickPermanent;
	scePadSetMicLed(g_ScePad[index], micLedState);
	if (!settings.udpConfig)
	{
		if (settings.speakerVolume == 0)
			scePadSetAudioOutPath(g_ScePad[index], SCE_PAD_AUDIO_PATH_STEREO_HEADSET);
		else
			scePadSetAudioOutPath(g_ScePad[index], settings.audioPath);
	}

	s_SceControllerType controllerType = {};
	scePadGetControllerType(g_ScePad[index], &controllerType);

	s_ScePadVolumeGain volume = {};
	volume.speakerVolume = settings.speakerVolume * 9;
	volume.micGain = settings.micGain * 6;
	scePadSetVolumeGain(g_ScePad[index], &volume);

	audio.SetHapticIntensityByUserId(index + 1, settings.hapticIntensity);
	audio.SetSpeakerVolumeByUserId(index + 1, (float)settings.speakerVolume / 8.0f);

	int l2Value = settings.rumbleToAt_swapTriggers ? settings.rumbleFromEmulatedController.smallMotor : settings.rumbleFromEmulatedController.largeMotor;
	int r2Value = settings.rumbleToAt_swapTriggers ? settings.rumbleFromEmulatedController.largeMotor : settings.rumbleFromEmulatedController.smallMotor;

	if (settings.triggersAsButtons && (settings.usingPeerController || settings.emulatedController != (int)EmulatedController::NONE))
	{
		uint8_t leftTrigger[11] = {};
		uint8_t rightTrigger[11] = {};

		CustomTriggerHardestB(leftTrigger, settings.triggersAsButtonStartPos);
		CustomTriggerHardestB(rightTrigger, settings.triggersAsButtonStartPos);
		scePadSetTriggerEffectCustom(g_ScePad[index], leftTrigger, rightTrigger, SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2);
	}
	else if (settings.hapticsToAt && settings.audioPassthrough)
	{
		uint8_t leftTrigger[11] = {};
		uint8_t rightTrigger[11] = {};

		float rawL = audio.GetHapticLevelL(index + 1);
		float rawR = audio.GetHapticLevelR(index + 1);

		float levelL = settings.hapticsToAt_swapTriggers ? rawR : rawL;
		float levelR = settings.hapticsToAt_swapTriggers ? rawL : rawR;

		levelL *= settings.hapticsToAt_intensity;
		levelR *= settings.hapticsToAt_intensity;

		const float noiseGate = 0.03f;
		if (levelL < noiseGate) levelL = 0.0f;
		if (levelR < noiseGate) levelR = 0.0f;

		levelL = std::clamp(levelL, 0.0f, 1.0f);
		levelR = std::clamp(levelR, 0.0f, 1.0f);

		auto applyHapticEffect = [](float level, int mode, uint8_t trigger[11])
		{
			if (level <= 0.0f)
			{
				CustomTriggerOFF(trigger);
				return;
			}

			if (mode == 0)
			{
				// Mode 0: Vibration (Pulse_B)
				uint8_t freq = static_cast<uint8_t>(std::clamp(static_cast<int>(5.0f + level * 30.0f), 5, 40));
				uint8_t intensity = static_cast<uint8_t>(std::clamp(static_cast<int>(level * 255.0f), 10, 255));
				uint8_t pos = 0;
				std::vector<uint8_t> param = { freq, intensity, pos };
				CustomTriggerBetterVibration(param, trigger);
			}
			else
			{
				// Mode 1: Dynamic Resistance (Feedback)
				uint8_t force = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(level * 8.0f)), 1, 8));
				uint8_t start = 0;
				std::vector<uint8_t> param = { start, force };
				CustomTriggerResistance(param, trigger);
			}
		};

		applyHapticEffect(levelL, settings.hapticsToAt_mode, leftTrigger);
		applyHapticEffect(levelR, settings.hapticsToAt_mode, rightTrigger);

		scePadSetTriggerEffectCustom(g_ScePad[index], leftTrigger, rightTrigger, SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2);
	}
	else if (settings.rumbleToAT && (settings.usingPeerController || settings.emulatedController != (int)EmulatedController::NONE))
	{
		uint8_t leftTrigger[11] = {};
		uint8_t rightTrigger[11] = {};

		std::vector<uint8_t> l2Param;
		l2Param.push_back(std::min(l2Value, settings.rumbleToAt_frequency[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2]));
		l2Param.push_back(std::min(l2Value, settings.rumbleToAt_intensity[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2]));
		l2Param.push_back(settings.rumbleToAt_position[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_L2]);

		std::vector<uint8_t> r2Param;
		r2Param.push_back(std::min(r2Value, settings.rumbleToAt_frequency[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2]));
		r2Param.push_back(std::min(r2Value, settings.rumbleToAt_intensity[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2]));
		r2Param.push_back(settings.rumbleToAt_position[SCE_PAD_TRIGGER_EFFECT_PARAM_INDEX_FOR_R2]);

		CustomTriggerBetterVibration(l2Param, leftTrigger);
		CustomTriggerBetterVibration(r2Param, rightTrigger);

		scePadSetTriggerEffectCustom(g_ScePad[index], leftTrigger, rightTrigger, SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 | SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2);
	}
	else
	{
		settings.stockTriggerParam.triggerMask |= settings.isLeftUsingDsxTrigger ? 0 : SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2;
		settings.stockTriggerParam.triggerMask |= settings.isRightUsingDsxTrigger ? 0 : SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2;
		scePadSetTriggerEffect(g_ScePad[index], &settings.stockTriggerParam);

		uint8_t triggerBitmask = 0;
		triggerBitmask |= settings.isLeftUsingDsxTrigger ? SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_L2 : 0;
		triggerBitmask |= settings.isRightUsingDsxTrigger ? SCE_PAD_TRIGGER_EFFECT_TRIGGER_MASK_R2 : 0;
		scePadSetTriggerEffectCustom(g_ScePad[index], settings.leftCustomTrigger.data(), settings.rightCustomTrigger.data(), triggerBitmask);
	}

	if (batteryAlert.IsRumbling(nowMs))
	{
		s_ScePadVibrationParam alertRumble = {200, 200};
		scePadSetVibrationMode(g_ScePad[index], SCE_PAD_RUMBLE_MODE);
		scePadSetVibration(g_ScePad[index], &alertRumble);
		g_BatteryAlertRumbled[index] = true;
	}
	else if (settings.useRumbleFromEmulatedController || settings.udpConfig)
	{
		g_BatteryAlertRumbled[index] = false;
		if (settings.rumbleFromEmulatedController.largeMotor > 0 || settings.rumbleFromEmulatedController.smallMotor > 0)
		{
			scePadSetVibrationMode(g_ScePad[index], SCE_PAD_RUMBLE_MODE);
		}
		else
		{
			scePadSetVibrationMode(g_ScePad[index], SCE_PAD_HAPTICS_MODE);
		}

		scePadSetVibration(g_ScePad[index], &settings.rumbleFromEmulatedController);
	}
	else if (g_BatteryAlertRumbled[index])
	{
		g_BatteryAlertRumbled[index] = false;
		s_ScePadVibrationParam stop = {0, 0};
		scePadSetVibration(g_ScePad[index], &stop);
	}
}