// Player 4 loops, storage stubs, byte-patch appliers, active-count and stride fixes, injected-client hold.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Player 4: the per-frame loops that need four allocated clients ------
		// The client-2 path widens these to 3. Each indexes per-client heap memory
		// sized by cl_maxLocalClients, so it may reach index 3 only once the engine
		// has allocated four. Each site must still read 03; anything else is left
		// alone and counted.
		bool round4_widened = false;

		void widen_round_for_four()
		{
			if (round4_widened || !lc2_fully_done())
			{
				return;
			}
			const auto b = base();
			const auto max_local = *reinterpret_cast<const volatile uint32_t*>(b + cl_max_local_clients_rva);
			if (max_local < 4)
			{
				return;
			}
			round4_widened = true;
			const uint8_t four = 0x04;
			uint32_t skipped = 0;
			const auto bump = [&](const uint32_t rva)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + rva);
				if (!readable(at, 1) || *at != 0x03 || !write_bytes(at, &four, 1))
				{
					++skipped;
				}
			};
			bump(cl_frame_pump_imm_rva);
			bump(netchan_poll_imm_rva);
			for (const auto rva : cg_frame_imms)
			{
				bump(rva);
			}
			// LUI context bound: `41 83 FF 03 90 90 90` (hold_lui_context_count)
			bump(lui_ctx_bound_rva + 3);
			if (skipped)
			{
				note("[splitscreen] player 4 round: %u per-frame loop bounds not at 3 - left\n", skipped);
			}
		}

		// The stock answer, computed here - never through invoke(): the original
		// (0x027C1AB0, PS4 0x01516BE0) is "no dvar -> 1, else jmp to the dvar getter",
		// and that getter's Arxan check (0x02261DF2: byte before the return address
		// must be a call) fails for a return address in this DLL and spins forever
		// (black screen at boot, 2026-09-29). The current integer is at +0x28.
		int stock_splitscreen_player_count()
		{
			uint64_t dvar = 0;
			std::memcpy(&dvar, reinterpret_cast<const void*>(base() + splitscreen_player_count_dvar_rva),
			            sizeof(dvar));
			if (dvar == 0)
			{
				return 1;
			}
			int32_t value = 1;
			std::memcpy(&value, reinterpret_cast<const void*>(dvar + dvar_current_offset), sizeof(value));
			return value;
		}

		int splitscreen_player_count_stub()
		{
			if (signin_relocated)
			{
				// Console semantics: PS4 CL_SplitscreenPlayerCount only reads the
				// splitscreen_playerCount dvar. The PC re-seats guests during map load and
				// the reallocation asks mid re-seat, when seats look signed out. So treat the
				// dvar as the committed count and commit seats upward into it here.
				// Panes are still gated by IsActive, which reads the live seats.
				const uint32_t seats = bridged_seat_count();
				uint32_t n = seats;

				// Hold the per-client HUD gate (roots + lc*0xB0 + 0xAC) at 0 for clients
				// without a HUD root; set, it lets the HUD draw resolve to the NULL sentinel.
				// Hot path: this stub answers over a million queries per session, so no
				// readable()/write_bytes() per call (it collapsed the menu frame rate).
				// Validate once, then plain reads; the roots block is component-owned RW.
				if (lui_roots_relocated && uiroot_new_base_rva)
				{
					static bool hud_flags_checked = false;
					static bool hud_flags_ok = false;
					if (!hud_flags_checked)
					{
						hud_flags_checked = true;
						hud_flags_ok = readable(
							reinterpret_cast<const uint8_t*>(
								base() + uiroot_new_base_rva + 2 * 0xB0 + 0xAC), 0xB0 + 1);
					}
					if (hud_flags_ok)
					{
						// Root 2 is always released, root 3 once four seats are in use. A held root
						// makes UI_CoD_GetRootNameForController answer UIRootFull, and a HUD there
						// swallows every other root's first_snapshot.
						const uint32_t held_from = seat_count() >= 4 ? 4u : 3u;
						for (uint32_t lc = held_from; lc < 4; ++lc)
						{
							auto* flag = reinterpret_cast<uint8_t*>(
								base() + uiroot_new_base_rva + lc * 0xB0 + 0xAC);
							if (*flag != 0)
							{
								*flag = 0;   // component-owned RW, no VirtualProtect
							}
						}
					}
				}

				// Commit the allocation floor: max(count, floor) at alloc_floor_rva feeds
				// every per-client allocation and cl_maxLocalClients. Once three (four) seats
				// have genuinely seated it stays 3 (4) for the session, so the map-load
				// reallocation cannot shrink below the party. Race-free: our caller is the
				// allocator itself. Only 02 -> 03 -> 04, one-way.
				if (raise_local_client_count)
				{
					const uint32_t constituted = seat_count();
					if (constituted > committed_seats)
					{
						committed_seats = constituted;
					}
					if (committed_seats >= 3)
					{
						// Plain read; VirtualProtect only on the actual edge.
						auto* floor_imm = reinterpret_cast<uint8_t*>(
							base() + alloc_floor_rva);
						const uint8_t target = committed_seats >= 4 ? 0x04 : 0x03;
						if (*floor_imm == 0x02 || (*floor_imm == 0x03 && target == 0x04))
						{
							write_bytes(floor_imm, &target, sizeof(target));
						}
						if (n < committed_seats)
						{
							// the same commitment, answered directly
							n = committed_seats;
						}
					}
				}
				uint64_t dvar = 0;
				std::memcpy(&dvar,
				            reinterpret_cast<const void*>(
					            base() + splitscreen_player_count_dvar_rva),
				            sizeof(dvar));
				if (dvar)
				{
					auto* current = reinterpret_cast<uint32_t*>(dvar + dvar_current_offset);
					static bool dvar_checked = false;
					static bool dvar_ok = false;
					static uint32_t dvar_pushes = 0;
					if (!dvar_checked)
					{
						dvar_checked = true;
						dvar_ok = readable(current, sizeof(uint32_t));
					}
					if (dvar_ok)
					{
						// Bounded: the engine's updater pushes this back down during the re-seat
						// window, and pushing on every query cost the frame rate. The allocation
						// floor guarantees the size anyway. Plain dvar field, no VirtualProtect.
						if (seats >= first_raised_player_count && seats > *current && dvar_pushes < 64)
						{
							*current = seats;
							++dvar_pushes;
						}
						if (*current > n)
						{
							n = *current;
						}
					}
				}
				// 1-2 players: the engine's own answer (see first_raised_player_count).
				if (n >= first_raised_player_count)
				{
					// The scheduler loops stop after a splitscreen sign-in, so CL_Init(2) is
					// triggered here: the allocator calls this stub at map load, on the game
					// thread, before the connect loop. cl_init2_done is set first in the callee,
					// so this cannot recurse.
					if (n >= 3 && !lc2_fully_done() && raise_local_client_count)
					{
						run_cl_init_for_local_client2();
					}
					if (n >= 4 && raise_local_client_count)
					{
						widen_round_for_four();
					}
					return static_cast<int>(n);
				}
			}
			return stock_splitscreen_player_count();
		}

		constexpr uint32_t per_controller_update_rva = 0x01E19AE0;
		constexpr uint8_t per_controller_update_prologue[] = {0x48, 0x8B, 0xC4, 0x55, 0x41, 0x54};
		utils::hook::detour per_controller_update_hook;

		void storage_pump_stub(const int controller)
		{
			storage_pump_hook.invoke<void>(controller);

			// Once storage has a real CAC root, retry only the publisher/update half;
			// it runs no completion handler, so the outer storage walk stays valid.
			if (guest2_lobby_enrolled && !guest2_profile_published)
			{
				refresh_guest2_lobby_profile();
			}

			// No reaping and no guest pumping here: Storage_Pump's caller is still on
			// the stack holding storage pointers, and completion handlers re-enter
			// storage. With s_targets widened the game pumps every controller itself.
			// History: LOG.md, "reaper".
		}

		// Per-controller update detour: advances the guest joins on the game's
		// own thread. (Reaping wedged guest storage tasks here was tried and
		// disabled: it blacked out the renderer. History: LOG.md, "reaper".)
		void per_controller_update_stub(const int controller)
		{
			per_controller_update_hook.invoke<void>(controller);

			// Guest sign-in: on the game's own thread (the renderer pipeline
			// answers engine predicates wrongly, see cl_init_watch) and outside
			// the Storage_Pump detour.
			if (controller == 2)
			{
				advance_guest2_join();
			}
			else if (controller == 3)
			{
				advance_guest3_join();
			}
		}

		uint32_t link_client_objects(const size_t table_slot, const size_t array_slot)
		{
			if (!new_base_rva[table_slot] || !new_base_rva[array_slot])
			{
				return 0;
			}

			const auto table = base() + new_base_rva[table_slot];
			const auto array = base() + new_base_rva[array_slot];
			guest_array_rva = new_base_rva[array_slot];

			uint32_t written = 0;
			for (size_t i = 0; i < 4; ++i)
			{
				const auto object = array + i * client_ui_stride;
				if (write_bytes(reinterpret_cast<void*>(table + i * sizeof(size_t)),
				                &object, sizeof(object)))
				{
					++written;
				}
			}
			return written;
		}

		// Verify the expected original byte before writing; skip with a log line on
		// mismatch.
		template <size_t N>
		uint32_t apply_byte_patches(const byte_patch (&patches)[N])
		{
			uint32_t done = 0;
			for (const auto& p : patches)
			{
				auto* site = reinterpret_cast<uint8_t*>(base() + p.rva);
				if (*site != p.expect)
				{
					note("[splitscreen] %s: 0x%zX holds 0x%02X, expected 0x%02X - skipped\n",
					       p.what, p.rva, *site, p.expect);
					continue;
				}
				if (write_bytes(site, &p.value, 1))
				{
					++done;
				}
			}
			return done;
		}

		uint32_t apply_storage_patches()
		{
			return apply_byte_patches(storage_patches);
		}

		uint32_t apply_local_client_count_patches()
		{
			if (!raise_local_client_count)
			{
				return 0;
			}
			return apply_byte_patches(local_client_count_patches);
		}

		// clientUIActives walker end-bounds: twelve loops end at
		// `lea reg, [clientUIActives[2]]` (four at field +8), among them the inlined
		// CL_AnyLocalClientsRunning. Unwidened, clients 2/3 miss the per-frame
		// upkeep. A thirteenth reference is an Arxan code copy and is left alone.
		struct end_bound_fix
		{
			uint32_t insn_rva;
			uint8_t expect[7];
		};

		constexpr end_bound_fix client_ui_end_bounds[] = {
			{0x0134B907, {0x48, 0x8D, 0x15, 0xA2, 0x03, 0x01, 0x04}},
			{0x0135950F, {0x48, 0x8D, 0x0D, 0x9A, 0x27, 0x00, 0x04}},
			{0x01359B92, {0x48, 0x8D, 0x0D, 0x17, 0x21, 0x00, 0x04}},
			{0x0135A27B, {0x48, 0x8D, 0x15, 0x2E, 0x1A, 0x00, 0x04}},
			{0x0135D1BB, {0x48, 0x8D, 0x15, 0xEE, 0xEA, 0xFF, 0x03}},
			{0x027C171E, {0x4C, 0x8D, 0x0D, 0x8B, 0xA5, 0xB9, 0x02}},
			{0x027C1789, {0x48, 0x8D, 0x15, 0x20, 0xA5, 0xB9, 0x02}},
			{0x027C1890, {0x48, 0x8D, 0x0D, 0x19, 0xA4, 0xB9, 0x02}},
			{0x020ECA0F, {0x48, 0x8D, 0x15, 0xA2, 0xF2, 0x26, 0x03}},
			{0x027C1617, {0x48, 0x8D, 0x0D, 0x9A, 0xA6, 0xB9, 0x02}},
			{0x027C1650, {0x48, 0x8D, 0x0D, 0x61, 0xA6, 0xB9, 0x02}},
			{0x027C1690, {0x48, 0x8D, 0x0D, 0x21, 0xA6, 0xB9, 0x02}},
		};

		uint32_t widen_client_ui_walker_bounds()
		{
			uint32_t done = 0;
			for (const auto& f : client_ui_end_bounds)
			{
				auto* insn = reinterpret_cast<uint8_t*>(base() + f.insn_rva);
				if (std::memcmp(insn, f.expect, sizeof(f.expect)) != 0)
				{
					note("[splitscreen] walker end-bound at 0x%X: unexpected "
					     "bytes - skipped\n", f.insn_rva);
					continue;
				}
				// The leas point at clientUIActives[2], the start of a foreign
				// array. Player 4: end at &[4]. Walkers read only +0 (flags) or +8
				// (connectionState) of an element, never the end address.
				int32_t disp = 0;
				std::memcpy(&disp, f.expect + 3, sizeof(disp));
				const int32_t new_disp = disp + 2 * 0x1078;
				if (write_bytes(insn + 3, &new_disp, sizeof(new_disp)))
				{
					++done;
				}
			}
			return done;
		}

		// End-of-match client loops. PS4 shuts down every local client:
		//   Com_ShutdownInternal (0xE47020):       CL_Disconnect(lc, false), lc < 4
		//   CL_ShutdownAllClientsCGame (0x3EADA0): CL_ShutdownCGame(lc), lc = 3..0
		// The PC copies stop at two, so client 2 kept its cgame at GAME OVER and the
		// next frame crashed on a NULL cg array. Both loops only act on a client
		// that is up, so two-player sessions are unaffected.
		uint32_t widen_client_shutdown_loops()
		{
			uint32_t done = 0;
			const auto b = base();

			auto* disconnect_bound = reinterpret_cast<uint8_t*>(b + 0x020F0797);
			constexpr uint8_t disconnect_old[] = {0x83, 0xFF, 0x02};
			if (readable(disconnect_bound, sizeof(disconnect_old))
			    && std::memcmp(disconnect_bound, disconnect_old, sizeof(disconnect_old)) == 0)
			{
				// Player 4: CL_Disconnect(3) returns unless flags bit 1 is set.
				const uint8_t four = 0x04;
				if (write_bytes(disconnect_bound + 2, &four, 1))
				{
					++done;
				}
			}
			else
			{
				note("[splitscreen] shutdown disconnect loop: bytes differ - skipped\n");
			}

			// The real Com_ShutdownInternal loop (passes false) and its inlined copy
			// also stopped at two; client 2 then stayed CA_ACTIVE through
			// CL_FreePerLocalClientMemory and hung the next round at CA_CONNECTED.
			// Their UI close loops (PS4 UI_CloseAll i < 4) index uiInfoArray, so they
			// widen only when its perclient_rows row moved it.
			struct shutdown_site { uint32_t rva; uint8_t modrm; const char* what; bool needs_uiinfo; };
			constexpr shutdown_site com_shutdown_sites[] = {
				{0x020F11CE, 0xFF, "Com_ShutdownInternal disconnect loop", false},
				{0x020F188B, 0xFB, "inlined Com_ShutdownInternal disconnect loop", false},
				{0x020F1219, 0xFB, "Com_ShutdownInternal UI close loop", true},
				{0x020F18DC, 0xFB, "inlined Com_ShutdownInternal UI close loop", true},
			};
			for (const auto& s : com_shutdown_sites)
			{
				if (s.needs_uiinfo && !perclient_new[perclient_row("uiinfo")])
				{
					note("[splitscreen] %s: uiInfoArray not moved - left at 2\n", s.what);
					continue;
				}
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva);
				const uint8_t want[] = {0x83, s.modrm, 0x02};
				if (!readable(at, sizeof(want)) || std::memcmp(at, want, sizeof(want)) != 0)
				{
					note("[splitscreen] %s: bytes differ - skipped\n", s.what);
					continue;
				}
				const uint8_t four = 0x04;   // player 4: uiInfoArray [4], seat record 3
				if (write_bytes(at + 2, &four, 1))
				{
					++done;
				}
			}

			auto* start = reinterpret_cast<uint8_t*>(b + 0x0132E31A);
			constexpr uint8_t start_old[] = {0xBF, 0x01, 0x00, 0x00, 0x00};
			// Player 4: start at client 3; the walk tests flags & 0x10 (cgame up).
			constexpr uint8_t start_new[] = {0xBF, 0x03, 0x00, 0x00, 0x00};
			auto* cursor = reinterpret_cast<uint8_t*>(b + 0x0132E31F);
			constexpr uint8_t cursor_old[] = {0x48, 0x8D, 0x1D, 0x12, 0xC9, 0x02, 0x04};
			if (!readable(start, sizeof(start_old)) || std::memcmp(start, start_old, sizeof(start_old)) != 0
			    || !readable(cursor, sizeof(cursor_old)) || std::memcmp(cursor, cursor_old, sizeof(cursor_old)) != 0)
			{
				note("[splitscreen] cgame shutdown loop: bytes differ - skipped\n");
				return done;
			}
			int32_t disp = 0;
			std::memcpy(&disp, cursor_old + 3, sizeof(disp));
			disp += 2 * 0x1078;   // clientUIActives[1] -> clientUIActives[3]
			if (!write_bytes(cursor + 3, &disp, sizeof(disp)))
			{
				return done;
			}
			if (!write_bytes(start, start_new, sizeof(start_new)))
			{
				write_bytes(cursor, cursor_old, sizeof(cursor_old));
				return done;
			}
			return done + 1;
		}

		// Active count fix ("only two screens"). PS4 SetAllUsedActive sets
		// splitscreen_playerCount from CL_LocalClient_GetActiveCount (0x1516A20,
		// i < 4); the PC inlines it unrolled to two elements, so the dvar and the
		// allocator never exceed 2. These 26 bytes jump to a cave that counts
		// Com_LocalClient_IsBeingUsed(lc) for lc = 0..3 and returns into the
		// engine's own Dvar_SetInt. The activation loop bound stays at 2: widening
		// it would write into clientUIActives[2].
		constexpr size_t active_count_rva = 0x027C1A0D;
		constexpr uint8_t active_count_bytes[] = {
			0x40, 0x84, 0x35, 0xAC, 0x81, 0xB9, 0x02, // test byte [rip+..], sil
			0xB8, 0x00, 0x00, 0x00, 0x00,             // mov eax, 0
			0x0F, 0x45, 0xC6,                         // cmovne eax, esi
			0x40, 0x84, 0x35, 0x15, 0x92, 0xB9, 0x02, // test byte [rip+..], sil
			0x74, 0x02,                               // je +2
			0xFF, 0xC0,                               // inc eax
		};

		bool install_active_count_fix()
		{
			auto* site = reinterpret_cast<uint8_t*>(base() + active_count_rva);
			if (std::memcmp(site, active_count_bytes, sizeof(active_count_bytes)) != 0)
			{
				note("[splitscreen] active count: unexpected bytes at 0x%zX\n",
				     active_count_rva);
				return false;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return false;
			}

			auto* slots = cave + 0x80; // +0 index, +4 count, +8 executions, +12 last
			const auto cave_addr = reinterpret_cast<size_t>(cave);

			std::vector<uint8_t> c;
			// `extra` = bytes after the displacement in the same instruction (an
			// immediate); getting it wrong aims the access off by that much.
			const auto rip32 = [&](const size_t target, const size_t extra = 0)
			{
				const auto value = static_cast<int32_t>(
					target - (cave_addr + c.size() + 4 + extra));
				const auto* p = reinterpret_cast<const uint8_t*>(&value);
				c.insert(c.end(), p, p + 4);
			};

			const auto slot = [&](const size_t i) { return reinterpret_cast<size_t>(slots + i); };

			// inc dword [executions]
			c.insert(c.end(), {0xFF, 0x05});
			rip32(slot(8));
			// mov dword [index], 0
			c.insert(c.end(), {0xC7, 0x05});
			rip32(slot(0), 4);
			c.insert(c.end(), {0x00, 0x00, 0x00, 0x00});
			// mov dword [count], 0
			c.insert(c.end(), {0xC7, 0x05});
			rip32(slot(4), 4);
			c.insert(c.end(), {0x00, 0x00, 0x00, 0x00});

			const auto loop_start = c.size();
			// mov ecx, dword [index]
			c.insert(c.end(), {0x8B, 0x0D});
			rip32(slot(0));
			// call Com_LocalClient_IsBeingUsed
			c.insert(c.end(), {0xE8});
			rip32(base() + is_being_used_rva);
			// test al, al ; je +6
			c.insert(c.end(), {0x84, 0xC0});
			c.insert(c.end(), {0x74, 0x06});
			// inc dword [count]            (6 bytes - the je above skips exactly this)
			c.insert(c.end(), {0xFF, 0x05});
			rip32(slot(4));
			// inc dword [index]
			c.insert(c.end(), {0xFF, 0x05});
			rip32(slot(0));
			// cmp dword [index], 4
			c.insert(c.end(), {0x83, 0x3D});
			rip32(slot(0), 1);
			c.insert(c.end(), {0x04});
			// jl loop
			c.insert(c.end(), {0x0F, 0x8C});
			{
				const auto target = cave_addr + loop_start;
				const auto value = static_cast<int32_t>(target - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&value);
				c.insert(c.end(), p, p + 4);
			}
			// mov eax, dword [count]   -> the value the engine then uses
			c.insert(c.end(), {0x8B, 0x05});
			rip32(slot(4));
			// mov dword [last], eax
			c.insert(c.end(), {0x89, 0x05});
			rip32(slot(12));
			// jmp back, past the 26 replaced bytes
			c.insert(c.end(), {0xE9});
			rip32(base() + active_count_rva + sizeof(active_count_bytes));

			if (c.size() > 0x80)
			{
				note("[splitscreen] active count: cave too small (%zu)\n", c.size());
				return false;
			}

			std::memcpy(cave, c.data(), c.size());
			std::memset(slots, 0, 0x30);

			uint8_t patch[sizeof(active_count_bytes)];
			std::memset(patch, 0x90, sizeof(patch)); // nop the remainder
			patch[0] = 0xE9;
			const auto rel = static_cast<int32_t>(cave_addr - (base() + active_count_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));

			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return false;
			}

			active_count_slots = slots;
			return true;
		}

		bool install_stride_fix()
		{
			auto* site = reinterpret_cast<uint8_t*>(base() + stride_site_rva);
			if (std::memcmp(site, stride_site_bytes, sizeof(stride_site_bytes)) != 0)
			{
				note("[splitscreen] stride fix: unexpected bytes at 0x%zX\n", stride_site_rva);
				return false;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return false;
			}

			auto* slots = cave + 0x80; // count, delivered, substituted
			const auto cave_addr = reinterpret_cast<size_t>(cave);

			std::vector<uint8_t> c;
			const auto rip32 = [&](const size_t target)
			{
				const auto value = static_cast<int32_t>(target - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&value);
				c.insert(c.end(), p, p + 4);
			};

			// inc dword [executions] first, so every entry is counted
			c.insert(c.end(), {0xFF, 0x05});
			rip32(reinterpret_cast<size_t>(slots + 4));
			// mov [delivered], rax
			c.insert(c.end(), {0x48, 0x89, 0x05});
			rip32(reinterpret_cast<size_t>(slots + 8));
			// Repair only a value that is clearly not a pointer (top 16 bits set);
			// substituting unconditionally killed the process with the menu up.
			c.insert(c.end(), {0x50}); // push rax
			c.insert(c.end(), {0x48, 0xC1, 0xE8, 0x30}); // shr rax, 48
			c.insert(c.end(), {0x85, 0xC0}); // test eax, eax
			c.insert(c.end(), {0x58}); // pop rax  (does not touch flags)
			c.insert(c.end(), {0x74, 20}); // jz -> skip the 20-byte repair
			// mov rax, [base_table]
			c.insert(c.end(), {0x48, 0x8B, 0x05});
			rip32(base() + base_table_rva);
			// inc dword [count]
			c.insert(c.end(), {0xFF, 0x05});
			rip32(reinterpret_cast<size_t>(slots));
			// mov [substituted], rax
			c.insert(c.end(), {0x48, 0x89, 0x05});
			rip32(reinterpret_cast<size_t>(slots + 16));
			// The displaced imul last, so the flags seen afterwards are unchanged.
			c.insert(c.end(), std::begin(stride_site_bytes), std::end(stride_site_bytes));
			// jmp back
			c.insert(c.end(), {0xE9});
			rip32(base() + stride_site_rva + sizeof(stride_site_bytes));

			std::memcpy(cave, c.data(), c.size());
			std::memset(slots, 0, 0x30);

			uint8_t patch[7] = {0xE9, 0, 0, 0, 0, 0x90, 0x90};
			const auto rel = static_cast<int32_t>(cave_addr - (base() + stride_site_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));

			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return false;
			}

			return true;
		}

		// The injected local client has no message channel, so the launch stalls
		// on the lobby state message. Hold its gate fields and ack equal to a real
		// local guest (slot 1) for the whole launch; this copies true state.
		void hold_injected_clients()
		{
			const auto session = base() + lobby_pool_rva + game_lobby_index * lobby_pool_stride;
			const auto sequence = *reinterpret_cast<uint32_t*>(base() + launch_sequence_rva);

			uint32_t target_ack = sequence + 1;
			for (size_t i = 0; i < 2; ++i)
			{
				const auto xuid = *reinterpret_cast<uint64_t*>(
					session + session_clients_offset + i * session_client_stride);
				if (!xuid)
				{
					continue;
				}
				const auto ack = *reinterpret_cast<uint32_t*>(session + acks_offset + i * 4);
				target_ack = std::max(target_ack, ack);
			}

			for (const auto slot : injected_slots)
			{
				const auto xuid = *reinterpret_cast<uint64_t*>(
					session + session_clients_offset + slot * session_client_stride);
				if (!xuid)
				{
					continue; // not signed in, nothing to hold
				}

				for (const auto gate : gate_arrays)
				{
					const auto ref = session + gate + reference_slot * session_client_stride;
					const auto dst = session + gate + slot * session_client_stride;
					for (const auto field : copy_fields)
					{
						auto* d = reinterpret_cast<uint32_t*>(dst + field);
						const auto v = *reinterpret_cast<uint32_t*>(ref + field);
						if (*d != v)
						{
							*d = v;
						}
					}
				}

				auto* ack = reinterpret_cast<uint32_t*>(session + acks_offset + slot * 4);
				if (*ack < target_ack)
				{
					*ack = target_ack;
				}
			}
		}

		// Status block for external readers (there is no console), in the reserved
		// .data window (LOG.md, "RESERVED `.data`").
		constexpr size_t status_rva = 0x1A828D00;
		constexpr uint32_t status_magic = 0xB03C0FFE;
