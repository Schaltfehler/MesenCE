#pragma once
#include "pch.h"

class TraceLogFileSaver
{
private:
	bool _enabled = false;
	string _outputFilepath;
	string _outputBuffer;
	ofstream _outputFile;

public:
	~TraceLogFileSaver()
	{
		StopLogging();
	}

	bool StartLogging(string filename)
	{
		StopLogging();
		_outputBuffer.clear();
		_outputFile.clear();
		_outputFile.open(filename, ios::out | ios::binary);
		_enabled = _outputFile.is_open();
		return _enabled;
	}

	void Flush()
	{
		if(_outputFile) {
			if(!_outputBuffer.empty()) {
				_outputFile << _outputBuffer;
				_outputBuffer.clear();
			}
			_outputFile.flush();
		}
	}

	void StopLogging()
	{
		if(_enabled || _outputFile.is_open()) {
			Flush();
			_enabled = false;
			_outputFile.close();
		}
	}

	__forceinline bool IsEnabled() { return _enabled; }

	void Log(string& log)
	{
		_outputBuffer += log + '\n';
		if(_outputBuffer.size() > 32768) {
			Flush();
		}
	}
};
