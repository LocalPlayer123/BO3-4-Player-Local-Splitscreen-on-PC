// bo3_local_splitscreen.dll - the mod as an ezz BOIII plugin.
//
// ezz loads every DLL in <game folder>\boiii\plugins\ (and
// %LOCALAPPDATA%\boiii\plugins\) while it creates its components, before the
// game is mapped (ezz src/client/component/plugins.cpp). It reads the name
// from the export p_name - required: without it ezz logs a null string - and
// calls post_load / post_unpack / pre_destroy if they exist. None of those is
// used here.
//
// WHEN THE COMPONENT STARTS. ezz runs all components' post_unpack from the
// stub it puts on the game's SetProcessDPIAware import, and that stub ends by
// calling SetProcessDPIAware through boiii.exe's own import table (ezz
// main.cpp set_process_dpi_aware_stub). Redirecting that import here starts
// the component after every ezz component - ezz's own detours, which
// splitscreen_ezz.hpp chains onto, exist by then - whatever order ezz gives
// its components and plugins. The game calls SetProcessDPIAware first thing
// in WinMain, before the Steam check (0x0231DEC3 vs 0x0231DF11).
//
// runtime::start() does nothing outside the supported game build, so the DLL
// is inert in any other process. It pins itself: ezz frees its plugins at
// exit, while game threads may still run through the mod's hooks.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <cstring>

#include "runtime.hpp"

namespace
{
	using set_process_dpi_aware_t = BOOL(WINAPI*)();
	set_process_dpi_aware_t original_set_process_dpi_aware = nullptr;
	std::atomic<bool> started{false};

	BOOL WINAPI set_process_dpi_aware_stub()
	{
		// start() returns false until the main module is the supported game,
		// so an earlier call cannot use up the one start.
		if (!started.load() && runtime::start())
		{
			started = true;
		}
		return original_set_process_dpi_aware ? original_set_process_dpi_aware() : TRUE;
	}

	void** find_import_slot(const HMODULE module, const char* dll, const char* function)
	{
		auto* base = reinterpret_cast<uint8_t*>(module);
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
			if (_stricmp(reinterpret_cast<const char*>(base + d->Name), dll) != 0)
			{
				continue;
			}
			const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(
				base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
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
		}
		return nullptr;
	}

	void install_trigger()
	{
		const auto host = GetModuleHandleW(nullptr);
		auto** slot = host ? find_import_slot(host, "USER32.dll", "SetProcessDPIAware") : nullptr;
		DWORD old{};
		if (!slot || !VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old))
		{
			return;
		}
		original_set_process_dpi_aware = reinterpret_cast<set_process_dpi_aware_t>(*slot);
		*slot = reinterpret_cast<void*>(&set_process_dpi_aware_stub);
		VirtualProtect(slot, sizeof(*slot), old, &old);
	}

	void pin_self()
	{
		HMODULE self{};
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
		                   reinterpret_cast<LPCWSTR>(&pin_self), &self);
	}
}

// Named in ezz_plugin.def.
extern "C" const char* plugin_name()
{
	return "BO3 Local Splitscreen";
}

BOOL WINAPI DllMain(HINSTANCE, const DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		pin_self();
		install_trigger();
	}
	return TRUE;
}
