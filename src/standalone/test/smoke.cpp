// Offline check of the built XINPUT9_1_0.dll - no game, no BOIII needed.
// build.ps1 compiles this file twice and runs both next to the DLL:
//
//   smoke_host.exe    loads the DLL with LoadLibrary, like BOIII does for the
//                     game. The DLL must redirect this exe's SetProcessDPIAware
//                     import, and the redirected call must return normally
//                     WITHOUT starting anything (this exe is not the game).
//   smoke_game.exe    imports XINPUT9_1_0.dll statically, like BlackOps3.exe.
//                     The DLL must stay a plain pass-through and leave
//                     SetProcessDPIAware alone.
//
// Both: every XInput export must return what the Windows copy returns.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifdef SMOKE_STATIC_IMPORT
extern "C" __declspec(dllimport) DWORD WINAPI XInputGetState(DWORD, void*);
#endif

namespace
{
	int failures = 0;

	void check(const bool ok, const char* what)
	{
		std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
		if (!ok)
		{
			++failures;
		}
	}

	void** dpi_slot()
	{
		auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->
			e_lfanew);
		const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d)
		{
			if (_stricmp(reinterpret_cast<char*>(base + d->Name), "USER32.dll") != 0)
			{
				continue;
			}
			auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->OriginalFirstThunk);
			auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->FirstThunk);
			for (; names->u1.AddressOfData; ++names, ++slots)
			{
				if (!IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)
					&& std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name,
					               "SetProcessDPIAware") == 0)
				{
					return reinterpret_cast<void**>(&slots->u1.Function);
				}
			}
		}
		return nullptr;
	}

	bool imports_xinput()
	{
		auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->
			e_lfanew);
		const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d)
		{
			if (_stricmp(reinterpret_cast<char*>(base + d->Name), "XINPUT9_1_0.dll") == 0)
			{
				return true;
			}
		}
		return false;
	}

	HMODULE module_of(const void* address)
	{
		HMODULE module{};
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		                   static_cast<LPCWSTR>(address), &module);
		return module;
	}

	int thread_count()
	{
		const auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		THREADENTRY32 e{sizeof(e)};
		int n = 0;
		for (auto ok = Thread32First(snap, &e); ok; ok = Thread32Next(snap, &e))
		{
			n += e.th32OwnerProcessID == GetCurrentProcessId();
		}
		CloseHandle(snap);
		return n;
	}

	std::wstring exe_dir()
	{
		wchar_t path[MAX_PATH]{};
		GetModuleFileNameW(nullptr, path, MAX_PATH);
		std::wstring s = path;
		return s.substr(0, s.find_last_of(L'\\') + 1);
	}

	template <typename T>
	T fn(const HMODULE m, const char* name)
	{
		return reinterpret_cast<T>(GetProcAddress(m, name));
	}
}

int main()
{
	auto** slot = dpi_slot();
	check(slot != nullptr, "test exe imports SetProcessDPIAware");
	if (!slot)
	{
		return 1;
	}
	auto* const before = *slot;

#ifdef SMOKE_STATIC_IMPORT
	std::printf("smoke_game: DLL imported like BlackOps3.exe imports it\n");
	uint8_t first_state[64]{};
	XInputGetState(0, first_state); // a real call, so the import cannot be optimised away
	check(imports_xinput(), "test exe imports XINPUT9_1_0.dll (like the game)");
	const auto mod = GetModuleHandleW(L"XINPUT9_1_0.dll");
#else
	std::printf("smoke_host: DLL loaded like BOIII loads the game's imports\n");
	const auto mod = LoadLibraryW((exe_dir() + L"XINPUT9_1_0.dll").c_str());
#endif

	wchar_t loaded[MAX_PATH]{};
	GetModuleFileNameW(mod, loaded, MAX_PATH);
	check(mod != nullptr && _wcsicmp(loaded, (exe_dir() + L"XINPUT9_1_0.dll").c_str()) == 0,
	      "the mod's DLL is the XINPUT9_1_0.dll in use (not the Windows copy)");

#ifdef SMOKE_STATIC_IMPORT
	check(*slot == before, "game-style import: SetProcessDPIAware left untouched");
#else
	check(module_of(*slot) == mod, "host-style load: SetProcessDPIAware redirected into the DLL");
#endif

	const auto threads_before = thread_count();
	check(SetProcessDPIAware() != FALSE, "SetProcessDPIAware still works through the redirect");
	Sleep(100);
	check(thread_count() == threads_before, "nothing started in a process that is not the game");

	wchar_t system_path[MAX_PATH]{};
	GetSystemDirectoryW(system_path, MAX_PATH);
	const auto sys = LoadLibraryW((std::wstring(system_path) + L"\\XInput9_1_0.dll").c_str());
	check(sys != nullptr && sys != mod, "Windows XInput9_1_0.dll loaded separately for comparison");

	using get_state_t = DWORD(WINAPI*)(DWORD, void*);
	using set_state_t = DWORD(WINAPI*)(DWORD, void*);
	using get_caps_t = DWORD(WINAPI*)(DWORD, DWORD, void*);
	using get_guids_t = DWORD(WINAPI*)(DWORD, GUID*, GUID*);

	bool all_exports = true;
	bool same = true;
	for (const auto* name : {"XInputGetState", "XInputSetState", "XInputGetCapabilities",
	                         "XInputGetDSoundAudioDeviceGuids"})
	{
		all_exports = all_exports && GetProcAddress(mod, name) != nullptr;
	}
	check(all_exports, "all four XInput 9.1.0 functions exported");

	for (DWORD pad = 0; pad < 4 && all_exports && sys; ++pad)
	{
		uint8_t a[64]{}, b[64]{};
		same = same && fn<get_state_t>(mod, "XInputGetState")(pad, a) == fn<get_state_t>(sys, "XInputGetState")(pad, b);
		uint8_t va[4]{}, vb[4]{};
		same = same && fn<set_state_t>(mod, "XInputSetState")(pad, va) == fn<set_state_t>(sys, "XInputSetState")(pad, vb);
		same = same && fn<get_caps_t>(mod, "XInputGetCapabilities")(pad, 0, a)
			== fn<get_caps_t>(sys, "XInputGetCapabilities")(pad, 0, b);
		GUID g1{}, g2{}, g3{}, g4{};
		same = same && fn<get_guids_t>(mod, "XInputGetDSoundAudioDeviceGuids")(pad, &g1, &g2)
			== fn<get_guids_t>(sys, "XInputGetDSoundAudioDeviceGuids")(pad, &g3, &g4);
	}
	check(same, "pads 0-3: every function returns the same as the Windows copy");

	std::printf("  %s\n", failures ? "SMOKE TEST FAILED" : "smoke test passed");
	return failures ? 1 : 0;
}
