// Runtime for the standalone build: everything the splitscreen component
// expects from the BOIII client, reimplemented without any BOIII code.
//
//   component registry  REGISTER_COMPONENT + post_load/post_unpack
//   scheduler           async thread + renderer pipeline, BOIII's semantics
//   utils::hook         detour and call on MinHook, BOIII's semantics
//   game                base address, client check, Com_IsRunningUILevel
//   release_policy      fixed switches, no file access
//
// runtime::start() is called by the loader (ezz_plugin.cpp) right after ezz
// has run its own components' post_unpack - see that file.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <MinHook.h>

#include "compat/loader/component_loader.hpp"
#include "compat/game/game.hpp"
#include "compat/scheduler.hpp"
#include "compat/utils/hook.hpp"
#include "compat/release_policy.hpp"
#include "runtime.hpp"

// ---------------------------------------------------------------------------
// game
// ---------------------------------------------------------------------------

namespace game
{
	namespace
	{
		size_t game_base = 0;
	}

	size_t get_base()
	{
		return game_base;
	}

	bool is_server()
	{
		return false;
	}

	bool is_client()
	{
		return true;
	}

	bool Com_IsRunningUILevel()
	{
		// BOIII: symbol<bool()> Com_IsRunningUILevel{0x142148350, ...}
		constexpr size_t com_is_running_ui_level_rva = 0x020EF8F0;
		return reinterpret_cast<bool(*)()>(game_base + com_is_running_ui_level_rva)();
	}

	void set_base(const size_t base)
	{
		game_base = base;
	}
}

// ---------------------------------------------------------------------------
// release_policy
// ---------------------------------------------------------------------------

namespace release_policy
{
	DWORD get_environment_variable(const LPCSTR name, const LPSTR buffer, const DWORD size)
	{
		// The configuration every verified three-player round ran with
		// (tools\launch_detached.ps1 -CgFrame): BO3_CG_FRAME=on, all other
		// switches unset. Anything else - including the variables of the machine
		// the mod is installed on - reads as not set.
		const auto answer = [&](const char* value) -> DWORD
		{
			const auto len = static_cast<DWORD>(std::strlen(value));
			if (!buffer || size <= len)
			{
				return len + 1;
			}
			std::memcpy(buffer, value, len + 1);
			return len;
		};
		if (name && std::strcmp(name, "BO3_CG_FRAME") == 0)
		{
			return answer("on");
		}

#ifdef SS_DIAG
		// DIAGNOSTIC BUILD ONLY: the trace path is built from LOCALAPPDATA.
		if (name && std::strcmp(name, "LOCALAPPDATA") == 0)
		{
			return ::GetEnvironmentVariableA(name, buffer, size);
		}
		// DIAGNOSTIC BUILD ONLY: bisecting switches baked in by build.ps1 -Skip
		// (/DSS_SKIP_<NAME>), for the component's BO3_SKIP_FIX / BO3_SUN4.
		if (name && std::strcmp(name, "BO3_SKIP_FIX") == 0)
		{
			static char skip[128] = "";
#ifdef SS_SKIP_LUITABLES
			strcat_s(skip, "luitables,");
#endif
#ifdef SS_SKIP_CGMARKS
			strcat_s(skip, "cgmarks,");
#endif
#ifdef SS_SKIP_PERCTRL
			strcat_s(skip, "perctrl,");
#endif
			if (skip[0])
			{
				return answer(skip);
			}
		}
#ifdef SS_SKIP_SUN4
		if (name && std::strcmp(name, "BO3_SUN4") == 0)
		{
			return answer("off");
		}
#endif
#ifdef SS_SUN_SLOT_MAX
		if (name && std::strcmp(name, "BO3_SUN_SLOT_MAX") == 0)
		{
			static const char value[2] = {static_cast<char>('0' + SS_SUN_SLOT_MAX), 0};
			return answer(value);
		}
#endif
#endif
		SetLastError(ERROR_ENVVAR_NOT_FOUND);
		return 0;
	}

	HANDLE create_file(const LPCSTR name, const DWORD access, const DWORD share, const LPSECURITY_ATTRIBUTES sa,
	                   const DWORD disposition, const DWORD flags, const HANDLE templ)
	{
#ifdef SS_DIAG
		// DIAGNOSTIC BUILD ONLY (release\build.ps1 -Diag): the component's own
		// trace files (%LOCALAPPDATA%\boiii\splitscreen_*.txt) may be written,
		// nothing else. Used to see which patches applied on a new game build.
		if (name && std::strstr(name, "\\boiii\\splitscreen_"))
		{
			wchar_t wide[MAX_PATH]{};
			if (MultiByteToWideChar(CP_ACP, 0, name, -1, wide, MAX_PATH) > 0)
			{
				return CreateFileW(wide, access, share, sa, disposition, flags, templ);
			}
		}
#else
		(void)name; (void)access; (void)share; (void)sa; (void)disposition; (void)flags; (void)templ;
#endif
		SetLastError(ERROR_ACCESS_DENIED);
		return INVALID_HANDLE_VALUE;
	}
}

// ---------------------------------------------------------------------------
// component registry
// ---------------------------------------------------------------------------

namespace component_loader
{
	namespace
	{
		struct registration
		{
			registration_functor create;
			component_type type;
		};

		std::vector<registration>& registrations()
		{
			static std::vector<registration> list;
			return list;
		}

		std::vector<std::unique_ptr<generic_component>>& components()
		{
			static std::vector<std::unique_ptr<generic_component>> list;
			return list;
		}
	}

	void register_component(registration_functor functor, const component_type type)
	{
		registrations().push_back({std::move(functor), type});
	}

	void activate_client_components()
	{
		for (auto& r : registrations())
		{
			if (r.type != component_type::server)
			{
				components().push_back(r.create());
			}
		}

		for (const auto& c : components())
		{
			c->post_load();
		}

		for (const auto& c : components())
		{
			c->post_unpack();
		}
	}
}

// ---------------------------------------------------------------------------
// utils::hook
// ---------------------------------------------------------------------------

namespace utils::hook
{
	namespace
	{
		bool initialize_min_hook()
		{
			static const auto status = MH_Initialize();
			return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
		}

		bool is_relatively_far(const void* pointer, const void* data, const int offset = 5)
		{
			const int64_t diff = reinterpret_cast<int64_t>(data) - (reinterpret_cast<int64_t>(pointer) + offset);
			const auto small_diff = static_cast<int32_t>(diff);
			return diff != static_cast<int64_t>(small_diff);
		}

		void write(void* place, const void* data, const size_t length)
		{
			DWORD old{};
			VirtualProtect(place, length, PAGE_EXECUTE_READWRITE, &old);
			std::memmove(place, data, length);
			VirtualProtect(place, length, old, &old);
			FlushInstructionCache(GetCurrentProcess(), place, length);
		}

		// Executable memory within rel32 range of `near_to`, handed out in
		// 16-byte pieces from 64 KB blocks, like BOIII's get_memory_near.
		uint8_t* allocate_near(const void* near_to, const size_t length)
		{
			struct block
			{
				uint8_t* start;
				size_t used;
			};
			static std::mutex mutex;
			static std::vector<block> blocks;
			std::lock_guard _(mutex);

			constexpr size_t block_size = 0x10000;
			for (auto& b : blocks)
			{
				if (b.used + length <= block_size && !is_relatively_far(near_to, b.start + b.used)
					&& !is_relatively_far(near_to, b.start + b.used + length))
				{
					auto* result = b.start + b.used;
					b.used += (length + 15) & ~static_cast<size_t>(15);
					return result;
				}
			}

			SYSTEM_INFO info{};
			GetSystemInfo(&info);
			const auto granularity = static_cast<size_t>(info.dwAllocationGranularity);

			// Candidates from 2 GB below to 2 GB above, first free one wins.
			constexpr size_t reach = 0x7FFF0000;
			const auto center = reinterpret_cast<size_t>(near_to);
			const auto low = center > reach ? center - reach : granularity;
			const auto high = center + reach - block_size;
			for (auto address = (low + granularity - 1) & ~(granularity - 1); address < high; address += granularity)
			{
				auto* memory = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(address), block_size,
				                                                  MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
				if (!memory)
				{
					continue;
				}
				if (is_relatively_far(near_to, memory) || is_relatively_far(near_to, memory + block_size))
				{
					VirtualFree(memory, 0, MEM_RELEASE);
					continue;
				}

				blocks.push_back({memory, (length + 15) & ~static_cast<size_t>(15)});
				return memory;
			}

			return nullptr;
		}
	}

	detour::~detour()
	{
		// Deliberately does not unhook. Detours live as long as the process;
		// these destructors only run while it is being torn down, when BOIII may
		// already have restored a function both of us hooked (R_EndFrame), and
		// writing our saved bytes back would re-point it at freed memory.
	}

	void detour::create(void* place, void* target)
	{
		this->clear();
		if (!initialize_min_hook())
		{
			throw std::runtime_error("MinHook initialisation failed");
		}

		this->place_ = place;
		if (MH_CreateHook(this->place_, target, &this->original_) != MH_OK)
		{
			this->place_ = nullptr;
			throw std::runtime_error("Unable to create hook");
		}

		MH_EnableHook(this->place_);
	}

	void detour::create(const size_t place, void* target)
	{
		this->create(reinterpret_cast<void*>(place), target);
	}

	void detour::clear()
	{
		if (this->place_)
		{
			MH_RemoveHook(this->place_);
		}

		this->place_ = nullptr;
		this->original_ = nullptr;
	}

	void call(void* pointer, void* data)
	{
		if (is_relatively_far(pointer, data))
		{
			// jmp qword ptr [rip+0]; dq data
			auto* thunk = allocate_near(pointer, 14);
			if (!thunk)
			{
				throw std::runtime_error("Too far away to create 32bit relative branch");
			}

			uint8_t jump[14] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
			std::memcpy(jump + 6, &data, sizeof(data));
			write(thunk, jump, sizeof(jump));
			data = thunk;
		}

		uint8_t bytes[5] = {0xE8};
		const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(data) - (reinterpret_cast<size_t>(pointer) + 5));
		std::memcpy(bytes + 1, &rel, sizeof(rel));
		write(pointer, bytes, sizeof(bytes));
	}

	void call(const size_t pointer, void* data)
	{
		call(reinterpret_cast<void*>(pointer), data);
	}
}

// ---------------------------------------------------------------------------
// scheduler
// ---------------------------------------------------------------------------

namespace scheduler
{
	namespace
	{
		struct task
		{
			std::function<bool()> handler{};
			std::chrono::milliseconds interval{};
			std::chrono::high_resolution_clock::time_point last_call{};
		};

		class task_pipeline
		{
		public:
			void add(task&& t)
			{
				std::lock_guard _(this->new_mutex_);
				this->new_tasks_.emplace_back(std::move(t));
			}

			void execute()
			{
				std::lock_guard _(this->mutex_);
				this->merge();

				for (auto i = this->tasks_.begin(); i != this->tasks_.end();)
				{
					const auto now = std::chrono::high_resolution_clock::now();
					if (now - i->last_call < i->interval)
					{
						++i;
						continue;
					}

					i->last_call = now;
					if (i->handler() == cond_end)
					{
						i = this->tasks_.erase(i);
					}
					else
					{
						++i;
					}
				}
			}

		private:
			void merge()
			{
				std::lock_guard _(this->new_mutex_);
				this->tasks_.insert(this->tasks_.end(), std::make_move_iterator(this->new_tasks_.begin()),
				                    std::make_move_iterator(this->new_tasks_.end()));
				this->new_tasks_.clear();
			}

			std::recursive_mutex mutex_;
			std::vector<task> tasks_;
			std::mutex new_mutex_;
			std::vector<task> new_tasks_;
		};

		task_pipeline pipelines[pipeline::count];
		utils::hook::detour r_end_frame_hook;

		void r_end_frame_stub()
		{
			pipelines[pipeline::renderer].execute();
			r_end_frame_hook.invoke<void>();
		}
	}

	void schedule(const std::function<bool()>& callback, const pipeline type, const std::chrono::milliseconds delay)
	{
		task t;
		t.handler = callback;
		t.interval = delay;
		t.last_call = std::chrono::high_resolution_clock::now();
		pipelines[type].add(std::move(t));
	}

	void loop(const std::function<void()>& callback, const pipeline type, const std::chrono::milliseconds delay)
	{
		schedule([callback]
		{
			callback();
			return cond_continue;
		}, type, delay);
	}

	void once(const std::function<void()>& callback, const pipeline type, const std::chrono::milliseconds delay)
	{
		schedule([callback]
		{
			callback();
			return cond_end;
		}, type, delay);
	}

	void start(const size_t game_base)
	{
		// The same two drivers BOIII's scheduler uses. The async thread is never
		// joined: it ends with the process, like the game's own threads.
		std::thread([]
		{
			while (true)
			{
				pipelines[pipeline::async].execute();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}).detach();

		// BOIII: r_end_frame_hook.create(0x142272B00_g, r_end_frame_stub).
		// BOIII hooks the same function; MinHook chains onto its jump.
		constexpr size_t r_end_frame_rva = 0x02215FD0;
		r_end_frame_hook.create(game_base + r_end_frame_rva, reinterpret_cast<void*>(&r_end_frame_stub));
	}
}

// ---------------------------------------------------------------------------
// start
// ---------------------------------------------------------------------------

namespace runtime
{
	namespace
	{
		// PE CheckSum of the one BlackOps3.exe the BOIII client runs
		// (game::is_client() in BOIII). Every RVA in the component belongs to it.
		constexpr DWORD supported_game_checksum = 0x06531394;

		bool is_supported_game(const HMODULE module)
		{
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
			if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
			{
				return false;
			}
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const uint8_t*>(module) + dos->
				e_lfanew);
			return nt->Signature == IMAGE_NT_SIGNATURE
				&& nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC
				&& nt->OptionalHeader.CheckSum == supported_game_checksum;
		}
	}

	bool start()
	{
		// By now BOIII has made the game the process's main module.
		const auto game_module = GetModuleHandleW(nullptr);
		if (!is_supported_game(game_module))
		{
			return false;
		}

		const auto base = reinterpret_cast<size_t>(game_module);

		// A development client (boiii.exe built with the component) has already
		// applied everything in its own post_unpack, which runs before this. Its
		// first act is to write the status magic, so seeing it means: stand down,
		// never patch the game twice.
		constexpr size_t status_rva = 0x1A828D00;
		constexpr uint32_t status_magic = 0xB03C0FFE;
		const auto* status = reinterpret_cast<const uint32_t*>(base + status_rva);
		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(status, &info, sizeof(info)) == sizeof(info) && info.State == MEM_COMMIT
			&& !(info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) && *status == status_magic)
		{
			return true;
		}

		game::set_base(base);

		try
		{
			scheduler::start(base);
			component_loader::activate_client_components();
		}
		catch (...)
		{
			// Same as BOIII's loader: a failed component does not take the game
			// down. Every patch verifies its original bytes before writing.
		}

		return true;
	}
}
