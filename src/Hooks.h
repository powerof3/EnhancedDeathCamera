#pragma once

class Camera;

namespace Hooks
{
	inline bool gameReloaded{ false };

    namespace detail
	{
		void SetHudMode(const char* a_mode, bool a_enable);
		RE::ThirdPersonState* GetThirdPersonState(const RE::PlayerCamera* a_camera);
		void TogglePOVSwitchOff();

		bool SetCamera(RE::PlayerCamera* a_playerCamera, const Camera* a_camSettings);
		void ReloadLastSave();
	}

	namespace PATCH
	{
		void InputWhenKnockedOut();
		void DoGetUpAction();
	}

    void InstallOnPostLoad();

	void InstallOnDataLoad();
}
