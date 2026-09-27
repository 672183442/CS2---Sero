#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace memory
{
    inline uintptr_t clientBase = 0;
    inline uintptr_t engineBase = 0;

    inline void Initialize()
    {
        clientBase = (uintptr_t)GetModuleHandleW(L"client.dll");
        engineBase = (uintptr_t)GetModuleHandleW(L"engine2.dll");
    }

    inline bool IsValidRange(uintptr_t address, size_t size, DWORD accessMask)
    {
        if (address < 0x10000 || size == 0) return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

        uintptr_t regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t regionEnd = regionStart + mbi.RegionSize;
        if (address < regionStart || address + size < address || address + size > regionEnd) return false;

        return (mbi.Protect & accessMask) != 0;
    }

    inline bool IsLikelyUserAddress(uintptr_t address, size_t size)
    {
        return address >= 0x10000 && address + size >= address && address + size < 0x0000800000000000ULL;
    }

    inline bool IsReadable(uintptr_t address, size_t size)
    {
        constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return IsValidRange(address, size, readable);
    }

    inline bool IsWritable(uintptr_t address, size_t size)
    {
        constexpr DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return IsValidRange(address, size, writable);
    }

    template <typename T>
    inline T Read(uintptr_t address)
    {
        if (!IsLikelyUserAddress(address, sizeof(T))) return T{};
        __try {
            return *reinterpret_cast<T*>(address);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return T{};
        }
    }

    template <typename T>
    inline void Write(uintptr_t address, T value)
    {
        if (!IsLikelyUserAddress(address, sizeof(T))) return;
        __try {
            *reinterpret_cast<T*>(address) = value;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    inline bool ReadRaw(uintptr_t address, void* buffer, size_t size)
    {
        if (!buffer || !IsReadable(address, size)) return false;
        __try {
            memcpy(buffer, reinterpret_cast<void*>(address), size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
}
