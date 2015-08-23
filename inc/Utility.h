#ifndef MANTLE_TEST_UTILITY_H_
#define MANTLE_TEST_UTILITY_H_

#include <vector>
#include <cstdint>

///////////////////////////////////////////////////////////////////////////////
template <typename T>
constexpr T RoundToNextMultiple (const T a, const T multiple)
{
	return ((a + multiple - 1) / multiple) * multiple;
}

std::vector<std::uint8_t> ReadFile (const char* filename);

#endif