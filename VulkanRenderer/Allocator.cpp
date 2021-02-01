#include "Allocator.h"

#include "Log.h"

Allocator::Allocator()
{
	usedMemory = 0;
	numAllocations = 0;
}

Allocator::~Allocator()
{
}

void* Allocator::Allocate(unsigned int size)
{
	usedMemory += size;
	numAllocations++;
	return malloc(size);
}

void Allocator::Free(void* ptr)
{
	//used
	numAllocations++;
	free(ptr);
}

void Allocator::PrintStats()
{
	Log::Print(LogLevel::LEVEL_INFO, "\nTotal allocated memory: %.2f mib\n", (float)usedMemory / 1024.0f / 1024.0f);
	Log::Print(LogLevel::LEVEL_INFO, "Num allocations: %u\n\n", numAllocations);
}
