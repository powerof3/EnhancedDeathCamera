#include "Hooks.h"
#include "Settings.h"

namespace Hooks::DEATH
{
	struct UpdateWhenAIControlledOrDead
	{
		static void thunk(RE::PlayerCharacter* a_player, float a_delta)
		{
			if (a_player->GetLifeState() == Settings::GetSingleton()->GetDeadState()) {
				if (const auto killer = a_player->GetKiller(); killer && !killer->IsDead()) {
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
			if (!detail::SetCamera(a_camera, Settings::GetSingleton()->GetDeathCamera())) {
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

		const auto settings = Settings::GetSingleton();
		if (!settings->UseAltThirdPersonCam() && settings->GetDeathCamera()->moveCamToKiller) {
			UpdateWhenAIControlledOrDead::Install();
		}
	}
}
