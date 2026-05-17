#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDKeys.h>

#include "MacOSHIDController.h"
#define Debugger MesenDebugger
#include "Shared/MessageManager.h"
#undef Debugger

MacOSHIDController::MacOSHIDController(Emulator* emu, IOHIDDeviceRef device)
{
	_emu = emu;
	_device = device;

	CFTypeRef name = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
	const char* utf8 = (name && CFGetTypeID(name) == CFStringGetTypeID())
		? CFStringGetCStringPtr((CFStringRef)name, kCFStringEncodingUTF8)
		: nullptr;
	MessageManager::Log(std::string("[Input Connected] (HID) Name: ") + (utf8 ? utf8 : "(unknown)"));
}

MacOSHIDController::~MacOSHIDController()
{
}

IOHIDDeviceRef MacOSHIDController::GetDevice() const
{
	return _device;
}

uint8_t* MacOSHIDController::GetReportBuffer()
{
	return _reportBuffer.data();
}

size_t MacOSHIDController::GetReportBufferSize() const
{
	return _reportBuffer.size();
}

void MacOSHIDController::HandleInputReport(uint32_t reportId, const uint8_t* report, size_t reportLength)
{
	if(reportId != 0x30 || reportLength < 12) {
		return;
	}

	uint8_t b0 = report[3];
	uint8_t b1 = report[4];
	uint8_t b2 = report[5];

	std::lock_guard<std::mutex> lock(_stateLock);
	_buttons[0] = (b0 & 0x08) != 0; // A
	_buttons[1] = (b0 & 0x04) != 0; // B
	_buttons[2] = (b0 & 0x02) != 0; // X
	_buttons[3] = (b0 & 0x01) != 0; // Y
	_buttons[4] = (b2 & 0x40) != 0; // L
	_buttons[5] = (b0 & 0x40) != 0; // R
	_buttons[6] = (b1 & 0x02) != 0; // Start
	_buttons[7] = (b1 & 0x01) != 0; // Select
	_buttons[8] = (b2 & 0x02) != 0; // Up
	_buttons[9] = (b2 & 0x01) != 0; // Down
	_buttons[10] = (b2 & 0x08) != 0; // Left
	_buttons[11] = (b2 & 0x04) != 0; // Right
	_buttons[12] = (b2 & 0x80) != 0;
	_buttons[13] = (b0 & 0x80) != 0;
}

bool MacOSHIDController::IsButtonPressed(int buttonNumber)
{
	std::lock_guard<std::mutex> lock(_stateLock);
	if(buttonNumber < 0 || buttonNumber >= (int)_buttons.size()) {
		return false;
	}
	return _buttons[buttonNumber];
}

std::optional<int16_t> MacOSHIDController::GetAxisPosition(int /*axis*/)
{
	return std::nullopt;
}

void MacOSHIDController::SetForceFeedback(uint16_t /*magnitudeRight*/, uint16_t /*magnitudeLeft*/)
{
}
