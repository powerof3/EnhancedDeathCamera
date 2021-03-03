#include "version.h"

namespace GLOBALS
{
	namespace DEATH
	{
		bool enableCam = true;
		bool hideUI = true;

		// 0 - third, 1 - ufo
		std::uint32_t camType = 0;

		float timeMult = 0.8f;
		float timeMultPC = 0.8f;

		// 0 - free rotation, 1 - animator cam, 2 - locked
		std::uint32_t tpsType = 1;

		bool moveCamToKiller = true;
		bool setWhenDead = true;

		float camDuration = 5.0f;

		RE::ACTOR_LIFE_STATE deadState = RE::ACTOR_LIFE_STATE::kDead;
	}

	namespace RAGDOLL
	{
		bool enableCam = true;
		bool hideUI = true;

		std::uint32_t camType = 0;

		float timeMult = 0.8f;
		float timeMultPC = 0.8f;

		std::uint32_t tpsType = 2;
	}

	namespace IMPROVED_CAM
	{
		bool installed = false;
		bool fpDeath = false;
		bool fpRagdoll = false;
	}

	bool altTPSMode = false;
}


namespace PATCH
{
	void BlockInputWhenKnockedOut()	 //nops out "player->IsKnockedOut()"
	{
		constexpr std::uint8_t NOP = 0x90;

		REL::Relocation<std::uintptr_t> CanAcceptInputs{ REL::ID(41288) };
		REL::safe_write(CanAcceptInputs.address() + 0xF2, NOP);
		REL::safe_write(CanAcceptInputs.address() + 0xF3, NOP);
	}

	void ActionGetUp()	//camera->state[bleedout] to camera->state[thirdperson/free]
	{
		struct Patch : Xbyak::CodeGenerator
		{
			Patch()
			{
				mov(rax, ptr[rcx + (GLOBALS::RAGDOLL::camType == 0 ? 0x100 : 0xD0)]);
			}
		};

		Patch patch;
		patch.ready();

		REL::Relocation<std::uintptr_t> ActionGetUp{ REL::ID(39086) };
		REL::safe_write(ActionGetUp.address() + 0xC7, stl::span{ patch.getCode(), patch.getSize() });
	}
}


namespace CAMERA
{
	using namespace GLOBALS;

	void SetCameraMode(const char* a_mode, bool a_enable)
	{
		using func_t = decltype(&SetCameraMode);
		REL::Relocation<func_t> func{ REL::ID(50747) };
		return func(a_mode, a_enable);
	}

	bool SetCamera(RE::PlayerCamera* a_camera, bool a_IC_FP, std::uint32_t a_camType, bool a_hideUI, float a_timeMult, float a_timeMultPC, std::uint32_t a_tpsType)
	{
		auto currState = a_camera->currentState.get();
		if (currState) {
			if (currState->id == RE::CameraState::kFirstPerson) {
				if (IMPROVED_CAM::installed && a_IC_FP || altTPSMode) {
					return false;
				} else {
					a_camera->EnterThirdPerson();
				}
			} else {
				switch (a_camType) {
				case 0:
					{
						if (IMPROVED_CAM::installed && a_IC_FP || altTPSMode) {
							return false;
						}

						a_camera->EnterThirdPerson();
						if (IMPROVED_CAM::installed) {
							auto controlMap = RE::ControlMap::GetSingleton();
							if (controlMap) {
								controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, false);
							}
						}
					}
					break;
				case 1:
					a_camera->EnterFreeCameraState(false);
					break;
				default:
					break;
				}
			}
		}

		SetCameraMode("VATSPlayback", a_hideUI);

		auto VATS = RE::VATSManager::GetSingleton();
		if (VATS) {
			VATS->SetGameTimeMult(a_timeMult, a_timeMultPC);
		}

		if (currState = a_camera->currentState.get(); currState && currState->id == RE::CameraState::kThirdPerson) {
			auto tps = static_cast<RE::ThirdPersonState*>(currState);
			if (tps) {
				switch (a_tpsType) {
				case 0:
					tps->freeRotationEnabled = true;
					break;
				case 1:
					tps->toggleAnimCam = true;
					break;
				case 2:
					{
						tps->freeRotationEnabled = false;
						tps->toggleAnimCam = false;
					}
					break;
				default:
					break;
				}
			}
		}

		return true;
	}


	class Death
	{
	public:
		static void Hook()
		{
			auto& trampoline = SKSE::GetTrampoline();

			REL::Relocation<std::uintptr_t> KillImpl{ REL::ID(36872) };	 //death
			_SetBleedoutCamera_Death = trampoline.write_call<5>(KillImpl.address() + 0x107E, SetBleedoutCamera_Death);

			REL::Relocation<std::uintptr_t> SetLifeState{ REL::ID(36604) };	 //bleedout
			_SetBleedoutCamera_Bleedout = trampoline.write_call<5>(SetLifeState.address() + 0x47F, SetBleedoutCamera_Bleedout);

			if (!altTPSMode && DEATH::moveCamToKiller) {
				REL::Relocation<std::uintptr_t> PlayerCharacterUpdate{ REL::ID(39375) };
				_UpdatePlayerDeath = trampoline.write_call<5>(PlayerCharacterUpdate.address() + 0xCE, UpdatePlayerDeath);
			}
		}

	private:
		static void SetBleedoutCamera_Death(RE::PlayerCamera* a_camera)
		{
			if (!SetCamera(a_camera, IMPROVED_CAM::fpDeath, DEATH::camType, DEATH::hideUI, DEATH::timeMult, DEATH::timeMultPC, DEATH::tpsType)) {
				return _SetBleedoutCamera_Death(a_camera);
			}
		}
		static inline REL::Relocation<decltype(SetBleedoutCamera_Death)> _SetBleedoutCamera_Death;

		static void SetBleedoutCamera_Bleedout(RE::PlayerCamera* a_camera)
		{
			if (!SetCamera(a_camera, IMPROVED_CAM::fpDeath, DEATH::camType, DEATH::hideUI, DEATH::timeMult, DEATH::timeMultPC, DEATH::tpsType)) {
				return _SetBleedoutCamera_Bleedout(a_camera);
			}
		}
		static inline REL::Relocation<decltype(SetBleedoutCamera_Bleedout)> _SetBleedoutCamera_Bleedout;

		static void UpdatePlayerDeath(RE::PlayerCharacter* a_player)
		{
			if (a_player->GetLifeState() == DEATH::deadState) {
				auto killer = a_player->GetKiller();
				if (killer && !killer->IsDead()) {
					auto camera = RE::PlayerCamera::GetSingleton();
					if (camera && camera->cameraTarget != a_player->myKiller) {
						auto tps = static_cast<RE::ThirdPersonState*>(camera->currentState.get());
						if (tps) {
							camera->cameraTarget = a_player->myKiller;
							tps->toggleAnimCam = true;
						} else {
							camera->EnterThirdPerson();
						}
					}
				}
			}

			_UpdatePlayerDeath(a_player);
		}
		static inline REL::Relocation<decltype(UpdatePlayerDeath)> _UpdatePlayerDeath;
	};


	class Ragdoll
	{
	public:
		static void Hook()
		{
			auto& trampoline = SKSE::GetTrampoline();

			REL::Relocation<std::uintptr_t> DoActionKnockdown{ REL::ID(39087) };
			_SetBleedoutCamera_Ragdoll = trampoline.write_call<5>(DoActionKnockdown.address() + 0xC7, SetBleedoutCamera_Ragdoll);

			if (!altTPSMode || RAGDOLL::camType == 1) {
				REL::Relocation<std::uintptr_t> StartActionGetUp{ REL::ID(39086) };
				_SetGameTimeMult = trampoline.write_call<5>(StartActionGetUp.address() + 0xE1, SetGameTimeMult);
			}

			if (RAGDOLL::camType == 1) {								  //UpdateKnockdownState/PlayerUpdate does not get called with flycam? might affect other game systems if patched
																		  //normally called in AnimUpdate for npcs but not for the player
				REL::Relocation<std::uintptr_t> vtbl{ REL::ID(261916) };  //player vtbl
				_UpdateAnim = vtbl.write_vfunc(0x07D, UpdateAnim);
			}
		}

	private:
		static void SetBleedoutCamera_Ragdoll(RE::PlayerCamera* a_camera)
		{
			if (!SetCamera(a_camera, IMPROVED_CAM::fpRagdoll, RAGDOLL::camType, RAGDOLL::hideUI, RAGDOLL::timeMult, RAGDOLL::timeMultPC, RAGDOLL::tpsType)) {
				return _SetBleedoutCamera_Ragdoll(a_camera);
			}
		}
		static inline REL::Relocation<decltype(SetBleedoutCamera_Ragdoll)> _SetBleedoutCamera_Ragdoll;

		static void SetGameTimeMult(RE::VATSManager* a_vats, float a_npcMult, float a_pcMult)
		{
			if (auto camera = RE::PlayerCamera::GetSingleton(); camera) {
				if (auto currState = camera->currentState.get(); currState) {
					if (currState->id == RE::CameraState::kThirdPerson) {
						auto tps = static_cast<RE::ThirdPersonState*>(currState);
						if (tps) {
							tps->toggleAnimCam = false;
						}
					} else {
						camera->EnterThirdPerson();
					}
				}
			}

			if (IMPROVED_CAM::installed) {
				auto controlMap = RE::ControlMap::GetSingleton();
				if (controlMap) {
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, true);
				}
			}

			SetCameraMode("VATSPlayback", false);
			
			_SetGameTimeMult(a_vats, a_npcMult, a_pcMult);
		}
		static inline REL::Relocation<decltype(SetGameTimeMult)> _SetGameTimeMult;

		static void UpdateAnim(RE::PlayerCharacter* a_player, float a_delta)
		{
			_UpdateAnim(a_player, a_delta);

			auto camera = RE::PlayerCamera::GetSingleton();
			if (camera && camera->currentState.get() == camera->cameraStates[RE::CameraState::kFree].get()) {
				auto process = a_player->currentProcess;
				if (process) {
					UpdateKnockDownState(process, a_player);
				}
			}
		}
		using UpdateAnimation_t = decltype(&RE::PlayerCharacter::UpdateAnimation);	// 07D
		static inline REL::Relocation<UpdateAnimation_t> _UpdateAnim;

		static void UpdateKnockDownState(RE::AIProcess* a_process, RE::Actor* a_actor)
		{
			using func_t = decltype(&UpdateKnockDownState);
			REL::Relocation<func_t> func{ REL::ID(38859) };
			return func(a_process, a_actor);
		}
	};


	class Bleedout
	{
	public:
		static void Hook()
		{
			REL::Relocation<std::uintptr_t> vtbl{ REL::ID(267819) };  //bleedout camera
			_BeginBleedout = vtbl.write_vfunc(0x01, BeginBleedout);
			_EndBleedout = vtbl.write_vfunc(0x02, EndBleedout);
			_UpdateBleedout = vtbl.write_vfunc(0x03, UpdateBleedout);
		}

	private:
		static void BeginBleedout(RE::BleedoutCameraState* a_state)
		{
			BeginTPS(a_state);

			auto player = RE::PlayerCharacter::GetSingleton();
			if (player && player->IsDead()) {
				SetCameraMode("VATSPlayback", DEATH::hideUI);

				auto VATS = RE::VATSManager::GetSingleton();
				if (VATS) {
					VATS->SetGameTimeMult(DEATH::timeMult, DEATH::timeMultPC);
				}

				switch (DEATH::tpsType) {
				case 0:
					a_state->freeRotationEnabled = true;
					break;
				case 1:
					a_state->toggleAnimCam = true;
					break;
				case 2:
					{
						a_state->freeRotationEnabled = false;
						a_state->toggleAnimCam = false;
					}
					break;
				default:
					break;
				}
			} else {
				SetCameraMode("VATSPlayback", RAGDOLL::hideUI);

				auto VATS = RE::VATSManager::GetSingleton();
				if (VATS) {
					VATS->SetGameTimeMult(RAGDOLL::timeMult, RAGDOLL::timeMultPC);
				}

				switch (RAGDOLL::tpsType) {
				case 0:
					a_state->freeRotationEnabled = true;
					break;
				case 1:
					a_state->toggleAnimCam = true;
					break;
				case 2:
					a_state->stateNotActive = true;
					break;
				default:
					break;
				}
			}
		}
		using BeginBleedout_t = decltype(&RE::BleedoutCameraState::Begin);	// 01
		static inline REL::Relocation<BeginBleedout_t> _BeginBleedout;


		static void EndBleedout(RE::BleedoutCameraState* a_state)
		{
			EndTPS(a_state);

			SetCameraMode("VATSPlayback", false);
			auto VATS = RE::VATSManager::GetSingleton();
			if (VATS) {
				VATS->SetGameTimeMult(0.0f, 0.0f);
			}
		}
		using EndBleedout_t = decltype(&RE::BleedoutCameraState::End);	// 01
		static inline REL::Relocation<EndBleedout_t> _EndBleedout;


		static void UpdateBleedout(RE::BleedoutCameraState* a_state, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
		{
			if (DEATH::moveCamToKiller) {
				auto player = RE::PlayerCharacter::GetSingleton();
				if (player && player->GetLifeState() == DEATH::deadState) {
					auto killer = player->GetKiller();
					if (killer && !killer->IsDead()) {
						auto camera = RE::PlayerCamera::GetSingleton();
						if (camera && camera->cameraTarget != player->myKiller) {
							camera->cameraTarget = player->myKiller;
						}
					}
				}
			}

			UpdateTPS(a_state, a_nextState);
		}
		using UpdateBleedoutation_t = decltype(&RE::BleedoutCameraState::Update);  // 03
		static inline REL::Relocation<UpdateBleedoutation_t> _UpdateBleedout;


		static void BeginTPS(RE::ThirdPersonState* a_state)
		{
			using func_t = decltype(&BeginTPS);
			REL::Relocation<func_t> func{ REL::ID(49958) };
			return func(a_state);
		}

		static void EndTPS(RE::ThirdPersonState* a_state)
		{
			using func_t = decltype(&EndTPS);
			REL::Relocation<func_t> func{ REL::ID(49959) };
			return func(a_state);
		}

		static void UpdateTPS(RE::ThirdPersonState* a_state, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
		{
			using func_t = decltype(&UpdateTPS);
			REL::Relocation<func_t> func{ REL::ID(49960) };
			return func(a_state, a_nextState);
		}
	};
}


namespace INI
{
	using namespace GLOBALS;

	bool Read()
	{
		static std::string pluginPath;
		if (pluginPath.empty()) {
			pluginPath = SKSE::GetPluginConfigPath("po3_EnhancedDeathCamera");
		}

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		auto rc = ini.LoadFile(pluginPath.c_str());
		if (rc < 0) {
			logger::error("Can't load 'po3_EnhancedDeathCamera.ini'");
			return false;
		}

		altTPSMode = ini.GetBoolValue("Main", "Enable Third Person Alt Camera", false);

		//death
		DEATH::enableCam = ini.GetBoolValue("Death Camera", "Enable", true);
		DEATH::camType = ini.GetLongValue("Death Camera", "Type", 0);
		DEATH::hideUI = ini.GetBoolValue("Death Camera", "Hide UI", true);
		DEATH::tpsType = ini.GetLongValue("Death Camera", "Rotation Type (third person)", 0);

		DEATH::timeMult = static_cast<float>(ini.GetDoubleValue("Death Camera", "Time multiplier", 0.8));
		DEATH::timeMultPC = static_cast<float>(ini.GetDoubleValue("Death Camera", "Time multiplier (Player)", 0.8));

		DEATH::moveCamToKiller = ini.GetBoolValue("Death Camera", "Snap Camera To Killer", false);

		DEATH::camDuration = static_cast<float>(ini.GetDoubleValue("Death Camera", "Camera Duration", 5.0));

		DEATH::setWhenDead = ini.GetBoolValue("Death Camera", "Set when dead", true);
		DEATH::deadState = DEATH::setWhenDead ? RE::ACTOR_LIFE_STATE::kDead : RE::ACTOR_LIFE_STATE::kDying;

		//ragdoll
		RAGDOLL::enableCam = ini.GetBoolValue("Ragdoll Camera", "Enable", true);
		RAGDOLL::hideUI = ini.GetBoolValue("Ragdoll Camera", "Hide UI", true);
		RAGDOLL::camType = ini.GetLongValue("Ragdoll Camera", "Type", 0);
		RAGDOLL::tpsType = ini.GetLongValue("Ragdoll Camera", "Rotation Type (third person)", 2);

		RAGDOLL::timeMult = static_cast<float>(ini.GetDoubleValue("Ragdoll Camera", "Time multiplier", 0.8));
		RAGDOLL::timeMultPC = static_cast<float>(ini.GetDoubleValue("Ragdoll Camera", "Time multiplier (Player)", 0.8));

		return true;
	}


	bool ReadImprovedCamera()
	{
		static std::string pluginPath;
		if (pluginPath.empty()) {
			pluginPath = SKSE::GetPluginConfigPath("ImprovedCamera");
		}

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		auto rc = ini.LoadFile(pluginPath.c_str());
		if (rc < 0) {
			logger::error("couldn't read Improved Camera INI");
			return false;
		}

		IMPROVED_CAM::fpDeath = ini.GetBoolValue("Main", "bFirstPersonDeath", true);
		logger::info("Improved Camera - FirstPersonOnDeath {}", IMPROVED_CAM::fpDeath ? "enabled" : "disabled");
		IMPROVED_CAM::fpRagdoll = ini.GetBoolValue("Main", "bFirstPersonKnockout", true);
		logger::info("Improved Camera - FirstPersonOnKnockout {}", IMPROVED_CAM::fpRagdoll ? "enabled" : "disabled");

		return true;
	}
}


void OnInit(SKSE::MessagingInterface::Message* a_msg)
{
	using namespace GLOBALS;

	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			IMPROVED_CAM::installed = GetModuleHandle("ImprovedCamera") != nullptr;
			logger::info("Improved Camera {}", IMPROVED_CAM::installed ? "found" : "not found");

			if (IMPROVED_CAM::installed) {
				INI::ReadImprovedCamera();
			}

			if (GetModuleHandle("SmoothCam") != nullptr) {
				altTPSMode = true;
				logger::info("Smooth Camera found");
			} else {
				logger::info("Smooth Camera not found");
			}


			PATCH::BlockInputWhenKnockedOut();

			if (altTPSMode && (DEATH::camType == 0 || RAGDOLL::camType == 0)) {
				CAMERA::Bleedout::Hook();
				logger::info("patching bleedout cam");
			}

			if (RAGDOLL::enableCam) {
				if (!altTPSMode || RAGDOLL::camType == 1) {
					PATCH::ActionGetUp();
				}
				CAMERA::Ragdoll::Hook();
				logger::info("patching ragdoll cam");
			}
			if (GLOBALS::DEATH::enableCam) {
				CAMERA::Death::Hook();
				logger::info("patching death cam");
			}
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			auto gameSettingCollection = RE::GameSettingCollection::GetSingleton();
			if (gameSettingCollection) {
				auto gameSetting = gameSettingCollection->GetSetting("fPlayerDeathReloadTime");
				if (gameSetting) {
					gameSetting->data.f = DEATH::camDuration;
				}
			}
		}
		break;
	default:
		break;
	}
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	try {
		auto path = logger::log_directory().value() / "po3_EnhancedDeathCam.log";
		auto log = spdlog::basic_logger_mt("global log", path.string(), true);
		log->flush_on(spdlog::level::info);

#ifndef NDEBUG
		log->set_level(spdlog::level::debug);
		log->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
		log->set_level(spdlog::level::info);

#endif
		spdlog::set_default_logger(log);
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");

		logger::info("Enhanced Death Cam {}", SOS_VERSION_VERSTRING);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "Enhanced Death Cam";
		a_info->version = SOS_VERSION_MAJOR;

		if (a_skse->IsEditor()) {
			logger::critical("Loaded in editor, marking as incompatible");
			return false;
		}

		const auto ver = a_skse->RuntimeVersion();
		if (ver < SKSE::RUNTIME_1_5_39) {
			logger::critical("Unsupported runtime version {}", ver.string());
			return false;
		}
	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	try {
		logger::info("Enhanced Death Cam loaded");

		SKSE::Init(a_skse);
		SKSE::AllocTrampoline(72);

		INI::Read();

		auto messaging = SKSE::GetMessagingInterface();
		if (!messaging->RegisterListener("SKSE", OnInit)) {
			return false;
		}

	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}