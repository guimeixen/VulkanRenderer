#pragma once

#include <random>

class Random
{
public:
	static void Init();

	// Returns a random float between 0 and 1
	static float Float();
	// Returns a random float between low and high
	static float Float(float low, float high);

private:
	static std::random_device rd;
	static std::mt19937 mt;
	static std::uniform_real_distribution<float> dist;
};

