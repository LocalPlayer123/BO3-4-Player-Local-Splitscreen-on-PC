// Split panes 3/4: view params, IsActive cave, pane count, snapshot and buffer guards.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ============ Pane fix: 3 and 4 rendered panes (PLAN_3-4_SCREENS.md) ====
		// The PC pane dispatcher (PS4: CL_SetupScreenPlacements 0x3EC820) caps
		// the pane count in four places: C1 GetActiveCount is inlined for two;
		// C2 the pane loop bound is `cmp ebx,2`; C3 clientUIActives is [2] with a
		// foreign slot 2; C4 the geometry table has no 3- and 4-pane rows.

		// clientUIActives stays [2]: moving it black-screened the frontend (a closed
		// dead end, CLAUDE.md). install_isactive_hook() and the walker bounds work
		// around its foreign slots 2/3.

		// ---- Phase 2: the pane geometry table (GetLocalClientViewParams) ----
		// PC: [2 wide][2 total][2 pane] x 0x10. PS4: [2][4][4] x 0x10 = 0x200,
		// indexed pane*0x10 + (total-1)*0x40 + wide*0x100. The patch doubles the
		// index scales and points both base leas at ps4_view_params, the PS4 ELF
		// .data at VA 0x03060D50 (data/ps4_clientviewparams.bin).
		// All six sites change together or none do: a partial patch indexed past
		// the table into FOV constants and blacked out the frontend.

		bool view_params_relocated = false;

		bool relocate_view_params()
		{
			if (view_params_relocated)
			{
				return true;
			}
			const auto b = base();

			for (const auto& p : viewparams_patches)
			{
				const auto* site = reinterpret_cast<const void*>(b + p.rva);
				if (!readable(site, p.len) || std::memcmp(site, p.expect, p.len) != 0)
				{
					return false;
				}
			}

			auto* destination = allocate_near_module(sizeof(ps4_view_params));
			if (!destination)
			{
				return false;
			}
			std::memcpy(destination, ps4_view_params, sizeof(ps4_view_params));
			const auto new_rva = reinterpret_cast<size_t>(destination) - b;

			// One transaction over all six sites, each remembered for rollback.
			uint8_t old_bytes[std::size(viewparams_patches)][4] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					const auto& p = viewparams_patches[i];
					auto* f = reinterpret_cast<void*>(b + p.rva + p.off);
					write_bytes(f, old_bytes[i], p.is_base ? 4u : 1u);
				}
			};

			for (size_t i = 0; i < std::size(viewparams_patches); ++i)
			{
				const auto& p = viewparams_patches[i];
				auto* field = reinterpret_cast<void*>(b + p.rva + p.off);
				const size_t n = p.is_base ? 4u : 1u;
				std::memcpy(old_bytes[i], field, n);
				bool wrote;
				if (p.is_base)
				{
					const int32_t disp = static_cast<int32_t>(
						new_rva - (p.rva + p.len));
					wrote = write_bytes(field, &disp, sizeof(disp));
				}
				else
				{
					wrote = write_bytes(field, &p.to, sizeof(p.to));
				}
				if (!wrote)
				{
					rollback();
					return false;
				}
				++done;
			}

			// Read every site back before declaring success.
			for (size_t i = 0; i < std::size(viewparams_patches); ++i)
			{
				const auto& p = viewparams_patches[i];
				const auto* field = reinterpret_cast<const uint8_t*>(b + p.rva + p.off);
				if (p.is_base)
				{
					int32_t seen = 0;
					std::memcpy(&seen, field, sizeof(seen));
					if (seen != static_cast<int32_t>(new_rva - (p.rva + p.len)))
					{
						rollback();
						return false;
					}
				}
				else if (*field != p.to)
				{
					rollback();
					return false;
				}
			}

			view_params_relocated = true;
			return true;
		}

		// ---- Two [2] arrays the third pane's code path indexes at 2 ----
		// scrPlaceView (stride 0x7C; PS4: ScreenPlacement scrPlaceView[4], index
		// assert compiled out on PC): pane 2's viewport setup overwrote a pointer
		// global past the array. And an unnamed 0x54-byte per-client screen-effect
		// state whose slot 2 is a live pointer global. Both are relocated, never
		// widened; the engine fills slots 2/3 when context 2 activates.

		// Per-local-client u16 LUI element handles, written by the UI registrar
		// loop; a missing handle for context 2 crashed the LUI renderer. Slots
		// 2/3 are foreign. One reference.

		// The table above moves only the writer. The one reader, an ABS32 load in
		// UI_CoD_HUD_UpdateVisibilityBits that a lea-only scan misses, must follow,
		// or visibility bits go to the old handles and panes 1 and 2 have no HUD.
		bool uielem_reader_retargeted = false;
		size_t uielem_new_rva = 0;
		bool uielem_relocated = false;

		void retarget_uielem_reader()
		{
			if (uielem_reader_retargeted || !uielem_relocated || !uielem_new_rva)
			{
				return;
			}
			auto* insn = reinterpret_cast<uint8_t*>(base() + uielem_reader_rva);
			if (!readable(insn, sizeof(uielem_reader_bytes))
				|| std::memcmp(insn, uielem_reader_bytes, sizeof(uielem_reader_bytes)) != 0)
			{
				note("[splitscreen] uielem reader: bytes differ - skipped\n");
				return;
			}
			const auto disp = static_cast<int32_t>(uielem_new_rva);
			if (write_bytes(insn + 5, &disp, sizeof(disp)))
			{
				uielem_reader_retargeted = true;
			}
		}

		bool scrplace_relocated = false;
		bool perclient54_relocated = false;
		bool aaglob_relocated = false;
		size_t scrplace_new_rva = 0;
		size_t perclient54_new_rva = 0;

		// Copies the two live elements into a zeroed 4-slot block and repoints
		// the 7-byte leas; verified before and after, rolled back on failure.
		bool relocate_flat24(const char* name, const uint32_t old_base,
		                     const uint32_t stride, const flat24_site* sites,
		                     const size_t count, bool& flag, size_t& new_rva_out)
		{
			if (flag)
			{
				return true;
			}
			const auto b = base();

			for (size_t i = 0; i < count; ++i)
			{
				const auto* site = reinterpret_cast<const void*>(b + sites[i].rva);
				if (!readable(site, sizeof(sites[i].expect))
					|| std::memcmp(site, sites[i].expect,
					               sizeof(sites[i].expect)) != 0)
				{
					note("[splitscreen] %s: unexpected bytes at 0x%X - skipped\n",
					     name, sites[i].rva);
					return false;
				}
			}

			auto* destination = static_cast<uint8_t*>(allocate_near_module(4u * stride));
			if (!destination)
			{
				return false;
			}
			std::memset(destination, 0, 4u * stride);
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + old_base), 2u * stride);
			const auto new_rva = reinterpret_cast<size_t>(destination) - b;

			// 32 slots (aaGlobArray has 23 sites); the check keeps rollback in bounds.
			int32_t old_disp[32] = {};
			if (count > std::size(old_disp))
			{
				note("[splitscreen] %s: too many sites (%zu)\n", name, count);
				return false;
			}
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					auto* f = reinterpret_cast<void*>(b + sites[i].rva + 3);
					write_bytes(f, &old_disp[i], sizeof(old_disp[i]));
				}
			};

			for (size_t i = 0; i < count; ++i)
			{
				auto* field = reinterpret_cast<void*>(b + sites[i].rva + 3);
				std::memcpy(&old_disp[i], field, sizeof(old_disp[i]));
				const int32_t disp = static_cast<int32_t>(new_rva - (sites[i].rva + 7));
				if (!write_bytes(field, &disp, sizeof(disp)))
				{
					rollback();
					return false;
				}
				++done;
			}

			for (size_t i = 0; i < count; ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen,
				            reinterpret_cast<const void*>(b + sites[i].rva + 3),
				            sizeof(seen));
				if (seen != static_cast<int32_t>(new_rva - (sites[i].rva + 7)))
				{
					rollback();
					return false;
				}
			}

			new_rva_out = new_rva;
			flag = true;
			return true;
		}

		// ---- The third screen without moving clientUIActives ----
		// CG_SetView does not touch clientUIActives, so the pane pipeline's only
		// use of slot 2 is the IsActive(lc) gate in the dispatcher loop. The cave
		// returns 0 for lc >= cl_maxLocalClients and otherwise does the stock read
		// clientUIActives[lc].flags & 1.

		// CL_LocalClient_IsActive(lc) is clientUIActives[lc].flags & 1 with no bound. Its
		// replacement answers 0 for lc 2/3 while lc >= cl_maxLocalClients, mirroring
		// CG_GetLocalClientGlobals, which is NULL there for callers that do not null-check
		// (the dvar only becomes 3 at map load; until then the pane loop skips client 2);
		// otherwise the stock read (slot 2 is real since voice_comm moved off it). Do not
		// answer from the seat table: back at the menu the seat stays in use while lc 2's
		// active bit is cleared -> black loading screen. Entered through preserving_thunk
		// (the stock code clobbers only rax/rcx); the original is never called.
		bool isactive_hooked = false;
		utils::hook::detour isactive_hook;
		const volatile int32_t* isactive_max_local = nullptr;
		const volatile uint8_t* isactive_ui = nullptr;

		uint32_t isactive_stub(const int lc)
		{
			if (lc >= 2 && lc >= *isactive_max_local)
			{
				return 0;
			}
			return *reinterpret_cast<const volatile uint32_t*>(isactive_ui + static_cast<int64_t>(lc) * uia_stride) & 1;
		}

		bool install_isactive_hook()
		{
			if (isactive_hooked)
			{
				return true;
			}
			if (!signin_relocated)
			{
				return false; // needs the sign-in relocation first
			}
			const auto b = base();
			if (!readable(reinterpret_cast<const void*>(b + isactive_rva), sizeof(isactive_expected)))
			{
				return false;
			}
			isactive_max_local = reinterpret_cast<const volatile int32_t*>(b + cl_max_local_clients_rva);
			isactive_ui = reinterpret_cast<const volatile uint8_t*>(b + uia_base_rva);
			auto* thunk = preserving_thunk(reinterpret_cast<const void*>(&isactive_stub));
			isactive_hooked = thunk && hook_if_stock(isactive_hook, isactive_rva, isactive_expected,
			                                         static_cast<void*>(thunk));
			return isactive_hooked;
		}

		// ---- Phase 3: the pane count and the dispatcher bound ----
		// The dispatcher has its own inline GetActiveCount, separate from the one
		// install_active_count_fix() caves; both are needed. Widen the bounds only
		// once storage and geometry are both live.

		// Only the CG_SetView dispatcher bound is widened. Not its siblings:
		// `mov edi,1` seeds a downward walk over clientUIActives (raising it writes
		// to element[-1]), and the two `cmp edi,2` 2D/HUD loops are unclassified.

		bool pane_counts_installed = false;

		// widen_ui_registrar_bound() may only run after the element-handle array
		// is relocated (later in try_apply than relocate_lui_roots).
		// Defined with the LUI roots relocation, further down.
		extern bool lui_roots_relocated;
		extern size_t uiroot_new_base_rva;

		// Caps the LUI render loop at three UI contexts:
		// `cmp r15d,[cl_maxLocalClients]` -> `cmp r15d,3` + NOPs. Context 2 (pane
		// 3's HUD) is safe only with the element handles relocated and their
		// reader retargeted; without them it crashed on a NULL element.
		// History: LOG.md, 0x0270D553
		bool lui_ctx_held = false;

		bool hold_lui_context_count()
		{
			if (lui_ctx_held)
			{
				return true;
			}
			auto* at = reinterpret_cast<uint8_t*>(base() + lui_ctx_bound_rva);
			if (!readable(at, sizeof(lui_ctx_expected))
				|| std::memcmp(at, lui_ctx_expected, sizeof(lui_ctx_expected)) != 0)
			{
				note("[splitscreen] lui ctx bound: unexpected bytes - skipped\n");
				return false;
			}
			if (!write_bytes(at, lui_ctx_patched, sizeof(lui_ctx_patched)))
			{
				return false;
			}
			uint8_t back[sizeof(lui_ctx_patched)] = {};
			std::memcpy(back, at, sizeof(back));
			if (std::memcmp(back, lui_ctx_patched, sizeof(back)) != 0)
			{
				write_bytes(at, lui_ctx_expected, sizeof(lui_ctx_expected));
				return false;
			}
			lui_ctx_held = true;
			return true;
		}

		bool ui_registrar_widened = false;

		bool widen_ui_registrar_bound()
		{
			if (ui_registrar_widened)
			{
				return true;
			}
			if (!uielem_relocated || !lui_roots_relocated)
			{
				return false;
			}
			auto* at = reinterpret_cast<uint8_t*>(base() + ui_registrar_bound_rva);
			if (!readable(at, sizeof(ui_registrar_bound_bytes))
				|| std::memcmp(at, ui_registrar_bound_bytes, sizeof(ui_registrar_bound_bytes)) != 0)
			{
				return false;
			}
			const uint8_t four = 0x04;
			if (!write_bytes(at + 2, &four, sizeof(four)))
			{
				return false;
			}
			uint8_t back = 0;
			std::memcpy(&back, at + 2, sizeof(back));
			if (back != four)
			{
				const uint8_t two = 0x02;
				write_bytes(at + 2, &two, sizeof(two));
				return false;
			}
			ui_registrar_widened = true;
			return true;
		}

		// Snapshot guard in the HUD-refresh reader, which reads cg->nextSnap
		// (+0x30). For client 2 it is NULL (CG_ProcessSnapshots stays bounded at
		// 2; widening it gave zero panes). The cave sends NULL to the function's
		// own "snapshot not usable -> return 0" exit, so nothing downstream runs.
		bool snapguard_installed = false;

		bool install_snapguard_cave()
		{
			if (snapguard_installed)
			{
				return true;
			}
			const auto b = base();
			auto* site = reinterpret_cast<uint8_t*>(b + snapguard_rva);
			if (!readable(site, sizeof(snapguard_expected))
				|| std::memcmp(site, snapguard_expected,
				               sizeof(snapguard_expected)) != 0)
			{
				return false;
			}
			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
			if (!cave)
			{
				return false;
			}
			const auto cave_addr = reinterpret_cast<size_t>(cave);
			std::vector<uint8_t> c;
			const auto rel32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};
			c.insert(c.end(), {0x48, 0x8B, 0x57, 0x30});   // mov rdx, [rdi+0x30]
			c.insert(c.end(), {0x48, 0x85, 0xD2});         // test rdx, rdx
			c.insert(c.end(), {0x0F, 0x84});               // jz -> return-0 exit
			rel32(b + snapguard_ret0);
			c.insert(c.end(), {0xF6, 0x02, 0x10});         // test byte [rdx], 0x10
			c.insert(c.end(), {0xE9});                     // jmp resume (the jne)
			rel32(b + snapguard_resume);
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}
			uint8_t patch[7] = {0xE9, 0, 0, 0, 0, 0x90, 0x90};
			const auto rel = static_cast<int32_t>(cave_addr - (b + snapguard_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return false;
			}
			snapguard_installed = true;
			return true;
		}

		// Guard for the per-client scene-buffer clear (see R_InitSceneBuffers
		// below): it memsets A[lc] and C[lc], which were NULL for lc 2.
		// Do not raise the allocator's client count (2): A uses the count as its
		// row stride behind 49 precomputed-index readers, and C's slot 2 is
		// another array's base.
		constexpr size_t scene_slots = 4;
		bool scene_b_relocated = false;
		bool scene_buffers_filled = false;
		uint32_t scene_b_new_rva = 0;
		// C's relocated 4-slot block (0 = not relocated). Never write old
		// C[2]/C[3]: three leas use C[2] as another array's base.
		size_t scene_c_new = 0;
		bool pcbuf_guarded = false;

		// R_InitSceneData(lc) memsets the client's dpvs and scene C buffers; C[lc] of an
		// unallocated client 2/3 is NULL (the memset crash at old 0x02C3DE7F). Return only
		// then - a count gate would leave client 2's buffers uncleared once
		// fill_scene_buffers() allocates them. The C table is taken once, at install (the
		// moved one if its relocation ran, which runs before this guard).
		utils::hook::detour r_init_scene_data_hook;
		void* const volatile* pcbuf_c_table = nullptr;

		void r_init_scene_data_stub(const int lc)
		{
			if (pcbuf_c_table[static_cast<int64_t>(lc)] == nullptr)
			{
				return;
			}
			r_init_scene_data_hook.invoke<void>(lc);
		}

		bool install_perclient_buffer_guard()
		{
			if (pcbuf_guarded)
			{
				return true;
			}
			const auto b = base();
			if (!readable(reinterpret_cast<const void*>(b + pcbuf_clear_rva), sizeof(pcbuf_expected)))
			{
				return false;
			}
			pcbuf_c_table = reinterpret_cast<void* const volatile*>(scene_c_new ? scene_c_new : b + scene_c_rva);
			pcbuf_guarded = hook_if_stock(r_init_scene_data_hook, pcbuf_clear_rva, pcbuf_expected,
			                              r_init_scene_data_stub);
			return pcbuf_guarded;
		}
