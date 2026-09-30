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

		// ---- Batch 14: LiveStats stat-change cache - no longer relocated ----
		// s_cachedStatsChanges is [4] on PS4 and [2] on the PC; controller 2 overwrote the
		// statics behind it (crash in Cmd_RemoveCommand at game over). SetStatChanged and the
		// cache reset are re-implemented over four slots in 15_stats_cache.inl.

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

		// ---- Batch 19: player-name drawing, the statics of cg_draw_names.cpp -----
		// PS4 keeps them [4]; the PC has [2] of each, packed in front of the global name list
		// drawNameEntities (0x049482C0): playerDetails [2][18] x 0x68, actorOverheadFade [2][64],
		// centOverheadFade [2][32] x 0x50, overheadFade [2][18], s_friendlyHeadTrace [2][18],
		// s_friendlyActorHeadTrace [2][64]. Slot 2 of each is the next array; slot 2 of the actor
		// head traces is the name list itself, so a head trace of player 3 turned a list entry into
		// {time, 1} and CG_DrawNames read entity 0x22FDC (4p MP crash at 0x0068293D). The zero
		// state is the reset state. Each reset memset covers two slots and is widened to four.
		bool widen_name_reset(const perclient_array& a, const size_t fresh)
		{
			const auto b = base();
			for (const auto& r : name_resets)
			{
				if (r.array_base != a.base)
				{
					continue;
				}
				int32_t disp = 0;
				std::memcpy(&disp, reinterpret_cast<const uint8_t*>(b + r.lea_rva) + 3, sizeof(disp));
				auto* len = reinterpret_cast<uint8_t*>(b + r.len_rva);
				if (b + r.lea_rva + 7 + static_cast<ptrdiff_t>(disp) != fresh || !readable(len, sizeof(r.len_bytes))
				    || std::memcmp(len, r.len_bytes, sizeof(r.len_bytes)) != 0)
				{
					note("[splitscreen] %s reset: bytes differ - not widened\n", a.name);
					return false;
				}
				const uint32_t four_slots = 4 * a.stride;
				return write_bytes(len + 2, &four_slots, sizeof(four_slots));
			}
			return false;
		}

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
			// batches 15-18 and the light queue
			{.array = visbits_array,
			 .post = widen_visbits_reset},
			{.array = conmsgbuf_array,
			 .pre = conmsgbuf_refs_match, .post = retarget_conmsgbuf_refs},
			{.array = uiinfo_array,
			 .pre = uiinfo_bound_matches, .post = widen_uiinfo_init},
			{ui3d_windows_array},
			// batch 19: player-name drawing
			{.array = playerdetails_array, .post = widen_name_reset},
			{.array = actoroverheadfade_array, .post = widen_name_reset},
			{.array = centoverheadfade_array, .post = widen_name_reset},
			{.array = overheadfade_array, .post = widen_name_reset},
			{.array = friendlyheadtrace_array, .post = widen_name_reset},
			{.array = friendlyactorheadtrace_array, .post = widen_name_reset},
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

		// ---- Lens flares for local clients 2/3: a second manager -----------------
		// PS4 FxLensFlaresManager keeps its per-client state as [4]. The PC class is a later
		// rework with [2]: +0xA058 persistent data, +0xA068 visible lists [2][0x100], +0xB068
		// counts, +0xB070/+0xB080 pool memory - lc 2 hit the members behind them. Clients 2/3
		// get a second manager of their own (the object is reached only through its 15
		// `lea rcx` sites, and its methods only through them): each entry point that takes lc
		// in edx continues with (second, lc - 2), and so does the backend buffer update, which
		// reads lc from the view. Its sources keep the accumulation range of their real lc
		// (lc * 0x300, PS4 GetAccumBuffersIndex), for which both accumulation buffers grow from
		// 2 x 0x300 to 4 x 0x300 entries before InitSharedResources creates them. The shared
		// members (+0xA000..+0xA050, query materials and mesh) are used only by
		// InitSharedResources, Shutdown() and KickOffVisibilityQueries, which keep the primary.
		// Locks follow the remapped lc (0x76/0x7B/0x80 + lc; five exist per family).
		uint8_t* lensflare_second = nullptr;
		bool lensflare_routed = false;

		void lensflare_route_entry(midhook::context& c)
		{
			const auto lc = static_cast<int32_t>(c.rdx);
			if (c.rcx == base() + lensflare_manager_rva && (lc == 2 || lc == 3))
			{
				c.rcx = reinterpret_cast<uint64_t>(lensflare_second);
				c.rdx = static_cast<uint32_t>(lc - 2);
			}
		}

		// after `movsxd r15, [view+0x398]`, before `mov rbp, rcx`
		void lensflare_route_view(midhook::context& c)
		{
			const auto lc = static_cast<int64_t>(c.r15);
			if (c.rcx == base() + lensflare_manager_rva && (lc == 2 || lc == 3))
			{
				c.rcx = reinterpret_cast<uint64_t>(lensflare_second);
				c.r15 = static_cast<uint64_t>(lc - 2);
			}
		}

		// ebp = lc * 0x300 with lc already remapped, rdi = persistent data of (this, lc)
		void lensflare_accum_index(midhook::context& c)
		{
			const auto lc = c.r14;
			const auto* persistent = reinterpret_cast<const uint64_t*>(lensflare_second + lensflare_persistent_off);
			if (lc < 2 && c.rdi && persistent[lc] == c.rdi)
			{
				c.rbp += 2 * 0x300;
			}
		}

		// All or nothing; false leaves every byte stock (the caller then gates instead).
		bool route_lensflares_for_extra_clients()
		{
			if (lensflare_routed)
			{
				return true;
			}
			const auto b = base();
			const auto stock = [b](const uint32_t rva, const uint8_t* bytes, const size_t n) {
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, bytes, n) == 0;
			};
			bool ok = stock(lensflare_view_lc_rva, lensflare_view_lc_bytes, sizeof(lensflare_view_lc_bytes))
				&& stock(lensflare_accum_premise_rva, lensflare_accum_premise, sizeof(lensflare_accum_premise));
			for (const auto rva : lensflare_accum_count_rvas)
			{
				ok = ok && stock(rva, lensflare_accum_count_stock, sizeof(lensflare_accum_count_stock));
			}
			// the larger counts only take effect if the buffers do not exist yet
			static constexpr uint8_t no_buffer[lensflare_accum_buffer_ptrs] = {};
			for (const auto rva : lensflare_accum_buffer_rvas)
			{
				ok = ok && stock(rva, no_buffer, sizeof(no_buffer));
			}
			if (!ok)
			{
				note("[splitscreen] lensflare routing: premises differ or buffers already created - not routed\n");
				return false;
			}
			if (!lensflare_second)
			{
				// zero-filled, the state of the static manager before Init
				lensflare_second = static_cast<uint8_t*>(allocate_near_module(lensflare_manager_size));
				if (!lensflare_second)
				{
					return false;
				}
			}

			struct write { uint32_t rva; const uint8_t* stock; uint8_t patch[16]; size_t n; };
			write writes[std::size(lensflare_lc_entries) + 2 + std::size(lensflare_accum_count_rvas)]{};
			size_t count = 0;
			for (const auto& e : lensflare_lc_entries)
			{
				auto& w = writes[count++];
				w = {e.rva, e.stock, {}, e.len};
				if (!midhook::prepare(b + e.rva, e.stock, e.len, &lensflare_route_entry, &allocate_near_module, w.patch))
				{
					note("[splitscreen] lensflare routing: 0x%08X not prepared - not routed\n", e.rva);
					return false;
				}
			}
			const struct { uint32_t rva; const uint8_t* stock; size_t n; midhook::callback fn; } mids[] = {
				{lensflare_view_route_rva, lensflare_view_route_stock, sizeof(lensflare_view_route_stock), &lensflare_route_view},
				{lensflare_accum_rva, lensflare_accum_stock, sizeof(lensflare_accum_stock), &lensflare_accum_index},
			};
			for (const auto& m : mids)
			{
				auto& w = writes[count++];
				w = {m.rva, m.stock, {}, m.n};
				if (!midhook::prepare(b + m.rva, m.stock, m.n, m.fn, &allocate_near_module, w.patch))
				{
					note("[splitscreen] lensflare routing: 0x%08X not prepared - not routed\n", m.rva);
					return false;
				}
			}
			for (const auto rva : lensflare_accum_count_rvas)
			{
				auto& w = writes[count++];
				w = {rva, lensflare_accum_count_stock, {}, sizeof(lensflare_accum_count_new)};
				std::memcpy(w.patch, lensflare_accum_count_new, sizeof(lensflare_accum_count_new));
			}
			for (size_t i = 0; i < count; ++i)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + writes[i].rva), writes[i].patch, writes[i].n))
				{
					while (i-- > 0)
					{
						write_bytes(reinterpret_cast<void*>(b + writes[i].rva), writes[i].stock, writes[i].n);
					}
					note("[splitscreen] lensflare routing: write failed - all restored\n");
					return false;
				}
			}
			lensflare_routed = true;
			note("[splitscreen] lensflare routing: second manager %p, %zu sites\n",
			     static_cast<void*>(lensflare_second), count);
			return true;
		}

		// ---- Lens flares: off for local clients >= 2 (fallback) -------------------
		// Used when the routing above cannot be installed. Each entry point that takes lc
		// in edx returns at once for lc >= 2 (SpawnInstance with -1, its own failure value)
		// and runs the stock function otherwise. Arguments pass through as raw 64-bit
		// values; the counts are each function's live-in registers and its caller's stack
		// stores (render: 4 + 7). None of the five has an Arxan caller check.
		bool lensflare_gated = false;
		utils::hook::detour lensflare_hooks[std::size(lensflare_gates)];

		bool lensflare_extra_client(const uint64_t lc)
		{
			return static_cast<int32_t>(lc) >= 2;   // as the game's `cmp edx, 2`
		}

		void lensflare_pool_setup_stub(const uint64_t m, const uint64_t lc)
		{
			if (!lensflare_extra_client(lc))
			{
				lensflare_hooks[0].invoke<void>(m, lc);
			}
		}

		void lensflare_set_persistent_stub(const uint64_t m, const uint64_t lc, const uint64_t data,
		                                   const uint64_t size)
		{
			if (!lensflare_extra_client(lc))
			{
				lensflare_hooks[1].invoke<void>(m, lc, data, size);
			}
		}

		void lensflare_update_stub(const uint64_t m, const uint64_t lc)
		{
			if (!lensflare_extra_client(lc))
			{
				lensflare_hooks[2].invoke<void>(m, lc);
			}
		}

		uint64_t lensflare_spawn_stub(const uint64_t m, const uint64_t lc, const uint64_t params,
		                              const uint64_t a4)
		{
			if (lensflare_extra_client(lc))
			{
				return 0xFFFFFFFF;   // eax = -1
			}
			return lensflare_hooks[3].invoke<uint64_t>(m, lc, params, a4);
		}

		void lensflare_render_stub(const uint64_t m, const uint64_t lc, const uint64_t a3, const uint64_t a4,
		                           const uint64_t a5, const uint64_t a6, const uint64_t a7, const uint64_t a8,
		                           const uint64_t a9, const uint64_t a10, const uint64_t a11)
		{
			if (!lensflare_extra_client(lc))
			{
				lensflare_hooks[4].invoke<void>(m, lc, a3, a4, a5, a6, a7, a8, a9, a10, a11);
			}
		}

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
			void* const stubs[] = {
				reinterpret_cast<void*>(&lensflare_pool_setup_stub), reinterpret_cast<void*>(&lensflare_set_persistent_stub),
				reinterpret_cast<void*>(&lensflare_update_stub), reinterpret_cast<void*>(&lensflare_spawn_stub),
				reinterpret_cast<void*>(&lensflare_render_stub),
			};
			static_assert(std::size(stubs) == std::size(lensflare_gates));
			for (size_t i = 0; i < std::size(lensflare_gates); ++i)
			{
				lensflare_hooks[i].create(b + lensflare_gates[i].rva, stubs[i]);
			}
			lensflare_gated = true;
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

		// Com_LocalClient_LastInput_Init runs once, in Com_Init. Before its body, create the
		// persistent model roots "controller2" / "controller3" with the game's own
		// UI_Model_CreatePersistentModelFromPath - PS4 UI_Model_Init does this for all four
		// controllers, the PC for two. Neither function has an Arxan caller check.
		utils::hook::detour lastinput_init_hook;

		void lastinput_init_stub()
		{
			const auto b = base();
			auto* slots = reinterpret_cast<uint16_t*>(b + ui_controller_model_rva);
			const auto* global = reinterpret_cast<const volatile uint16_t*>(b + ui_global_model_rva);
			const auto create = reinterpret_cast<uint16_t (*)(uint16_t parent, const char* path)>(
				b + ui_create_persistent_rva);
			static constexpr const char* names[] = {"controller2", "controller3"};
			for (int slot = 2; slot <= 3; ++slot)
			{
				const uint16_t parent = *global;
				if (slots[slot] == 0 && parent != 0)
				{
					slots[slot] = create(parent, names[slot - 2]);
				}
			}
			lastinput_init_hook.invoke<void>();
		}

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

			controller_models_hooked = hook_if_stock(lastinput_init_hook, lastinput_init_rva,
			                                         lastinput_init_expected, lastinput_init_stub);
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
