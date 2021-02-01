#pragma once

class Allocator
{
public:
	Allocator();
	~Allocator();

	void* Allocate(unsigned int size);
	void Free(void* ptr);

	void PrintStats();

private:
	unsigned int usedMemory;
	unsigned int numAllocations;
};
