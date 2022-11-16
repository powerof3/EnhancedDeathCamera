#include "Hooks.h"
#include "Hooks/BleedoutCam.h"
#include "Hooks/DeathCam.h"
#include "Hooks/RagdollCam.h"
#include "Settings.h"

namespace Hooks
{
	namespace detail
	{
		void SetHudMode(const char* a_mode, bool a_enable)
		{
			using func_t = decltype(&SetHudMode);
			REL::Relocation<func_t> func{ RELOCATION_ID(50747, 51642) };
			return func(a_mode, a_enable);
		}

		RE::ThirdPersonState* GetThirdPersonState(const RE::PlayerCamera* a_camera)
		{
			return a_camera->IsInThirdPerson() ? static_cast<RE::ThirdPersonState*>(a_camera->currentState.get()) : nullptr;
		}

		void TogglePOVSwitchOff()
		{
			if (Settings::GetSingleton()->GetUseImprovedCam()) {
				if (const auto controlMap = RE::ControlMap::GetSingleton()) {
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, false);
				}
			}
		}

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
						if (settings->UseAltThirdPersonCam()) {
							return false;
						}
						TogglePOVSwitchOff();
					}
					break;
				case Camera::CAM::kUFO:
					{
						a_playerCamera->ToggleFreeCameraMode(false);
						if (a_camSettings->type == Camera::TYPE::kDeath) {
							std::jthread t(ReloadLastSave);
							t.detach();
						}
					}
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

        void ReloadLastSave()
		{
			const auto camDuration = Settings::GetSingleton()->GetDeathCamera()->camDuration;
			std::this_thread::sleep_for(std::chrono::seconds(camDuration));

			RE::SubtitleManager::GetSingleton()->KillSubtitles();
		    if (!RE::BGSSaveLoadManager::GetSingleton()->LoadMostRecentSaveGame()) {
				RE::Main::GetSingleton()->resetGame = true;
			}
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

	void InstallOnPostLoad()
	{
		const auto settings = Settings::GetSingleton();

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

	void InstallOnDataLoad()
	{
		if (const auto gameSetting = RE::GameSettingCollection::GetSingleton()->GetSetting("fPlayerDeathReloadTime")) {
			gameSetting->data.f = static_cast<float>(Settings::GetSingleton()->GetDeathCamera()->camDuration);
		}
	}
}
