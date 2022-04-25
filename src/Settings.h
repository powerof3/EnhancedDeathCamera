#pragma once

namespace INI
{
	template <class T>
	static void get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key, const char* a_comment)
	{
		if constexpr (std::is_same_v<T, bool>) {
			a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
			a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
		} else if constexpr (std::is_floating_point_v<T>) {
			a_value = static_cast<float>(a_ini.GetDoubleValue(a_section, a_key, a_value));
			a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
		} else if constexpr (std::is_enum_v<T> || std::is_arithmetic_v<T>) {
			a_value = string::lexical_cast<T>(a_ini.GetValue(a_section, a_key, std::to_string(stl::to_underlying(a_value)).c_str()));
			a_ini.SetValue(a_section, a_key, std::to_string(stl::to_underlying(a_value)).c_str(), a_comment);
		} else {
			a_value = a_ini.GetValue(a_section, a_key, a_value.c_str());
			a_ini.SetValue(a_section, a_key, a_value.c_str(), a_comment);
		}
	}
}

class Camera
{
public:
    enum class TYPE : std::uint32_t
	{
		kRagdoll = 0,
		kDeath = 1
	};

	enum class CAM : std::uint32_t
	{
		kThird,
		kUFO
	};

	enum class TPS : std::uint32_t
	{
		kFreeRotation,
		kAnimatorCam,
		kLocked
	};

	explicit Camera(TYPE a_type, std::string a_typeStr, CAM a_camType, TPS a_tpsType) :
		type(a_type),
		typeStr(std::move(a_typeStr)),
		camType(a_camType),
		thirdPersonStateType(a_tpsType)
	{}

	virtual ~Camera() = default;
	virtual void LoadSettings(CSimpleIniA& a_ini, bool a_writeComments);

	// members
	TYPE type;
	std::string typeStr;

	bool enableCam{ true };
	bool hideUI{ true };

	// 0 - third, 1 - ufo
	CAM camType;

	float timeMult{ 0.8f };
	float timeMultPC{ 0.8f };

    // 0 - free rotation, 1 - animator cam, 2 - locked
	TPS thirdPersonStateType;

    bool improvedCamCompability{ false };
};

class RagdollCamera final : public Camera
{
public:
	explicit RagdollCamera(CAM a_camType, TPS a_tpsType) :
		Camera(TYPE::kRagdoll, "Ragdoll Camera", a_camType, a_tpsType)
	{}
};

class DeathCamera final : public Camera
{
public:
	explicit DeathCamera(CAM a_camType, TPS a_tpsType) :
		Camera(TYPE::kDeath, "Death Camera", a_camType, a_tpsType)
	{}

	void LoadSettings(CSimpleIniA& a_ini, bool a_writeComments) override;

	bool moveCamToKiller{ false };
	bool setWhenDead{ true };

	float camDuration{ 5.0f };
};

class Settings
{
public:
	[[nodiscard]] static Settings* GetSingleton()
	{
		static Settings singleton;
		return std::addressof(singleton);
	}

	[[nodiscard]] bool LoadSettings();

	DeathCamera* GetDeathCamera();
	RagdollCamera* GetRagdollCamera();

	[[nodiscard]] RE::ACTOR_LIFE_STATE GetDeadState() const;

	[[nodiscard]] bool GetUseImprovedCam() const;
    [[nodiscard]] bool UseAltThirdPersonCam() const;

private:
	void CheckImprovedCamera();
	void CheckSmoothCam();

	DeathCamera deathCam{
		Camera::CAM::kUFO,
		Camera::TPS::kAnimatorCam
	};

	RagdollCamera ragdollCam{
		Camera::CAM::kThird,
		Camera::TPS::kLocked
	};

	RE::ACTOR_LIFE_STATE deadState{ RE::ACTOR_LIFE_STATE::kDead };

	bool altTPSMode{ false };

	bool improvedCamInstalled{ false };
};
