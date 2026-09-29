// Offline check of the built plugin - no game, no ezz needed. build.ps1
// compiles this file and runs it next to the DLL. It loads the DLL with
// LoadLibrary, as ezz's plugin loader does, and checks:
//   - p_name is exported and returns the name (ezz needs it)
//   - the DLL redirected this exe's SetProcessDPIAware import (the start
//     trigger), and the redirected call returns normally WITHOUT starting
//     anything - this exe is not the game
//   - FreeLibrary does not unload it (ezz frees plugins at exit)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>

#include <cstdio>
#include <cstring>
#include <string>

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
}

int main()
{
	std::printf("smoke: plugin loaded like ezz BOIII loads plugins\n");
	auto** slot = dpi_slot();
	check(slot != nullptr, "test exe imports SetProcessDPIAware");
	if (!slot)
	{
		return 1;
	}

	const auto path = exe_dir() + L"bo3_local_splitscreen.dll";
	const auto mod = LoadLibraryW(path.c_str());
	check(mod != nullptr, "bo3_local_splitscreen.dll loads");
	if (!mod)
	{
		return 1;
	}

	using name_t = const char* (*)();
	const auto p_name = reinterpret_cast<name_t>(GetProcAddress(mod, "p_name"));
	check(p_name && p_name() && std::strcmp(p_name(), "BO3 Local Splitscreen") == 0, "p_name returns the plugin name");

	check(module_of(*slot) == mod, "SetProcessDPIAware redirected into the plugin (start trigger)");

	const auto threads_before = thread_count();
	check(SetProcessDPIAware() != FALSE, "SetProcessDPIAware still works through the redirect");
	Sleep(100);
	check(thread_count() == threads_before, "nothing started in a process that is not the game");

	FreeLibrary(mod);
	check(GetModuleHandleW(path.c_str()) == mod, "still loaded after FreeLibrary (pinned)");

	std::printf("  %s\n", failures ? "SMOKE TEST FAILED" : "smoke test passed");
	return failures ? 1 : 0;
}
