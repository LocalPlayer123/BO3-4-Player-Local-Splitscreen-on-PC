// Core: patch tables (local-client count, storage), helpers (base, readable, write_bytes, allocate_near_module, note, relocate), guest fill.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// --- the per-client base that arrives mangled ---
		//
		// The table at base_table_rva holds the correct per-client base, but with a third player the
		// value reaching the consumer is corrupted: it travels through
		// mov rax,[rsp+0x50] / ror rax,0x20 behind an Arxan integrity compare.
		// Re-reading it from the table restores it. This is a repair, not a null
		// guard: the value is neither null nor missing.

		// imul rcx, rcx, 0x1e940   - displaced into the cave

		// Do not repair the second consumer of this value at 0x00F7E918: doing so
		// kills the process with no minidump, with or without a guard.

		// --- the local-client count ---
		//
		// cl_maxLocalClients (RVA 0x053A2720) holds 2 and is read by ~2630
		// `cmp <reg>, [cl_maxLocalClients]` bounds checks across the engine. PS4
		// CG_GetLocalClientGlobals (0x1D76850) returns NULL for any localClientNum
		// >= cl_maxLocalClients, which is the NULL controller 2 hit. Most other
		// per-client failures are downstream of this value.
		//
		// Raising it is safe because the containers are ready: the per-client CG
		// blocks are allocated as count * stride (PS4 CG_AllocateClientMemory
		// 0x21FD70; PC 0x00840922 `imul rdx, rdx, 0x342720`), and the fixed arrays
		// (s_storage, client_objs, client_ui) are relocated by this component.
		//
		// Ordering rule: containers first, count second. Raising the count while a
		// target array is still [2] makes per-client loops write slots 2/3 over
		// adjacent live memory.
		//
		// CL_AllocatePerLocalClientMemory (0x0135D650) computes
		// max(CL_SplitscreenPlayerCount(), 2) (PS4 0x416A32). That floor feeds
		// CG/FX/CL_AllocateClientMemory and the cl_maxLocalClients store at
		// 0x0135D489. It is not raised statically - telling the allocator four
		// while splitscreen_playerCount said one crashed a solo round at start.
		// splitscreen_player_count_stub() commits it to 3 or 4 once that many
		// seats have really seated.

		// --- storage: give controllers 2 and 3 real buffers ---
		//
		// AllocateMemory (PS4 storage_api.cpp, called once from Storage_Init)
		// carves every controller's file buffers and scratch out of one pool sized
		// for LOCAL_CLIENT_COUNT controllers. On PC that count is 2 in two places:
		//   0x02275922  lea eax, [r9 + r10*2]   pool multiplier (SIB scale)
		//   0x02275A80  cmp r12d, 2             controller loop bound
		// Raising both lets the game's own allocator carve four controllers, as on
		// the console; nothing is hand-written. Shared files (properties->[0x20]
		// == 3) still resolve to controller 0's buffer.
		//
		// The same TU bounds the controller count in eight more places, found
		// mechanically (every `cmp <r32>, 2` whose loop indexes s_storage by
		// 0x8958). The key one is the gatekeeper 0x02276E30
		// (`cmp esi, 2 / jl ok / xor al, al / ret`) on the file-lookup paths:
		// without it the new buffers exist but nothing can find them.
		// All are `cmp r32, imm8`, so every patch is one byte.

		// storage_pool_rva (s_storageMem.pool): zero until AllocateMemory has run, so it
		// tells whether the patches can still get in before Storage_Init.

		// --- launch handshake -------------------------------------------------
		constexpr size_t lobby_pool_stride = 0x66828;
		constexpr size_t game_lobby_index = 1;
		constexpr size_t session_clients_offset = 0xF8;
		constexpr size_t session_client_stride = 0x30;
		constexpr size_t acks_offset = 0x2780;
		constexpr size_t gate_arrays[] = {0x23D0, 0x65FA0};
		constexpr size_t copy_fields[] = {0x08, 0x0C, 0x10, 0x20};
		constexpr size_t reference_slot = 1;
		constexpr size_t injected_slots[] = {2, 3};

		size_t base()
		{
			return game::get_base();
		}

		// Reference tables come from outside the image, and an entry can point at
		// a page that is not committed yet (common in the Arxan region at
		// post_unpack). Reading it would throw and abort post_unpack, so every
		// table read checks here first.
		bool readable(const void* p, const size_t size)
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
			{
				return false;
			}
			if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
			{
				return false;
			}
			const auto start = reinterpret_cast<size_t>(mbi.BaseAddress);
			const auto end = start + mbi.RegionSize;
			return reinterpret_cast<size_t>(p) + size <= end;
		}

		bool write_bytes(void* target, const void* data, const size_t size)
		{
			DWORD old{};
			if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old))
			{
				return false;
			}

			const auto restore = utils::finally([&]
			{
				DWORD tmp{};
				VirtualProtect(target, size, old, &tmp);
			});

			std::memcpy(target, data, size);
			return true;
		}

		// Relocated arrays must land near the module: rip-relative and ABS32
		// displacements are 32-bit, and a plain VirtualAlloc lands out of reach.
		// Probe 64 KB steps just past the image, as tools/reloc_range.py does.
		//
		// Every allocation is padded on both sides. The game indexes
		// per-controller arrays with -1 ("no controller yet") and bounds checks only
		// test the upper bound. In the stock image s_storage[-1] reads mapped .data;
		// at the front of a fresh allocation it faulted (signing in the second
		// client crashed at 0x02276E71). The zero-filled padding turns an invalid
		// index into a null that the caller's own `test rax,rax` handles.
		constexpr size_t reloc_padding = 0x10000; // > any observed element size

		void* allocate_near_module(const size_t size)
		{
			const auto module_base = base();

			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
				module_base + dos->e_lfanew);
			const size_t image_size = nt->OptionalHeader.SizeOfImage;

			auto candidate = (module_base + image_size + 0xFFFF) & ~static_cast<size_t>(0xFFFF);

			for (size_t i = 0; i < 0x4000; ++i, candidate += 0x10000)
			{
				if (candidate - module_base > 0x60000000)
				{
					break; // beyond the reach of a 32-bit displacement
				}

				auto* p = VirtualAlloc(reinterpret_cast<void*>(candidate),
				                       size + 2 * reloc_padding,
				                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
				if (p)
				{
					// Hand back the middle. VirtualAlloc zero-fills, so the slack
					// on both sides reads as null for out-of-range indices.
					return reinterpret_cast<void*>(
						reinterpret_cast<size_t>(p) + reloc_padding);
				}
			}

			return nullptr;
		}

		// Never call printf from here: it takes post_unpack down. note() formats
		// into a buffer and goes only to the component's trace file, so a patch
		// that stands down on a new build still says so. Diagnostic build only.
		void trace_text(const char* text);   // defined after trace_write

		template <typename... Args>
		void note(const char* fmt, Args... args)
		{
#ifndef SS_DIAG
			(void)fmt;
			((void)args, ...);
#else
			char buf[512]{};
			std::snprintf(buf, sizeof(buf), fmt, args...);
			for (auto* p = buf; *p; ++p)
			{
				if (*p == '\n' || *p == '\r')
				{
					*p = ' ';
				}
			}
			trace_text(buf);
#endif
		}

		// Where each table landed, filled by relocate(). The client-object table
		// holds absolute pointers into the client-UI array, so once both have
		// moved, every pointer (entries 0 and 1 too) is recomputed from the new
		// bases.
		size_t new_base_rva[8] = {};

		constexpr size_t gamepads_reserved_size = 0x1C0;
		size_t gamepads_reserved_rva = 0;

		bool reserve_gamepads_region()
		{
			if (gamepads_reserved_rva)
			{
				return true;
			}
			auto* reserved = allocate_near_module(gamepads_reserved_size);
			if (!reserved)
			{
				return false;
			}
			gamepads_reserved_rva = reinterpret_cast<size_t>(reserved) - base();
			return true;
		}

		bool relocate(const reloc_table& t, const size_t slot)
		{
			auto* fresh = allocate_near_module(t.new_size);

			if (!fresh)
			{
				note("[splitscreen] %s: no memory within reach of a 32-bit offset\n", t.name);
				return false;
			}

			const auto new_rva = reinterpret_cast<size_t>(fresh) - base();

			std::memcpy(fresh, reinterpret_cast<void*>(base() + t.base_rva), t.old_size);
			std::memset(static_cast<uint8_t*>(fresh) + t.old_size, 0, t.new_size - t.old_size);

			// Verify every reference against the image before writing any of them, so
			// a stale table cannot corrupt code.
			size_t deviating = 0;
			for (size_t i = 0; i < t.count; ++i)
			{
				const auto& r = t.refs[i];
				const auto* field = reinterpret_cast<const int32_t*>(
					base() + r.insn_rva + r.disp_offset);
				const auto expected = r.rip_relative
					                      ? static_cast<int32_t>(r.target_rva - (r.insn_rva + r.length))
					                      : static_cast<int32_t>(r.target_rva);
				if (!readable(field, sizeof(int32_t)) || *field != expected)
				{
					++deviating;
				}
			}

			// End-bound refs are verified in the same pass: all or nothing.
			for (size_t i = 0; i < t.end_count; ++i)
			{
				const auto& r = t.end_refs[i];
				const auto* field = reinterpret_cast<const int32_t*>(
					base() + r.insn_rva + r.disp_offset);
				const auto expected = r.rip_relative
					                      ? static_cast<int32_t>(r.target_rva - (r.insn_rva + r.length))
					                      : static_cast<int32_t>(r.target_rva);
				if (!readable(field, sizeof(int32_t)) || *field != expected)
				{
					++deviating;
				}
			}

			if (deviating)
			{
				note("[splitscreen] %s: %zu of %zu references do not match the image - "
				       "NOTHING written\n", t.name, deviating, t.count);
				return false;
			}

			size_t done = 0;
			for (size_t i = 0; i < t.count; ++i)
			{
				const auto& r = t.refs[i];
				const auto moved = new_rva + (r.target_rva - t.base_rva);
				const auto value = r.rip_relative
					                   ? static_cast<int32_t>(moved - (r.insn_rva + r.length))
					                   : static_cast<int32_t>(moved);

				if (write_bytes(reinterpret_cast<void*>(base() + r.insn_rva + r.disp_offset),
				                &value, sizeof(value)))
				{
					++done;
				}
			}

			// End-bound references: loops like `for (p = &a[0]; p < &a[N]; p++)` bake
			// in the end address. They get new base + NEW size; shifting them like
			// ordinary refs would still walk the old element count.
			size_t end_done = 0;
			for (size_t i = 0; i < t.end_count; ++i)
			{
				const auto& r = t.end_refs[i];
				auto* field = reinterpret_cast<void*>(base() + r.insn_rva + r.disp_offset);
				if (!readable(field, sizeof(int32_t)))
				{
					continue;
				}
				const auto moved = new_rva + t.new_size;
				const auto value = r.rip_relative
					                   ? static_cast<int32_t>(moved - (r.insn_rva + r.length))
					                   : static_cast<int32_t>(moved);
				if (write_bytes(field, &value, sizeof(value)))
				{
					++end_done;
				}
			}

			if (done == t.count && end_done == t.end_count)
			{
				if (slot < std::size(new_base_rva))
				{
					new_base_rva[slot] = new_rva;
				}
				return true;
			}
			note("[splitscreen] %s: only %zu/%zu references and %zu/%zu end bounds moved to RVA 0x%zX\n",
			       t.name, done, t.count, end_done, t.end_count, new_rva);
			return false;
		}

		// PC element size of the client-UI array (static ctor: `mov edx,0x1170 /
		// mov r8d,2`). PS4 clientUIActive_t is 0x1078; strides differ by platform.
		constexpr size_t client_ui_stride = 0x1170;

		// Guest identities for controllers 2 and 3 in the relocated userData
		// array. The client-object table has no writer (it is statically
		// initialised), so its entries are produced, not moved.
		//
		// Guests get a private copy of element 1's identity head plus their own
		// XUID (element 1's + i) and gamertag. Element 1, not 0: it is a signed-in
		// secondary profile (+0x29 = 01), and existing that way at boot is what
		// earns controller 1 its storage and stats readiness. Element 0 is the
		// primary; copying it gave an empty name and a duplicate identity.
		constexpr size_t userdata_xuid = 0x00;
		// char[32] (PS4 userData_t gamertag[32]); LiveUser_GetClientName returns +8.
		constexpr size_t userdata_gamertag = 0x08;
		constexpr size_t userdata_gamertag_size = 0x20;
		constexpr size_t userdata_signedin = 0x30;

		// "<name>(<controller + 1>)", the way ezz names local guests (live.cpp
		// LiveUser_UserGetName), built from the donor's name without its own
		// "(n)". The old version overwrote one fixed character with '0' + i: a
		// duplicate "(2)" for controller 2, and wrong for any name whose digit
		// did not sit at that offset.
		void write_guest_gamertag(const size_t dst, const size_t donor, const size_t controller)
		{
			char name[userdata_gamertag_size + 1]{};
			std::memcpy(name, reinterpret_cast<const void*>(donor + userdata_gamertag), userdata_gamertag_size);
			size_t len = strnlen(name, userdata_gamertag_size);
			if (len >= 3 && name[len - 1] == ')')
			{
				size_t k = len - 2;
				while (k > 0 && name[k] >= '0' && name[k] <= '9')
				{
					--k;
				}
				if (name[k] == '(' && k < len - 2)
				{
					len = k;
				}
			}
			const char suffix[] = {'(', static_cast<char>('1' + controller), ')'};
			len = std::min(len, userdata_gamertag_size - 1 - sizeof(suffix));
			char out[userdata_gamertag_size]{};
			std::memcpy(out, name, len);
			std::memcpy(out + len, suffix, sizeof(suffix));
			std::memcpy(reinterpret_cast<void*>(dst + userdata_gamertag), out, sizeof(out));
		}

		// The copy cannot happen at post_unpack, where element 1 is still zeroed.
		// The table pointers are written at post_unpack; the guest records are
		// filled once element 1 is signed in.
		size_t guest_array_rva = 0;
		bool guests_filled = false;

		// Defined further down with the count patches; the session-mode fix in
		// fill_guests_when_ready() is gated on it.
		extern bool raise_local_client_count;

		void fill_guests_when_ready()
		{
			if (guests_filled || !guest_array_rva)
			{
				return;
			}

			const auto array = base() + guest_array_rva;
			const auto donor = array + client_ui_stride; // element 1

			uint32_t donor_signedin = 0;
			std::memcpy(&donor_signedin,
			            reinterpret_cast<const void*>(donor + userdata_signedin),
			            sizeof(donor_signedin));

			// signInState (+0x30; PS4 userData_t +0x70): 0 not signed in, 1 locally,
			// 2 to Live. LiveUser_IsSignedIn is `signInState > 0` on both platforms,
			// so fill as soon as the donor reaches 1: the boot pump runs only a few
			// times, and mirror_signin_state keeps the guests level afterwards.
			if (donor_signedin == 0)
			{
				return; // not populated yet - try again next tick
			}

			// Do not delay the fill past boot: controller 2 then gets no storage.

			// Copy only the identity head, never the whole record. Storage_Pump needs
			// only xuid and signInState (both in the first 0x40 bytes, which hold no
			// pointers). The whole 0x1170 record duplicates heap pointers the profile
			// owns; copying it crashed.
			constexpr size_t identity_head = 0x40;
			static_assert(identity_head > userdata_signedin, "signInState must be copied");

			for (size_t i = 2; i < 4; ++i)
			{
				const auto dst = array + i * client_ui_stride;
				std::memcpy(reinterpret_cast<void*>(dst),
				            reinterpret_cast<const void*>(donor), identity_head);

				// Guest xuid = donor xuid + i. Do not use the console guest-id formula
				// (dwGetGuestUserID): the PC does not implement it, and the game's own
				// working guest (element 1) already uses an id space unrelated to the
				// host's.
				uint64_t xuid = 0;
				std::memcpy(&xuid, reinterpret_cast<const void*>(donor + userdata_xuid),
				            sizeof(xuid));
				xuid += i;
				std::memcpy(reinterpret_cast<void*>(dst + userdata_xuid), &xuid, sizeof(xuid));

				write_guest_gamertag(dst, donor, i);
			}

			// Put the session in local-splitscreen mode, as the console does when
			// splitscreen is activated (Lua SessionModeSetOffline). Otherwise a lobby
			// entered from the main menu stays a bot-game session ("2 Players
			// (18 Max)") and the third seat joins through the network path. Calls the
			// engine's own setter (SetNetworkMode, reached from the
			// SessionModeSetOffline binding 0x01FD0170) instead of poking the bitfield.
			// Only ever forces offline; this mod is offline-only.
			// Known gap: this runs once at boot, while the mode is still 0, so a later
			// menu choice is not corrected (PRIVATE GAME works because that menu sets
			// offline itself). A real fix must run when the lobby is created.
			if (raise_local_client_count)
			{
				constexpr uint32_t network_mode_mask = 0x3C0;
				constexpr uint32_t network_mode_shift = 6;

				const auto* state = reinterpret_cast<const uint32_t*>(
					base() + session_state_rva);
				if (readable(state, sizeof(uint32_t)))
				{
					const auto net = (*state & network_mode_mask) >> network_mode_shift;
					if (net != 0)
					{
						const auto set_net = reinterpret_cast<void (*)(int)>(
							base() + set_network_mode_rva);
						set_net(0);
					}
				}
			}

			guests_filled = true;
		}

		// PC Storage_Pump(ControllerIndex_t) (PS4 0xF7F120). The game runs it only
		// a few times during boot and not again once the menu is up, so a guest
		// that appears late gets no storage. Calling the game's own function once
		// more is the same call the boot sequence makes.

