#include "Random.h"

std::random_device Random::rd;
std::mt19937 Random::mt;
std::uniform_real_distribution<float> Random::dist;

void Random::Init()
{
	mt.seed(rd());
	dist.param(std::uniform_real_distribution<float>::param_type(0.0f, 1.0f));
}

float Random::Float()
{
	return dist(mt);
}

float Random::Float(float low, float high)
{
	return low + dist(mt) * (high - low);
}