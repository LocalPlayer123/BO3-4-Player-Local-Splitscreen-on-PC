// XINPUT9_1_0.dll - how the mod gets into the game without changing any file.
//
// BOIII resolves the game's imports with LoadLibraryA (loader.cpp), which looks
// in the game folder first, and BlackOps3.exe imports XINPUT9_1_0.dll. A DLL of
// that name next to boiii.exe is therefore loaded into the game at startup. It
// forwards every XInput call to the Windows copy, so controllers work exactly
// as without it.
//
// WHEN THE COMPONENT STARTS. BOIII runs its components' post_unpack from a
// stub it puts on the game's SetProcessDPIAware import; the stub ends by
// calling the real SetProcessDPIAware through boiii.exe's own import table
// (main.cpp). The game calls SetProcessDPIAware first thing in WinMain, before
// the Steam check (0x0231DEC3 vs 0x0231DF11), so it is the call that runs
// post_unpack. While this DLL loads, the main module is still boiii.exe (BOIII
// swaps it to the game only after the load, main.cpp:147), so redirecting
// boiii.exe's SetProcessDPIAware import here makes the component start right
// after BOIII's own components - exactly where it ran as a BOIII component.
//
// Everything fails safe: a host that is the game itself (no BOIII), a BOIII
// build without that import, or a game that is not the supported build all
// leave the DLL a plain XInput pass-through.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <cstring>
#include <iterator>

#include "runtime.hpp"

namespace
{
	// ------------------------------------------------------------------ XInput

	HMODULE system_xinput()
	{
		static const HMODULE module = []() -> HMODULE
		{
			wchar_t path[MAX_PATH]{};
			const auto length = GetSystemDirectoryW(path, MAX_PATH);
			constexpr wchar_t leaf[] = L"\\XInput9_1_0.dll";
			if (length == 0 || length + std::size(leaf) > MAX_PATH)
			{
				return nullptr;
			}
			std::memcpy(path + length, leaf, sizeof(leaf));
			return LoadLibraryW(path);
		}();
		return module;
	}

	template <typename T>
	T system_function(const char* name)
	{
		const auto module = system_xinput();
		return module ? reinterpret_cast<T>(GetProcAddress(module, name)) : nullptr;
	}

	// ------------------------------------------------------------------ trigger

	using set_process_dpi_aware_t = BOOL(WINAPI*)();
	set_process_dpi_aware_t original_set_process_dpi_aware = nullptr;
	std::atomic<bool> started{false};

	BOOL WINAPI set_process_dpi_aware_stub()
	{
		// start() does nothing and returns false until the main module is the
		// supported game, so an earlier call cannot use up the one start.
		if (!started.load() && runtime::start())
		{
			started = true;
		}
		return original_set_process_dpi_aware ? original_set_process_dpi_aware() : TRUE;
	}

	const IMAGE_IMPORT_DESCRIPTOR* find_import(const HMODULE module, const char* dll)
	{
		const auto* base = reinterpret_cast<const uint8_t*>(module);
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		{
			return nullptr;
		}
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			return nullptr;
		}
		const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!directory.VirtualAddress)
		{
			return nullptr;
		}

		for (auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); d->Name; ++d)
		{
			if (_stricmp(reinterpret_cast<const char*>(base + d->Name), dll) == 0)
			{
				return d;
			}
		}
		return nullptr;
	}

	void** find_import_slot(const HMODULE module, const char* dll, const char* function)
	{
		const auto* d = find_import(module, dll);
		if (!d)
		{
			return nullptr;
		}

		auto* base = reinterpret_cast<uint8_t*>(module);
		const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + (d->OriginalFirstThunk
			                                                                        ? d->OriginalFirstThunk
			                                                                        : d->FirstThunk));
		auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->FirstThunk);
		for (; names->u1.AddressOfData; ++names, ++slots)
		{
			if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))
			{
				continue;
			}
			const auto* by_name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
			if (std::strcmp(reinterpret_cast<const char*>(by_name->Name), function) == 0)
			{
				return reinterpret_cast<void**>(&slots->u1.Function);
			}
		}
		return nullptr;
	}

	void install_trigger()
	{
		const auto host = GetModuleHandleW(nullptr);
		if (!host)
		{
			return;
		}

		// Loaded by the game directly (no client in between): stay a pass-through.
		if (find_import(host, "XINPUT9_1_0.dll"))
		{
			return;
		}

		auto** slot = find_import_slot(host, "USER32.dll", "SetProcessDPIAware");
		if (!slot)
		{
			return;
		}

		DWORD old{};
		if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old))
		{
			return;
		}
		original_set_process_dpi_aware = reinterpret_cast<set_process_dpi_aware_t>(*slot);
		*slot = reinterpret_cast<void*>(&set_process_dpi_aware_stub);
		VirtualProtect(slot, sizeof(*slot), old, &old);
	}
}

// ---------------------------------------------------------------------- exports
// Named in xinput9_1_0.def. Signatures use plain types; the structures are
// passed through untouched.

extern "C" DWORD WINAPI proxy_XInputGetState(const DWORD user_index, void* state)
{
	static const auto f = system_function<DWORD(WINAPI*)(DWORD, void*)>("XInputGetState");
	return f ? f(user_index, state) : ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI proxy_XInputSetState(const DWORD user_index, void* vibration)
{
	static const auto f = system_function<DWORD(WINAPI*)(DWORD, void*)>("XInputSetState");
	return f ? f(user_index, vibration) : ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI proxy_XInputGetCapabilities(const DWORD user_index, const DWORD flags, void* capabilities)
{
	static const auto f = system_function<DWORD(WINAPI*)(DWORD, DWORD, void*)>("XInputGetCapabilities");
	return f ? f(user_index, flags, capabilities) : ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI proxy_XInputGetDSoundAudioDeviceGuids(const DWORD user_index, GUID* render, GUID* capture)
{
	static const auto f = system_function<DWORD(WINAPI*)(DWORD, GUID*, GUID*)>("XInputGetDSoundAudioDeviceGuids");
	return f ? f(user_index, render, capture) : ERROR_DEVICE_NOT_CONNECTED;
}

BOOL WINAPI DllMain(HINSTANCE, const DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		install_trigger();
	}
	return TRUE;
}
