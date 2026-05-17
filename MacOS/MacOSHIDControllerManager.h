#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

class Emulator;
class MacOSHIDController;

class MacOSHIDControllerManager
{
private:
	Emulator* _emu;
	IOHIDManagerRef _hidManager;
	CFRunLoopRef _runLoop;

	std::vector<std::shared_ptr<MacOSHIDController>> _controllers;
	std::mutex _controllersLock;

	std::thread _runLoopThread;
	std::atomic<bool> _runLoopReady;
	std::atomic<bool> _stopFlag;

	void ThreadEntry();

	static void OnDeviceMatched(void* ctx, IOReturn r, void* sender, IOHIDDeviceRef device);
	static void OnDeviceRemoved(void* ctx, IOReturn r, void* sender, IOHIDDeviceRef device);
	static void OnInputReport(void* ctx, IOReturn r, void* sender, IOHIDReportType type, uint32_t reportId, uint8_t* report, CFIndex reportLength);

public:
	MacOSHIDControllerManager(Emulator* emu);
	~MacOSHIDControllerManager();

	MacOSHIDControllerManager(const MacOSHIDControllerManager&) = delete;
	MacOSHIDControllerManager& operator=(const MacOSHIDControllerManager&) = delete;

	size_t GetControllerCount();
	bool IsButtonPressed(uint8_t port, uint8_t button);
	std::optional<int16_t> GetAxisPosition(uint8_t port, uint8_t axis);
	void SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft);
};
