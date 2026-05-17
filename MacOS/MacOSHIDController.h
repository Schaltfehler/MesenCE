#pragma once
#include <IOKit/hid/IOHIDDevice.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

class Emulator;

class MacOSHIDController
{
private:
	Emulator* _emu;
	IOHIDDeviceRef _device;

	std::mutex _stateLock;
	std::array<bool, 24> _buttons{};
	std::array<uint8_t, 64> _reportBuffer{};

public:
	MacOSHIDController(Emulator* emu, IOHIDDeviceRef device);
	~MacOSHIDController();

	MacOSHIDController(const MacOSHIDController&) = delete;
	MacOSHIDController& operator=(const MacOSHIDController&) = delete;

	IOHIDDeviceRef GetDevice() const;

	bool IsButtonPressed(int buttonNumber);
	std::optional<int16_t> GetAxisPosition(int axis);
	void SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft);

	uint8_t* GetReportBuffer();
	size_t GetReportBufferSize() const;
	void HandleInputReport(uint32_t reportId, const uint8_t* report, size_t reportLength);
};
