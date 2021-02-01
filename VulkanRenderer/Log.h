#pragma once

#include "glm/glm.hpp"

#include <cstdarg>
#include <string>
#include <functional>
#include <fstream>

enum class LogLevel
{
	LEVEL_INFO,
	LEVEL_WARNING,
	LEVEL_ERROR
};

// Channels, like animation, physics, etc

class Log
{
public:

	// Calls func whenever something calls Print. Useful for the console window in the editor
	static void SetCallbackFunc(const std::function<void(LogLevel, const char*)>& func);
	static int Print(LogLevel level, const char* str, ...);
	static void Close();

private:
	static int VPrint(LogLevel level, const char* str, va_list argList);

private:
	static std::function<void(LogLevel, const char*)> callbackFunc;
	static std::ofstream logFile;
};
