#pragma once

// Players 3 and 4 behind ezz BOIII.
//
// ezz BOIII 3.0.0 replaces three engine functions with versions sized for two
// controllers (docs/EZZ_REQUIRED_CHANGES.md, items 2 and 3):
//   LiveUser_GetXuid, LiveUser_UserGetXuid  2-entry XUID table: controllers 2/3
//                                           get controller 0's XUID, so the
//                                           lobby takes a guest for the host
//   LiveUser_GetClientName                  reads the game's [2] user-data map,
//                                           which the mod moved to [4]
// Until ezz sizes these for four controllers, controllers 2/3 run the engine's
// own code and 0/1 stay with ezz.
//
// The engine code is ezz's MinHook trampoline (the replaced instructions plus a
// jump back); MinHook places the relay, its jump's target, right after it. A
// function is chained only if the trampoline holds exactly the official build's
// instructions and jumps back to the expected address. On official BOIII
// nothing is hooked there and nothing is chained.

#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>

#include "splitscreen_addresses.hpp"
#include "splitscreen_midhook.hpp"

namespace splitscreen::ezz
{
	namespace detail
	{
		inline bool readable(const void* p, const size_t n)
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT
				|| (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
			{
				return false;
			}
			const auto end = reinterpret_cast<size_t>(mbi.BaseAddress) + mbi.RegionSize;
			return reinterpret_cast<size_t>(p) + n <= end;
		}

		inline bool bytes_at(const size_t address, const uint8_t* bytes, const size_t n)
		{
			const auto* p = reinterpret_cast<const void*>(address);
			return readable(p, n) && std::memcmp(p, bytes, n) == 0;
		}

		inline void put64(std::vector<uint8_t>& v, const uint64_t x)
		{
			for (int i = 0; i < 8; ++i)
			{
				v.push_back(static_cast<uint8_t>(x >> (8 * i)));
			}
		}

		// MinHook x64: JMP_ABS = jmp [rip+0]; dq target. CALL_ABS = call [rip+2];
		// jmp +8; dq target.
		inline void jmp_abs(std::vector<uint8_t>& v, const uint64_t target)
		{
			v.insert(v.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});
			put64(v, target);
		}

		inline void call_abs(std::vector<uint8_t>& v, const uint64_t target)
		{
			v.insert(v.end(), {0xFF, 0x15, 0x02, 0x00, 0x00, 0x00, 0xEB, 0x08});
			put64(v, target);
		}

		// The engine function behind a host jump at `fn`: the host's trampoline,
		// if it is exactly `trampoline` followed by the relay. `tail` are the
		// bytes the host left in place after its 5-byte jump. 0 = do not chain.
		inline size_t engine_behind_host_jump(const size_t fn, const std::vector<uint8_t>& trampoline,
		                                      const uint8_t* tail, const size_t tail_size)
		{
			const auto* p = reinterpret_cast<const uint8_t*>(fn);
			if (!readable(p, 5 + tail_size) || p[0] != 0xE9 || std::memcmp(p + 5, tail, tail_size) != 0)
			{
				return 0;
			}
			int32_t rel = 0;
			std::memcpy(&rel, p + 1, sizeof(rel));
			const size_t relay = fn + 5 + static_cast<int64_t>(rel);
			static constexpr uint8_t relay_head[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
			if (!bytes_at(relay, relay_head, sizeof(relay_head)))
			{
				return 0;
			}
			const size_t engine = relay - trampoline.size();
			return bytes_at(engine, trampoline.data(), trampoline.size()) ? engine : 0;
		}

		inline size_t get_xuid_engine = 0;
		inline size_t user_get_xuid_engine = 0;
		inline size_t get_client_name_engine = 0;

		inline utils::hook::detour get_xuid_hook;
		inline utils::hook::detour user_get_xuid_hook;
		inline utils::hook::detour get_client_name_hook;

		inline bool engine_owns(const int controller)
		{
			return controller == 2 || controller == 3;
		}

		inline uint64_t get_xuid_stub(const int controller)
		{
			if (engine_owns(controller))
			{
				return reinterpret_cast<uint64_t (*)(int)>(get_xuid_engine)(controller);
			}
			return get_xuid_hook.invoke<uint64_t>(controller);
		}

		inline bool user_get_xuid_stub(const int controller, uint64_t* xuid)
		{
			if (engine_owns(controller))
			{
				return reinterpret_cast<bool (*)(int, uint64_t*)>(user_get_xuid_engine)(controller, xuid);
			}
			return user_get_xuid_hook.invoke<bool>(controller, xuid);
		}

		inline const char* get_client_name_stub(const int controller)
		{
			if (engine_owns(controller))
			{
				return reinterpret_cast<const char* (*)(int)>(get_client_name_engine)(controller);
			}
			return get_client_name_hook.invoke<const char*>(controller);
		}
	}

	// Bit per chained function: 1 GetXuid, 2 UserGetXuid, 4 GetClientName.
	inline uint32_t chained = 0;

	inline bool get_xuid_chained()
	{
		return (chained & 1) != 0;
	}

	// Call once from post_unpack, after the host's own hooks exist. Returns
	// the chained mask.
	inline uint32_t install(const size_t base)
	{
		using namespace detail;
		static bool done = false;   // a second pass would see the mod's own jump
		if (done)
		{
			return chained;
		}
		done = true;

		// Expected trampolines: the official build's first instructions of each
		// function (build 0x06517980: 0x01EBAF40 / 0x01EBB280 / 0x01EBAF10;
		// PS4: 0xC96930 / 0xC96980 / 0xC96EC0).
		{
			// push rbx; sub rsp,20h | mov ebx,ecx; call LiveUser_IsSignedIn ...
			std::vector<uint8_t> t = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20};
			jmp_abs(t, base + get_xuid_rva + 6);
			static constexpr uint8_t tail[] = {0x20, 0x8B, 0xD9, 0xE8, 0x13, 0xFD, 0xFF, 0xFF, 0x84, 0xC0, 0x75};
			get_xuid_engine = engine_behind_host_jump(base + get_xuid_rva, t, tail, sizeof(tail));
		}
		{
			// mov [rsp+18h],rbx | push rdi; sub rsp,40h ...
			std::vector<uint8_t> t = {0x48, 0x89, 0x5C, 0x24, 0x18};
			jmp_abs(t, base + user_get_xuid_rva + 5);
			static constexpr uint8_t tail[] = {0x57, 0x48, 0x83, 0xEC, 0x40};
			user_get_xuid_engine = engine_behind_host_jump(base + user_get_xuid_rva, t, tail, sizeof(tail));
		}
		{
			// sub rsp,28h; call LiveUser_GetUserDataForController | add rax,8;
			// add rsp,28h; ret - the gamertag inside the controller's user data
			std::vector<uint8_t> t = {0x48, 0x83, 0xEC, 0x28};
			call_abs(t, base + get_user_data_rva);
			jmp_abs(t, base + get_client_name_rva + 9);
			static constexpr uint8_t tail[] = {0x97, 0xFB, 0xFF, 0xFF, 0x48, 0x83, 0xC0, 0x08, 0x48, 0x83, 0xC4, 0x28, 0xC3};
			// movsxd rax,ecx; lea rcx,[map] - the accessor itself is not hooked
			static constexpr uint8_t accessor[] = {0x48, 0x63, 0xC1, 0x48, 0x8D, 0x0D};
			if (bytes_at(base + get_user_data_rva, accessor, sizeof(accessor)))
			{
				get_client_name_engine = engine_behind_host_jump(base + get_client_name_rva, t, tail, sizeof(tail));
			}
		}

		const auto chain = [&](utils::hook::detour& hook, const size_t engine, const uint32_t rva,
		                       void* stub, const uint32_t bit)
		{
			if (!engine)
			{
				return;
			}
			try
			{
				hook.create(base + rva, stub);
				chained |= bit;
			}
			catch (...)
			{
			}
		};
		chain(get_xuid_hook, get_xuid_engine, get_xuid_rva, reinterpret_cast<void*>(&get_xuid_stub), 1);
		chain(user_get_xuid_hook, user_get_xuid_engine, user_get_xuid_rva,
		      reinterpret_cast<void*>(&user_get_xuid_stub), 2);
		chain(get_client_name_hook, get_client_name_engine, get_client_name_rva,
		      reinterpret_cast<void*>(&get_client_name_stub), 4);
		return chained;
	}

	// ------------------------------------------------------------ cgame pools
	//
	// ezz redirects five Hunk_UserAlloc calls to static pools sized for two
	// local clients (docs/EZZ_REQUIRED_CHANGES.md item 4). With 3-4 local
	// clients the engine writes past them and overwrites local client 0's
	// entities. Requests that fit ezz's pool still go to ezz (1-2 players
	// unchanged); larger requests, and entity pools for local clients 2/3, get
	// memory of their own. ezz never frees engine memory, so these buffers are
	// kept and zeroed on every allocation.

	namespace detail
	{
		using hunk_alloc_fn = void* (*)(void* user, size_t size, int alignment, const char* name);

		// One allocation site of CG_AllocateClientMemory (cg_alloc_sites) and what
		// the stub learned about it.
		struct array_site
		{
			const array_alloc_site& at;
			hunk_alloc_fn host = nullptr;
			uint8_t* own = nullptr;
		};

		inline array_site array_sites[] = {{cg_alloc_sites[0]}, {cg_alloc_sites[1]}, {cg_alloc_sites[2]}};

		inline void* zeroed(uint8_t*& own, const size_t capacity, const size_t size, const int alignment)
		{
			const size_t a = alignment > 0 ? static_cast<size_t>(alignment) : 1;
			if (!own)
			{
				own = static_cast<uint8_t*>(VirtualAlloc(nullptr, capacity + a, MEM_COMMIT | MEM_RESERVE,
				                                         PAGE_READWRITE));
			}
			if (!own || size > capacity)
			{
				return nullptr;
			}
			auto* p = reinterpret_cast<uint8_t*>((reinterpret_cast<size_t>(own) + a - 1) & ~(a - 1));
			std::memset(p, 0, size);
			return p;
		}

		template <size_t I>
		void* array_alloc(void* user, const size_t size, const int alignment, const char* name)
		{
			auto& s = array_sites[I];
			if (size > 2 * s.at.elem)
			{
				if (auto* p = zeroed(s.own, 4 * s.at.elem, size, alignment))
				{
					return p;
				}
			}
			return s.host(user, size, alignment, name);
		}

		// CG_InitAndAllocCGEntsArray (0x0085B990) allocates one entity pool per
		// local client in a loop indexed by rsi (xor esi,esi at +0x2B); the index is
		// never spilled to memory. A mid-function hook on the size load just before
		// the call records it, and the call itself goes to entity_alloc.
		inline hunk_alloc_fn entity_host = nullptr;
		inline volatile uint32_t entity_lc = 0xFFFFFFFF;
		inline uint8_t* entity_pools[4] = {};

		inline void record_entity_lc(midhook::context& ctx)
		{
			entity_lc = static_cast<uint32_t>(ctx.rsi);
		}

		inline void* entity_alloc(void* user, const size_t size, const int alignment, const char* name)
		{
			const uint32_t lc = entity_lc;
			if ((lc == 2 || lc == 3) && size == entity_pool_size)
			{
				if (auto* p = zeroed(entity_pools[lc], entity_pool_size, size, alignment))
				{
					return p;
				}
			}
			return entity_host(user, size, alignment, name);
		}

		// `call rel32` at `site` that leaves the game image for ezz: ezz's redirect,
		// not the engine's own call. ezz's utils::hook::call (src/common/utils/hook.cpp)
		// writes it in one of two forms, chosen by where ASLR placed boiii.exe:
		//   far (> 2 GB)  call -> a `jmp [rip+0]` thunk near the game image
		//   near          call -> ezz's function in boiii.exe, directly
		// Accepting only the thunk left all four pool sites unhooked whenever boiii.exe
		// loaded within 2 GB of the game (1.07 GB in the 2026-09-30 09:07 crash, mask
		// 0x10F): local clients 2/3 then ran on ezz's 2-client static pools and
		// overwrote local client 0's entities. Both forms are callable as the host.
		inline size_t host_call_target(const size_t base, const size_t image_size, const uint32_t site)
		{
			const auto* p = reinterpret_cast<const uint8_t*>(base + site);
			if (!readable(p, 5) || p[0] != 0xE8)
			{
				return 0;
			}
			int32_t rel = 0;
			std::memcpy(&rel, p + 1, sizeof(rel));
			const size_t target = base + site + 5 + static_cast<int64_t>(rel);
			if (target >= base && target < base + image_size)
			{
				return 0; // the engine's own call: no host redirect (official BOIII)
			}
			static constexpr uint8_t thunk[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
			if (bytes_at(target, thunk, sizeof(thunk)))
			{
				return target;
			}
			// The near form: code inside another loaded module - not the game, not
			// this DLL.
			HMODULE owner{};
			HMODULE self{};
			const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
			MEMORY_BASIC_INFORMATION mbi{};
			constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
			if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(target), &owner)
				|| !GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&host_call_target), &self)
				|| owner == self || reinterpret_cast<size_t>(owner) == base
				|| !VirtualQuery(reinterpret_cast<const void*>(target), &mbi, sizeof(mbi))
				|| mbi.State != MEM_COMMIT || !(mbi.Protect & executable))
			{
				return 0;
			}
			return target;
		}

		inline bool write_code(const size_t address, const void* bytes, const size_t n)
		{
			DWORD old{};
			if (!VirtualProtect(reinterpret_cast<void*>(address), n, PAGE_EXECUTE_READWRITE, &old))
			{
				return false;
			}
			std::memcpy(reinterpret_cast<void*>(address), bytes, n);
			DWORD tmp{};
			VirtualProtect(reinterpret_cast<void*>(address), n, old, &tmp);
			FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), n);
			return true;
		}
	}

	// Bits added to `chained`: 16 per array site (cg, cgs, viewmodel = 16/32/64),
	// 128 entity pools. `alloc_near` must return executable memory within rel32
	// reach of the image.
	inline uint32_t install_pools(const size_t base, const size_t image_size, void* (*alloc_near)(size_t))
	{
		using namespace detail;
		static bool done = false;
		if (done)
		{
			return chained;
		}
		done = true;

		void* (*const stubs[])(void*, size_t, int, const char*) = {
			&array_alloc<0>, &array_alloc<1>, &array_alloc<2>,
		};
		for (size_t i = 0; i < std::size(array_sites); ++i)
		{
			auto& s = array_sites[i];
			const size_t host = host_call_target(base, image_size, s.at.call_rva);
			if (!host || !bytes_at(base + s.at.imul_rva, s.at.imul, sizeof(s.at.imul)))
			{
				continue;
			}
			s.host = reinterpret_cast<hunk_alloc_fn>(host);
			try
			{
				utils::hook::call(base + s.at.call_rva, reinterpret_cast<void*>(stubs[i]));
				chained |= 16u << i;
			}
			catch (...)
			{
			}
		}

		const size_t host = host_call_target(base, image_size, entity_call_rva);
		if (host && alloc_near && bytes_at(base + entity_size_rva, entity_size_bytes, sizeof(entity_size_bytes)))
		{
			entity_host = reinterpret_cast<hunk_alloc_fn>(host);
			// the size load `mov edx, 0x3F0000` (position-independent) runs as stock after it
			if (midhook::install(base + entity_size_rva, entity_size_bytes, sizeof(entity_size_bytes),
			                     &record_entity_lc, alloc_near))
			{
				try
				{
					utils::hook::call(base + entity_call_rva, reinterpret_cast<void*>(&entity_alloc));
					chained |= 128;
				}
				catch (...)
				{
				}
			}
		}
		return chained;
	}

	// A host detour at `rva` that left the rest of the function intact: its
	// 5-byte jump followed by `tail`. Used where the mod stacks its own detour
	// on top (MinHook relocates the host's jump into the mod's trampoline).
	inline bool host_jump_then(const size_t base, const uint32_t rva, const uint8_t* tail, const size_t tail_size)
	{
		const auto* p = reinterpret_cast<const uint8_t*>(base + rva);
		return detail::readable(p, 5 + tail_size) && p[0] == 0xE9 && std::memcmp(p + 5, tail, tail_size) == 0;
	}

	// ------------------------------------------------------ ClientCommand guard
	//
	// ezz detours ClientCommand and calls the original from boiii.exe
	// (client_command.cpp). ClientCommand opens with an Arxan caller check:
	// bits 12..15 of the PEB address pick one of 16 variants, nine of which
	// test the return address against the game image (above image+0x20000000
	// or below the image base). A caller outside the image sends the check into
	// an endless two-state loop: the server thread spins and the client times
	// out ("Connection Interrupted"). boiii.exe lies above the image, so 6 of 16
	// launches hang on the first client command ezz passes on, with any number
	// of players. docs/EZZ_REQUIRED_CHANGES.md item 12.
	//
	// Each range test ends in cmova/cmovb edx,r10d, which loads the failing
	// state; a 4-byte nop keeps the passing one. The other seven variants check
	// for a call instruction before the return address, which ezz's `call rax`
	// passes. Sites: client_command_* (splitscreen_addresses.hpp).

	// Bit 256 in `chained`. All nine tests or none.
	inline uint32_t install_client_command_guard(const size_t base)
	{
		using namespace detail;
		static bool done = false;
		if (done)
		{
			return chained;
		}
		done = true;

		// Only the body after the first 5 bytes is checked: whether the host's
		// jump is in place yet does not matter, the tests are harmless without it.
		if (!bytes_at(base + client_command_rva + 5, client_command_tail, sizeof(client_command_tail)))
		{
			return chained;
		}
		static constexpr uint8_t cmova[] = {0x41, 0x0F, 0x47, 0xD2};
		static constexpr uint8_t cmovb[] = {0x41, 0x0F, 0x42, 0xD2};
		for (const auto rva : client_command_range_tests)
		{
			if (!bytes_at(base + rva, cmova, sizeof(cmova)) && !bytes_at(base + rva, cmovb, sizeof(cmovb)))
			{
				return chained;
			}
		}
		static constexpr uint8_t nop4[] = {0x0F, 0x1F, 0x40, 0x00};
		bool all = true;
		for (const auto rva : client_command_range_tests)
		{
			all = write_code(base + rva, nop4, sizeof(nop4)) && all;
		}
		if (all)
		{
			chained |= 256;
		}
		return chained;
	}
}
