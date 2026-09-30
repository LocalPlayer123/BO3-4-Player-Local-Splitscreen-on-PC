// Relocations, part A: completion fixes, per-client array engine (relocate_perclient), batches 1-11
// (sites and steps; the order is perclient_rows in 10_relocations_b.inl).
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ============ Completion of earlier relocations ============
		// Sites that tools/audit_reloc_tables.py found still pointing at the old
		// array of playersKb (key-state bytes) and s_gamePads while the engine
		// used the moved one (data/reloc_sites/completion_2026-09-28.txt).
		// Not completed: numdestructibles' hits are the previous array's end
		// markers (rewriting them crashed at boot); read the loop before
		// rewriting a "still targets the old base" hit.

		// The array's current base: a moved site's target minus its field offset.
		// 0 if the site still points at the old array (then complete nothing).
		size_t moved_base_from_site(const entcoll_site& s, const uint32_t old_base)
		{
			const auto b = base();
			const auto* insn = reinterpret_cast<const uint8_t*>(b + s.rva);
			if (!readable(insn, s.insn_len))
			{
				return 0;
			}
			int32_t disp = 0;
			std::memcpy(&disp, insn + s.disp_off, sizeof(disp));
			const auto target = s.rip ? static_cast<int64_t>(b + s.rva + s.insn_len) + disp
			                          : static_cast<int64_t>(b) + static_cast<uint32_t>(disp);
			const auto moved = static_cast<size_t>(target) - s.target_off;
			return moved == b + old_base ? 0 : moved;
		}

		void complete_relocation(const char* name, const entcoll_site& moved_site,
		                         const uint32_t old_base, const entcoll_site* sites,
		                         const size_t count, int32_t* saved, bool& done)
		{
			if (done)
			{
				return;
			}
			const auto fresh = moved_base_from_site(moved_site, old_base);
			if (!fresh)
			{
				note("[splitscreen] %s completion: base not moved - skipped\n", name);
			}
			else if (!rewrite_entcoll(sites, count, old_base, fresh, saved))
			{
				note("[splitscreen] %s completion: a site did not match - NOTHING written\n", name);
			}
			else
			{
				done = true;
			}
		}

		bool players_kb_completed = false;
		bool gamepads_completed = false;

		// Probe site the playersKb relocation always rewrites (IN_Attack_Up's
		// kbutton read). s_gamePads uses its own verified destination instead.

		void complete_players_kb()
		{
			static int32_t saved[std::size(players_kb_completion_sites)]{};
			complete_relocation("playersKb", players_kb_probe, players_kb_base_rva,
			                    players_kb_completion_sites, std::size(players_kb_completion_sites),
			                    saved, players_kb_completed);
		}

		void complete_gamepads(const size_t destination_abs)
		{
			if (gamepads_completed)
			{
				return;
			}
			static int32_t saved[std::size(gamepads_completion_sites)]{};
			if (rewrite_entcoll(gamepads_completion_sites, std::size(gamepads_completion_sites),
			                    gamepads_reloc_table.base_rva, destination_abs, saved))
			{
				gamepads_completed = true;
			}
			else
			{
				note("[splitscreen] s_gamePads completion: a site did not match - NOTHING written\n");
			}
		}

		// Retarget one RIP-relative loop end marker (`lea reg,[rip+d32]`), which site
		// tables cannot hold because it points past the array (tools/endmarker_scan.py).
		// Writes nothing and returns false unless it currently targets old_target_rva.
		bool retarget_end_marker(const uint32_t insn_rva, const uint8_t disp_off, const uint8_t insn_len,
		                         const uint32_t old_target_rva, const size_t new_target_abs)
		{
			const auto b = base();
			auto* disp_at = reinterpret_cast<uint8_t*>(b + insn_rva + disp_off);
			int32_t cur = 0;
			if (!readable(disp_at, sizeof(cur)))
			{
				return false;
			}
			std::memcpy(&cur, disp_at, sizeof(cur));
			const auto end = static_cast<int64_t>(b + insn_rva + insn_len);
			if (end + cur != static_cast<int64_t>(b + old_target_rva))
			{
				note("[splitscreen] end marker 0x%08X does not point at 0x%08X - untouched\n",
				     insn_rva, old_target_rva);
				return false;
			}
			const auto delta = static_cast<int64_t>(new_target_abs) - end;
			if (delta > INT32_MAX || delta < INT32_MIN)
			{
				return false;
			}
			const auto d32 = static_cast<int32_t>(delta);
			return write_bytes(disp_at, &d32, sizeof(d32));
		}

		bool relocate_entity_collision()
		{
			if (entcoll_relocated)
			{
				return true;
			}
			const auto b = base();

			auto* world = static_cast<uint8_t*>(
				allocate_near_module(entcoll_slots * entcoll_world_stride));
			auto* nodes = static_cast<uint8_t*>(
				allocate_near_module(entcoll_slots * sizeof(void*)));
			if (!world || !nodes)
			{
				return false;
			}
			std::memset(world, 0, entcoll_slots * entcoll_world_stride);
			std::memset(nodes, 0, entcoll_slots * sizeof(void*));

			// carry the two live elements across before any code reads them
			std::memcpy(world, reinterpret_cast<const void*>(b + entcoll_world_base),
			            2 * entcoll_world_stride);
			std::memcpy(nodes, reinterpret_cast<const void*>(b + entcoll_nodes_base),
			            2 * sizeof(void*));

			// Node storage for slots 2/3, which the engine never allocates.
			// CG_ClearEntityCollWorld clears it; it only has to exist.
			auto** node_ptrs = reinterpret_cast<void**>(nodes);
			for (size_t lc = 2; lc < entcoll_slots; ++lc)
			{
				auto* buf = VirtualAlloc(nullptr, entcoll_node_bytes,
				                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				if (!buf)
				{
					return false;
				}
				node_ptrs[lc] = buf;
			}

			static int32_t saved_world[std::size(entcoll_world_sites)]{};
			static int32_t saved_nodes[std::size(entcoll_node_sites)]{};

			if (!rewrite_entcoll(entcoll_world_sites, std::size(entcoll_world_sites),
			                     entcoll_world_base, reinterpret_cast<size_t>(world), saved_world))
			{
				return false;
			}
			if (!rewrite_entcoll(entcoll_node_sites, std::size(entcoll_node_sites),
			                     entcoll_nodes_base, reinterpret_cast<size_t>(nodes), saved_nodes))
			{
				// undo the world group too - either both move or neither does
				for (size_t j = 0; j < std::size(entcoll_world_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(
						b + entcoll_world_sites[j].rva);
					write_bytes(insn + entcoll_world_sites[j].disp_off,
					            &saved_world[j], sizeof(int32_t));
				}
				return false;
			}

			entcoll_relocated = true;
			return true;
		}

		constexpr size_t cf_new_bytes = 0x40010;   // 4 * 0x10000 + 4 * 4
		bool cf_relocated = false;

		bool relocate_clientfield_callbacks()
		{
			if (cf_relocated)
			{
				return true;
			}
			const auto b = base();

			// every site must still be stock before anything is written
			for (const auto& s : cf_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				uint32_t cur = 0;
				if (!readable(at, sizeof(cur)))
				{
					return false;
				}
				std::memcpy(&cur, at, sizeof(cur));
				if (cur != s.was)
				{
					note("[splitscreen] clientfield imm 0x%08X reads 0x%X, want 0x%X"
					     " - not patching\n", s.rva, cur, s.was);
					return false;
				}
			}
			int32_t saved_lea[std::size(cf_lea_sites)]{};
			for (size_t i = 0; i < std::size(cf_lea_sites); ++i)
			{
				const auto* insn = reinterpret_cast<const uint8_t*>(b + cf_lea_sites[i]);
				if (!readable(insn, 7) || insn[1] != 0x8D)
				{
					return false;
				}
				std::memcpy(&saved_lea[i], insn + 3, sizeof(int32_t));
				const auto tgt = static_cast<size_t>(cf_lea_sites[i] + 7 + saved_lea[i]);
				if (tgt != cf_buffer_rva)
				{
					note("[splitscreen] clientfield lea 0x%08X -> 0x%zX, not the buffer\n",
					     cf_lea_sites[i], tgt);
					return false;
				}
			}

			auto* fresh = static_cast<uint8_t*>(allocate_near_module(cf_new_bytes));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, cf_new_bytes);
			// Carry the two live client regions and their counts across, in
			// case the engine's init already ran.
			std::memcpy(fresh, reinterpret_cast<const void*>(b + cf_buffer_rva), 0x20000);
			std::memcpy(fresh + 0x40000,
			            reinterpret_cast<const void*>(b + cf_buffer_rva + 0x20000), 8);

			size_t done_lea = 0, done_imm = 0;
			const auto rollback = [&]
			{
				for (size_t j = 0; j < done_lea; ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + cf_lea_sites[j]);
					write_bytes(insn + 3, &saved_lea[j], sizeof(int32_t));
				}
				for (size_t j = 0; j < done_imm; ++j)
				{
					auto* at = reinterpret_cast<uint8_t*>(b + cf_imms[j].rva + cf_imms[j].off);
					write_bytes(at, &cf_imms[j].was, sizeof(uint32_t));
				}
			};

			for (size_t i = 0; i < std::size(cf_lea_sites); ++i)
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + cf_lea_sites[i]);
				const auto end = static_cast<int64_t>(b + cf_lea_sites[i] + 7);
				const auto delta = static_cast<int64_t>(reinterpret_cast<size_t>(fresh)) - end;
				if (delta > INT32_MAX || delta < INT32_MIN)
				{
					rollback();
					return false;
				}
				const auto d32 = static_cast<int32_t>(delta);
				if (!write_bytes(insn + 3, &d32, sizeof(d32)))
				{
					rollback();
					return false;
				}
				++done_lea;
			}
			for (const auto& s : cf_imms)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva + s.off);
				if (!write_bytes(at, &s.want, sizeof(s.want)))
				{
					rollback();
					return false;
				}
				++done_imm;
			}

			// Also update the pointer the consumers read, in case the engine's
			// init already stored the old buffer there.
			auto* ptr = reinterpret_cast<void**>(b + cf_pointer_rva);
			if (readable(ptr, sizeof(void*)))
			{
				void* v = fresh;
				write_bytes(ptr, &v, sizeof(v));
			}

			cf_relocated = true;
			return true;
		}

		// clientObjMap: per-client DObj handle table, PC [2][0x702] -> PS4 layout
		// [4][0x704] (PS4 0x101D3470; viewmodel handle = 0x700 + lc). Client 2 hit
		// MSVC's range check (fail-fast 0xC0000409, so no crash dialog), and with
		// 0x702-word rows lc 2's viewmodel aliased lc 3's entity 0. The table cannot
		// grow in place, so it moves, and every row-dependent constant changes with
		// it: stride, row count, byte size 0x1C08 -> 0x3820 (memset and /GS bound)
		// and the client count 2 -> 4 of the PC-only free-all / rebuild-all pair.
		constexpr uint32_t entword_old_row = 0x702;             // PC handles per client
		constexpr uint32_t entword_row = 0x704;                 // PS4 handles per client
		constexpr uint32_t entword_client_bytes = entword_row * 2;   // 0xE08
		constexpr size_t entword_slots = 4;
		bool entword_relocated = false;

		bool entword_imm_reads(const entword_imm& s, uint32_t want)
		{
			const auto* at = reinterpret_cast<const uint8_t*>(base() + s.rva + s.off);
			uint32_t cur = 0;
			if (!readable(at, s.size))
			{
				return false;
			}
			std::memcpy(&cur, at, s.size);
			return cur == want;
		}

		bool relocate_entword_table()
		{
			if (entword_relocated)
			{
				return true;
			}
			const auto b = base();
			for (const auto& s : entword_imms)
			{
				if (!entword_imm_reads(s, s.was))
				{
					note("[splitscreen] entword imm 0x%08X is not 0x%X - not patching\n", s.rva, s.was);
					return false;
				}
			}

			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(entword_slots * entword_client_bytes));
			if (!fresh)
			{
				note("[splitscreen] clientObjMap: NOT moved - allocation failed\n");
				return false;
			}
			// Re-stride the two stock rows: old row r (0x702 words) -> new row r
			// (0x704 words). Handles 0x702/0x703 of a row start empty.
			std::memset(fresh, 0, entword_slots * entword_client_bytes);
			for (uint32_t r = 0; r < 2; ++r)
			{
				std::memcpy(fresh + r * entword_client_bytes,
				            reinterpret_cast<const void*>(b + entword_base + r * entword_old_row * 2),
				            entword_old_row * 2);
			}

			static int32_t saved[std::size(entword_sites)]{};
			if (!rewrite_entcoll(entword_sites, std::size(entword_sites),
			                     entword_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			size_t done = 0;
			for (const auto& s : entword_imms)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva + s.off);
				if (!write_bytes(at, &s.want, s.size))
				{
					for (size_t j = 0; j < done; ++j)
					{
						auto* back = reinterpret_cast<uint8_t*>(
							b + entword_imms[j].rva + entword_imms[j].off);
						write_bytes(back, &entword_imms[j].was, entword_imms[j].size);
					}
					for (size_t j = 0; j < std::size(entword_sites); ++j)
					{
						auto* insn = reinterpret_cast<uint8_t*>(b + entword_sites[j].rva);
						write_bytes(insn + entword_sites[j].disp_off, &saved[j], sizeof(int32_t));
					}
					note("[splitscreen] clientObjMap: NOT moved - an immediate write failed (rolled back)\n");
					return false;
				}
				++done;
			}
			entword_relocated = true;
			return true;
		}

		// s_exposureAdaptions [3] -> [5]: one auto-exposure buffer per local client
		// plus the extra cam, as on PS4 (0xAE89550; RB_FxBloomLDRColorGrade picks
		// `isExtraCam ? 4 : localClientNum`). The PC picked `extraCam ? 2 : lc`, so
		// player 3 shared the extra cam's buffer and player 4 read
		// exposureOutputBuffer (pane 4 overexposed). The slots after [3] are
		// foreign, so the array moves and the selector's 30 bytes are rewritten to
		// PS4's rule. Runs at post_unpack, before R_InitLightingData.
		constexpr uint32_t exposure_new_count = 5;
		constexpr uint32_t exposure_texture_off = 0x108;
		constexpr uint32_t exposure_select_lea_off = 19;   // lea rsi,[rip+d] inside the patch
		size_t exposure_new = 0;

		bool relocate_exposure_adaptions()
		{
			if (exposure_new)
			{
				return true;
			}
			const auto b = base();
			auto* select = reinterpret_cast<uint8_t*>(b + exposure_select_rva);
			if (!readable(select, sizeof(exposure_select_stock))
				|| std::memcmp(select, exposure_select_stock, sizeof(exposure_select_stock)) != 0)
			{
				note("[splitscreen] exposure adaptions: NOT moved - selector bytes differ at 0x%08X\n",
				     exposure_select_rva);
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(exposure_new_count * exposure_stride));
			if (!fresh)
			{
				note("[splitscreen] exposure adaptions: NOT moved - allocation failed\n");
				return false;
			}
			std::memset(fresh, 0, exposure_new_count * exposure_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + exposure_base),
			            exposure_old_count * exposure_stride);
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			// New selector bytes; the lea is rip-relative to its own end.
			uint8_t patch[sizeof(exposure_select_stock)] = {
				0xB8, 0x04, 0x00, 0x00, 0x00,                   // mov eax,4 (extra cam = MAX_LOCAL_CLIENTS)
				0x75, 0x06,                                     // jne +6
				0x8B, 0x82, 0x98, 0x03, 0x00, 0x00,             // mov eax,[rdx+0x398] (localClientNum)
				0x69, 0xC0, 0x10, 0x01, 0x00, 0x00,             // imul eax,eax,0x110
				0x48, 0x8D, 0x35, 0x00, 0x00, 0x00, 0x00,       // lea rsi,[rip+d] -> new+0x108
				0x48, 0x01, 0xC6,                               // add rsi,rax
				0x90,                                           // nop
			};
			const auto lea_end = static_cast<int64_t>(b + exposure_select_rva + exposure_select_lea_off + 7);
			const auto lea_disp = static_cast<int64_t>(fresh_abs + exposure_texture_off) - lea_end;
			if (lea_disp < INT32_MIN || lea_disp > INT32_MAX)
			{
				note("[splitscreen] exposure adaptions: NOT moved - new block out of rip range\n");
				return false;
			}
			const auto d32 = static_cast<int32_t>(lea_disp);
			std::memcpy(patch + exposure_select_lea_off + 3, &d32, sizeof(d32));

			static int32_t saved_base[std::size(exposure_base_sites)]{};
			static int32_t saved_fill[std::size(exposure_fill_end_site)]{};
			static int32_t saved_all[std::size(exposure_all_end_sites)]{};
			if (!rewrite_entcoll(exposure_base_sites, std::size(exposure_base_sites),
			                     exposure_base, fresh_abs, saved_base))
			{
				note("[splitscreen] exposure adaptions: NOT moved - a base lea did not match\n");
				return false;
			}
			const auto undo_base = [&]
			{
				for (size_t j = 0; j < std::size(exposure_base_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exposure_base_sites[j].rva);
					write_bytes(insn + exposure_base_sites[j].disp_off, &saved_base[j], sizeof(int32_t));
				}
			};
			if (!rewrite_entcoll(exposure_fill_end_site, std::size(exposure_fill_end_site),
			                     exposure_base, fresh_abs, saved_fill))
			{
				undo_base();
				note("[splitscreen] exposure adaptions: NOT moved - the fill end lea did not match\n");
				return false;
			}
			const auto undo_fill = [&]
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + exposure_fill_end_site[0].rva);
				write_bytes(insn + exposure_fill_end_site[0].disp_off, &saved_fill[0], sizeof(int32_t));
			};
			// target = new_abs + target_off(3*0x110): passing new + 2*0x110 lands on new + 5*0x110.
			if (!rewrite_entcoll(exposure_all_end_sites, std::size(exposure_all_end_sites), exposure_base,
			                     fresh_abs + (exposure_new_count - exposure_old_count) * exposure_stride,
			                     saved_all))
			{
				undo_fill();
				undo_base();
				note("[splitscreen] exposure adaptions: NOT moved - a create/free end lea did not match\n");
				return false;
			}
			if (!write_bytes(select, patch, sizeof(patch)))
			{
				for (size_t j = 0; j < std::size(exposure_all_end_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exposure_all_end_sites[j].rva);
					write_bytes(insn + exposure_all_end_sites[j].disp_off, &saved_all[j], sizeof(int32_t));
				}
				undo_fill();
				undo_base();
				note("[splitscreen] exposure adaptions: NOT moved - selector write failed (rolled back)\n");
				return false;
			}
			exposure_new = fresh_abs;
			return true;
		}

		// UI model node pool 0x9000 -> 0xFFFF nodes (the node index is a u16).
		// MP ran out of LUI memory at 3-/4-player match start: each player's
		// CustomClassList takes ~11.1k nodes. History: LOG.md, "ROOT CAUSE of the MP LUI".
		// PS4: UI_Model_Init 0xD68140, UI_Model_ResetNode 0xD682E0. PC node is 0x28
		// bytes: +0x1C own index, +0x1E next sibling / next free.
		// The sites include leaf getters without .pdata (missing them made values
		// nil). GetModel's 0x9000 is an end sentinel and stays; the reset loop bound
		// becomes 0xFFFF. Runs at post_unpack, before the hidden UI_Model_Init, which
		// still fills the old array; the new one is pre-built in the reset state,
		// its free list skipping node 0x9000.
		constexpr uint32_t model_pool_stride = 0x28;
		constexpr uint32_t model_pool_new_count = 0xFFFF;
		constexpr uint32_t model_pool_sentinel = 0x9000;
		constexpr uint32_t model_pool_self_off = 0x1C;
		constexpr uint32_t model_pool_next_off = 0x1E;
		// Command buffers for local clients 2/3. MP players 3/4 never spawned: their
		// class choice (a client command, Cbuf_AddText(lc)) was dropped because cbuf
		// records 2/3 were empty, and Com_Frame executed only lc < 2. As PS4
		// Cbuf_Init 0xE2FB20 does, give records 2/3 a 64 KB buffer each (the records
		// are already [4], reloc_tables "cbuf"), then widen Cbuf_Execute's range
		// check (the bytes it guards for 2/3 are padding) and Com_Frame's loop,
		// both 2 -> 4. Runs at post_unpack, before Cbuf_Init; all or nothing.
		constexpr uint32_t cbuf_text_size = 0x10000;
		constexpr size_t cbuf_record_stride = 0x10;

		bool install_cbuf_for_players34()
		{
			const auto b = base();
			const auto* lea = reinterpret_cast<const uint8_t*>(b + cbuf_exec_lea_rva);
			if (!readable(lea, 7) || std::memcmp(lea, cbuf_exec_lea_head, sizeof(cbuf_exec_lea_head)) != 0)
			{
				note("[splitscreen] command buffers 2/3: NOT installed - Cbuf_ExecuteInternal lea differs\n");
				return false;
			}
			int32_t disp = 0;
			std::memcpy(&disp, lea + 3, sizeof(disp));
			auto* records = reinterpret_cast<uint8_t*>(b + cbuf_exec_lea_rva + 7 + static_cast<int64_t>(disp));
			if (records == reinterpret_cast<uint8_t*>(b + cbuf_old_records_rva))
			{
				note("[splitscreen] command buffers 2/3: NOT installed - the cbuf records were not relocated\n");
				return false;
			}
			if (!readable(records, 4 * cbuf_record_stride))
			{
				note("[splitscreen] command buffers 2/3: NOT installed - records unreadable\n");
				return false;
			}
			for (size_t i = 2 * cbuf_record_stride; i < 4 * cbuf_record_stride; ++i)
			{
				if (records[i] != 0)
				{
					note("[splitscreen] command buffers 2/3: NOT installed - records 2/3 are not empty\n");
					return false;
				}
			}
			auto* range = reinterpret_cast<uint8_t*>(b + cbuf_range_check_rva);
			auto* bound = reinterpret_cast<uint8_t*>(b + cbuf_frame_bound_rva);
			if (!readable(range, sizeof(cbuf_range_check_stock))
				|| std::memcmp(range, cbuf_range_check_stock, sizeof(cbuf_range_check_stock)) != 0
				|| !readable(bound, sizeof(cbuf_frame_bound_stock))
				|| std::memcmp(bound, cbuf_frame_bound_stock, sizeof(cbuf_frame_bound_stock)) != 0)
			{
				note("[splitscreen] command buffers 2/3: NOT installed - range check or Com_Frame bound bytes differ\n");
				return false;
			}
			auto* text = static_cast<uint8_t*>(
				VirtualAlloc(nullptr, 2 * cbuf_text_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
			if (!text)
			{
				note("[splitscreen] command buffers 2/3: NOT installed - allocation failed\n");
				return false;
			}
			for (size_t lc = 2; lc < 4; ++lc)
			{
				auto* rec = records + lc * cbuf_record_stride;
				auto* data = text + (lc - 2) * cbuf_text_size;
				const int32_t maxsize = static_cast<int32_t>(cbuf_text_size);
				const int32_t cursize = 0;
				std::memcpy(rec + 0x0, &data, sizeof(data));
				std::memcpy(rec + 0x8, &maxsize, sizeof(maxsize));
				std::memcpy(rec + 0xC, &cursize, sizeof(cursize));
			}
			const uint8_t four = 0x04, two = 0x02;
			if (!write_bytes(range + 3, &four, 1))
			{
				std::memset(records + 2 * cbuf_record_stride, 0, 2 * cbuf_record_stride);
				note("[splitscreen] command buffers 2/3: NOT installed - range check write failed\n");
				return false;
			}
			if (!write_bytes(bound + 2, &four, 1))
			{
				write_bytes(range + 3, &two, 1);
				std::memset(records + 2 * cbuf_record_stride, 0, 2 * cbuf_record_stride);
				note("[splitscreen] command buffers 2/3: NOT installed - Com_Frame bound write failed (rolled back)\n");
				return false;
			}
			cbuf_range_resting = 0x04;
			return true;
		}

		// Lobby join clients [2] -> [4] and LobbyMsgTransport_Update 2 -> 4. With
		// 3-4 players seated, "Failed to host lobby": the party join waits for every
		// member to agree, and controllers 2/3 were never polled (PS4 0xCD2100 loops
		// c < 4). The agreement request handler indexes s_joinClient (PC [2] x
		// 0xB0, PS4 [4] x 0xB8), whose slot 2 is foreign, so it moves first (9 sites
		// + 2 end markers). Slots 2/3 start as copies of slot 1 with state (+0) 0 and
		// controller index (+0xAC) 2/3.
		constexpr uint32_t joinclient_stride = 0xB0;
		constexpr uint32_t joinclient_old_count = 2;
		constexpr uint32_t joinclient_new_count = 4;
		constexpr uint32_t joinclient_ci_off = 0xAC;
		size_t joinclient_new = 0;

		bool relocate_join_clients()
		{
			if (joinclient_new)
			{
				return true;
			}
			const auto b = base();
			auto* bound = reinterpret_cast<uint8_t*>(b + lobbymsg_bound_rva);
			if (!readable(bound, sizeof(lobbymsg_bound_stock))
				|| std::memcmp(bound, lobbymsg_bound_stock, sizeof(lobbymsg_bound_stock)) != 0)
			{
				note("[splitscreen] lobby join clients: NOT moved - LobbyMsgTransport_Update bytes differ\n");
				return false;
			}
			// The widened loop reads controllers 2/3 through the netchan table, so
			// it must already be the relocated [4] one (reloc_tables "netchan").
			const auto* nlea = reinterpret_cast<const uint8_t*>(b + netchan_get_lea_rva);
			int32_t nd = 0;
			if (!readable(nlea, 7))
			{
				note("[splitscreen] lobby join clients: NOT moved - netchan lea unreadable\n");
				return false;
			}
			std::memcpy(&nd, nlea + 3, sizeof(nd));
			if (netchan_get_lea_rva + 7 + static_cast<int64_t>(nd) == netchan_old_base)
			{
				note("[splitscreen] lobby join clients: NOT moved - the netchan table is still [2]\n");
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(joinclient_new_count * joinclient_stride));
			if (!fresh)
			{
				note("[splitscreen] lobby join clients: NOT moved - allocation failed\n");
				return false;
			}
			const auto* old = reinterpret_cast<const uint8_t*>(b + joinclient_base);
			std::memcpy(fresh, old, joinclient_old_count * joinclient_stride);
			for (uint32_t s = joinclient_old_count; s < joinclient_new_count; ++s)
			{
				auto* slot = fresh + s * joinclient_stride;
				std::memcpy(slot, old + joinclient_stride, joinclient_stride);   // like slot 1
				const int32_t idle = 0, ci = static_cast<int32_t>(s);
				std::memcpy(slot + 0, &idle, sizeof(idle));
				std::memcpy(slot + joinclient_ci_off, &ci, sizeof(ci));
			}
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);
			static int32_t saved[std::size(joinclient_sites)]{};
			static int32_t saved_end[std::size(joinclient_end_sites)]{};
			if (!rewrite_entcoll(joinclient_sites, std::size(joinclient_sites), joinclient_base, fresh_abs, saved))
			{
				note("[splitscreen] lobby join clients: NOT moved - a reference did not match\n");
				return false;
			}
			// end markers: target_off is 2*stride past their start; the new end is 4*stride
			if (!rewrite_entcoll(joinclient_end_sites, std::size(joinclient_end_sites), joinclient_base,
			                     fresh_abs + (joinclient_new_count - joinclient_old_count) * joinclient_stride, saved_end))
			{
				for (size_t j = 0; j < std::size(joinclient_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + joinclient_sites[j].rva);
					write_bytes(insn + joinclient_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				note("[splitscreen] lobby join clients: NOT moved - an end marker did not match (rolled back)\n");
				return false;
			}
			const uint8_t four = 0x04;
			if (!write_bytes(bound + 2, &four, 1))
			{
				note("[splitscreen] lobby join clients [2] -> [4] moved, but the message loop widen FAILED\n");
				joinclient_new = fresh_abs;
				return false;
			}
			joinclient_new = fresh_abs;
			return true;
		}

		bool widen_lua_controller_checks()
		{
			const auto b = base();
			for (const auto& p : lua_ctrl_checks)
			{
				const auto* site = reinterpret_cast<const uint8_t*>(b + p.rva);
				if (!readable(site, sizeof(p.stock)) || std::memcmp(site, p.stock, sizeof(p.stock)) != 0)
				{
					note("[splitscreen] lua controller checks: NOT widened - bytes differ\n");
					return false;
				}
			}
			uint32_t done = 0;
			for (const auto& p : lua_ctrl_checks)
			{
				const uint8_t three = 0x03;   // ja when controller > 3
				if (write_bytes(reinterpret_cast<void*>(b + p.rva + 2), &three, 1))
				{
					++done;
				}
			}
			if (done != std::size(lua_ctrl_checks))
			{
				note("[splitscreen] lua controller checks: a write FAILED\n");
				return false;
			}
			return true;
		}

		// UI model string hunk "UIModelAllocator" 0xC0000 -> 4 MB (PS4 0x80000). With
		// four class lists Hunk_UserAlloc returned NULL and UI_Model_SetString
		// crashed. The hidden UI_Model_Init creates the hunk through the visible
		// Hunk_UserCreateFromBuffer (PS4 0x10D1C90); this detour swaps in a bigger
		// buffer for exactly that call. Nothing else references the static buffer.
		constexpr size_t model_string_stock_size = 0xC0000;
		constexpr size_t model_string_new_size = 0x400000;
		utils::hook::detour hunk_create_hook;
		void* model_string_buffer = nullptr;

		// "ClientCache_ClientPool" hunk 0x3880 -> 0x7100. Each player centity takes
		// two blocks per local client from it; with four players it ran full and the
		// ET_PLAYER handler did memset(NULL). The PS4 pool is sized for 4 local
		// clients, the PC one for two, so it doubles.
		constexpr size_t client_cache_stock_size = 0x3880;
		constexpr size_t client_cache_new_size = 0x7100;
		void* client_cache_buffer = nullptr;

		void* hunk_create_stub(void* buffer, size_t size, int scheme, int flags, void* arg5,
		                       const char* name, int arg7)
		{
			if (name && size == client_cache_stock_size && std::strcmp(name, "ClientCache_ClientPool") == 0)
			{
				// One buffer per process: a re-created pool reuses it, like the
				// stock static buffer.
				if (!client_cache_buffer)
				{
					client_cache_buffer = VirtualAlloc(nullptr, client_cache_new_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				}
				if (client_cache_buffer)
				{
					buffer = client_cache_buffer;
					size = client_cache_new_size;
				}
				else
				{
					note("[splitscreen] client cache pool: allocation failed - stock 0x3880 kept\n");
				}
			}
			if (!model_string_buffer && name && size == model_string_stock_size
				&& std::strcmp(name, "UIModelAllocator") == 0)
			{
				auto* bigger = VirtualAlloc(nullptr, model_string_new_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				if (bigger)
				{
					model_string_buffer = bigger;
					buffer = bigger;
					size = model_string_new_size;
				}
				else
				{
					note("[splitscreen] ui model string hunk: allocation failed - stock 0xC0000 kept\n");
				}
			}
			return hunk_create_hook.invoke<void*>(buffer, size, scheme, flags, arg5, name, arg7);
		}

		bool install_model_string_hunk()
		{
			const auto* p = reinterpret_cast<const uint8_t*>(base() + hunk_create_rva);
			if (!readable(p, sizeof(hunk_create_prologue))
				|| std::memcmp(p, hunk_create_prologue, sizeof(hunk_create_prologue)) != 0)
			{
				note("[splitscreen] ui model string hunk: NOT hooked - Hunk_UserCreateFromBuffer bytes differ\n");
				return false;
			}
			hunk_create_hook.create(reinterpret_cast<void*>(base() + hunk_create_rva), hunk_create_stub);
			return true;
		}

		size_t model_pool_new = 0;

		bool relocate_ui_model_pool()
		{
			if (model_pool_new)
			{
				return true;
			}
			const auto b = base();
			auto* bound = reinterpret_cast<uint8_t*>(b + model_pool_bound_rva);
			if (!readable(bound, sizeof(model_pool_bound_stock))
				|| std::memcmp(bound, model_pool_bound_stock, sizeof(model_pool_bound_stock)) != 0)
			{
				note("[splitscreen] ui model pool: NOT moved - reset bound bytes differ at 0x%08X\n",
				     model_pool_bound_rva);
				return false;
			}
			// The old array must still be all zero: if UI_Model_Init had already
			// run, live nodes would be left behind.
			const auto* old_nodes = reinterpret_cast<const uint8_t*>(b + model_pool_base);
			if (!readable(old_nodes, 16 * model_pool_stride))
			{
				note("[splitscreen] ui model pool: NOT moved - old array unreadable\n");
				return false;
			}
			for (size_t i = 0; i < 16 * model_pool_stride; ++i)
			{
				if (old_nodes[i] != 0)
				{
					note("[splitscreen] ui model pool: NOT moved - old array already initialised\n");
					return false;
				}
			}
			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(static_cast<size_t>(model_pool_new_count) * model_pool_stride));
			if (!fresh)
			{
				note("[splitscreen] ui model pool: NOT moved - allocation failed\n");
				return false;
			}
			std::memset(fresh, 0, static_cast<size_t>(model_pool_new_count) * model_pool_stride);
			for (uint32_t i = 0; i < model_pool_new_count; ++i)
			{
				auto* node = fresh + static_cast<size_t>(i) * model_pool_stride;
				uint32_t next = i + 1;
				if (next == model_pool_sentinel)
				{
					next = model_pool_sentinel + 1;
				}
				if (next >= model_pool_new_count || i == 0 || i == model_pool_sentinel)
				{
					next = 0;   // end of list; node 0 is the null handle, 0x9000 the end sentinel
				}
				const auto self = static_cast<uint16_t>(i);
				const auto link = static_cast<uint16_t>(next);
				std::memcpy(node + model_pool_self_off, &self, sizeof(self));
				std::memcpy(node + model_pool_next_off, &link, sizeof(link));
			}
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			static int32_t saved[std::size(model_pool_sites)]{};
			if (!rewrite_entcoll(model_pool_sites, std::size(model_pool_sites), model_pool_base, fresh_abs, saved))
			{
				note("[splitscreen] ui model pool: NOT moved - a reference did not match\n");
				return false;
			}
			if (!write_bytes(bound, model_pool_bound_new, sizeof(model_pool_bound_new)))
			{
				for (size_t j = 0; j < std::size(model_pool_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + model_pool_sites[j].rva);
					write_bytes(insn + model_pool_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				note("[splitscreen] ui model pool: NOT moved - reset bound write failed (rolled back)\n");
				return false;
			}
			model_pool_new = fresh_abs;
			install_model_string_hunk();
			return true;
		}

		// Per-view sun-shadow (SST) ring 4 -> 8 entries. Each view takes a record
		// (0x21F0 bytes of GPU buffers) from a 4-entry ring; with four views a record
		// is reused every frame while the GPU may still draw from it (pane 4 shadow
		// flicker). 8 = 4 views x 2 frames, the stock margin. Patched: base leas,
		// mask 3 -> 7, free count 4 -> 8, static constructor count 3 -> 7. The
		// constructors only write zeros, so a zeroed block is constructed. Runs at
		// post_unpack, before the renderer creates the buffers.
		size_t sst_new = 0;

		bool relocate_sst_ring()
		{
			if (sst_new)
			{
				return true;
			}
			const auto b = base();
			for (const auto& s : sst_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				uint32_t cur = 0;
				if (!readable(at, s.size))
				{
					note("[splitscreen] sun-shadow ring: NOT moved - immediate unreadable\n");
					return false;
				}
				std::memcpy(&cur, at, s.size);
				if (cur != s.was)
				{
					note("[splitscreen] sun-shadow ring: NOT moved - an immediate differs\n");
					return false;
				}
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(sst_new_count * sst_stride));
			if (!fresh)
			{
				note("[splitscreen] sun-shadow ring: NOT moved - allocation failed\n");
				return false;
			}
			std::memset(fresh, 0, sst_new_count * sst_stride);
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			static int32_t saved[std::size(sst_sites)]{};
			static int32_t saved_end[std::size(sst_end_site)]{};
			if (!rewrite_entcoll(sst_sites, std::size(sst_sites), sst_base, fresh_abs, saved))
			{
				note("[splitscreen] sun-shadow ring: NOT moved - a base lea did not match\n");
				return false;
			}
			const auto undo_sites = [&]
			{
				for (size_t j = 0; j < std::size(sst_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + sst_sites[j].rva);
					write_bytes(insn + sst_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
			};
			// target = new_abs + target_off(4*stride): passing new + 4*stride lands on new + 8*stride.
			if (!rewrite_entcoll(sst_end_site, std::size(sst_end_site), sst_base,
			                     fresh_abs + (sst_new_count - sst_old_count) * sst_stride, saved_end))
			{
				undo_sites();
				note("[splitscreen] sun-shadow ring: NOT moved - the end lea did not match\n");
				return false;
			}
			size_t done = 0;
			for (const auto& s : sst_imms)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva + s.off);
				if (!write_bytes(at, &s.want, s.size))
				{
					for (size_t j = 0; j < done; ++j)
					{
						write_bytes(reinterpret_cast<uint8_t*>(b + sst_imms[j].rva + sst_imms[j].off),
						            &sst_imms[j].was, sst_imms[j].size);
					}
					auto* insn = reinterpret_cast<uint8_t*>(b + sst_end_site[0].rva);
					write_bytes(insn + sst_end_site[0].disp_off, &saved_end[0], sizeof(int32_t));
					undo_sites();
					note("[splitscreen] sun-shadow ring: NOT moved - an immediate write failed (rolled back)\n");
					return false;
				}
				++done;
			}
			sst_new = fresh_abs;
			return true;
		}

		// cl_voiceCommunication (no code here). The engine writes client 2's
		// clientUIActives record ([2] x 0x1078) past the array, into
		// cl_voiceCommunication, which voice code also writes. Moving clientUIActives
		// is a closed dead end (History: LOG.md, clientUIActives), so reloc_tables'
		// "voice_comm" entry moves the neighbour to a [4] block at startup (12 refs;
		// PS4 CL_GetLocalClientVoiceCommunication 0x1DA6C40). The other 12 refs in
		// that span are clientUIActives' loop end markers, owned by
		// widen_client_ui_walker_bounds(). Slot 3 is not handled here.

		// DWARF-map batch 1: PS4 LOCAL_CLIENT_COUNT globals on the cgame path, found
		// on the PC by stride, sites from tools/gen_reloc_sites.py (data/reloc_sites/).
		// Slots 2..3 are foreign, so each is relocated.
		// cgDC - the per-client display context (CG_Init memsets cgDC[lc]), [2] x 0x1838.

		// Not relocated: a stride-0x1660 array of 18 per-client elements and a
		// stride-0x188 pool of handle-indexed entries. A stride match is not enough,
		// the index must be lc; gen_reloc_sites.py v2 refuses arrays a vector
		// constructor sizes other than [2].

		// playerKeys: per-client key/binding state (PS4 PlayerKeyState[4], 0x1810;
		// PC [2] x 0x1940). Client 2's state was foreign memory, so the key-event
		// walker called stricmp on a dangling binding pointer. 82 sites (38 RIP,
		// 44 ABS32) from tools/gen_reloc_sites.py v2.1, incl. leaf accessors and
		// ABS32 stores with an immediate (missing those split the state in two).
		// Until CL_ClearKeys(lc) runs, client 2's binding pointers are NULL, which
		// stricmp's null guards accept.
		constexpr uint32_t playerkeys_stride = 0x1940;
		bool playerkeys_relocated = false;

		bool relocate_playerkeys()
		{
			if (playerkeys_relocated) { return true; }
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * playerkeys_stride));
			if (!fresh) { return false; }
			std::memset(fresh, 0, 4 * playerkeys_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + playerkeys_base), 2 * playerkeys_stride);
			static int32_t saved[std::size(playerkeys_sites)]{};
			if (!rewrite_entcoll(playerkeys_sites, std::size(playerkeys_sites), playerkeys_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			// The binding-clear loop ends on a pointer, &playerKeys[2]+0x148; without
			// this it clears player 1's bindings only. New end: &new[4]+0x148.
			if (!retarget_end_marker(playerkeys_end_marker_rva, 3, 7, playerkeys_base + 2 * playerkeys_stride + 0x148,
			                         reinterpret_cast<size_t>(fresh) + 4 * playerkeys_stride + 0x148))
			{
				for (size_t j = 0; j < std::size(playerkeys_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + playerkeys_sites[j].rva);
					write_bytes(insn + playerkeys_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			playerkeys_relocated = true;
			return true;
		}

		// g_notetrackLerps: PS4 [LOCAL_CLIENT_COUNT][16] x 0x34 (CG_InitNotetrackLerps
		// 0x13C6A0), [2] on PC. For lc 2, CG_UpdateNotetrackLerps read an entity number
		// from foreign memory and wrote through a wild pointer. 34 sites (5 RIP,
		// 29 ABS32), data/reloc_sites/sites_notetracklerps.txt.
		// CG_InitNotetrackLerps(2) initializes the new row. PC [2] x 0x340.

		// One step of perclient_rows (10_relocations_b.inl), run in order by
		// relocate_perclient_rows(). A plain row moves `array` with relocate_perclient;
		// an `own` row is a relocation with a layout of its own and is called instead.
		struct perclient_row
		{
			perclient_array array;                                  // own rows: the name only
			bool (*pre)(const perclient_array&) = nullptr;          // false: skipped, nothing written
			bool (*post)(const perclient_array&, size_t) = nullptr; // after a move, gets the new block
			bool (*own)() = nullptr;
		};

		// cg_pmove - pmove_t[LOCAL_CLIENT_COUNT] (PS4 0x0451EFE0, 0x1660), used every
		// frame by CG_PredictPlayerState_Internal. The element constructor stores a
		// vtable at +0x2C0 (a zeroed slot would call through NULL), so it runs on 2/3.

		// s_cameraShakeSet - CameraShakeSet[4] (PS4 0x03F61EF0, 0x104). CG_ClearCameraShakes
		// (0x005830A0) memsets [lc]; CG_ShakeCamera reads it every frame.

		// moverInfos - mover_info_t[4] (PS4 0x03F60F50, 0x390), camera-tween mover records.
		// Its constructor is a no-op, so zero is the initial state.

		// moveInfoEntNum - int[4] (PS4 0x03F61D90), read next to moverInfos at 0x004F0FCA.
		// Its slot 2 is used by 3 foreign leas.

		// rumbleGlobArray - RumbleGlobals[4] (PS4 0x045269F0, 0x410). GetRumbleGlobals is
		// inlined 8 times; every one indexes with the lc argument.

		// atGlobArray - AimTargetGlob[4] (PS4 0x03159380, 0x1604). AimTarget_GetGlobArray
		// (0x000771E0) and the clear (0x0007E100) take lc; used per frame by aim assist.

		// g_aimtarget_cmd - AimTarget_Cmd[4] (PS4 0x031592E0, 0x10), indexed lc*16 next to
		// atGlobArray at 0x0008672C. Slot 2 lands on a foreign global.

		// gArcData - ARC_DATA[4] (PS4 0x03FF7A10, 0xEEC), grenade arc prediction
		// (CG_ArcPrediction_Update/Render). Indexed by lc next to cg_t (0x342720).

		// Per-local-client [2][18] x 0x132 array: the game session's 18 member slots
		// for each local client. lc 2 ran past it into a static cmd_function_t node.
		// Moved to [4]; its clear (memset 0x2B08) widens to four rows.
		constexpr uint32_t session_member_stride = 18 * 0x132;   // 0x1584
		size_t session_member_new = 0;

		bool relocate_session_members()
		{
			if (session_member_new)
			{
				return true;
			}
			const auto b = base();
			auto* len = reinterpret_cast<uint8_t*>(b + session_member_clear_rva);
			constexpr uint8_t len_old[] = {0x41, 0xB8, 0x08, 0x2B, 0x00, 0x00}; // mov r8d, 0x2B08
			constexpr uint8_t len_new[] = {0x41, 0xB8, 0x10, 0x56, 0x00, 0x00}; // mov r8d, 0x5610
			if (!readable(len, sizeof(len_old)) || std::memcmp(len, len_old, sizeof(len_old)) != 0)
			{
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * session_member_stride));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, 4 * session_member_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + session_member_base),
			            2 * session_member_stride);
			int32_t saved[std::size(session_member_sites)] = {};
			if (!rewrite_entcoll(session_member_sites, std::size(session_member_sites),
			                     session_member_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			if (!write_bytes(len, len_new, sizeof(len_new)))
			{
				for (size_t j = 0; j < std::size(session_member_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + session_member_sites[j].rva);
					write_bytes(insn + session_member_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			session_member_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		// One [2]->[4] move: new block near the module, live slots 0..1 copied,
		// 2..3 zeroed (and constructed if the array has an element constructor),
		// then every site rewritten, verified first, all or nothing. Returns the
		// new block or 0.
		size_t relocate_perclient(const perclient_array& a)
		{
			const auto b = base();
			if (a.ctor_rva)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + a.ctor_rva);
				if (!readable(at, sizeof(a.ctor_sig)) || std::memcmp(at, a.ctor_sig, sizeof(a.ctor_sig)) != 0)
				{
					note("[splitscreen] %s: constructor bytes differ - nothing moved\n", a.name);
					return 0;
				}
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * a.stride));
			if (!fresh)
			{
				return 0;
			}
			std::memset(fresh, 0, 4 * a.stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + a.base), 2 * a.stride);
			if (a.ctor_rva)
			{
				using ctor_t = void* (*)(void*);
				const auto ctor = reinterpret_cast<ctor_t>(b + a.ctor_rva);
				for (size_t lc = 2; lc < 4; ++lc)
				{
					ctor(fresh + lc * a.stride);
				}
			}
			std::vector<int32_t> saved(a.count);
			if (!rewrite_entcoll(a.sites, a.count, a.base, reinterpret_cast<size_t>(fresh), saved.data()))
			{
				return 0;
			}
			return reinterpret_cast<size_t>(fresh);
		}

		// aaGlobArray ([2] x 0x4E30), complete table: 50 sites (28 rip, 22 abs,
		// data/reloc_sites/sites_aimglob.txt). Moving only the `lea reg,[base]`
		// sites left field accessors on the old array (player 4 crashed aiming).
		// 0x00039BB9 is a site: rcx holds the module base there. Zero-fill is the
		// initial state (AimTarget_Init memsets each slot).

		// ---- cg_zbarriers: re-implemented, not relocated ----
		// cgZBarrier_t cg_zbarriers[4][128] (PS4 0x03F2FE90, 0x188 each) and int
		// numcgZBarriers[4] (PS4 0x03F60EA0) are [2] on the PC, and exactly two functions
		// address them (every reference scanned 2026-09-30): the record allocator that
		// CG_InitZBarrier calls, and CG_InitZBarriers (from CG_Init and CG_MapRestart).
		// Both are replaced whole and work on four rows of our own; the engine's rows are
		// never touched again. Records leave only as a pointer (centity +0x650) and the free
		// routine clears their byte 0 through it, so they may live anywhere. Neither function
		// has an Arxan caller check, the replacements call no engine code, and no caller reads
		// a volatile register after either call (both stock bodies call memset).
		struct zbarrier_store
		{
			uint8_t rows[4][zbarriers_per_client][zbarrier_size];
			int32_t count[4];
		};
		zbarrier_store* zbarriers = nullptr;
		bool zbarriers_replaced = false;
		utils::hook::detour zbarrier_alloc_hook;
		utils::hook::detour cg_init_zbarriers_hook;

		// The allocator (PC only; PS4 CG_InitZBarrier appends inline and never reuses): the
		// first of the client's `count` records whose in-use byte 0 is clear, else the next
		// one while count < 128, else NULL. The record is zeroed and marked in use. count
		// stays 0..128 (starts 0; only the clear and the append set it). Two unreachable
		// inputs get NULL where stock would index foreign memory: lc outside 0..3 and a
		// negative count (stock appends at row[n] for n < 0).
		uint8_t* zbarrier_alloc(zbarrier_store& s, const int lc)
		{
			if (static_cast<uint32_t>(lc) >= std::size(s.count))
			{
				return nullptr;
			}
			auto& row = s.rows[lc];
			const int32_t n = s.count[lc];
			uint8_t* rec = nullptr;
			for (int32_t i = 0; i < n; ++i)
			{
				if (row[i][0] == 0)
				{
					rec = row[i];
					break;
				}
			}
			if (!rec)
			{
				if (n < 0 || n >= static_cast<int32_t>(zbarriers_per_client))
				{
					return nullptr;
				}
				rec = row[n];
				s.count[lc] = n + 1;
			}
			std::memset(rec, 0, zbarrier_size);
			rec[0] = 1;
			return rec;
		}

		// CG_InitZBarriers: all four rows and counts, as PS4 (the stock PC body clears two).
		void zbarrier_clear(zbarrier_store& s)
		{
			std::memset(&s, 0, sizeof(s));
		}

		uint8_t* zbarrier_alloc_stub(const int lc)
		{
			return zbarrier_alloc(*zbarriers, lc);
		}

		void cg_init_zbarriers_stub()
		{
			note("[splitscreen] zbarriers: clear, counts %d %d %d %d\n", zbarriers->count[0],
			     zbarriers->count[1], zbarriers->count[2], zbarriers->count[3]);
			zbarrier_clear(*zbarriers);
		}

		// Both or neither: one replaced alone would split the records between two stores.
		// Every byte of both bodies is verified first (the old relocation row, if left in,
		// changes both, so this then stands down). detour::create throws on a MinHook
		// failure; that path must clear too, or F2 could stay detoured with F1 stock.
		bool replace_zbarrier_functions()
		{
			if (zbarriers_replaced)
			{
				return true;
			}
			const auto b = base();
			const auto* alloc = reinterpret_cast<const void*>(b + zbarrier_alloc_rva);
			const auto* init = reinterpret_cast<const void*>(b + cg_init_zbarriers_rva);
			if (!readable(alloc, sizeof(zbarrier_alloc_expected))
			    || std::memcmp(alloc, zbarrier_alloc_expected, sizeof(zbarrier_alloc_expected)) != 0
			    || !readable(init, sizeof(cg_init_zbarriers_expected))
			    || std::memcmp(init, cg_init_zbarriers_expected, sizeof(cg_init_zbarriers_expected)) != 0)
			{
				note("[splitscreen] zbarriers: function bytes differ - not replaced\n");
				return false;
			}
			if (!zbarriers)
			{
				// VirtualAlloc zero-fills: the state CG_InitZBarriers leaves.
				zbarriers = static_cast<zbarrier_store*>(allocate_near_module(sizeof(zbarrier_store)));
				if (!zbarriers)
				{
					return false;
				}
			}
			try
			{
				if (!hook_if_stock(cg_init_zbarriers_hook, cg_init_zbarriers_rva, cg_init_zbarriers_expected,
				                   cg_init_zbarriers_stub))
				{
					return false;
				}
				if (!hook_if_stock(zbarrier_alloc_hook, zbarrier_alloc_rva, zbarrier_alloc_expected,
				                   zbarrier_alloc_stub))
				{
					cg_init_zbarriers_hook.clear();
					return false;
				}
			}
			catch (...)
			{
				zbarrier_alloc_hook.clear();
				cg_init_zbarriers_hook.clear();
				note("[splitscreen] zbarriers: hook failed - both removed, not replaced\n");
				return false;
			}
			zbarriers_replaced = true;
			note("[splitscreen] zbarriers: replaced, store %p\n", static_cast<void*>(zbarriers));
			return true;
		}

		// DWARF-map batch 2.
		// totalCoverageArea_s - totalCoverageArea_t[4][18] (PS4 0x04E73820, row 0x360),
		// CG_TotalCoverage_Frame(lc). Slot 2 overlaps foreign globals.
		// gaGlobs - GpadAxesGlob[4] x 0x48 (PS4 0x05A4F090): per-client gamepad axis
		// bindings, read by CL_GamepadAxisValue(lc, axis). Slot 2 is foreign.
		// CL_InitGamepadAxisBindings loops up to an end marker, &gaGlobs[2] + 0x1C;
		// it moves to &new[4] + 0x1C, else the loop stops after client 0.
		// s_rightStickModels (word[5] per controller, stride 0xA) overlaps
		// s_gamepadButtons[0] with its slot 3, so it moves. s_gamepadButtons (PC
		// stride 0x2E, PS4 0x2A) moves too: slots 2..3 have no code refs but hold
		// static list nodes reached through links (a widen without the move crashed).
		size_t gaglobs_new = 0;

		bool relocate_gaglobs()
		{
			if (gaglobs_new)
			{
				return true;
			}
			const auto b = base();
			auto* end_disp = reinterpret_cast<uint8_t*>(b + gaglobs_end_marker_rva + 3);
			int32_t end_old = 0;
			if (!readable(end_disp, sizeof(end_old)))
			{
				return false;
			}
			std::memcpy(&end_old, end_disp, sizeof(end_old));
			const auto* end_insn = reinterpret_cast<const uint8_t*>(b + gaglobs_end_marker_rva);
			if (end_insn[0] != 0x4C || end_insn[1] != 0x8D || end_insn[2] != 0x15
			    || static_cast<int64_t>(gaglobs_end_marker_rva) + 7 + end_old != static_cast<int64_t>(gaglobs_base_rva) + 2 * gaglobs_stride + 0x1C)
			{
				note("[splitscreen] gaGlobs: end marker bytes differ - nothing moved\n");
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * gaglobs_stride));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, 4 * gaglobs_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + gaglobs_base_rva), 2 * gaglobs_stride);
			std::vector<int32_t> saved(std::size(gaglobs_sites));
			if (!rewrite_entcoll(gaglobs_sites, std::size(gaglobs_sites), gaglobs_base_rva,
			                     reinterpret_cast<size_t>(fresh), saved.data()))
			{
				return false;
			}
			const auto end_new = static_cast<int64_t>(reinterpret_cast<size_t>(fresh) + 4 * gaglobs_stride + 0x1C)
			                     - static_cast<int64_t>(b + gaglobs_end_marker_rva + 7);
			const auto end_new32 = static_cast<int32_t>(end_new);
			if (end_new != end_new32 || !write_bytes(end_disp, &end_new32, sizeof(end_new32)))
			{
				for (size_t j = 0; j < std::size(gaglobs_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + gaglobs_sites[j].rva);
					write_bytes(insn + gaglobs_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			gaglobs_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		// cgExploderTriggers (1000 x 0x30 per client, row 0xBB80) and cgExploderTriggerCount
		// (int per client): [4] on PS4 (0x03E9C550), [2] on PC. Trigger row 2 is foreign data
		// and count slot 3 is written by a static initializer. CG_ExplodersInit clears the
		// counts with one 7-byte qword store and memsets the triggers (0x17700), so both move
		// into one block, the counts right behind the four rows, and the memset length becomes
		// 0x2EE10: one clear covers everything, as on PS4.
		// Sites: exploder_triggers_reloc.validated.txt, not the generator's output.
		constexpr uint32_t exploder_trig_stride = 0xBB80;
		size_t exploder_trig_new = 0;

		bool relocate_exploder_triggers()
		{
			if (exploder_trig_new)
			{
				return true;
			}
			const auto b = base();
			auto* len = reinterpret_cast<uint8_t*>(b + exploder_trig_len_rva);
			const auto& len_old = exploder_trig_len_bytes;
			const auto& len_new = exploder_trig_len_new;
			if (!readable(len, sizeof(len_old)) || std::memcmp(len, len_old, sizeof(len_old)) != 0)
			{
				note("[splitscreen] exploder triggers: memset length bytes differ - nothing moved\n");
				return false;
			}
			constexpr size_t rows = 4 * exploder_trig_stride; // 0x2EE00
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(rows + 0x10));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, rows + 0x10);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + exploder_trig_base), 2 * exploder_trig_stride);
			std::memcpy(fresh + rows, reinterpret_cast<const void*>(b + exploder_count_base), 2 * sizeof(int32_t));
			std::vector<int32_t> saved_trig(std::size(exploder_trig_sites));
			std::vector<int32_t> saved_count(std::size(exploder_count_sites));
			if (!rewrite_entcoll(exploder_trig_sites, std::size(exploder_trig_sites), exploder_trig_base,
			                     reinterpret_cast<size_t>(fresh), saved_trig.data()))
			{
				return false;
			}
			const auto undo_trig = [&]
			{
				for (size_t j = 0; j < std::size(exploder_trig_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exploder_trig_sites[j].rva);
					write_bytes(insn + exploder_trig_sites[j].disp_off, &saved_trig[j], sizeof(int32_t));
				}
			};
			if (!rewrite_entcoll(exploder_count_sites, std::size(exploder_count_sites), exploder_count_base,
			                     reinterpret_cast<size_t>(fresh + rows), saved_count.data()))
			{
				undo_trig();
				return false;
			}
			if (!write_bytes(len, len_new, sizeof(len_new)))
			{
				undo_trig();
				for (size_t j = 0; j < std::size(exploder_count_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exploder_count_sites[j].rva);
					write_bytes(insn + exploder_count_sites[j].disp_off, &saved_count[j], sizeof(int32_t));
				}
				return false;
			}
			exploder_trig_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		// ---- Batch 3: screen effects and compass ----
		// s_screenBlur (x 0x1C), s_screenElectrified and s_screenBurn (x 0xC) are [2] arrays
		// packed back to back (PS4 0x04001E50 / EC0 / EF0), so each slot 2 is the next base.
		// CG_CompassUpdateActors(lc) runs every frame, so player 3 wrote 0x2C00 bytes past
		// s_compassActors across the other compass tables. CG_ClearCompassPingData clears each
		// table with a two-row length (compass_clear_*); it becomes four rows only for a table
		// that actually moved.

		// Pre-step of a compass row: the table's clear, `mov r8d, imm32` at Len, must
		// still be two rows long (the screen-effect arrays have no clear site).
		template <uint32_t Len>
		bool compass_clear_is_two_rows(const perclient_array& a)
		{
			const auto* len = reinterpret_cast<const uint8_t*>(base() + Len);
			uint32_t cur = 0;
			if (!readable(len, 6) || len[0] != 0x41 || len[1] != 0xB8)
			{
				return false;
			}
			std::memcpy(&cur, len + 2, sizeof(cur));
			if (cur != 2 * a.stride)
			{
				note("[splitscreen] %s: clear length differs - not moved\n", a.name);
				return false;
			}
			return true;
		}

		// Post-step of a compass row: the same clear now covers four rows.
		template <uint32_t Len>
		bool compass_clear_to_four_rows(const perclient_array& a, size_t)
		{
			const uint32_t len_new = 4 * a.stride;
			return write_bytes(reinterpret_cast<uint8_t*>(base() + Len) + 2, &len_new, sizeof(len_new));
		}

		// ---- Batch 4: CG_AllocateClientMemory's pointer tables and the destructibles ----
		// CG_AllocateClientMemory (PS4 0x21FD70) allocates the cg_weaponsArray, cg_destructibles
		// and cg_ikBuf buffers of every local client, client 2 included, but the pointer tables
		// are [2], so slot 2 was stored over foreign globals (weapons[2] replaced a weapon-info
		// pointer used by everyone). Moving the tables is the whole fix. cg_numDestructibles ->
		// cg_updateTime and s_destructible_gamestates ([2][32] x 0x84, row 0x1080) ->
		// s_num_destructible_gamestates are packed the same way: each slot 2 is the next global.
		// ikStates is not in this batch: see relocate_ikstates and ikstates_step.

		bool ik_reset_widened = false;

		// ikStates: PS4 `IKState* ikStates[5]` (0x120DB5D0), the server's state plus one per
		// local client. The PC table has three slots ([0] server, [1 + lc]), walked by the IK
		// reset loop up to its end marker. Client 2's slot is that unreferenced end address,
		// but client 3's is a foreign byte flag, so the table moves to [5] and the reset loop's
		// end marker (lea r14) moves with it.
		constexpr uint32_t ikstates_old_slots = 3;
		constexpr uint32_t ikstates_new_slots = 5;
		size_t ikstates_new = 0;

		bool relocate_ikstates()
		{
			if (ikstates_new)
			{
				return true;
			}
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(ikstates_new_slots * 8));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, ikstates_new_slots * 8);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + ikstates_base), ikstates_old_slots * 8);
			int32_t saved[std::size(ikstates_sites)] = {};
			if (!rewrite_entcoll(ikstates_sites, std::size(ikstates_sites), ikstates_base,
			                     reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			if (!retarget_end_marker(ikstates_end_marker_rva, 3, 7, ikstates_base + ikstates_old_slots * 8,
			                         reinterpret_cast<size_t>(fresh) + ikstates_new_slots * 8))
			{
				for (size_t j = 0; j < std::size(ikstates_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + ikstates_sites[j].rva);
					write_bytes(insn + ikstates_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			ikstates_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		// ---- Batch 5: two unnamed per-client cgame arrays (no PS4 [4] global has either shape) ----
		// cg_clientents30: 30 entries of 0x11E0 per client (stride 0x21840). Row 2 holds foreign
		// pointer globals, so player 3's entity interpolation read a NULL pointer and crashed.
		// The static initializer only zeroes a field, so zero-fill is the initial state.
		// cg_perclient_3c0: 8 entries of 0x78 per client, right before s_screenBlur; row 2 ran
		// over the screen-effect arrays and foreign globals.

		// ---- Batch 6: cgame threaded-notify queues ----
		// PS4 CG_ThreadedNotifyList_* (Init 0x2956D0): per local client 100 items plus
		// s_processQueueHead/Tail and s_firstFree, all [4]. On PC (items 0x50, stride 0x1F40) all
		// four are [2] and packed back to back, so CG_Init(2) at map load linked 100 items over
		// player 1's queue pointers and ~8 KB of live globals: a wild writer consistent with the
		// Arxan faults, the lost default_aitype and "Data is corrupt" in 3-player rounds.

		// The engine's static initializer sets every item of both clients to zero except
		// +0x04 = 0x3FF. Its lea is in the site table, so it fills slots 0/1 of the new block;
		// tnotify_init_items gives slots 2/3 the same values. If its bytes differ, nothing moves.

		// Pre-step of the tnotify_list row.
		bool tnotify_init_matches(const perclient_array&)
		{
			const auto b = base();
			const auto bytes_at = [b](const uint32_t rva, const uint8_t* expect, const size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, expect, n) == 0;
			};
			if (!bytes_at(tnotify_static_init_rva, tnotify_static_init, sizeof(tnotify_static_init))
				|| !bytes_at(tnotify_static_init_body_rva, tnotify_static_init_body, sizeof(tnotify_static_init_body)))
			{
				note("[splitscreen] tnotify: static initializer differs - queues not moved\n");
				return false;
			}
			return true;
		}

		// Post-step of the tnotify_list row.
		bool tnotify_init_items(const perclient_array&, const size_t fresh)
		{
			auto* items = reinterpret_cast<uint8_t*>(fresh);
			for (size_t lc = 2; lc < 4; ++lc)
			{
				for (size_t i = 0; i < 100; ++i)
				{
					*reinterpret_cast<uint32_t*>(items + lc * 0x1F40 + i * 0x50 + 0x04) = 0x3FF;
				}
			}
			return true;
		}

		// Pre-step of the head/tail/free rows: the pointer arrays only make sense
		// with the list moved. Defined after perclient_rows.
		bool tnotify_list_moved(const perclient_array&);

		// ---- Batch 7: renderer [2] x 0x240 array that slid the image by 8 bytes ----
		// An element holds 16 {int id, int age} entries, a qword count at +0x80, an id bitmask
		// at +0xC0 and a flag at +0x238. A routine drops stale entries by shifting the list down
		// 8 bytes up to &e[count-1]. With index 2 the element was foreign and the count garbage,
		// so ~190 MB of the image through .idata slid down by 8: Arxan faults, "Cannot find AI
		// Type" and "Data is corrupt". The initializer writes only zeros. History: LOG.md, "190 MB".

		// ---- Batch 8: renderer scene buffers ----
		// scene_pc480: 4 entries of 0x120 per client, sized for two. Client 2's slot lies over
		// the dpvs globals (PS4 GfxSceneDpvs: entVisData[4] etc.), so renderer workers crashed
		// using a float as entVisData[lc].
		// scene_c: the per-client pointer table R_InitSceneBuffers fills (PS4 dpvsGlob); slot 2
		// is the base of another array. The allocator's store is among its sites, so C[0]/C[1]
		// land in the new block too.

		// Post-step of the scene_c row. install_perclient_buffer_guard() bakes this
		// base into its cave, so it runs after the move.
		bool publish_scene_c(const perclient_array&, const size_t fresh)
		{
			scene_c_new = fresh;
			return true;
		}

		// ---- Batch 9: renderer per-client array (x 0xA24) ----
		// Client 2's slot covers the frame-limiter target and the globals around it (the
		// limiter's actual writer is batch 10). The static initializer only writes zero
		// fields, so zero-filled slots 2/3 are the constructed state.

		// ---- Batch 10: frame-limiter stall (renderer per-view array, x 0x30) ----
		// With three views the main thread sat in the frame limiter (`while (Sys_Milliseconds()
		// < target) Sys_Sleep(1)`) because the target held a float: no frames, no LUI tick,
		// frozen players. The writer is this array, indexed by the view's local client (`lea
		// r,[i+i*2]; shl r,4`, so perclient_sweep missed it); element 2 covers a pointer global
		// and the limiter target. On PS4 it is a member of a larger renderer struct. No static
		// initializer references it. History: LOG.md, "frame limiter".

		// ---- Batch 11: aim-target actor lists ----
		// PS4 aim_target_actors [4]: 64 centity pointers per client (base + lc*0x200), built by
		// the aim_target submitter and read by a worker job. [2] on PC; slot 2 was
		// AimTarget_Cmd's old home (moved in batch 1b), so player 3's pass read stale bytes as
		// entity pointers and crashed. Zero-filled slots mean "no actors".
