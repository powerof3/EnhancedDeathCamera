#include "Hooks.h"
#include "Settings.h"

namespace Hooks::BLEEDOUT
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
