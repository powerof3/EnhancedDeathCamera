#include "Settings.h"

void Camera::LoadSettings(CSimpleIniA& a_ini)
{
	INI::get_value(a_ini, enableCam, typeStr.c_str(), "Enable", nullptr);
	INI::get_value(a_ini, hideUI, typeStr.c_str(), "Hide UI", nullptr);
	INI::get_value(a_ini, camType, typeStr.c_str(), "Camera Type", nullptr);
	INI::get_value(a_ini, thirdPersonStateType, typeStr.c_str(), "Rotation Type (third person)", nullptr);

	INI::get_value(a_ini, timeMult, typeStr.c_str(), "Time speed multiplier", nullptr);
	INI::get_value(a_ini, timeMultPC, typeStr.c_str(), "Time speed multiplier (Player)", nullptr);
}

void DeathCamera::LoadSettings(CSimpleIniA& a_ini)
{
	Camera::LoadSettings(a_ini);

	INI::get_value(a_ini, moveCamToKiller, typeStr.c_str(), "Snap Camera To Killer", nullptr);
	INI::get_value(a_ini, camDuration, typeStr.c_str(), "Camera Duration", nullptr);
	INI::get_value(a_ini, setWhenDead, typeStr.c_str(), "Set when dead", nullptr);
}

void Settings::CheckImprovedCamera()
{
	improvedCamInstalled = GetModuleHandleA("ImprovedCamera") != nullptr;
	if (!improvedCamInstalled) {
		return;
	}

	constexpr auto path = L"Data/SKSE/Plugins/ImprovedCamera.ini";

	CSimpleIniA ini;
	ini.SetUnicode();

	if (const auto rc = ini.LoadFile(path); rc < 0) {
		logger::error("couldn't read Improved Camera INI");
		return;
	}

	deathCam.improvedCamCompability = ini.GetBoolValue("Main", "bFirstPersonDeath", false);
	ragdollCam.improvedCamCompability = ini.GetBoolValue("Main", "bFirstPersonKnockout", true);

	logger::info("Improved Camera - FirstPersonOnDeath {}", deathCam.improvedCamCompability ? "enabled" : "disabled");
	logger::info("Improved Camera - FirstPersonOnKnockout {}", ragdollCam.improvedCamCompability ? "enabled" : "disabled");
}

void Settings::CheckSmoothCam()
{
	if (GetModuleHandleA("SmoothCam") != nullptr) {
		altTPSMode = true;
		logger::info("Smooth Camera found");
	} else {
		logger::info("Smooth Camera not found");
	}
}

bool Settings::LoadSettings()
{
	constexpr auto path = L"Data/SKSE/Plugins/po3_EnhancedDeathCamera.ini";

	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(path);

	deathCam.LoadSettings(ini);
	ragdollCam.LoadSettings(ini);

	deadState = deathCam.setWhenDead ? RE::ACTOR_LIFE_STATE::kDead : RE::ACTOR_LIFE_STATE::kDying;

	CheckImprovedCamera();
	CheckSmoothCam();

	(void)ini.SaveFile(path);

	return true;
}

DeathCamera* Settings::GetDeathCamera()
{
	return &deathCam;
}

RagdollCamera* Settings::GetRagdollCamera()
{
	return &ragdollCam;
}

RE::ACTOR_LIFE_STATE Settings::GetDeadState() const
{
	return deadState;
}

bool Settings::UseAltThirdPersonCam() const
{
	return altTPSMode;
}

bool Settings::GetUseImprovedCam() const
{
	return improvedCamInstalled;
}
