#include "SdlGameControllerManager.h"
#include "SdlGameController.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/MessageManager.h"
#include "Utilities/FolderUtilities.h"

#include <chrono>

namespace
{
	bool IsNintendoClassicDevice(int deviceIndex)
	{
		if(SDL_JoystickGetDeviceVendor(deviceIndex) != 0x057E) {
			return false;
		}

		switch(SDL_JoystickGetDeviceProduct(deviceIndex)) {
			case 0x2017: // NSO SNES Controller
				return true;
			default:
				return false;
		}
	}
}

SdlGameControllerManager::SdlGameControllerManager(Emulator* emu)
{
	_emu = emu;
	_stopFlag = false;
	_initialized = false;

	// SDL2's Nintendo Classic HIDAPI driver breaks NSO SNES input reports;
	// keep that device family on the raw IOKit path until SDL fixes it.
	SDL_SetHint("SDL_JOYSTICK_HIDAPI_NINTENDO_CLASSIC", "0");

	if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
		MessageManager::Log(std::string("[Input] Failed to initialize SDL gamepad subsystem: ") + SDL_GetError());
		return;
	}
	_initialized = true;

	// SDL's event pump is main-thread work; this thread only refreshes state.

	//Optional community-maintained mapping database.  Built-in SDL mappings
	//apply regardless; this only extends coverage for less common pads.
	std::string dbPath = FolderUtilities::GetHomeFolder() + "/gamecontrollerdb.txt";
	int added = SDL_GameControllerAddMappingsFromFile(dbPath.c_str());
	if(added > 0) {
		MessageManager::Log(std::string("[Input] Loaded ") + std::to_string(added) + " controller mappings from gamecontrollerdb.txt");
	}

	for(int i = 0; i < SDL_NumJoysticks(); i++) {
		if(!IsNintendoClassicDevice(i) && SDL_IsGameController(i)) {
			OpenController(i);
		}
	}

	_updateThread = std::thread([this]() { UpdateLoop(); });
}

SdlGameControllerManager::~SdlGameControllerManager()
{
	_stopFlag = true;
	if(_updateThread.joinable()) {
		_updateThread.join();
	}

	{
		std::lock_guard<std::mutex> lock(_controllersLock);
		_controllers.clear();
	}

	// Avoid SDL2's macOS joystick teardown here; after HID reconnects it can
	// block in IOHIDManagerUnscheduleFromRunLoop on the UI thread.
}

void SdlGameControllerManager::UpdateLoop()
{
	while(!_stopFlag) {
		if(_initialized) {
			SDL_GameControllerUpdate();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(4));
	}
}

void SdlGameControllerManager::UpdateDevices()
{
	if(!_initialized) {
		return;
	}

	SDL_PumpEvents();

	SDL_Event ev;
	while(SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMAPPED) > 0) {
		if(ev.type == SDL_CONTROLLERDEVICEADDED && !IsNintendoClassicDevice(ev.cdevice.which)) {
			OpenController(ev.cdevice.which);
		} else if(ev.type == SDL_CONTROLLERDEVICEREMOVED) {
			CloseController(ev.cdevice.which);
		}
	}
}

void SdlGameControllerManager::OpenController(int deviceIndex)
{
	if(IsNintendoClassicDevice(deviceIndex)) {
		return;
	}

	SDL_JoystickID deviceInstanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
	{
		std::lock_guard<std::mutex> lock(_controllersLock);
		for(auto& existing : _controllers) {
			if(existing->GetInstanceId() == deviceInstanceId) {
				return;
			}
		}
	}

	auto controller = std::make_shared<SdlGameController>(_emu, deviceIndex);
	if(controller->GetInstanceId() == -1) {
		return;
	}

	std::lock_guard<std::mutex> lock(_controllersLock);
	_controllers.push_back(controller);
}

void SdlGameControllerManager::CloseController(SDL_JoystickID instanceId)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	for(auto it = _controllers.begin(); it != _controllers.end(); ++it) {
		if((*it)->GetInstanceId() == instanceId) {
			_controllers.erase(it);
			MessageManager::Log("[Input Device] Disconnected");
			return;
		}
	}
}

size_t SdlGameControllerManager::GetControllerCount()
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	return _controllers.size();
}

bool SdlGameControllerManager::IsButtonPressed(uint8_t port, uint8_t button)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	if(port < _controllers.size()) {
		return _controllers[port]->IsButtonPressed(button);
	}
	return false;
}

std::optional<int16_t> SdlGameControllerManager::GetAxisPosition(uint8_t port, uint8_t axis)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	if(port < _controllers.size()) {
		return _controllers[port]->GetAxisPosition(axis);
	}
	return std::nullopt;
}

void SdlGameControllerManager::SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	for(auto& controller : _controllers) {
		controller->SetForceFeedback(magnitudeRight, magnitudeLeft);
	}
}
