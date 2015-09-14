#ifndef ANTERU_D3D12_SAMPLE_IMAGEIO_H_
#define ANTERU_D3D12_SAMPLE_IMAGEIO_H_

#include <vector>
#include <cstdint>

#ifdef LoadImage
#undef LoadImage
#endif

std::vector<std::uint8_t> LoadImageFromFile (const char* path, const int rowAlignment,
	int* width, int* height);

std::vector<std::uint8_t> LoadImageFromMemory(const void* data, const std::size_t size, const int rowAlignment,
	int* width, int* height);

#endif