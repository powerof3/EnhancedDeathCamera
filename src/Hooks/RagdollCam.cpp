#include "Hooks.h"
#include "Settings.h"

namespace Hooks::RAGDOLL
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
		static inline size_t                           index = 0x07D;

		static void Install()
		{
			stl::write_vfunc<RE::PlayerCharacter, UpdateAnimation>();
		}

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
			if (!detail::SetCamera(a_camera, Settings::GetSingleton()->GetRagdollCamera())) {
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
			UpdateAnimation::Install();
		}
	}
}
