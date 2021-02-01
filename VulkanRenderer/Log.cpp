#include "Log.h"

#include <iostream>
#include <chrono>
#include <ctime>

std::function<void(LogLevel, const char*)> Log::callbackFunc;
std::ofstream Log::logFile;

void Log::SetCallbackFunc(const std::function<void(LogLevel, const char*)>& func)
{
	callbackFunc = func;
}

int Log::Print(LogLevel level, const char* str, ...)
{
	int charsWritten = 0;

	va_list argList;
	va_start(argList, str);

	charsWritten = VPrint(level, str, argList);

	va_end(argList);

	return charsWritten;
}

int Log::VPrint(LogLevel level, const char* str, va_list argList)
{
	const unsigned int MAX_CHARS = 4096;
	static char buffer[MAX_CHARS];

	int charsWritten = vsnprintf(buffer, MAX_CHARS, str, argList);

	if (callbackFunc)
		callbackFunc(level, buffer);

	std::cout << buffer;

	if (logFile.is_open() == false)
	{
		std::chrono::system_clock::time_point time = std::chrono::system_clock::now();
		std::time_t t = std::chrono::system_clock::to_time_t(time);
		std::tm localTime = *std::localtime(&t);

		char filename[20];
		if (std::strftime(filename, sizeof(filename), "%d-%m-%Y_%H.%M.%S", &localTime) == 0)
		{
			std::cout << "Failed to write log time to string\n";
			logFile.open("log.txt");
		}
		else
		{
			logFile.open(std::string(std::string(filename) + "_log.txt"));
		}
	}
	else
	{
		logFile << buffer /*<< '\n'*/;
		logFile.flush();
	}

	return charsWritten;
}

void Log::Close()
{
	//if (logFile.is_open())
	//	logFile.close();
}
