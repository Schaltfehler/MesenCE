#pragma once
#include "pch.h"
#include "Shared/CpuType.h"
#include "Shared/SettingTypes.h"
#include "Shared/MemoryType.h"

struct lua_State;
class ScriptingContext;
class Debugger;
class Emulator;
class MemoryDumper;
class DebugHud;
class BaseVideoFilter;
class Serializer;

class LuaApi
{
public:
	static void SetContext(ScriptingContext* context);
	static int GetLibrary(lua_State* lua);

	static void LuaPushIntValue(lua_State* lua, string name, int value);

	static DebugHud* GetHud();

	static int SelectDrawSurface(lua_State* lua);

	static int GetMemorySize(lua_State* lua);
	static int GetMemoryRegions(lua_State* lua);

	static int ReadMemory(lua_State* lua);
	static int WriteMemory(lua_State* lua);
	static int ReadMemory16(lua_State* lua);
	static int WriteMemory16(lua_State* lua);
	static int ReadMemory32(lua_State* lua);
	static int WriteMemory32(lua_State* lua);

	static int GetLabelAddress(lua_State* lua);
	static int ConvertAddress(lua_State* lua);

	static int RegisterMemoryCallback(lua_State* lua);
	static int UnregisterMemoryCallback(lua_State* lua);
	static int RegisterMemoryPayloadCallback(lua_State* lua);
	static int UnregisterMemoryPayloadCallback(lua_State* lua);
	static int RegisterEventCallback(lua_State* lua);
	static int UnregisterEventCallback(lua_State* lua);
	static int RegisterEventPayloadCallback(lua_State* lua);
	static int UnregisterEventPayloadCallback(lua_State* lua);

	static int MeasureString(lua_State* lua);
	static int DrawString(lua_State* lua);

	static int DrawLine(lua_State* lua);
	static int DrawPixel(lua_State* lua);
	static int DrawRectangle(lua_State* lua);
	static int ClearScreen(lua_State* lua);

	static int GetScreenSize(lua_State* lua);
	static int GetDrawSurfaceSize(lua_State* lua);
	static int GetScreenBuffer(lua_State* lua);
	static int SetScreenBuffer(lua_State* lua);

	static int GetPixel(lua_State* lua);
	static int GetMouseState(lua_State* lua);

	static int Log(lua_State* lua);
	static int DisplayMessage(lua_State* lua);

	static int Reset(lua_State* lua);
	static int Stop(lua_State* lua);
	static int BreakExecution(lua_State* lua);
	static int Resume(lua_State* lua);
	static int Step(lua_State* lua);
	static int Rewind(lua_State* lua);

	static int TakeScreenshot(lua_State* lua);

	static int CreateSavestate(lua_State* lua);
	static int LoadSavestate(lua_State* lua);

	static int IsKeyPressed(lua_State* lua);

	static int GetInput(lua_State* lua);
	static int SetInput(lua_State* lua);

	static int AddCheat(lua_State* lua);
	static int ClearCheats(lua_State* lua);

	static int GetScriptDataFolder(lua_State* lua);
	static int GetRomInfo(lua_State* lua);
	static int GetScriptInfo(lua_State* lua);
	static int GetRuntimeCapabilities(lua_State* lua);
	static int GetPpuCheckpoint(lua_State* lua);
	static int BeginPixelProvenance(lua_State* lua);
	static int GetPixelProvenance(lua_State* lua);
	static int BeginAudioCapture(lua_State* lua);
	static int GetAudioCaptureStatus(lua_State* lua);
	static int StopAudioCapture(lua_State* lua);
	static int GetLogWindowLog(lua_State* lua);

	static void GenerateStateTable(Serializer& s, lua_State* lua);
	static void ReadStateTable(Serializer& s, lua_State* lua);

	static int GetCpuCycleCount(lua_State* lua);
	static int GetMasterClock(lua_State* lua);

	static int GetState(lua_State* lua);
	static int GetCpuState(lua_State* lua);
	static int ReadRegister(lua_State* lua);

	static int SetState(lua_State* lua);
	static int SetCpuState(lua_State* lua);
	static int WriteRegister(lua_State* lua);

	static int GetAccessCounters(lua_State* lua);
	static int GetAccessCountersRange(lua_State* lua);
	static int GetAccessCounterRows(lua_State* lua);
	static int GetAccessSummary(lua_State* lua);
	static int ResetAccessCounters(lua_State* lua);

	static int GetCdlData(lua_State* lua);
	static int GetCdlDataRange(lua_State* lua);
	static int GetCdlRows(lua_State* lua);
	static int GetCdlSummary(lua_State* lua);
	static int GetCdlFunctions(lua_State* lua);
	static int ResetCdl(lua_State* lua);

	static int GetTraceRows(lua_State* lua);
	static int GetTraceSize(lua_State* lua);
	static int ClearTrace(lua_State* lua);
	static int SetTraceOptions(lua_State* lua);
	static int StartTraceLoggerFile(lua_State* lua);
	static int FlushTraceLoggerFile(lua_State* lua);
	static int StopTraceLoggerFile(lua_State* lua);

	static int GetDebuggerFeatures(lua_State* lua);
	static int GetInstructionProgress(lua_State* lua);
	static int GetCallstack(lua_State* lua);
	static int GetProfilerData(lua_State* lua);
	static int ResetProfiler(lua_State* lua);

	static int GetDisassemblyRows(lua_State* lua);
	static int DecodeInstructions(lua_State* lua);
	static int GetDisassemblyRowAddress(lua_State* lua);
	static int SearchDisassembly(lua_State* lua);
	static int FindDisassemblyOccurrences(lua_State* lua);

private:
	static FrameInfo InternalGetScreenSize();

	static Emulator* _emu;
	static Debugger* _debugger;
	static MemoryDumper* _memoryDumper;
	static ScriptingContext* _context;
	static Serializer _serializer;

	static std::pair<unique_ptr<BaseVideoFilter>, FrameInfo> GetRenderedFrame();
	template<typename T> static void GenerateEnumDefinition(lua_State* lua, string enumName, unordered_set<T> excludedValues = {});
};
