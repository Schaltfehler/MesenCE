#pragma once
#include "SDL.h"
#include <cstdint>
#include <optional>

class Emulator;

class SdlGameController
{
private:
	Emulator* _emu;
	SDL_GameController* _controller;
	SDL_JoystickID _instanceId;
	bool _hasRumble;

	int16_t ReadAxis(SDL_GameControllerAxis axis);
	bool AxisAsButton(SDL_GameControllerAxis axis, bool positive);

public:
	SdlGameController(Emulator* emu, int deviceIndex);
	~SdlGameController();

	SdlGameController(const SdlGameController&) = delete;
	SdlGameController& operator=(const SdlGameController&) = delete;

	SDL_JoystickID GetInstanceId() const;

	bool IsButtonPressed(int buttonNumber);
	std::optional<int16_t> GetAxisPosition(int axis);

	void SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft);
};
