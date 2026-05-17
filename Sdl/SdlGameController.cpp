#include "SdlGameController.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"

SdlGameController::SdlGameController(Emulator* emu, int deviceIndex)
{
	_emu = emu;
	_controller = SDL_GameControllerOpen(deviceIndex);
	_instanceId = -1;
	_hasRumble = false;

	if(_controller) {
		_instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(_controller));
		_hasRumble = SDL_GameControllerHasRumble(_controller) == SDL_TRUE;
		const char* name = SDL_GameControllerName(_controller);
		MessageManager::Log(std::string("[Input Connected] Name: ") + (name ? name : "(unknown)"));
	}
}

SdlGameController::~SdlGameController()
{
	if(_controller) {
		SDL_GameControllerClose(_controller);
	}
}

SDL_JoystickID SdlGameController::GetInstanceId() const
{
	return _instanceId;
}

int16_t SdlGameController::ReadAxis(SDL_GameControllerAxis axis)
{
	return _controller ? SDL_GameControllerGetAxis(_controller, axis) : 0;
}

bool SdlGameController::AxisAsButton(SDL_GameControllerAxis axis, bool positive)
{
	double deadZoneRatio = _emu->GetSettings()->GetControllerDeadzoneRatio() * 0.4;
	int16_t threshold = (int16_t)(INT16_MAX * deadZoneRatio);
	int16_t value = ReadAxis(axis);
	return positive ? value > threshold : value < -threshold;
}

bool SdlGameController::IsButtonPressed(int buttonNumber)
{
	if(!_controller) {
		return false;
	}

	switch(buttonNumber) {
		case 0: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_A);
		case 1: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_B);
		case 2: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_X);
		case 3: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_Y);
		case 4: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
		case 5: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
		case 6: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_START);
		case 7: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_BACK);

		case 8: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
		case 9: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		case 10: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
		case 11: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

		case 12: return AxisAsButton(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true);
		case 13: return AxisAsButton(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true);
		case 14: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_LEFTSTICK);
		case 15: return SDL_GameControllerGetButton(_controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK);

		case 16: return AxisAsButton(SDL_CONTROLLER_AXIS_LEFTX, true);
		case 17: return AxisAsButton(SDL_CONTROLLER_AXIS_LEFTX, false);
		case 18: return AxisAsButton(SDL_CONTROLLER_AXIS_LEFTY, true);
		case 19: return AxisAsButton(SDL_CONTROLLER_AXIS_LEFTY, false);

		case 20: return AxisAsButton(SDL_CONTROLLER_AXIS_RIGHTX, true);
		case 21: return AxisAsButton(SDL_CONTROLLER_AXIS_RIGHTX, false);
		case 22: return AxisAsButton(SDL_CONTROLLER_AXIS_RIGHTY, true);
		case 23: return AxisAsButton(SDL_CONTROLLER_AXIS_RIGHTY, false);
	}
	return false;
}

std::optional<int16_t> SdlGameController::GetAxisPosition(int axis)
{
	//Mirrors MacOSGameController: caller passes the raw per-port keycode (24-27).
	axis -= 24;
	if(!_controller || axis < 0 || axis >= 4) {
		return std::nullopt;
	}

	switch(axis) {
		case 0: return ReadAxis(SDL_CONTROLLER_AXIS_LEFTX);
		case 1: return ReadAxis(SDL_CONTROLLER_AXIS_LEFTY);
		case 2: return ReadAxis(SDL_CONTROLLER_AXIS_RIGHTX);
		case 3: return ReadAxis(SDL_CONTROLLER_AXIS_RIGHTY);
	}
	return std::nullopt;
}

void SdlGameController::SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft)
{
	if(_controller && _hasRumble) {
		//100ms duration; callers refresh each frame to keep the effect active.
		SDL_GameControllerRumble(_controller, magnitudeLeft, magnitudeRight, 100);
	}
}
