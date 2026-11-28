#pragma once
// Deliberately small API/PE *model*, not a Windows SDK ABI replacement.
// Tests construct images using these types; no real executable is loaded.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <strings.h>
#define WINAPI
using BOOL = int;
using DWORD = uint32_t;
using LONG = int32_t;
using WORD = uint16_t;
using HANDLE = void*;
using HMODULE = void*;
using HINSTANCE = void*;
using LPVOID = void*;
using LPCSTR = const char*;
using LPSTR = char*;
using LPCWSTR = const wchar_t*;
using LPSECURITY_ATTRIBUTES = void*;
constexpr BOOL TRUE = 1, FALSE = 0;
inline const HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<void*>(~uintptr_t(0));
constexpr DWORD MEM_COMMIT = 0x1000, MEM_RESERVE = 0x2000, MEM_RELEASE = 0x8000;
constexpr DWORD PAGE_NOACCESS = 1, PAGE_READONLY = 2, PAGE_READWRITE = 4, PAGE_WRITECOPY = 8;
constexpr DWORD PAGE_EXECUTE_READ = 32, PAGE_EXECUTE_READWRITE = 64, PAGE_EXECUTE_WRITECOPY = 128, PAGE_GUARD = 256;
constexpr DWORD ERROR_ENVVAR_NOT_FOUND = 203, ERROR_ACCESS_DENIED = 5;
constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 4, GET_MODULE_HANDLE_EX_FLAG_PIN = 1;
constexpr DWORD DLL_PROCESS_ATTACH = 1;
constexpr WORD IMAGE_DOS_SIGNATURE = 0x5A4D, IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B;
constexpr DWORD IMAGE_NT_SIGNATURE = 0x4550;
constexpr int IMAGE_DIRECTORY_ENTRY_IMPORT = 1;
struct IMAGE_DOS_HEADER { WORD e_magic; unsigned char unused[58]; LONG e_lfanew; };
struct IMAGE_DATA_DIRECTORY { DWORD VirtualAddress, Size; };
struct IMAGE_OPTIONAL_HEADER64
{
    WORD Magic{};
    DWORD CheckSum{}, SizeOfImage{}, SizeOfHeaders{}, NumberOfRvaAndSizes{};
    IMAGE_DATA_DIRECTORY DataDirectory[16]{};
};
struct IMAGE_FILE_HEADER { WORD SizeOfOptionalHeader{}; };
struct IMAGE_NT_HEADERS64 { DWORD Signature{}; IMAGE_FILE_HEADER FileHeader{}; IMAGE_OPTIONAL_HEADER64 OptionalHeader{}; };
struct IMAGE_IMPORT_DESCRIPTOR { DWORD OriginalFirstThunk, TimeDateStamp, ForwarderChain, Name, FirstThunk; };
struct IMAGE_THUNK_DATA64 { union { uint64_t AddressOfData, Ordinal, Function; } u1; };
inline bool IMAGE_SNAP_BY_ORDINAL64(uint64_t value) { return (value >> 63) != 0; }
inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
struct MEMORY_BASIC_INFORMATION { void* BaseAddress{}; void* AllocationBase{}; size_t RegionSize{}; DWORD State{}, Protect{}; };
struct SYSTEM_INFO { DWORD dwAllocationGranularity{}; };
namespace winmock
{
    inline void* module = nullptr;
    inline size_t module_size = 0;
    inline DWORD protection = PAGE_READWRITE, state = MEM_COMMIT;
    inline int protect_calls = 0, protect_failure = 0, flush_calls = 0;
    inline bool flush_ok = true, query_ok = true;
    inline DWORD last_error = 0;
}
inline HMODULE GetModuleHandleW(LPCWSTR) { return winmock::module; }
inline BOOL GetModuleHandleExW(DWORD, LPCWSTR, HMODULE* result) { *result = winmock::module; return TRUE; }
inline void SetLastError(DWORD error) { winmock::last_error = error; }
inline HANDLE GetCurrentProcess() { return nullptr; }
inline BOOL VirtualProtect(void*, size_t, DWORD, DWORD* old)
{ *old = PAGE_READONLY; return ++winmock::protect_calls != winmock::protect_failure; }
inline BOOL FlushInstructionCache(HANDLE, const void*, size_t) { ++winmock::flush_calls; return winmock::flush_ok; }
inline void GetSystemInfo(SYSTEM_INFO* info) { info->dwAllocationGranularity = 0x10000; }
inline void* VirtualAlloc(void*, size_t, DWORD, DWORD) { return nullptr; }
inline BOOL VirtualFree(void*, size_t, DWORD) { return TRUE; }
inline size_t VirtualQuery(const void* pointer, MEMORY_BASIC_INFORMATION* info, size_t)
{
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    const auto base = reinterpret_cast<uintptr_t>(winmock::module);
    if (!winmock::query_ok || address < base || address - base >= winmock::module_size) return 0;
    *info = {winmock::module, winmock::module, winmock::module_size, winmock::state, winmock::protection};
    return sizeof(*info);
}