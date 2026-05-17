#pragma once
#include "SDL.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

class Emulator;
class SdlGameController;

class SdlGameControllerManager
{
private:
	Emulator* _emu;
	std::vector<std::shared_ptr<SdlGameController>> _controllers;
	std::mutex _controllersLock;

	std::thread _updateThread;
	std::atomic<bool> _stopFlag;
	bool _initialized;

	void UpdateLoop();
	void OpenController(int deviceIndex);
	void CloseController(SDL_JoystickID instanceId);

public:
	SdlGameControllerManager(Emulator* emu);
	~SdlGameControllerManager();

	SdlGameControllerManager(const SdlGameControllerManager&) = delete;
	SdlGameControllerManager& operator=(const SdlGameControllerManager&) = delete;

	void UpdateDevices();
	size_t GetControllerCount();
	bool IsButtonPressed(uint8_t port, uint8_t button);
	std::optional<int16_t> GetAxisPosition(uint8_t port, uint8_t axis);
	void SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft);
};
