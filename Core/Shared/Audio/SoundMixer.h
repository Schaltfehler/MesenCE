#pragma once
#include "pch.h"
#include "Core/Shared/Interfaces/IAudioDevice.h"
#include "Core/Shared/SettingTypes.h"
#include "Utilities/safe_ptr.h"
#include "Utilities/Audio/HermiteResampler.h"

class Emulator;
class Equalizer;
class SoundResampler;
class WaveRecorder;
class IAudioProvider;
class CrossFeedFilter;
class ReverbFilter;

struct InvestigationAudioCaptureState
{
	bool Started = false;
	bool Active = false;
	bool Closed = false;
	bool Overflow = false;
	bool LimitReached = false;
	bool WriteError = false;
	uint32_t SampleRate = 0;
	uint8_t Channels = 2;
	uint8_t BitsPerSample = 16;
	uint64_t SampleFrameCount = 0;
	uint64_t SilentSampleFrameCount = 0;
	uint64_t MaxSampleFrames = 0;
	uint32_t EffectiveMasterVolume = 100;
	bool EqualizerEnabled = false;
	double EqualizerBandGains[20] = {};
	bool ReverbEnabled = false;
	uint32_t ReverbStrength = 0;
	uint32_t ReverbDelay = 0;
	bool CrossFeedEnabled = false;
	uint32_t CrossFeedRatio = 0;
	bool IntegerFpsMode = false;
	bool SettingsChanged = false;
	string OutputPath;
};

class SoundMixer
{
private:
	IAudioDevice* _audioDevice;
	vector<IAudioProvider*> _audioProviders;
	Emulator* _emu;
	unique_ptr<Equalizer> _equalizer;
	unique_ptr<SoundResampler> _resampler;
	safe_ptr<WaveRecorder> _waveRecorder;
	unique_ptr<WaveRecorder> _investigationWaveRecorder;
	InvestigationAudioCaptureState _investigationCapture = {};
	int16_t* _sampleBuffer = nullptr;

	HermiteResampler _pitchAdjust;
	int16_t* _pitchAdjustBuffer = nullptr;

	int16_t _leftSample = 0;
	int16_t _rightSample = 0;

	unique_ptr<CrossFeedFilter> _crossFeedFilter;
	unique_ptr<ReverbFilter> _reverbFilter;

	void ProcessEqualizer(int16_t* samples, uint32_t sampleCount, uint32_t targetRate);
	void WriteInvestigationCapture(int16_t* samples, uint32_t sampleCount, uint32_t sampleRate, AudioConfig& cfg, uint32_t effectiveMasterVolume);

public:
	SoundMixer(Emulator* emu);
	~SoundMixer();

	void PlayAudioBuffer(int16_t* samples, uint32_t sampleCount, uint32_t sourceRate);
	void StopAudio(bool clearBuffer = false);

	void RegisterAudioDevice(IAudioDevice* audioDevice);

	void RegisterAudioProvider(IAudioProvider* provider);
	void UnregisterAudioProvider(IAudioProvider* provider);

	AudioStatistics GetStatistics();
	double GetRateAdjustment();

	void StartRecording(string filepath);
	void StopRecording();
	bool IsRecording();
	bool StartInvestigationCapture(string filepath, uint64_t maxSampleFrames);
	void StopInvestigationCapture();
	InvestigationAudioCaptureState GetInvestigationCaptureState();
	void GetLastSamples(int16_t& left, int16_t& right);
};
