#import <Foundation/Foundation.h>

#include "MacOSHIDControllerManager.h"
#include "MacOSHIDController.h"
#define Debugger MesenDebugger
#include "Shared/MessageManager.h"
#undef Debugger

#include <chrono>

namespace
{
	// SDL2 handles general gamepads. This IOKit path is only for NSO Classic
	// devices whose SDL2 Nintendo Classic HIDAPI driver corrupts input reports.
	// Add more PIDs only after validating the same SDL failure mode.
	const uint32_t kNintendoVendorId = 0x057E;
	const uint32_t kSupportedProducts[] = { 0x2017 };

	CFArrayRef CreateMatchingDictionaries()
	{
		CFMutableArrayRef arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		for(uint32_t pid : kSupportedProducts) {
			CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			int32_t vid = (int32_t)kNintendoVendorId;
			int32_t product = (int32_t)pid;
			CFNumberRef vidNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vid);
			CFNumberRef pidNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &product);
			CFDictionarySetValue(d, CFSTR(kIOHIDVendorIDKey), vidNum);
			CFDictionarySetValue(d, CFSTR(kIOHIDProductIDKey), pidNum);
			CFRelease(vidNum);
			CFRelease(pidNum);
			CFArrayAppendValue(arr, d);
			CFRelease(d);
		}
		return arr;
	}
}

MacOSHIDControllerManager::MacOSHIDControllerManager(Emulator* emu)
{
	_emu = emu;
	_hidManager = nullptr;
	_runLoop = nullptr;
	_runLoopReady = false;
	_stopFlag = false;

	_runLoopThread = std::thread([this]() { ThreadEntry(); });

	//Wait briefly for the run-loop thread to publish its CFRunLoopRef, so
	//destructor can safely signal it.  Bounded so a failed init never hangs.
	for(int i = 0; i < 50 && !_runLoopReady; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

MacOSHIDControllerManager::~MacOSHIDControllerManager()
{
	_stopFlag = true;
	if(_runLoop) {
		CFRunLoopStop(_runLoop);
	}
	if(_runLoopThread.joinable()) {
		_runLoopThread.join();
	}

	std::lock_guard<std::mutex> lock(_controllersLock);
	_controllers.clear();
}

void MacOSHIDControllerManager::ThreadEntry()
{
	_hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if(!_hidManager) {
		MessageManager::Log("[Input] Failed to create IOHIDManager");
		_runLoopReady = true;
		return;
	}

	CFArrayRef matchingArray = CreateMatchingDictionaries();
	IOHIDManagerSetDeviceMatchingMultiple(_hidManager, matchingArray);
	CFRelease(matchingArray);

	IOHIDManagerRegisterDeviceMatchingCallback(_hidManager, OnDeviceMatched, this);
	IOHIDManagerRegisterDeviceRemovalCallback(_hidManager, OnDeviceRemoved, this);

	_runLoop = CFRunLoopGetCurrent();
	IOHIDManagerScheduleWithRunLoop(_hidManager, _runLoop, kCFRunLoopDefaultMode);

	IOReturn result = IOHIDManagerOpen(_hidManager, kIOHIDOptionsTypeNone);
	if(result != kIOReturnSuccess) {
		MessageManager::Log(std::string("[Input] IOHIDManagerOpen failed: 0x") + std::to_string((unsigned)result));
	}

	_runLoopReady = true;

	while(!_stopFlag) {
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);
	}

	IOHIDManagerUnscheduleFromRunLoop(_hidManager, _runLoop, kCFRunLoopDefaultMode);
	IOHIDManagerClose(_hidManager, kIOHIDOptionsTypeNone);
	CFRelease(_hidManager);
	_hidManager = nullptr;
}

void MacOSHIDControllerManager::OnDeviceMatched(void* ctx, IOReturn /*r*/, void* /*sender*/, IOHIDDeviceRef device)
{
	auto* self = static_cast<MacOSHIDControllerManager*>(ctx);
	auto controller = std::make_shared<MacOSHIDController>(self->_emu, device);

	std::lock_guard<std::mutex> lock(self->_controllersLock);
	self->_controllers.push_back(controller);
	IOHIDDeviceRegisterInputReportCallback(device, controller->GetReportBuffer(), controller->GetReportBufferSize(), OnInputReport, self);
}

void MacOSHIDControllerManager::OnDeviceRemoved(void* ctx, IOReturn /*r*/, void* /*sender*/, IOHIDDeviceRef device)
{
	auto* self = static_cast<MacOSHIDControllerManager*>(ctx);

	std::lock_guard<std::mutex> lock(self->_controllersLock);
	for(auto it = self->_controllers.begin(); it != self->_controllers.end(); ++it) {
		if((*it)->GetDevice() == device) {
			self->_controllers.erase(it);
			MessageManager::Log("[Input Device] (HID) Disconnected");
			return;
		}
	}
}

void MacOSHIDControllerManager::OnInputReport(void* ctx, IOReturn /*r*/, void* sender, IOHIDReportType /*type*/, uint32_t reportId, uint8_t* report, CFIndex reportLength)
{
	auto* self = static_cast<MacOSHIDControllerManager*>(ctx);
	IOHIDDeviceRef device = (IOHIDDeviceRef)sender;

	std::lock_guard<std::mutex> lock(self->_controllersLock);
	for(auto& controller : self->_controllers) {
		if(controller->GetDevice() == device) {
			controller->HandleInputReport(reportId, report, (size_t)reportLength);
			return;
		}
	}
}

size_t MacOSHIDControllerManager::GetControllerCount()
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	return _controllers.size();
}

bool MacOSHIDControllerManager::IsButtonPressed(uint8_t port, uint8_t button)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	if(port < _controllers.size()) {
		return _controllers[port]->IsButtonPressed(button);
	}
	return false;
}

std::optional<int16_t> MacOSHIDControllerManager::GetAxisPosition(uint8_t port, uint8_t axis)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	if(port < _controllers.size()) {
		return _controllers[port]->GetAxisPosition(axis);
	}
	return std::nullopt;
}

void MacOSHIDControllerManager::SetForceFeedback(uint16_t magnitudeRight, uint16_t magnitudeLeft)
{
	std::lock_guard<std::mutex> lock(_controllersLock);
	for(auto& controller : _controllers) {
		controller->SetForceFeedback(magnitudeRight, magnitudeLeft);
	}
}
