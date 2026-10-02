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

		// Count per match (see set_active_stub): a match with fewer players than the session
		// has seated gets the state of a fresh match of its size before it is allocated - the
		// four per-frame loop bounds (they index heap memory sized by cl_maxLocalClients, see
		// above), the stock allocation floor 2 and the dvar. The LUI context bound goes back
		// to 3, the value held since startup that every 1-3 player match runs with. Lowering
		// only ever visits fewer clients; run_cl_init_for_local_client2 and
		// widen_round_for_four raise the bounds again once a larger match is allocated.
		void size_match_for_players(const uint32_t players)
		{
			const auto target = static_cast<uint8_t>(std::clamp<uint32_t>(players, 2, 4));
			const auto b = base();
			const auto lower = [&](const size_t rva, const uint8_t to)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + rva);
				return readable(at, 1) && *at > to && *at <= 4 && write_bytes(at, &to, 1);
			};

			bool lowered = lower(cl_frame_pump_imm_rva, target);
			lowered = lower(netchan_poll_imm_rva, target) || lowered;

			// The cgame frame loop is one group: all five or none (run_cl_init_for_local_client2).
			uint8_t group = 0;
			bool group_above = true;
			for (const auto rva : cg_frame_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + rva);
				if (!readable(at, 1) || *at <= target || *at > 4 || (group && *at != group))
				{
					group_above = false;
					break;
				}
				group = *at;
			}
			if (group_above)
			{
				uint32_t wrote = 0;
				for (const auto rva : cg_frame_imms)
				{
					wrote += write_bytes(reinterpret_cast<uint8_t*>(b + rva), &target, 1) ? 1 : 0;
				}
				if (wrote != std::size(cg_frame_imms))
				{
					for (const auto rva : cg_frame_imms)
					{
						write_bytes(reinterpret_cast<uint8_t*>(b + rva), &group, 1);
					}
				}
				else
				{
					lowered = true;
				}
			}

			if (target < 4 && lui_ctx_held)
			{
				lowered = lower(lui_ctx_bound_rva + 3, 3) || lowered;
			}

			auto* floor_imm = reinterpret_cast<uint8_t*>(b + alloc_floor_rva);
			if (readable(floor_imm, 1) && *floor_imm > 0x02 && *floor_imm <= 0x04)
			{
				const uint8_t stock_floor = 0x02;
				write_bytes(floor_imm, &stock_floor, 1);
			}

			if (lowered)
			{
				if (target < 3)
				{
					lc2_widens_done = false;
				}
				if (target < 4)
				{
					round4_widened = false;
				}
			}
			set_splitscreen_player_count(std::max<uint32_t>(players, 1));
			note("[splitscreen] match of %u players: bounds %u, floor 2%s", players, target,
			     lowered ? ", lowered" : "");
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

				// Count per match (set_active_stub, size_match_for_players). The allocator asks
				// first thing (alloc_count_return_rva) and consumes the latch this match's setup
				// set. A match with fewer players than the session's commitment lowers the
				// commitment to its own size, so the logic below sizes and answers it as itself,
				// also through the map-load re-seat dip. Afterwards the commitment grows only as
				// players seat again - the state of a fresh session, whose menus run with the
				// stock allocation and loop bounds while players join; the widens return once a
				// larger match has been allocated.
				if (alloc_count_site_ok && match_latch_pending
				    && _ReturnAddress() == reinterpret_cast<void*>(base() + alloc_count_return_rva))
				{
					match_latch_pending = false;
					if (raise_local_client_count && match_latch < committed_seats)
					{
						committed_seats = match_latch;
						size_match_for_players(match_latch);
					}
				}

				// Commit the allocation floor: max(count, floor) at alloc_floor_rva feeds
				// every per-client allocation and cl_maxLocalClients. Once three (four) seats
				// have genuinely seated it stays 3 (4) for the session, so the map-load
				// reallocation cannot shrink below the party. Race-free: our caller is the
				// allocator itself. Only 02 -> 03 -> 04, one-way - except for the match
				// itself, above.
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

		// The device assignment's last test (assign_player_count_call_rva) gives a new
		// controller to player 2 when the count is above 1 and player 2 has no device.
		// The count can read 2 with one player signed in (2026-10-01: ezz restored a 2
		// from config.cfg, see unarchive_player_count, and the first pad plugged in
		// became player 2 on its first press). Here the smaller of the count and the
		// signed-in seats wins:
		// the pad moves only when a second player exists, and never where the count
		// alone would have kept it.
		int assign_player_count()
		{
			const int count = player_count_detoured ? splitscreen_player_count_stub()
			                                        : stock_splitscreen_player_count();
			const auto seats = static_cast<int>(bridged_seat_count());
			return seats > 0 ? std::min(count, seats) : count;
		}

		void install_assign_player_count()
		{
			if (!engine_bytes_match(assign_player_count_call_rva, assign_player_count_call_bytes)
			    || !call_site_to(assign_player_count_call_rva, sizeof(assign_player_count_call_bytes),
			                     reinterpret_cast<const void*>(&assign_player_count)))
			{
				note("[splitscreen] assign_player_count: engine bytes differ - not installed\n");
			}
		}

		// DynEntCl_CleanUpOldModels removes the extra dynent model furthest from every
		// viewer once the limit is reached (destructible debris - the Nuketown cars). It
		// collects one view origin per active local client into a stack array: [4] on PS4,
		// room for 2 on the PC, while the loop runs to cl_maxLocalClients. A third player
		// wrote over the stack cookie: __fastfail(2) at 0x02BC814C (2026-10-01, 4-player
		// MP on Nuk3town, WER dump boiii.exe.8172.dmp). The frame cannot grow, so the loop
		// stops at two viewers: debris near players 3/4 may be cleaned up a little sooner.
		void bound_dynent_cleanup_viewers()
		{
			if (!engine_bytes_match(dynent_cleanup_viewer_bound_rva, dynent_cleanup_viewer_bound_stock)
			    || !write_bytes(reinterpret_cast<void*>(base() + dynent_cleanup_viewer_bound_rva),
			                    dynent_cleanup_viewer_bound_fixed, sizeof(dynent_cleanup_viewer_bound_fixed)))
			{
				note("[splitscreen] dynent cleanup: engine bytes differ - not bounded\n");
			}
		}

		// Start and Back for players 2-4: after the game runs a guest's pad layout, the two
		// binds player 1 gets from default_bindings_<language>.cfg are run for that guest
		// too (button_config_exec_call_rva). Without them the guest's Start and Back reach
		// the game and are bound to nothing: no pause menu, no scoreboard.
		void exec_button_config(const int lc, const int controller, const char* text, const int arg)
		{
			const auto exec = reinterpret_cast<void (*)(int, int, const char*, int)>(
				base() + cmd_execute_single_command_rva);
			exec(lc, controller, text, arg);
			if (lc > 0)
			{
				exec(lc, controller, "bind BUTTON_START \"togglemenu\"", arg);
				exec(lc, controller, "bind BUTTON_BACK \"togglescores\"", arg);
			}
		}

		void install_guest_pad_binds()
		{
			if (!engine_bytes_match(button_config_exec_call_rva, button_config_exec_call_bytes)
			    || !call_site_to(button_config_exec_call_rva, sizeof(button_config_exec_call_bytes),
			                     reinterpret_cast<const void*>(&exec_button_config)))
			{
				note("[splitscreen] guest pad binds: engine bytes differ - not installed\n");
			}
		}

		// With two or more local players the game no longer pauses, in any mode: the PC
		// already applies that to Zombies and Campaign, and in Multiplayer one player's
		// Start paused the match and opened the pause menu on every screen (user,
		// 2026-10-01: "only for the one who presses it"). Console Multiplayer never pauses.
		// CG_CanPauseGame takes its own false exit (rsp is that function's frame there).
		void cg_can_pause_guard(midhook::context& c)
		{
			if (stock_splitscreen_player_count() > 1)
			{
				c.rip = base() + cg_can_pause_false_rva;
			}
		}

		void install_no_shared_pause()
		{
			if (!engine_bytes_match(cg_can_pause_false_rva, cg_can_pause_false_bytes)
			    || !midhook::install(base() + cg_can_pause_mp_rva, cg_can_pause_mp_bytes,
			                         sizeof(cg_can_pause_mp_bytes), &cg_can_pause_guard, &allocate_near_module))
			{
				note("[splitscreen] no shared pause: engine bytes differ - not installed\n");
			}
		}

		// The mesh/image streamer keeps one view position per rendered client: prev[2],
		// cur[2] and a bool[2] on the PC, [4] on PS4. A third view wrote over
		// numClientsLastFrame, the streamer's combine/sort never ran again and no streamed
		// mesh loaded for anyone (Nuk3town cars gone with 3-4 players: the car models got
		// LOD 0xFF from XModelSelectStreamableLod). The three arrays move to a block with
		// room for 8 views, the size of PS4's s_viewPos. Their accesses are the three
		// stream_* sites, patched all-or-nothing.
		struct stream_view_block
		{
			float prev[8][3];
			float cur[8][3];
			uint8_t still[8]; // PC-only: set -> no prev->cur extrapolation for that view
		};
		static_assert(offsetof(stream_view_block, cur) == 0x60);

		stream_view_block* stream_views = nullptr;

		// Replaces R_Stream_BeginUpdateFrame's copy-and-clear of the [2] arrays.
		void stream_begin_views()
		{
			std::memcpy(stream_views->prev, stream_views->cur, sizeof(stream_views->cur));
			std::memset(stream_views->cur, 0, sizeof(stream_views->cur));
			std::memset(stream_views->still, 0, sizeof(stream_views->still));
		}

		// The static update then appends the 8 streamer hints to its stack StreamUpdateCmd,
		// whose streamView array holds 10: 2 views + 8 hints fit stock, 4 views + 8 would
		// reach the stack cookie. Hook on the hint loop's `movss xmm1,[rbx+0x10] ; comiss
		// xmm1,xmm6` (its `jbe skip` stays in place): a full cmd skips the hint.
		void stream_hint_guard(midhook::context& c)
		{
			const auto views = *reinterpret_cast<const int32_t*>(midhook::site_rsp(c) + 0x58);
			if (views >= stream_update_cmd_views)
			{
				c.rip = base() + stream_hint_skip_rva;
			}
		}

		void relocate_stream_views()
		{
			const auto b = base();
			bool stock = engine_bytes_match(stream_begin_views_rva, stream_begin_views_bytes)
				&& engine_bytes_match(stream_static_bool_lea_rva, stream_static_bool_lea_bytes)
				&& engine_bytes_match(stream_static_cur_lea_rva, stream_static_cur_lea_bytes)
				&& engine_bytes_match(stream_static_prev_subs_rva, stream_static_prev_subs_bytes)
				&& engine_bytes_match(stream_hint_body_rva, stream_hint_body_bytes);
			for (const auto& s : stream_view_stores)
			{
				stock = stock && engine_bytes_match(s.rva, s.bytes);
			}
			if (!stock)
			{
				note("[splitscreen] stream views: engine bytes differ - not relocated\n");
				return;
			}

			auto* block = static_cast<stream_view_block*>(allocate_near_module(sizeof(stream_view_block)));
			uint8_t begin_call[sizeof(stream_begin_views_bytes)]{};
			uint8_t hint_hook[stream_hint_hook_len]{};
			if (!block
				|| !call_site_bytes(stream_begin_views_rva, sizeof(begin_call),
				                    reinterpret_cast<const void*>(&stream_begin_views), begin_call)
				|| !midhook::prepare(b + stream_hint_body_rva, stream_hint_body_bytes, stream_hint_hook_len,
				                     &stream_hint_guard, &allocate_near_module, hint_hook))
			{
				note("[splitscreen] stream views: allocation failed - not relocated\n");
				return;
			}

			const auto disp32 = [](const size_t to, const size_t from, int32_t& out)
			{
				const auto d = static_cast<int64_t>(to) - static_cast<int64_t>(from);
				out = static_cast<int32_t>(d);
				return d == out;
			};
			const auto glob = b + stream_glob_rva;
			const auto at = reinterpret_cast<size_t>(block);
			int32_t store[4]{}, bool_lea{}, cur_lea{};
			bool fits = disp32(at + offsetof(stream_view_block, cur), glob, store[0])
				&& disp32(at + offsetof(stream_view_block, cur) + 4, glob, store[1])
				&& disp32(at + offsetof(stream_view_block, cur) + 8, glob, store[2])
				&& disp32(at + offsetof(stream_view_block, still), glob, store[3])
				&& disp32(at + offsetof(stream_view_block, still), b + stream_static_bool_lea_rva + 7, bool_lea)
				&& disp32(at + offsetof(stream_view_block, cur) + 4, b + stream_static_cur_lea_rva + 7, cur_lea);
			if (!fits)
			{
				note("[splitscreen] stream views: block out of disp32 reach - not relocated\n");
				return;
			}

			// prev[i] sits sizeof(prev) below cur[i]; the loop reads it from rbx = &cur[i].y.
			uint8_t subs[sizeof(stream_static_prev_subs_bytes)];
			std::memcpy(subs, stream_static_prev_subs_bytes, sizeof(subs));
			for (size_t i = 0; i < std::size(stream_static_prev_disp8_offs); ++i)
			{
				subs[stream_static_prev_disp8_offs[i]] = static_cast<uint8_t>(
					static_cast<int8_t>(-static_cast<int>(sizeof(stream_view_block::prev)) - 4 + 4 * static_cast<int>(i)));
			}
			stream_views = block;
			uint32_t failed = 0;
			const auto put = [&](const uint32_t rva, const void* data, const size_t len)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + rva), data, len))
				{
					++failed;
				}
			};
			// Readers first, then the writer, then the frame reset: until the last write the
			// stock code still clears the old arrays, and the new block starts zeroed.
			put(stream_static_bool_lea_rva + 3, &bool_lea, 4);
			put(stream_static_cur_lea_rva + 3, &cur_lea, 4);
			put(stream_static_prev_subs_rva, subs, sizeof(subs));
			put(stream_hint_body_rva, hint_hook, sizeof(hint_hook));
			for (size_t i = 0; i < std::size(stream_view_stores); ++i)
			{
				put(stream_view_stores[i].rva + 4, &store[i], 4);
			}
			put(stream_begin_views_rva, begin_call, sizeof(begin_call));
			if (failed)
			{
				note("[splitscreen] stream views: %u writes failed\n", failed);
			}
		}

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
		void publish_update_notice(int controller);   // 16_update_check.inl

		void per_controller_update_stub(const int controller)
		{
			per_controller_update_hook.invoke<void>(controller);
			publish_update_notice(controller);

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

			auto* disconnect_bound = reinterpret_cast<uint8_t*>(b + disconnect_loop_bound_rva);
			const auto& disconnect_old = disconnect_loop_bound_bytes;
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

			auto* start = reinterpret_cast<uint8_t*>(b + cgame_shutdown_start_rva);
			const auto& start_old = cgame_shutdown_start_bytes;
			// Player 4: start at client 3; the walk tests flags & 0x10 (cgame up).
			const auto& start_new = cgame_shutdown_start_new;
			auto* cursor = reinterpret_cast<uint8_t*>(b + cgame_shutdown_cursor_rva);
			const auto& cursor_old = cgame_shutdown_cursor_bytes;
			if (!readable(start, sizeof(start_old)) || std::memcmp(start, start_old, sizeof(start_old)) != 0
			    || !readable(cursor, sizeof(cursor_old)) || std::memcmp(cursor, cursor_old, sizeof(cursor_old)) != 0)
			{
				note("[splitscreen] cgame shutdown loop: bytes differ - skipped\n");
				return done;
			}
			int32_t disp = 0;
			std::memcpy(&disp, cursor_old + 3, sizeof(disp));
			disp += 2 * uia_stride;   // clientUIActives[1] -> clientUIActives[3]
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
		// allocator never exceed 2. These 26 bytes become a call of active_count_stub,
		// which counts Com_LocalClient_IsBeingUsed(lc) for lc = 0..3; its eax flows into
		// the engine's own Dvar_SetInt (which has an Arxan caller check, so that call stays
		// the game's). Registers other than eax are dead at the site. The activation loop
		// bound stays at 2: widening it would write into clientUIActives[2].
		uint32_t active_count_stub()
		{
			const auto used = reinterpret_cast<bool (*)(int)>(base() + is_being_used_rva);
			uint32_t count = 0;
			for (int lc = 0; lc < 4; ++lc)
			{
				if (used(lc))
				{
					++count;
				}
			}
			return count;
		}

		bool install_active_count_fix()
		{
			auto* site = reinterpret_cast<uint8_t*>(base() + active_count_rva);
			if (std::memcmp(site, active_count_bytes, sizeof(active_count_bytes)) != 0)
			{
				note("[splitscreen] active count: unexpected bytes at 0x%zX\n", active_count_rva);
				return false;
			}
			active_count_installed = call_site_to(active_count_rva, sizeof(active_count_bytes),
			                                      reinterpret_cast<const void*>(&active_count_stub));
			return active_count_installed;
		}

		// The 0x1E940 per-client base reaches this consumer through [rsp+0x50] and is
		// delivered mangled (top 16 bits set) once client 2 exists (LOG.md, "THE 0x1E940
		// BASE IS GENUINELY CORRUPT"). A mid-function hook on its `imul rcx,rcx,0x1E940`
		// puts the table's real base into rax in that case; the imul then runs as stock,
		// so the flags are the game's. Only a value that is clearly not a pointer is
		// replaced: substituting always killed the process with the menu up.
		void stride_repair_stub(midhook::context& ctx)
		{
			if (ctx.rax >> 48)
			{
				ctx.rax = *reinterpret_cast<const uint64_t*>(base() + base_table_rva);
			}
		}

		bool install_stride_fix()
		{
			if (!midhook::install(base() + stride_site_rva, stride_site_bytes, sizeof(stride_site_bytes),
			                      &stride_repair_stub, &allocate_near_module))
			{
				note("[splitscreen] stride fix: not installed at 0x%zX\n", stride_site_rva);
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

		// Status block for external readers (there is no console) at status_rva, in the
		// reserved .data window (LOG.md, "RESERVED `.data`").
		constexpr uint32_t status_magic = 0xB03C0FFE;
