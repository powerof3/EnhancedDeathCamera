#include "Settings.h"

struct detail
{
	static void SetHudMode(const char* a_mode, bool a_enable)
	{
		using func_t = decltype(&SetHudMode);
		REL::Relocation<func_t> func{ RELOCATION_ID(50747, 51642) };
		return func(a_mode, a_enable);
	}

	static RE::ThirdPersonState* GetThirdPersonState(const RE::PlayerCamera* a_camera)
	{
		return a_camera->IsInThirdPerson() ? static_cast<RE::ThirdPersonState*>(a_camera->currentState.get()) : nullptr;
	}

	static void TogglePOVSwitchOff()
	{
		if (Settings::GetSingleton()->GetUseImprovedCam()) {
			if (const auto controlMap = RE::ControlMap::GetSingleton()) {
				controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, false);
			}
		}
	}
};

namespace util
{
	bool SetCamera(RE::PlayerCamera* a_playerCamera, const Camera* a_camSettings)
	{
		const auto settings = Settings::GetSingleton();

		if (a_playerCamera->IsInFirstPerson()) {
			if (a_camSettings->improvedCamCompability || settings->UseAltThirdPersonCam()) {
				return false;
			}
            a_playerCamera->ForceThirdPerson();
        } else {
			switch (a_camSettings->camType) {
			case Camera::CAM::kThird:
				{
					if (a_camSettings->improvedCamCompability || settings->UseAltThirdPersonCam()) {
						return false;
					}

					a_playerCamera->ForceThirdPerson();

					detail::TogglePOVSwitchOff();
				}
				break;
			case Camera::CAM::kUFO:
				a_playerCamera->ToggleFreeCameraMode(false);
				break;
			default:
				break;
			}
		}

		detail::SetHudMode("VATSPlayback", a_camSettings->hideUI);

		if (const auto VATS = RE::VATS::GetSingleton()) {
			VATS->SetMagicTimeSlowdown(a_camSettings->timeMult, a_camSettings->timeMultPC);
		}

		if (const auto tps = detail::GetThirdPersonState(a_playerCamera)) {
			switch (a_camSettings->thirdPersonStateType) {
			case Camera::TPS::kFreeRotation:
				tps->freeRotationEnabled = true;
				break;
			case Camera::TPS::kAnimatorCam:
				tps->toggleAnimCam = true;
				break;
			case Camera::TPS::kLocked:
				{
					tps->freeRotationEnabled = false;
					tps->toggleAnimCam = false;
				}
				break;
			default:
				break;
			}
		}

		return true;
	}
}

namespace DEATH
{
	struct UpdateWhenAIControlledOrDead
	{
		static void thunk(RE::PlayerCharacter* a_player, float a_delta)
		{
			if (a_player->GetLifeState() == Settings::GetSingleton()->GetDeadState()) {
				if (const auto killer = a_player->GetKiller(); !killer->IsDead()) {
					if (const auto camera = RE::PlayerCamera::GetSingleton(); camera->cameraTarget != a_player->myKiller) {
						if (const auto tps = detail::GetThirdPersonState(camera)) {
							camera->cameraTarget = a_player->myKiller;
							tps->toggleAnimCam = true;
						} else {
							camera->ForceThirdPerson();
						}
					}
				}
			}

			func(a_player, a_delta);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(39375, 40447), OFFSET(0xDC5, 0x1477) };  //PlayerCharacter::Update
			stl::write_thunk_call<UpdateWhenAIControlledOrDead>(target.address());
		}
	};

	struct StartBleedoutMode
	{
		static void thunk(RE::PlayerCamera* a_camera)
		{
			if (!util::SetCamera(a_camera, Settings::GetSingleton()->GetDeathCamera())) {
				return func(a_camera);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			std::array targets{
				std::make_pair(RELOCATION_ID(36872, 37896), OFFSET(0x107E, 0x1122)),  // Actor::KillImpl (Death)
				std::make_pair(RELOCATION_ID(36604, 37612), OFFSET(0x47F, 0x408))     // Actor::SetLifeState (Bleedout)
			};

			for (const auto& [id, offset] : targets) {
				REL::Relocation<std::uintptr_t> target{ id, offset };
				stl::write_thunk_call<StartBleedoutMode>(target.address());
			}
		}
	};

	void Install()
	{
		StartBleedoutMode::Install();

		if (const auto settings = Settings::GetSingleton(); !settings->UseAltThirdPersonCam() && settings->GetDeathCamera()->moveCamToKiller) {
			UpdateWhenAIControlledOrDead::Install();
		}
	}
}

namespace RAGDOLL
{
	struct SetMagicTimeSlowdown
	{
		static void thunk(RE::VATS* a_vats, float a_magicTimeSlowdown, float a_playerMagicTimeSlowdown)
		{
			if (const auto camera = RE::PlayerCamera::GetSingleton()) {
				if (const auto tps = detail::GetThirdPersonState(camera)) {
					tps->toggleAnimCam = false;
				} else {
					camera->ForceThirdPerson();
				}
			}

			detail::TogglePOVSwitchOff();
			detail::SetHudMode("VATSPlayback", false);

			func(a_vats, a_magicTimeSlowdown, a_playerMagicTimeSlowdown);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(39086, 40150), OFFSET(0xE1, 0x42C) };  //Actor::DoGetUpAction, inlined into Actor::DoGetUp in AE
			stl::write_thunk_call<SetMagicTimeSlowdown>(target.address());
		}
	};

	//UpdateKnockdownState/PlayerUpdate does not get called with flycam? might affect other game systems if patched
	//normally called in AnimUpdate for npcs but not for the player
	struct UpdateAnimation
	{
		static void thunk(RE::PlayerCharacter* a_player, float a_delta)
		{
			func(a_player, a_delta);

			if (const auto camera = RE::PlayerCamera::GetSingleton(); camera->IsInFreeCameraMode()) {
				if (const auto process = a_player->currentProcess) {
					UpdateKnockState(process, a_player);
				}
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline size_t index = 0x07D;

	private:
		static void UpdateKnockState(RE::AIProcess* a_process, RE::Actor* a_actor)
		{
			using func_t = decltype(&UpdateKnockState);
			REL::Relocation<func_t> function{ RELOCATION_ID(38859, 39896) };
			return function(a_process, a_actor);
		}
	};

	struct StartBleedoutMode
	{
		static void thunk(RE::PlayerCamera* a_camera)
		{
			if (!util::SetCamera(a_camera, Settings::GetSingleton()->GetRagdollCamera())) {
				return func(a_camera);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(39087, 40151), OFFSET(0xC7, 0x86) };  //DoKnockAction
			stl::write_thunk_call<StartBleedoutMode>(target.address());
		}
	};

	void Install()
	{
		StartBleedoutMode::Install();

		const auto settings = Settings::GetSingleton();

		if (!settings->UseAltThirdPersonCam() || settings->GetRagdollCamera()->camType == Camera::CAM::kUFO) {
			SetMagicTimeSlowdown::Install();
		}

		if (settings->GetRagdollCamera()->camType == Camera::CAM::kUFO) {
			stl::write_vfunc<RE::PlayerCharacter, UpdateAnimation>();
		}
	}
}

namespace BLEEDOUT
{
	struct Begin
	{
		static void thunk(RE::BleedoutCameraState* a_state)
		{
			BeginTPS(a_state);

			if (const auto player = RE::PlayerCharacter::GetSingleton()) {
				const Camera* cam;
			    if (player->IsDead()) {
					cam = Settings::GetSingleton()->GetDeathCamera();
			    } else {
					cam = Settings::GetSingleton()->GetRagdollCamera();
			    }

				detail::SetHudMode("VATSPlayback", cam->hideUI);

				if (const auto VATS = RE::VATS::GetSingleton()) {
					VATS->SetMagicTimeSlowdown(cam->timeMult, cam->timeMultPC);
				}

				switch (cam->thirdPersonStateType) {
				case Camera::TPS::kFreeRotation:
					a_state->freeRotationEnabled = true;
					break;
				case Camera::TPS::kAnimatorCam:
					a_state->toggleAnimCam = true;
					break;
				case Camera::TPS::kLocked:
					{
						if (cam->type == Camera::TYPE::kDeath) {
							a_state->freeRotationEnabled = false;
							a_state->toggleAnimCam = false;
						} else {
							a_state->stateNotActive = true;
						}
					}
					break;
				default:
					break;
				}
			}
		}
		[[maybe_unused]] static inline REL::Relocation<decltype(thunk)> func;
		static inline size_t index = 0x1;

	private:
		static void BeginTPS(RE::ThirdPersonState* a_state)
		{
			using func_t = decltype(&BeginTPS);
			REL::Relocation<func_t> function{ RELOCATION_ID(49958, 50894) };
			return function(a_state);
		}
	};

	struct End
	{
		static void thunk(RE::BleedoutCameraState* a_state)
		{
			EndTPS(a_state);

			detail::SetHudMode("VATSPlayback", false);
			if (const auto VATS = RE::VATS::GetSingleton()) {
				VATS->SetMagicTimeSlowdown(0.0f, 0.0f);
			}
		}
		[[maybe_unused]] static inline REL::Relocation<decltype(thunk)> func;
		static inline size_t index = 0x2;

	private:
		static void EndTPS(RE::ThirdPersonState* a_state)
		{
			using func_t = decltype(&EndTPS);
			REL::Relocation<func_t> function{ RELOCATION_ID(49959, 50895) };
			return function(a_state);
		}
	};

	struct Update
	{
		static void thunk(RE::BleedoutCameraState* a_state, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
		{
			if (const auto settings = Settings::GetSingleton(); settings->GetDeathCamera()->moveCamToKiller) {
				if (const auto player = RE::PlayerCharacter::GetSingleton(); player->GetLifeState() == settings->GetDeadState()) {
					if (const auto killer = player->GetKiller(); killer && !killer->IsDead()) {
						if (const auto camera = RE::PlayerCamera::GetSingleton(); camera && camera->cameraTarget != player->myKiller) {
							camera->cameraTarget = player->myKiller;
						}
					}
				}
			}

			UpdateTPS(a_state, a_nextState);
		}
		[[maybe_unused]] static inline REL::Relocation<decltype(thunk)> func;
		static inline size_t index = 0x3;

	private:
		static void UpdateTPS(RE::ThirdPersonState* a_state, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
		{
			using func_t = decltype(&UpdateTPS);
			REL::Relocation<func_t> function{ RELOCATION_ID(49960, 50896) };
			return function(a_state, a_nextState);
		}
	};

	void Install()
	{
		stl::write_vfunc<RE::BleedoutCameraState, Begin>();
		stl::write_vfunc<RE::BleedoutCameraState, End>();
		stl::write_vfunc<RE::BleedoutCameraState, Update>();
	}
}

namespace PATCH
{
	void InputWhenKnockedOut()  //nops out "player->IsKnockedOut()", enable input during ragdoll
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(41288, 42338), OFFSET(0xF2, 0x171) };  //PlayerControls::ShouldProcessPlayerInput, inlined into PerformInputProcessing in AE

#ifdef SKYRIM_AE
		constexpr std::array<std::uint8_t, 6> nops{ 0x66, 0x0F, 0x1F, 0x44, 0x0, 0x0 };
#else
		constexpr std::array<std::uint8_t, 2> nops{ 0x66, 0x90 };
#endif

		REL::safe_write(target.address(), nops.data(), nops.size());
	}

	void DoGetUpAction()  //camera->state[bleedout] to camera->state[thirdperson/free]
	{
		const auto ragdollCamType = Settings::GetSingleton()->GetRagdollCamera()->camType;

		struct Patch : Xbyak::CodeGenerator
		{
			explicit Patch(const bool a_useThirdPerson)
			{
				mov(rax, ptr[rcx + (a_useThirdPerson ? 0x100 : 0xD0)]);
			}
		};

		Patch patch(ragdollCamType == Camera::CAM::kThird);
		patch.ready();

		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(39086, 40150), OFFSET(0xC7, 0x412) };  //Actor::DoGetUpAction, inlined into Actor::DoGetUp in AE
		REL::safe_write(target.address(), std::span{ patch.getCode(), patch.getSize() });
	}
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			if (const auto settings = Settings::GetSingleton(); settings->LoadSettings()) {
				SKSE::AllocTrampoline(72);

				PATCH::InputWhenKnockedOut();

				const auto deathCam = settings->GetDeathCamera();
				const auto ragdollCam = settings->GetRagdollCamera();

				const bool useAltTPS = settings->UseAltThirdPersonCam();

				if (useAltTPS && (deathCam->enableCam && deathCam->camType == Camera::CAM::kThird || ragdollCam->enableCam && ragdollCam->camType == Camera::CAM::kThird)) {
					BLEEDOUT::Install();
					logger::info("patching bleedout cam");
				}

				if (ragdollCam->enableCam) {
					if (!useAltTPS || ragdollCam->camType == Camera::CAM::kUFO) {
						PATCH::DoGetUpAction();
					}
					RAGDOLL::Install();
					logger::info("patching ragdoll cam");
				}
				if (deathCam->enableCam) {
					DEATH::Install();
					logger::info("patching death cam");
				}
			}
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			if (const auto gameSettingCollection = RE::GameSettingCollection::GetSingleton()) {
				if (const auto gameSetting = gameSettingCollection->GetSetting("fPlayerDeathReloadTime")) {
					gameSetting->data.f = Settings::GetSingleton()->GetDeathCamera()->camDuration;
				}
			}
		}
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Enhanced Death Cam");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "Enhanced Death Cam";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%l] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("loaded");

	SKSE::Init(a_skse);

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
