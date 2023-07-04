#include "Settings.h"

void Camera::LoadSettings(CSimpleIniA& a_ini, bool a_writeComments)
{
	ini::get_value(a_ini, enableCam, typeStr.c_str(), "Enable", nullptr);
	ini::get_value(a_ini, camType, typeStr.c_str(), "Camera Type", a_writeComments ? ";0 - third person , 1 - fly camera (same as tfc)" : nullptr);
	ini::get_value(a_ini, hideUI, typeStr.c_str(), "Hide UI", a_writeComments ? ";Hide UI?" : nullptr);
	ini::get_value(a_ini, thirdPersonStateType, typeStr.c_str(), "Rotation Type (third person)", a_writeComments ? ";0 - free rotation , 1 - animator camera, 2 - locked" : nullptr);

	ini::get_value(a_ini, timeMult, typeStr.c_str(), "Time speed multiplier", a_writeComments ? ";1.0 is base time speed. Smaller values slow down time, larger values speed up time" : nullptr);
	ini::get_value(a_ini, timeMultPC, typeStr.c_str(), "Time speed multiplier (Player)", nullptr);
}

void DeathCamera::LoadSettings(CSimpleIniA& a_ini, bool a_writeComments)
{
	Camera::LoadSettings(a_ini, a_writeComments);

	ini::get_value(a_ini, moveCamToKiller, typeStr.c_str(), "Snap Camera To Killer", ";Sets the camera target to follow the player's killer, when the player is dying or is dead.");
	ini::get_value(a_ini, setWhenDead, typeStr.c_str(), "Set when dead", nullptr);
	ini::get_value(a_ini, camDuration, typeStr.c_str(), "Camera Duration", ";Will be overriden by mods that modify this setting during game, such as Frozen Electrocuted Combustion.");
}

void Settings::CheckImprovedCamera()
{
	improvedCamInstalled = GetModuleHandleA("ImprovedCameraSE") != nullptr;

	if (!improvedCamInstalled) {
		logger::error("Improved Camera SE is not installed");
	    return;
	}

	const auto read_IC_profile = [&](const std::string& a_profileName) {
		const auto path = fmt::format(R"(Data\SKSE\Plugins\ImprovedCameraSE\Profiles\{})", a_profileName);

		CSimpleIniA ini;
		ini.SetUnicode();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("couldn't read Improved Camera profile");
			return;
		}

		deathCam.improvedCamCompability = static_cast<bool>(ini.GetLongValue("EVENTS", "bDeath", 0));
		ragdollCam.improvedCamCompability = static_cast<bool>(ini.GetLongValue("EVENTS", "bRagdoll", 0));

		logger::info("Improved Camera - EventDeath {}", deathCam.improvedCamCompability ? "enabled" : "disabled");
		logger::info("Improved Camera - EventRagdoll {}", ragdollCam.improvedCamCompability ? "enabled" : "disabled");
	};

	CSimpleIniA improvedCameraSE;
	improvedCameraSE.SetUnicode();

	if (const auto rc = improvedCameraSE.LoadFile(LR"(Data\SKSE\Plugins\ImprovedCameraSE\ImprovedCameraSE.ini)"); rc < 0) {
		logger::error("couldn't read Improved Camera SE config");
		return;
	}

    const std::string profileName = improvedCameraSE.GetValue("MODULE DATA", "ProfileName");

	if (!profileName.empty()) {
		read_IC_profile(profileName);
	}
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

void Settings::LoadSettings()
{
	constexpr auto path = L"Data/SKSE/Plugins/po3_EnhancedDeathCamera.ini";

	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(path);

	deathCam.LoadSettings(ini, true);
	ragdollCam.LoadSettings(ini, false);

	deadState = deathCam.setWhenDead ? RE::ACTOR_LIFE_STATE::kDead : RE::ACTOR_LIFE_STATE::kDying;

	CheckImprovedCamera();
	CheckSmoothCam();

	(void)ini.SaveFile(path);
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
