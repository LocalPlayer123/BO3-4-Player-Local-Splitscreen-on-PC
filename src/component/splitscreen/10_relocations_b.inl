// Relocations, part B: batches 12-18, light queue, the per-client relocation table (perclient_rows),
// Umbra, lens flares, controller UI models, lobby max players.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Batch 12: zombies HUD player list ----
		// The PlayerList HUD update keeps six per-client arrays, all [2] and packed back to back:
		// scores, a second score, shown flags, client ids (-1 = none), icon names (char*) and the
		// own index. For lc 2 the icon row is the own-index array, so ints were read as string
		// pointers (crash in strcmp from the UI model string setter). The engine's reset leaves
		// set icons and ids per lc before use, so zero-filled slots 2/3 are fine.

		// ---- Batch 13: previous-frame view ----
		// Pane 3 drew a white void: two [2] per-view renderer arrays have a foreign slot 2.
		// The other one, s_sunVolumeTransitions (PS4 [4] x 0x2A28), is moved by
		// relocate_percg_context (same six sites). Slots 2/3 need no static init: the
		// per-client view init resets each element, -1 at +0x2BB0 included (store at
		// 0x010CD118, after FX_SetNextUpdateCamera(lc, 2) as in PS4 CG_InitView 0x2CB240).
		// g_prevFrameViewParmsDraw (PS4 GfxViewParms[4] x 0x290): R_RenderScene copies each
		// frame's view parms to prev[localClientNum]; slot 2 covered another renderer object.

		// ---- Batch 14: LiveStats per-controller stat-change cache ----
		// PS4 LiveStats_SetStatChanged (0xC63D00) decodes change messages into
		// s_cachedStatsChanges[controller], [4]. The PC cache (0x100 entries, stride 0x4404,
		// count at +0x4400) is [2], so controller 2 overwrote the statics behind it, including
		// the "statReadDDLExt" cmd node (crash in Cmd_RemoveCommand at game over).
		// LiveStats_ResetCache clears all four slots on PS4; the PC memset (0x8808) clears two
		// and is widened to 0x11010 after the move (an uncleared count reaching 0x100 is
		// EXE_PATCH_STATSOVERFLOW).

		// ---- Batch 15: per-client UI visibility bits ----
		// The zombie HUD shows its widgets through the "UIVisibilityBit.<n>" models. PS4 keeps
		// the bits as u64[4] (sharedUiInfo +0x14240), the PC as u64[2]. Client 2's u64 lies on
		// the per-client visibility-bit model handles and the scoreboard team-model handles,
		// which CL_UpdateUIVisibilityBits(2) overwrote every frame: no HUD in panes 1 and 2.
		// Known and left: one routine clears match bits for lc 0/1 with a single 16-byte
		// and/andn; client 2's bits are recomputed every frame anyway.

		// Post-step of the visbits row: widen the per-client reset loop that zeroes
		// bits[lc] from 2 to 4.
		bool widen_visbits_reset(const perclient_array&, size_t)
		{
			auto* bound = reinterpret_cast<uint8_t*>(base() + visbits_reset_bound_rva);
			const auto& bound_old = visbits_reset_bound_bytes;
			if (readable(bound, sizeof(bound_old)) && std::memcmp(bound, bound_old, sizeof(bound_old)) == 0)
			{
				const uint8_t four = 0x04;
				return write_bytes(bound + 2, &four, 1);
			}
			return false;
		}

		// ---- Batch 16: console message buffers, con.messageBuffer [2] -> [4] ----
		// PS4 struct Console has MessageBuffer[4] at +0x11078, followed by color and
		// operationBuffer (the console text). The PC has MessageBuffer[2] there (stride 0x2BC0),
		// so Con_NudgeMessageWindowTimes for lc 2 walked con.color and text as a window and
		// crashed. Three reference forms, all verified before anything is written: RIP/ABS32
		// sites (conmsgbuf_sites); the end marker of Con_InitMessageBuffer's loop, retargeted to
		// &new[4]+0x2B10; con base plus a displacement inside the array (conmsgbuf_con_rel,
		// invisible to range scans), each rewritten to old + (new block - old array).
		// Nothing in the game has initialised con at post_unpack.

		constexpr uint32_t conmsgbuf_end_field = 0x2B10;

		// Pre-step of the conmsgbuf row: every con-relative value and the end marker
		// still hold the stock offsets.
		bool conmsgbuf_refs_match(const perclient_array&)
		{
			const auto b = base();
			for (const auto& s : conmsgbuf_con_rel)
			{
				uint32_t have = 0;
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				if (!readable(at, sizeof(have)))
				{
					return false;
				}
				std::memcpy(&have, at, sizeof(have));
				if (have != s.old_value)
				{
					note("[splitscreen] conmsgbuf: 0x%08X holds 0x%X - nothing moved\n", s.rva, have);
					return false;
				}
			}
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + conmsgbuf_end_marker_rva + 3);
				int32_t d32 = 0;
				if (!readable(at, sizeof(d32)))
				{
					return false;
				}
				std::memcpy(&d32, at, sizeof(d32));
				if (conmsgbuf_end_marker_rva + 7 + d32
				    != conmsgbuf_base + 2 * conmsgbuf_stride + conmsgbuf_end_field)
				{
					note("[splitscreen] conmsgbuf: end marker differs - nothing moved\n");
					return false;
				}
			}
			return true;
		}

		// Post-step of the conmsgbuf row: the con-relative values and the end marker.
		bool retarget_conmsgbuf_refs(const perclient_array&, const size_t fresh)
		{
			const auto b = base();
			// allocate_near_module keeps every new value within 32 bits; checked anyway.
			const auto delta = static_cast<int64_t>(fresh) - static_cast<int64_t>(b + conmsgbuf_base);
			uint32_t rewritten = 0;
			for (const auto& s : conmsgbuf_con_rel)
			{
				const auto v = static_cast<int64_t>(s.old_value) + delta;
				if (v > INT32_MAX || v < INT32_MIN)
				{
					break;
				}
				const auto v32 = static_cast<int32_t>(v);
				if (!write_bytes(reinterpret_cast<void*>(b + s.rva + s.off), &v32, sizeof(v32)))
				{
					break;
				}
				++rewritten;
			}
			const bool marker = retarget_end_marker(conmsgbuf_end_marker_rva, 3, 7,
				conmsgbuf_base + 2 * conmsgbuf_stride + conmsgbuf_end_field,
				fresh + 4 * conmsgbuf_stride + conmsgbuf_end_field);
			if (!marker || rewritten != std::size(conmsgbuf_con_rel))
			{
				note("[splitscreen] conmsgbuf [2]->[4]: %u/%zu con-relative, end marker %s\n",
				     rewritten, std::size(conmsgbuf_con_rel), marker ? "moved" : "FAILED");
			}
			return marker && rewritten == std::size(conmsgbuf_con_rel);
		}

		// ---- uiInfoArray [2] -> [4] ---------------------------------------------
		// PS4 ui_main.cpp: uiInfoArray[4], element 0x1B68 (same size on PC). The PC
		// has [2]; client 2 wrote its menu state into foreign memory, which crashed
		// the game at exit. After the move, UI_InitUIInfos' `cmp ebp,2` loop runs
		// to 4 like PS4. widen_client_shutdown_loops depends on this move.

		// Pre-step of the uiinfo row.
		bool uiinfo_bound_matches(const perclient_array&)
		{
			const auto* bound = reinterpret_cast<const uint8_t*>(base() + uiinfo_init_bound_rva);
			if (!readable(bound, sizeof(uiinfo_init_bound_old))
			    || std::memcmp(bound, uiinfo_init_bound_old, sizeof(uiinfo_init_bound_old)) != 0)
			{
				note("[splitscreen] uiinfo: init loop bound differs - nothing moved\n");
				return false;
			}
			return true;
		}

		// Post-step of the uiinfo row: UI_InitUIInfos' loop runs to 4.
		bool widen_uiinfo_init(const perclient_array&, size_t)
		{
			auto* bound = reinterpret_cast<uint8_t*>(base() + uiinfo_init_bound_rva);
			const uint8_t four = 0x04;
			const bool widened = write_bytes(bound + 2, &four, 1);
			if (!widened)
			{
				note("[splitscreen] uiinfo [2]->[4]: init loop FAILED\n");
			}
			return widened;
		}

		// ---- UI3D texture windows per local client [2] -> [4] -------------------
		// The PC saves 6 UI3D windows (0x438 bytes) per local client in
		// R_UI3D_SetupBackendData (0x01D100D0) and restores them next frame in
		// R_UI3D_PerframeInit (0x01D0FF20). With [2], client 2 overwrote the data
		// behind the array (player 3's white HUD panels in MP). PS4 has a single
		// g_ui3d_windows. Three other hits in the range are loop end markers and stay.

		// ---- Light queue: records [2][1024] + counters [2] -> [4] ----------------
		// Per-client ring of light records (1024 x 0x28, stride 0xA000) with
		// read/write counters in two int[2] arrays A and B, 8 bytes apart. Client
		// 2's records overlay the counters, so on Revelations the consumer read a
		// garbage pointer. New block: records[4], A[4] at +0x28000, B[4] at +0x28010.
		// The reset's two `mov qword [rip+d],rax` become `movups [rip+d],xmm0` (same
		// length, xmm0 already zero) so all four clients' counters clear.
		constexpr uint32_t lightq_stride = 0xA000;
		constexpr size_t lightq_records_new = 4 * lightq_stride;   // 0x28000

		bool lightq_relocated = false;

		bool relocate_lightq()
		{
			if (lightq_relocated)
			{
				return true;
			}
			const auto b = base();
			for (const auto& e : lightq_fixed_sites)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + e.rva);
				if (!readable(at, e.len) || std::memcmp(at, e.old_bytes, e.len) != 0)
				{
					note("[splitscreen] lightq: bytes at 0x%08X differ - nothing moved\n", e.rva);
					return false;
				}
			}

			auto* fresh = static_cast<uint8_t*>(allocate_near_module(lightq_records_new + 0x20));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, lightq_records_new + 0x20);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + lightq_base), 2 * lightq_stride);
			std::memcpy(fresh + lightq_records_new, reinterpret_cast<const void*>(b + lightq_a), 8);
			std::memcpy(fresh + lightq_records_new + 0x10, reinterpret_cast<const void*>(b + lightq_b), 8);

			int32_t saved_r[std::size(lightq_sites)] = {};
			int32_t saved_a[std::size(lightq_a_sites)] = {};
			int32_t saved_b[std::size(lightq_b_sites)] = {};
			const auto restore = [&](const entcoll_site* sites, const size_t n, const int32_t* saved)
			{
				for (size_t i = 0; i < n; ++i)
				{
					write_bytes(reinterpret_cast<void*>(b + sites[i].rva + sites[i].disp_off), &saved[i], sizeof(int32_t));
				}
			};
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);
			if (!rewrite_entcoll(lightq_sites, std::size(lightq_sites), lightq_base, fresh_abs, saved_r))
			{
				return false;
			}
			if (!rewrite_entcoll(lightq_a_sites, std::size(lightq_a_sites), lightq_a, fresh_abs + lightq_records_new, saved_a))
			{
				restore(lightq_sites, std::size(lightq_sites), saved_r);
				return false;
			}
			if (!rewrite_entcoll(lightq_b_sites, std::size(lightq_b_sites), lightq_b, fresh_abs + lightq_records_new + 0x10, saved_b))
			{
				restore(lightq_a_sites, std::size(lightq_a_sites), saved_a);
				restore(lightq_sites, std::size(lightq_sites), saved_r);
				return false;
			}
			for (const auto& e : lightq_fixed_sites)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + e.rva), e.new_bytes, e.len))
				{
					note("[splitscreen] lightq [2]->[4]: reset/memset/loop patch at 0x%08X FAILED\n", e.rva);
				}
			}
			lightq_relocated = true;
			return true;
		}

		static_assert(umbra_params_new % 0x14 == 0, "the distance-scale setter indexes params as (lc + bias) * 0x14");

		bool umbra_grown = false;

		bool grow_umbra_client_arrays()
		{
			if (umbra_grown)
			{
				return true;
			}
			const auto b = base();
			const auto* object = reinterpret_cast<const uint64_t*>(b + umbra_object_rva);
			if (!readable(object, sizeof(*object)) || *object != 0)
			{
				note("[splitscreen] umbra: object already allocated - not grown\n");
				return false;
			}
			for (const auto& s : umbra_disps)
			{
				uint32_t have = 0;
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				if (!readable(at, sizeof(have)))
				{
					return false;
				}
				std::memcpy(&have, at, sizeof(have));
				if (have != s.old_value)
				{
					note("[splitscreen] umbra: 0x%08X holds 0x%X - nothing written\n", s.rva, have);
					return false;
				}
			}
			for (const auto& u : umbra_bounds)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + u.rva + 3);
				if (!readable(at, 1) || *at != u.old_value)
				{
					note("[splitscreen] umbra: bound 0x%08X differs - nothing written\n", u.rva);
					return false;
				}
			}

			size_t done_disps = 0;
			size_t done_bounds = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done_disps; ++i)
				{
					write_bytes(reinterpret_cast<void*>(b + umbra_disps[i].rva + umbra_disps[i].off),
					            &umbra_disps[i].old_value, sizeof(uint32_t));
				}
				for (size_t i = 0; i < done_bounds; ++i)
				{
					write_bytes(reinterpret_cast<void*>(b + umbra_bounds[i].rva + 3),
					            &umbra_bounds[i].old_value, 1);
				}
			};
			for (const auto& s : umbra_disps)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + s.rva + s.off), &s.new_value, sizeof(uint32_t)))
				{
					rollback();
					return false;
				}
				++done_disps;
			}
			for (const auto& u : umbra_bounds)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + u.rva + 3), &u.new_value, 1))
				{
					rollback();
					return false;
				}
				++done_bounds;
			}
			umbra_grown = true;
			return true;
		}

		// LiveStats_ResetCache: memset length `mov r8d, 0x8808` -> 0x11010,
		// only when its lea already points at the moved array. Post-step of the statscache row.
		bool widen_statscache_reset(const perclient_array&, const size_t cache_new)
		{
			const auto b = base();
			auto* imm = reinterpret_cast<uint8_t*>(b + statscache_reset_len_rva);
			const auto& imm_old = statscache_reset_len_bytes;
			const auto& imm_new = statscache_reset_len_new;
			const auto* lea = reinterpret_cast<const uint8_t*>(b + statscache_reset_lea_rva);
			int32_t lea_disp = 0;
			std::memcpy(&lea_disp, lea + 3, sizeof(lea_disp));
			if (!cache_new || b + statscache_reset_lea_rva + 7 + lea_disp != cache_new
			    || !readable(imm, sizeof(imm_old)) || std::memcmp(imm, imm_old, sizeof(imm_old)) != 0)
			{
				note("[splitscreen] statscache reset: bytes differ - not widened\n");
				return false;
			}
			return write_bytes(imm, imm_new, sizeof(imm_new));
		}

		// Own row: ikStates to [5]. If a reference fails to verify, fall back to moving the
		// reset loop's end marker one slot, which covers client 2 (three players); with two
		// players that slot is NULL and the loop skips it.
		bool ikstates_step()
		{
			if (!ik_reset_widened)
			{
				ik_reset_widened = relocate_ikstates()
					|| retarget_end_marker(ikstates_end_marker_rva, 3, 7, ikstates_base + ikstates_old_slots * 8,
					                       base() + ikstates_base + (ikstates_old_slots + 1) * 8);
			}
			if (!ikstates_new)
			{
				note("[splitscreen] ikStates: NOT moved - reset loop widened one slot (3 players only)\n");
			}
			return ik_reset_widened;
		}

		// Own row: the session member slots. CG_Init(2) runs whenever player 3's cgame
		// initialises at map load.
		bool session_members_step()
		{
			const bool moved = relocate_session_members();
			if (!moved)
			{
				note("[splitscreen] session members: NOT moved (bytes differ)\n");
			}
			return moved;
		}

		// ---- Every per-client [2] -> [4] relocation, in the order they run ------
		// relocate_perclient_rows() walks this once from try_apply(). Sites and the
		// PS4 names are next to each array's site table (batches 1-11:
		// 06_relocations_a.inl, 12-18: above). A row that fails stands down alone.
		constexpr perclient_row perclient_rows[] = {
			// batch 1 (cl_voiceCommunication is reloc_tables' voice_comm)
			{cgdc_array},
			{.array = {"playerKeys"}, .own = relocate_playerkeys},
			{notetracklerps_array},
			// batch 1b
			{cg_pmove_array},
			{camerashake_array},
			{moverinfos_array},
			{moveinfoentnum_array},
			{rumble_array},
			{atglob_array},
			{aimtargetcmd_array},
			{arcdata_array},
			{.array = zbarriers_array,
			 .post = widen_zbarrier_clear},
			// batch 2
			{totalcoverage_array},
			{rightstick_array},
			{gamepadbuttons_array},
			{.array = {"cgExploderTriggers"}, .own = relocate_exploder_triggers},
			{.array = {"gaGlobs"}, .own = relocate_gaglobs},
			// batch 3
			{screenblur_array},
			{screenelec_array},
			{screenburn_array},
			{.array = compass_actors_array,
			 .pre = compass_clear_is_two_rows<compass_actors_clear_rva>, .post = compass_clear_to_four_rows<compass_actors_clear_rva>},
			{.array = compass_vehicles_array,
			 .pre = compass_clear_is_two_rows<compass_vehicles_clear_rva>, .post = compass_clear_to_four_rows<compass_vehicles_clear_rva>},
			{.array = compass_artillery_array,
			 .pre = compass_clear_is_two_rows<compass_artillery_clear_rva>, .post = compass_clear_to_four_rows<compass_artillery_clear_rva>},
			{.array = compass_heli_array,
			 .pre = compass_clear_is_two_rows<compass_heli_clear_rva>, .post = compass_clear_to_four_rows<compass_heli_clear_rva>},
			{.array = compass_0240_array,
			 .pre = compass_clear_is_two_rows<compass_0240_clear_rva>, .post = compass_clear_to_four_rows<compass_0240_clear_rva>},
			{.array = compass_0120_array,
			 .pre = compass_clear_is_two_rows<compass_0120_clear_rva>, .post = compass_clear_to_four_rows<compass_0120_clear_rva>},
			{.array = compass_0500_array,
			 .pre = compass_clear_is_two_rows<compass_0500_clear_rva>, .post = compass_clear_to_four_rows<compass_0500_clear_rva>},
			{.array = compass_0400_array,
			 .pre = compass_clear_is_two_rows<compass_0400_clear_rva>, .post = compass_clear_to_four_rows<compass_0400_clear_rva>},
			// batch 4
			{cg_weaponsarray_array},
			{cg_ikbuf_array},
			{cg_destructibles_array},
			{numdestructibles_array},
			{cg_updatetime_array},
			{destr_gamestates_array},
			{destr_numgamestates_array},
			{.array = {"ikStates"}, .own = ikstates_step},
			// batch 5
			{cg_clientents30_array},
			{cg_perclient_3c0_array},
			{.array = {"session_members"}, .own = session_members_step},
			// batch 6
			{.array = tnotify_list_array,
			 .pre = tnotify_init_matches, .post = tnotify_init_items},
			{.array = tnotify_head_array,
			 .pre = tnotify_list_moved},
			{.array = tnotify_tail_array,
			 .pre = tnotify_list_moved},
			{.array = tnotify_free_array,
			 .pre = tnotify_list_moved},
			// batch 7 (ungated: the 190 MB slide happened with the third pane off too)
			{fxgpu_client_array},
			// batch 8 (scene_c before install_perclient_buffer_guard: its cave bakes C's base)
			{scene_pc480_array},
			{.array = scene_c_array,
			 .post = publish_scene_c},
			// batches 9-13
			{rview_a24_array},
			{rview_org30_array},
			{aimactors_array},
			{hudpl_score_array},
			{hudpl_gap_array},
			{hudpl_flags_array},
			{hudpl_ids_array},
			{hudpl_icons_array},
			{hudpl_self_array},
			{prevview_array},
			// batches 14-18 and the light queue
			{.array = statscache_array,
			 .post = widen_statscache_reset},
			{.array = visbits_array,
			 .post = widen_visbits_reset},
			{.array = conmsgbuf_array,
			 .pre = conmsgbuf_refs_match, .post = retarget_conmsgbuf_refs},
			{.array = uiinfo_array,
			 .pre = uiinfo_bound_matches, .post = widen_uiinfo_init},
			{ui3d_windows_array},
			{.array = {"lightq"}, .own = relocate_lightq},
		};

		// The new block of each plain row, 0 while it is not moved.
		size_t perclient_new[std::size(perclient_rows)] = {};

		// Row index by name, resolved while compiling: a name missing from the table
		// does not build, and reordering rows cannot point a reader at another array.
		consteval size_t perclient_row(const std::string_view name)
		{
			for (size_t i = 0; i < std::size(perclient_rows); ++i)
			{
				if (name == perclient_rows[i].array.name)
				{
					return i;
				}
			}
			throw "perclient_row: no such row";
		}

		bool tnotify_list_moved(const perclient_array&)
		{
			return perclient_new[perclient_row("tnotify_list")] != 0;
		}

		// Runs every row once, in order. A row whose pre-step refuses or whose move
		// fails keeps 0; a post-step reports its own failure, the result is unused.
		void relocate_perclient_rows()
		{
			for (size_t i = 0; i < std::size(perclient_rows); ++i)
			{
				const auto& r = perclient_rows[i];
				if (r.own)
				{
					r.own();   // guards itself against a second run
					continue;
				}
				if (perclient_new[i] || (r.pre && !r.pre(r.array)))
				{
					continue;
				}
				perclient_new[i] = relocate_perclient(r.array);
				if (perclient_new[i] && r.post)
				{
					r.post(r.array, perclient_new[i]);
				}
			}
		}

		bool lensflare_gated = false;

		void gate_lensflares_for_extra_clients()
		{
			if (lensflare_gated)
			{
				return;
			}
			const auto b = base();
			for (const auto& g : lensflare_gates)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + g.rva);
				if (!readable(at, g.len) || std::memcmp(at, g.prologue, g.len) != 0)
				{
					note("[splitscreen] lensflare gate: 0x%08X prologue not stock - no gate installed\n", g.rva);
					return;
				}
			}
			uint32_t installed = 0;
			for (const auto& g : lensflare_gates)
			{
				auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
				if (!cave)
				{
					break;
				}
				std::vector<uint8_t> c;
				c.insert(c.end(), {0x83, 0xFA, 0x02});       // cmp edx, 2
				c.insert(c.end(), {0x7C, 0x00});             // jl original (patched)
				const auto jl_at = c.size() - 1;
				if (g.returns_minus_one)
				{
					c.insert(c.end(), {0xB8, 0xFF, 0xFF, 0xFF, 0xFF}); // mov eax, -1
				}
				c.insert(c.end(), {0xC3});                   // ret
				c[jl_at] = static_cast<uint8_t>(c.size() - (jl_at + 1));
				c.insert(c.end(), g.prologue, g.prologue + g.len);
				c.insert(c.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00}); // jmp [rip+0]
				const uint64_t back = b + g.rva + g.len;
				const auto* back_bytes = reinterpret_cast<const uint8_t*>(&back);
				c.insert(c.end(), back_bytes, back_bytes + 8);
				if (!write_bytes(cave, c.data(), c.size()))
				{
					break;
				}
				uint8_t patch[9];
				std::memset(patch, 0x90, sizeof(patch));
				patch[0] = 0xE9;
				const auto rel = static_cast<int32_t>(
					reinterpret_cast<size_t>(cave) - (b + g.rva + 5));
				std::memcpy(patch + 1, &rel, sizeof(rel));
				if (!write_bytes(reinterpret_cast<uint8_t*>(b + g.rva), patch, g.len))
				{
					break;
				}
				++installed;
			}
			lensflare_gated = installed == std::size(lensflare_gates);
			if (!lensflare_gated)
			{
				note("[splitscreen] lensflare gate: %u of %zu entry points gated for local clients >= 2\n",
				     installed, std::size(lensflare_gates));
			}
		}

		// ---- Quit hang: lens-flare manager destructor at process exit ----------
		// Quitting skips FX_ShutdownLensFlareSystem, so at exit the destructor's
		// PMem_Free("LensFlareManager") hits an already-freed block, Com_Error fires
		// and the crash handler recurses until the stack overflows. At exit this
		// only frees memory the OS reclaims anyway, so the exit thunk returns at
		// once. The level-end shutdown takes another path and is untouched.

		void skip_lensflare_exit_shutdown()
		{
			auto* at = reinterpret_cast<uint8_t*>(base() + lensflare_exit_thunk_rva);
			if (!readable(at, sizeof(lensflare_exit_thunk_expected))
				|| std::memcmp(at, lensflare_exit_thunk_expected, sizeof(lensflare_exit_thunk_expected)) != 0)
			{
				note("[splitscreen] lensflare exit thunk: bytes differ - left alone\n");
				return;
			}
			const uint8_t ret = 0xC3;
			write_bytes(at, &ret, 1);
		}

		// ---- Per-controller UI model roots 2..3 --------------------------------
		// Engine.GetModelForController(2) was nil, so controller 2's data-bound HUD
		// widgets (ammo, scores) had nothing to subscribe to. PS4 UI_Model_Init
		// (0xD68140) creates s_controllerModel[i] = AllocateNode(global,
		// "controller%d", true) for 4 controllers; the hidden PC init makes 2.
		// Slots 2/3 are unreferenced padding, so nothing moves. A cave at the entry
		// of Com_LocalClient_LastInput_Init (run once, right after UI_Model_Init)
		// creates the missing roots with UI_Model_CreatePersistentModelFromPath;
		// they must be persistent or UI_Shutdown frees them and leaves stale handles.
		// Known hazard, not fixed: setupArmBladeTarget / setupRocketLauncherTarget
		// just before are [2] per client (PS4: 4); lc 2 would overwrite these roots.
		bool controller_models_hooked = false;

		void create_extra_controller_models()
		{
			if (controller_models_hooked)
			{
				return;
			}
			const auto b = base();
			const struct
			{
				uint32_t rva;
				const uint8_t* bytes;
				size_t len;
			} checks[] = {
				{lastinput_init_rva, lastinput_init_expected, sizeof(lastinput_init_expected)},
				{ui_controller_model_getter_rva, ui_controller_model_getter_expected,
				 sizeof(ui_controller_model_getter_expected)},
				{ui_global_model_getter_rva, ui_global_model_getter_expected,
				 sizeof(ui_global_model_getter_expected)},
				{ui_create_persistent_rva, ui_create_persistent_expected,
				 sizeof(ui_create_persistent_expected)},
				{ui_create_persistent_alloc_rva, ui_create_persistent_alloc_expected,
				 sizeof(ui_create_persistent_alloc_expected)},
			};
			for (const auto& c : checks)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + c.rva);
				if (!readable(p, c.len))
				{
					note("[splitscreen] controller models: 0x%08X not readable - not hooked\n", c.rva);
					return;
				}
				if (std::memcmp(p, c.bytes, c.len) == 0)
				{
					continue;
				}
				// BOIII 1.1.0.1445 hooks UI_CreatePersistent with a 5-byte `jmp rel32`.
				// Accept that (the cave only calls it); every later byte must match.
				const bool client_hooked_entry = c.rva == ui_create_persistent_rva && c.len > 5 && p[0] == 0xE9
					&& std::memcmp(p + 5, c.bytes + 5, c.len - 5) == 0;
				if (!client_hooked_entry)
				{
					note("[splitscreen] controller models: bytes differ at 0x%08X - not hooked\n", c.rva);
					return;
				}
			}
			const auto* slots = reinterpret_cast<const uint16_t*>(b + ui_controller_model_rva);
			if (!readable(slots, 8) || slots[2] != 0 || slots[3] != 0)
			{
				note("[splitscreen] controller models: slots 2/3 not zero - not hooked\n");
				return;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return;
			}
			const auto put64 = [](std::vector<uint8_t>& v, uint64_t x)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(&x);
				v.insert(v.end(), p, p + 8);
			};
			std::vector<uint8_t> c;
			c.insert(c.end(), {0x48, 0x83, 0xEC, 0x28});                 // sub rsp, 0x28
			size_t name_fixups[2] = {};
			for (int slot = 2; slot <= 3; ++slot)
			{
				const uint64_t slot_va = b + ui_controller_model_rva + slot * 2;
				c.insert(c.end(), {0x48, 0xB8}); put64(c, slot_va);        // mov rax, &slot
				c.insert(c.end(), {0x66, 0x83, 0x38, 0x00});               // cmp word [rax], 0
				const size_t jne_at = c.size();
				c.insert(c.end(), {0x75, 0x00});                           // jne next
				c.insert(c.end(), {0x48, 0xB8}); put64(c, b + ui_global_model_rva); // mov rax, &global
				c.insert(c.end(), {0x0F, 0xB7, 0x08});                     // movzx ecx, word [rax]
				c.insert(c.end(), {0x85, 0xC9});                           // test ecx, ecx
				const size_t jz_at = c.size();
				c.insert(c.end(), {0x74, 0x00});                           // jz next
				c.insert(c.end(), {0x48, 0xBA});                           // mov rdx, name
				name_fixups[slot - 2] = c.size();
				put64(c, 0);
				c.insert(c.end(), {0x48, 0xB8}); put64(c, b + ui_create_persistent_rva); // mov rax, create
				c.insert(c.end(), {0xFF, 0xD0});                           // call rax
				c.insert(c.end(), {0x48, 0xB9}); put64(c, slot_va);        // mov rcx, &slot
				c.insert(c.end(), {0x66, 0x89, 0x01});                     // mov word [rcx], ax
				const size_t next = c.size();
				c[jne_at + 1] = static_cast<uint8_t>(next - (jne_at + 2));
				c[jz_at + 1] = static_cast<uint8_t>(next - (jz_at + 2));
			}
			c.insert(c.end(), {0x48, 0x83, 0xC4, 0x28});                 // add rsp, 0x28
			c.insert(c.end(), lastinput_init_expected,
			         lastinput_init_expected + sizeof(lastinput_init_expected)); // replayed
			c.insert(c.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});     // jmp [rip+0]
			put64(c, b + lastinput_init_rva + sizeof(lastinput_init_expected));
			static const char names[] = "controller2\0controller3";
			for (int i = 0; i < 2; ++i)
			{
				const uint64_t at = reinterpret_cast<uint64_t>(cave) + c.size();
				std::memcpy(c.data() + name_fixups[i], &at, sizeof(at));
				const char* s = names + i * 12;
				c.insert(c.end(), s, s + std::strlen(s) + 1);
			}
			if (c.size() > 0x100 || !write_bytes(cave, c.data(), c.size()))
			{
				return;
			}

			uint8_t patch[sizeof(lastinput_init_expected)];
			patch[0] = 0xE9;
			const auto rel = static_cast<int32_t>(
				reinterpret_cast<size_t>(cave) - (b + lastinput_init_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(reinterpret_cast<uint8_t*>(b + lastinput_init_rva), patch, sizeof(patch)))
			{
				return;
			}
			controller_models_hooked = true;
		}

		// ---- Gamepad button models for controllers 2..3 -------------------------
		// Stock CoDMenu.lua joins an unused controller through its "ButtonBits.*"
		// models; controller 2 had none, so its A press never reached
		// LobbyAddLocalClient. PS4 CL_InitGamepadModels (0x3FF160) loops lc 0..3;
		// the PC stops at 2. Widened only when every array the loop touches exists
		// for lc 2/3: s_rightStickModels and s_gamepadButtons moved by batch2
		// (widening without that move crashed at launch), the model roots, and the
		// seat records.

		bool gamepad_models_widened = false;

		void widen_gamepad_button_models()
		{
			const auto rightstick_new = perclient_new[perclient_row("rightstick")];
			const auto buttons_new = perclient_new[perclient_row("gamepadbuttons")];
			if (!rightstick_new || !buttons_new || !signin_relocated || !controller_models_hooked)
			{
				note("[splitscreen] gamepad button models: stock, missing rightstick=%d buttons=%d signin=%d roots=%d\n",
				     rightstick_new ? 1 : 0, buttons_new ? 1 : 0, signin_relocated ? 1 : 0,
				     controller_models_hooked ? 1 : 0);
				return;
			}
			auto* p = reinterpret_cast<uint8_t*>(base() + gamepad_models_bound_rva);
			if (!readable(p, sizeof(gamepad_models_bound_expected))
				|| std::memcmp(p, gamepad_models_bound_expected, sizeof(gamepad_models_bound_expected)) != 0)
			{
				note("[splitscreen] gamepad button models: stock, bytes differ at 0x%08X\n", gamepad_models_bound_rva);
				return;
			}
			// 4: seat records, model roots and both model arrays exist for lc 0..3.
			const uint8_t four = 0x04;
			if (write_bytes(p + 2, &four, 1))
			{
				gamepad_models_widened = true;
			}
		}

		// ---- lobby_maxLocalPlayers: range max 2 -> 4 -----------------------------
		// The stock Lua join needs GetLobbyLocalClientCount < lobby_maxLocalPlayers,
		// but the PC registers the dvar with max 2 (PS4 LobbyConfig_Init 0xCC0437:
		// 1..4). Only that Lua reads the dvar. Default stays 2.
		constexpr size_t lobby_max_local_max_off = 20;

		void widen_lobby_max_local_players()
		{
			if (!gamepad_models_widened)
			{
				return;   // no stock join for controller 2 - keep the stock range
			}
			auto* p = reinterpret_cast<uint8_t*>(base() + lobby_max_local_reg_rva);
			if (!readable(p, sizeof(lobby_max_local_reg_expected))
				|| std::memcmp(p, lobby_max_local_reg_expected, sizeof(lobby_max_local_reg_expected)) != 0)
			{
				note("[splitscreen] lobby_maxLocalPlayers: stock, bytes differ at 0x%08X\n", lobby_max_local_reg_rva);
				return;
			}
			const uint8_t four = 0x04;   // PS4's own maximum
			write_bytes(p + lobby_max_local_max_off, &four, 1);
		}
