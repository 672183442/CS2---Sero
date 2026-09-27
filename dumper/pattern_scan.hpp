#pragma once
#include <Windows.h>
#include <vector>
#include <string>

namespace Scanner {

	inline void* PatternScan(const char* module_name, const char* signature) {
		const auto module_handle = GetModuleHandleA(module_name);

		if (!module_handle)
			return nullptr;

		static auto pattern_to_byte = [](const char* pattern) {
			auto bytes = std::vector<int>{};
			auto start = const_cast<char*>(pattern);
			auto end = const_cast<char*>(pattern) + std::strlen(pattern);

			for (auto current = start; current < end; ++current) {
				if (*current == '?') {
					++current;

					if (*current == '?')
						++current;

					bytes.push_back(-1);
				}
				else {
					bytes.push_back(std::strtoul(current, &current, 16));
				}
			}
			return bytes;
		};

		auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_handle);
		if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

		auto nt_headers =
			reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(module_handle) + dos_header->e_lfanew);
		if (nt_headers->Signature != IMAGE_NT_SIGNATURE) return nullptr;

		auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
		auto pattern_bytes = pattern_to_byte(signature);
		auto scan_bytes = reinterpret_cast<std::uint8_t*>(module_handle);

		auto s = pattern_bytes.size();
		auto d = pattern_bytes.data();

		if (size_of_image < s) return nullptr;

		for (auto i = 0ul; i < size_of_image - s; ++i) {
			bool found = true;

			for (auto j = 0ul; j < s; ++j) {
				if (scan_bytes[i + j] != d[j] && d[j] != -1) {
					found = false;
					break;
				}
			}
			if (found)
				return &scan_bytes[i];
		}

		// Pattern not found - return nullptr instead of throwing
		// to avoid C++ exception / SEH conflicts
		return nullptr;
	}
}

