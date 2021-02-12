#include "Utils.h"

unsigned int utils::Align(unsigned int value, unsigned int alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
