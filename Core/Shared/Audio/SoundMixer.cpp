#include "pch.h"
#include "Shared/Audio/SoundMixer.h"
#include "Shared/Audio/AudioPlayerHud.h"
#include "Shared/Emulator.h"
#include "Shared/EmuSettings.h"
#include "Shared/Audio/SoundResampler.h"
#include "Shared/RewindManager.h"
#include "Shared/Video/VideoRenderer.h"
#include "Shared/Audio/WaveRecorder.h"
#include "Shared/Interfaces/IAudioProvider.h"
#include "Utilities/Audio/Equalizer.h"
#include "Utilities/Audio/ReverbFilter.h"
#include "Utilities/Audio/CrossFeedFilter.h"

static void CopyEqualizerBandGains(AudioConfig& cfg, double* output)
{
	double gains[20] = {
		cfg.Band1Gain, cfg.Band2Gain, cfg.Band3Gain, cfg.Band4Gain, cfg.Band5Gain,
		cfg.Band6Gain, cfg.Band7Gain, cfg.Band8Gain, cfg.Band9Gain, cfg.Band10Gain,
		cfg.Band11Gain, cfg.Band12Gain, cfg.Band13Gain, cfg.Band14Gain, cfg.Band15Gain,
		cfg.Band16Gain, cfg.Band17Gain, cfg.Band18Gain, cfg.Band19Gain, cfg.Band20Gain
	};
	memcpy(output, gains, sizeof(gains));
}

static bool AudioCaptureSettingsMatch(InvestigationAudioCaptureState& state, AudioConfig& cfg, uint32_t effectiveMasterVolume, bool integerFpsMode)
{
	double gains[20] = {};
	CopyEqualizerBandGains(cfg, gains);
	return state.SampleRate == cfg.SampleRate &&
		state.EffectiveMasterVolume == effectiveMasterVolume &&
		state.EqualizerEnabled == cfg.EnableEqualizer &&
		memcmp(state.EqualizerBandGains, gains, sizeof(gains)) == 0 &&
		state.ReverbEnabled == cfg.ReverbEnabled &&
		state.ReverbStrength == cfg.ReverbStrength &&
		state.ReverbDelay == cfg.ReverbDelay &&
		state.CrossFeedEnabled == cfg.CrossFeedEnabled &&
		state.CrossFeedRatio == cfg.CrossFeedRatio &&
		state.IntegerFpsMode == integerFpsMode;
}

SoundMixer::SoundMixer(Emulator* emu)
{
	_emu = emu;
	_audioDevice = nullptr;
	_resampler.reset(new SoundResampler(emu));
	_sampleBuffer = new int16_t[0x10000];
	_pitchAdjustBuffer = new int16_t[0x8000];
	_reverbFilter.reset(new ReverbFilter());
	_crossFeedFilter.reset(new CrossFeedFilter());
}

SoundMixer::~SoundMixer()
{
	delete[] _sampleBuffer;
	delete[] _pitchAdjustBuffer;
}

void SoundMixer::RegisterAudioDevice(IAudioDevice* audioDevice)
{
	_audioDevice = audioDevice;
}

void SoundMixer::RegisterAudioProvider(IAudioProvider* provider)
{
	_audioProviders.push_back(provider);
}

void SoundMixer::UnregisterAudioProvider(IAudioProvider* provider)
{
	vector<IAudioProvider*>& vec = _audioProviders;
	vec.erase(std::remove(vec.begin(), vec.end(), provider), vec.end());
}

AudioStatistics SoundMixer::GetStatistics()
{
	if(_audioDevice) {
		return _audioDevice->GetStatistics();
	} else {
		return AudioStatistics();
	}
}

void SoundMixer::StopAudio(bool clearBuffer)
{
	if(_audioDevice) {
		if(clearBuffer) {
			_audioDevice->Stop();
		} else {
			_audioDevice->Pause();
		}
	}
}

void SoundMixer::PlayAudioBuffer(int16_t* samples, uint32_t sampleCount, uint32_t sourceRate)
{
	if(sampleCount == 0) {
		return;
	}

	EmuSettings* settings = _emu->GetSettings();
	AudioPlayerHud* audioPlayer = _emu->GetAudioPlayerHud();
	AudioConfig cfg = settings->GetAudioConfig();
	bool isRecording = IsRecording() || _emu->GetVideoRenderer()->IsRecording();

	uint32_t masterVolume = audioPlayer ? audioPlayer->GetVolume() : cfg.MasterVolume;
	if(!isRecording) {
		if(!audioPlayer && settings->CheckFlag(EmulationFlags::InBackground)) {
			if(cfg.MuteSoundInBackground) {
				masterVolume = 0;
			} else if(cfg.ReduceSoundInBackground) {
				masterVolume = cfg.VolumeReduction == 100 ? 0 : masterVolume * (100 - cfg.VolumeReduction) / 100;
			}
		} else if(cfg.ReduceSoundInFastForward && settings->CheckFlag(EmulationFlags::TurboOrRewind)) {
			masterVolume = cfg.VolumeReduction == 100 ? 0 : masterVolume * (100 - cfg.VolumeReduction) / 100;
		}
	}

	_leftSample = samples[0];
	_rightSample = samples[1];

	int16_t* out = _sampleBuffer;
	uint32_t count = _resampler->Resample(samples, sampleCount, sourceRate, cfg.SampleRate, out, 0x10000 / 2);

	uint32_t targetRate = (uint32_t)(cfg.SampleRate * _resampler->GetRateAdjustment());
	for(IAudioProvider* provider : _audioProviders) {
		provider->MixAudio(out, count, targetRate);
	}

	if(cfg.EnableEqualizer) {
		ProcessEqualizer(out, count, targetRate);
	}

	if(audioPlayer) {
		audioPlayer->ProcessSamples(out, count, targetRate);
	}

	if(cfg.ReverbEnabled) {
		if(cfg.ReverbStrength > 0) {
			_reverbFilter->ApplyFilter(out, count, cfg.SampleRate, cfg.ReverbStrength / 10.0, cfg.ReverbDelay / 10.0);
		} else {
			_reverbFilter->ResetFilter();
		}
	}

	if(cfg.CrossFeedEnabled) {
		_crossFeedFilter->ApplyFilter(out, count, cfg.CrossFeedRatio);
	}

	if(masterVolume < 100) {
		//Apply volume if not using the default value
		for(uint32_t i = 0; i < count * 2; i++) {
			out[i] = (int32_t)out[i] * (int32_t)masterVolume / 100;
		}
	}

	WriteInvestigationCapture(out, count, cfg.SampleRate, cfg, masterVolume);

	RewindManager* rewindManager = _emu->GetRewindManager();
	if(!_emu->IsRunAheadFrame() && rewindManager && rewindManager->SendAudio(out, count)) {
		if(isRecording) {
			shared_ptr<WaveRecorder> recorder = _waveRecorder.lock();
			if(recorder) {
				if(!recorder->WriteSamples(out, count, cfg.SampleRate, true)) {
					StopRecording();
				}
			}
			_emu->GetVideoRenderer()->AddRecordingSound(out, count, cfg.SampleRate);
		}

		//Only send the audio to the device if the emulation is running
		//(this is to prevent playing an audio blip when loading a save state)
		if(!_emu->IsPaused() && _audioDevice) {
			if(cfg.EnableAudio) {
				uint32_t emulationSpeed = _emu->GetSettings()->GetEmulationSpeed();
				if(emulationSpeed > 0 && emulationSpeed < 100) {
					//Slow down playback when playing at less than 100% speed
					_pitchAdjust.SetSampleRates(targetRate, targetRate * 100.0 / emulationSpeed);
					count = _pitchAdjust.Resample<false>(_sampleBuffer, count, _pitchAdjustBuffer, 0x8000 / 2);
					if(count >= 0x4000) {
						//Mute sound when playing so slowly that the 64k buffer is not large enough to hold everything
						memset(_pitchAdjustBuffer, 0, 0x8000 * sizeof(int16_t));
					}
					out = _pitchAdjustBuffer;
				}

				_audioDevice->PlayBuffer(out, count, cfg.SampleRate, true);
				_audioDevice->ProcessEndOfFrame();
			} else {
				_audioDevice->Stop();
			}
		}
	}
}

void SoundMixer::WriteInvestigationCapture(int16_t* samples, uint32_t sampleCount, uint32_t sampleRate, AudioConfig& cfg, uint32_t effectiveMasterVolume)
{
	if(!_investigationCapture.Active || !_investigationWaveRecorder) {
		return;
	}

	bool integerFpsMode = _emu->GetSettings()->GetVideoConfig().IntegerFpsMode;
	if(!AudioCaptureSettingsMatch(_investigationCapture, cfg, effectiveMasterVolume, integerFpsMode)) {
		_investigationCapture.SettingsChanged = true;
	}

	uint64_t remaining = _investigationCapture.MaxSampleFrames - _investigationCapture.SampleFrameCount;
	uint32_t retainedCount = (uint32_t)std::min<uint64_t>(sampleCount, remaining);
	if(retainedCount > 0) {
		if(!_investigationWaveRecorder->WriteSamples(samples, retainedCount, sampleRate, true)) {
			_investigationCapture.WriteError = true;
			_investigationCapture.Active = false;
			_investigationCapture.Closed = true;
			_investigationWaveRecorder.reset();
			return;
		}
		for(uint32_t i = 0; i < retainedCount; i++) {
			if(samples[i * 2] == 0 && samples[i * 2 + 1] == 0) {
				_investigationCapture.SilentSampleFrameCount++;
			}
		}
		_investigationCapture.SampleFrameCount += retainedCount;
	}
	if(retainedCount < sampleCount || _investigationCapture.SampleFrameCount >= _investigationCapture.MaxSampleFrames) {
		_investigationCapture.Overflow = retainedCount < sampleCount;
		_investigationCapture.LimitReached = true;
		_investigationCapture.Active = false;
		_investigationCapture.Closed = true;
		_investigationWaveRecorder.reset();
	}
}

void SoundMixer::ProcessEqualizer(int16_t* samples, uint32_t sampleCount, uint32_t targetRate)
{
	AudioConfig cfg = _emu->GetSettings()->GetAudioConfig();
	if(!_equalizer) {
		_equalizer.reset(new Equalizer());
	}
	vector<double> bandGains = {
		cfg.Band1Gain, cfg.Band2Gain, cfg.Band3Gain, cfg.Band4Gain, cfg.Band5Gain, cfg.Band6Gain, cfg.Band7Gain, cfg.Band8Gain, cfg.Band9Gain, cfg.Band10Gain, cfg.Band11Gain, cfg.Band12Gain, cfg.Band13Gain, cfg.Band14Gain, cfg.Band15Gain, cfg.Band16Gain, cfg.Band17Gain, cfg.Band18Gain, cfg.Band19Gain, cfg.Band20Gain
	};

	_equalizer->UpdateEqualizers(bandGains, cfg.SampleRate);
	_equalizer->ApplyEqualizer(sampleCount, samples);
}

double SoundMixer::GetRateAdjustment()
{
	return _resampler->GetRateAdjustment();
}

void SoundMixer::StartRecording(string filepath)
{
	_waveRecorder.reset(new WaveRecorder(filepath, _emu->GetSettings()->GetAudioConfig().SampleRate, true));
}

void SoundMixer::StopRecording()
{
	_waveRecorder.reset();
}

bool SoundMixer::IsRecording()
{
	return _waveRecorder != nullptr || _investigationCapture.Active;
}

bool SoundMixer::StartInvestigationCapture(string filepath, uint64_t maxSampleFrames)
{
	StopInvestigationCapture();
	_investigationCapture = {};
	_investigationCapture.Started = true;
	AudioConfig cfg = _emu->GetSettings()->GetAudioConfig();
	AudioPlayerHud* audioPlayer = _emu->GetAudioPlayerHud();
	_investigationCapture.SampleRate = cfg.SampleRate;
	_investigationCapture.MaxSampleFrames = maxSampleFrames;
	_investigationCapture.EffectiveMasterVolume = audioPlayer ? audioPlayer->GetVolume() : cfg.MasterVolume;
	_investigationCapture.EqualizerEnabled = cfg.EnableEqualizer;
	CopyEqualizerBandGains(cfg, _investigationCapture.EqualizerBandGains);
	_investigationCapture.ReverbEnabled = cfg.ReverbEnabled;
	_investigationCapture.ReverbStrength = cfg.ReverbStrength;
	_investigationCapture.ReverbDelay = cfg.ReverbDelay;
	_investigationCapture.CrossFeedEnabled = cfg.CrossFeedEnabled;
	_investigationCapture.CrossFeedRatio = cfg.CrossFeedRatio;
	_investigationCapture.IntegerFpsMode = _emu->GetSettings()->GetVideoConfig().IntegerFpsMode;
	_investigationCapture.OutputPath = filepath;
	_investigationWaveRecorder.reset(new WaveRecorder(filepath, _investigationCapture.SampleRate, true));
	if(!_investigationWaveRecorder->IsOpen()) {
		_investigationCapture.WriteError = true;
		_investigationCapture.Closed = true;
		_investigationWaveRecorder.reset();
		return false;
	}
	_investigationCapture.Active = true;
	return true;
}

void SoundMixer::StopInvestigationCapture()
{
	if(_investigationCapture.Started) {
		_investigationCapture.Active = false;
		_investigationCapture.Closed = true;
	}
	_investigationWaveRecorder.reset();
}

InvestigationAudioCaptureState SoundMixer::GetInvestigationCaptureState()
{
	return _investigationCapture;
}

void SoundMixer::GetLastSamples(int16_t& left, int16_t& right)
{
	left = _leftSample;
	right = _rightSample;
}