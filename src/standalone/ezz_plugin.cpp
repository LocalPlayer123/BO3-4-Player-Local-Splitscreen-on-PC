// bo3_local_splitscreen.dll - the mod as an ezz BOIII plugin.
//
// ezz loads every DLL in <game folder>\boiii\plugins\ (and
// %LOCALAPPDATA%\boiii\plugins\) while it creates its components, before the
// game is mapped (ezz src/client/component/plugins.cpp). The export p_name is
// required (without it ezz logs a null string); the optional post_load /
// post_unpack / pre_destroy exports are not used.
//
// Start time: ezz runs all components' post_unpack from its stub on the game's
// SetProcessDPIAware import, and that stub ends by calling SetProcessDPIAware
// through boiii.exe's own import table (ezz main.cpp). Redirecting that import
// starts the component after every ezz component, whatever their order, so the
// ezz detours that splitscreen_ezz.hpp chains onto already exist. The game
// calls SetProcessDPIAware first thing in WinMain, before the Steam check
// (0x0231DEC3 vs 0x0231DF11).
//
// runtime::start() does nothing outside the supported game build, so the DLL
// is inert in any other process. It pins itself: ezz frees its plugins at exit
// while game threads may still run through the mod's hooks.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <mutex>

#include "runtime.hpp"

namespace
{
	using set_process_dpi_aware_t = BOOL(WINAPI*)();
	set_process_dpi_aware_t original_set_process_dpi_aware = nullptr;

	BOOL WINAPI set_process_dpi_aware_stub()
	{
		try
		{
			// Created/locked only here, never under DllMain's loader lock.
			// Keep it alive while the pinned DLL's hooks can still be called.
			static auto* start_mutex = new std::mutex;
			static bool started = false;
			std::lock_guard lock(*start_mutex);
			// Unsupported executables remain retryable. Supported-game failures
			// are consumed by runtime::start() to avoid repeating partial patches.
			if (!started)
			{
				started = runtime::start();
			}
		}
		catch (...)
		{
			// No startup C++ exception may cross the WINAPI boundary.
		}
		try
		{
			return original_set_process_dpi_aware ? original_set_process_dpi_aware() : TRUE;
		}
		catch (...)
		{
			return FALSE;
		}
	}

	bool readable(const void* pointer, const size_t length, const HMODULE module)
	{
		const auto address = reinterpret_cast<uintptr_t>(pointer);
		if (length > UINTPTR_MAX - address)
		{
			return false;
		}
		auto cursor = address;
		const auto end = address + length;
		while (cursor < end)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) != sizeof(info)
				|| info.AllocationBase != module || info.State != MEM_COMMIT
				|| (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))
				|| !(info.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
				                     | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
			{
				return false;
			}
			const auto region = reinterpret_cast<uintptr_t>(info.BaseAddress);
			if (info.RegionSize > UINTPTR_MAX - region || region + info.RegionSize <= cursor)
			{
				return false;
			}
			const auto next = region + info.RegionSize;
			cursor = next < end ? next : end;
		}
		return true;
	}

	void** find_import_slot(const HMODULE module, const char* dll, const char* function)
	{
		auto* base = reinterpret_cast<uint8_t*>(module);
		if (!module || !readable(base, sizeof(IMAGE_DOS_HEADER), module))
		{
			return nullptr;
		}
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		// Bound the header offset before SizeOfImage is available.
		if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < static_cast<LONG>(sizeof(IMAGE_DOS_HEADER))
			|| dos->e_lfanew > 0x100000
			|| static_cast<uintptr_t>(dos->e_lfanew) > UINTPTR_MAX - reinterpret_cast<uintptr_t>(base))
		{
			return nullptr;
		}
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		if (!readable(nt, sizeof(*nt), module) || nt->Signature != IMAGE_NT_SIGNATURE
			|| nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)
			|| nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
			|| nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT)
		{
			return nullptr;
		}
		const size_t image_size = nt->OptionalHeader.SizeOfImage;
		const size_t header_end = static_cast<size_t>(dos->e_lfanew) + sizeof(*nt);
		if (image_size > UINTPTR_MAX - reinterpret_cast<uintptr_t>(base)
			|| nt->OptionalHeader.SizeOfHeaders < header_end
			|| nt->OptionalHeader.SizeOfHeaders > image_size)
		{
			return nullptr;
		}
		const auto in_image = [base, image_size, module](const uint64_t rva, const size_t length)
		{
			return rva <= image_size && length <= image_size - static_cast<size_t>(rva)
				&& readable(base + static_cast<size_t>(rva), length, module);
		};
		const auto string_at = [base, image_size, &in_image](const uint64_t rva) -> const char*
		{
			if (rva >= image_size)
			{
				return nullptr;
			}
			// Import identifiers need not consume an unbounded image scan.
			const auto available = image_size - static_cast<size_t>(rva);
			const auto limit = available < 4096 ? available : 4096;
			for (size_t i = 0; i < limit; ++i)
			{
				if (!in_image(rva + i, 1))
				{
					return nullptr;
				}
				if (base[static_cast<size_t>(rva) + i] == 0)
				{
					return reinterpret_cast<const char*>(base + static_cast<size_t>(rva));
				}
			}
			return nullptr;
		};
		const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!directory.VirtualAddress || directory.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR)
			|| !in_image(directory.VirtualAddress, directory.Size))
		{
			return nullptr;
		}

		const auto descriptor_count = directory.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
		const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
		for (size_t index = 0; index < descriptor_count; ++index)
		{
			const auto* d = &descriptors[index];
			if (!d->Name)
			{
				return nullptr;
			}
			const auto* dll_name = string_at(d->Name);
			if (!dll_name)
			{
				return nullptr;
			}
			if (_stricmp(dll_name, dll) != 0)
			{
				continue;
			}
			// A loaded IAT contains resolved VAs, not name RVAs. Without the
			// original name table we cannot safely identify this import.
			if (!d->OriginalFirstThunk || !d->FirstThunk)
			{
				return nullptr;
			}
			uint64_t name_rva = d->OriginalFirstThunk;
			uint64_t slot_rva = d->FirstThunk;
			for (;; name_rva += sizeof(IMAGE_THUNK_DATA64), slot_rva += sizeof(IMAGE_THUNK_DATA64))
			{
				if (!in_image(name_rva, sizeof(IMAGE_THUNK_DATA64))
					|| !in_image(slot_rva, sizeof(IMAGE_THUNK_DATA64)))
				{
					return nullptr;
				}
				const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + static_cast<size_t>(name_rva));
				auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + static_cast<size_t>(slot_rva));
				if (!names->u1.AddressOfData)
				{
					break;
				}
				if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))
				{
					continue;
				}
				const auto import_rva = names->u1.AddressOfData;
				if (!in_image(import_rva, sizeof(WORD)))
				{
					return nullptr;
				}
				const auto* import_name = string_at(import_rva + sizeof(WORD));
				if (!import_name)
				{
					return nullptr;
				}
				if (std::strcmp(import_name, function) == 0)
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
