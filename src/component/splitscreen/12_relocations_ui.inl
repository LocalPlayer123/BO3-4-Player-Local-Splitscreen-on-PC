// Relocations, part C: UI/cgame context, LUI roots, s_perController, LUI target tables, markers, local entities, exploders.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// --- Unnamed per-client CG/UI context array [2] -> [4] --------------------
		// Element 0x2BB8, indexed by localClientNum; no PS4 match by size. Slot 2
		// overlaps live globals (47 refs); a client-2 write turned a pointer into
		// floats and crashed. The array is not statically zero, so slots 0/1 are
		// copied. The per-map reset walker (`mov ebp,1 / dec / jns`, two elements)
		// is set to 3 only after the move.
		constexpr uint32_t percg_base_rva = 0x04CADB40;
		constexpr uint32_t percg_stride = 0x2BB8;
		constexpr uint32_t percg_old_slots = 2;
		constexpr uint32_t percg_new_slots = 4;
		constexpr uint32_t percg_old_size = percg_old_slots * percg_stride;  // 0x5770
		constexpr uint32_t percg_new_size = percg_new_slots * percg_stride;  // 0xAEE0

		constexpr uint32_t percg_walker_count_rva = 0x02D2CC1A;
		constexpr uint8_t percg_walker_expected[] = {0xBD, 0x01, 0x00, 0x00, 0x00};

		struct percg_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint32_t delta;   // byte offset within element 0
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[12];
		};

		constexpr percg_ref percg_refs[] = {
			{0x010EB4F0, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0x49, 0x26, 0xBC, 0x03}},
			{0x010F30C2, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x77, 0xAA, 0xBB, 0x03}},
			{0x02D2CC1F, 7, 3, 0x830, true, {0x48, 0x8D, 0x1D, 0x4A, 0x17, 0xF8, 0x01}},
			{0x010CD118, 12, 4, 0x2BB0, false, {0x42, 0xC7, 0x84, 0x30, 0xF0, 0x06, 0xCB, 0x04, 0xFF, 0xFF, 0xFF, 0xFF}},
			{0x010CD124, 8, 4, 0x2BA0, false, {0x4A, 0x89, 0x8C, 0x30, 0xE0, 0x06, 0xCB, 0x04}},
			{0x010CD12C, 8, 4, 0x2BA8, false, {0x4A, 0x89, 0x8C, 0x30, 0xE8, 0x06, 0xCB, 0x04}},
		};

		bool percg_relocated = false;

		bool relocate_percg_context()
		{
			if (percg_relocated)
			{
				return true;
			}

			const auto b = base();

			for (const auto& r : percg_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}
			if (!readable(reinterpret_cast<const void*>(b + percg_walker_count_rva),
			              sizeof(percg_walker_expected))
				|| std::memcmp(reinterpret_cast<const void*>(b + percg_walker_count_rva),
				               percg_walker_expected, sizeof(percg_walker_expected)) != 0)
			{
				return false;
			}

			auto* destination = allocate_near_module(percg_new_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			if (!readable(reinterpret_cast<const void*>(b + percg_base_rva), percg_old_size))
			{
				return false;
			}
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + percg_base_rva),
			            percg_old_size);

			int32_t old_values[std::size(percg_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + percg_refs[i].rva + percg_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			// rip-relative counts from the end of the instruction; the ABS32 form
			// is `[reg + disp32]` with reg = image base, so disp32 is the RVA.
			const auto field_value = [&](const percg_ref& r)
			{
				const size_t tgt = new_base + r.delta;
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			for (size_t i = 0; i < std::size(percg_refs); ++i)
			{
				const auto& r = percg_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			for (size_t i = 0; i < std::size(percg_refs); ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + percg_refs[i].rva + percg_refs[i].disp_off),
				            sizeof(seen));
				if (seen != field_value(percg_refs[i]))
				{
					rollback();
					return false;
				}
			}

			// Only now widen the walker: elements 2/3 land in the new block.
			const uint8_t three = 0x03;
			auto* count = reinterpret_cast<void*>(b + percg_walker_count_rva + 1);
			const uint8_t old_count = *reinterpret_cast<const uint8_t*>(count);
			if (!write_bytes(count, &three, sizeof(three)))
			{
				rollback();
				return false;
			}
			uint8_t count_seen = 0;
			std::memcpy(&count_seen, count, sizeof(count_seen));
			if (count_seen != three)
			{
				write_bytes(count, &old_count, sizeof(old_count));
				rollback();
				return false;
			}

			percg_relocated = true;
			return true;
		}

		// --- Per-client LUI root array [2] -> [4], flat relocation -----------------
		// Element 0xB0; the init loop builds "UIRoot%d" only for ebx < 2. Element 0
		// starts at the lower of the loop's two cursors (0x80 apart); an earlier
		// relocation used the upper one and was retracted. The bound cannot be
		// widened in place: element 2 would land on s_perController. So the array
		// moves first and the bound follows, in one transaction, at post_unpack
		// before the loop runs. If the old startup int3 at 0x01D492DB returns, the
		// move is wrong, not the timing: revert rather than retry at menu time.
		// History: LOG.md, s_rootData
		constexpr uint32_t uiroot_base_rva = 0x162631B0;
		constexpr uint32_t uiroot_stride = 0xB0;
		constexpr uint32_t uiroot_old_slots = 2;
		constexpr uint32_t uiroot_new_slots = 4;
		constexpr uint32_t uiroot_old_size = uiroot_old_slots * uiroot_stride;   // 0x160
		constexpr uint32_t uiroot_new_size = uiroot_new_slots * uiroot_stride;   // 0x2C0

		// The init loop bound `cmp ebx,2` (83 FB 02, immediate at +2).
		constexpr uint32_t uiroot_bound_rva = 0x01F1CC37;
		constexpr uint8_t uiroot_bound_expected[] = {0x83, 0xFB, 0x02};

		// Second `cmp ebx,2`: the per-client UI-context setup loop. At 2, context 2
		// is never built and later reads of it deref NULL. Uses the LUI roots, so
		// it is widened in the same function, after they moved.
		constexpr uint32_t uiroot_bound2_rva = 0x01F1CCDB;
		constexpr uint8_t uiroot_bound2_expected[] = {0x83, 0xFB, 0x02};

		struct uiroot_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint8_t delta;    // byte offset within element 0
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[9];
		};

		constexpr uiroot_ref uiroot_refs[] = {
			{0x01F1C1CC, 7, 3, 0x00, true, {0x48, 0x8D, 0x0D, 0xDD, 0x6F, 0x34, 0x14}},
			{0x01F1C3D5, 8, 3, 0xAC, false, {0x80, 0xBC, 0x38, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F1C3DF, 7, 3, 0x8C, false, {0x4C, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F1C46C, 8, 3, 0xAC, false, {0x80, 0xBC, 0x38, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F1C476, 7, 3, 0x8C, false, {0x4C, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F1C5E6, 8, 3, 0xAC, false, {0x80, 0xBC, 0x28, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F1C5F0, 7, 3, 0x8C, false, {0x48, 0x8D, 0x9D, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F1C79E, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x0B, 0x6A, 0x34, 0x14}},
			{0x01F1C9E8, 7, 3, 0x00, true, {0x48, 0x8D, 0x2D, 0xC1, 0x67, 0x34, 0x14}},
			{0x01F1CB8E, 7, 3, 0x80, true, {0x48, 0x8D, 0x35, 0x9B, 0x66, 0x34, 0x14}},
			{0x01F1CD35, 7, 3, 0xAC, true, {0x48, 0x8D, 0x1D, 0x20, 0x65, 0x34, 0x14}},
			{0x01F1CD3C, 7, 3, 0x8C, true, {0x48, 0x8D, 0x35, 0xF9, 0x64, 0x34, 0x14}},
			{0x01F1D832, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x77, 0x59, 0x34, 0x14}},
			{0x01F21A19, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x90, 0x17, 0x34, 0x14}},
			{0x01F25915, 7, 3, 0x00, true, {0x48, 0x8D, 0x15, 0x94, 0xD8, 0x33, 0x14}},
			{0x01F25D13, 9, 4, 0xAC, false, {0x42, 0x80, 0xBC, 0x39, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F25D1E, 7, 3, 0x8C, false, {0x4D, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F26428, 7, 3, 0x00, true, {0x4C, 0x8D, 0x2D, 0x81, 0xCD, 0x33, 0x14}},
			{0x01F26AB4, 7, 3, 0xAC, true, {0x48, 0x8D, 0x3D, 0xA1, 0xC7, 0x33, 0x14}},
			{0x01F26ABB, 7, 3, 0x8C, true, {0x48, 0x8D, 0x35, 0x7A, 0xC7, 0x33, 0x14}},
		};

		bool lui_roots_relocated = false;
		size_t uiroot_new_base_rva = 0;

		bool relocate_lui_roots()
		{
			if (lui_roots_relocated)
			{
				return true;
			}

			const auto b = base();

			// Every site must match its recorded bytes, or nothing is written.
			for (const auto& r : uiroot_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}
			if (!readable(reinterpret_cast<const void*>(b + uiroot_bound_rva),
			              sizeof(uiroot_bound_expected))
				|| std::memcmp(reinterpret_cast<const void*>(b + uiroot_bound_rva),
				               uiroot_bound_expected, sizeof(uiroot_bound_expected)) != 0)
			{
				return false;
			}

			auto* destination = allocate_near_module(uiroot_new_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			// Static content: copy slots 0/1; the init loop fills slots 2/3 later.
			if (!readable(reinterpret_cast<const void*>(b + uiroot_base_rva), uiroot_old_size))
			{
				return false;
			}
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + uiroot_base_rva),
			            uiroot_old_size);

			int32_t old_values[std::size(uiroot_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + uiroot_refs[i].rva + uiroot_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			// rip-relative counts from the end of the instruction; the ABS32 form
			// is `[reg + disp32]` with reg = image base, so disp32 is the RVA.
			const auto field_value = [&](const uiroot_ref& r)
			{
				const size_t tgt = new_base + r.delta;
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			for (size_t i = 0; i < std::size(uiroot_refs); ++i)
			{
				const auto& r = uiroot_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			for (size_t i = 0; i < std::size(uiroot_refs); ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + uiroot_refs[i].rva + uiroot_refs[i].disp_off),
				            sizeof(seen));
				if (seen != field_value(uiroot_refs[i]))
				{
					rollback();
					return false;
				}
			}

			// Only now widen the bound: element 2 no longer lands on s_perController.
			const uint8_t four = 0x04;
			auto* bound = reinterpret_cast<void*>(b + uiroot_bound_rva + 2);
			const uint8_t old_bound = *reinterpret_cast<const uint8_t*>(bound);
			if (!write_bytes(bound, &four, sizeof(four)))
			{
				rollback();
				return false;
			}
			uint8_t bound_seen = 0;
			std::memcpy(&bound_seen, bound, sizeof(bound_seen));
			if (bound_seen != four)
			{
				write_bytes(bound, &old_bound, sizeof(old_bound));
				rollback();
				return false;
			}

			// The second bound; on a byte mismatch only this site is skipped.
			if (readable(reinterpret_cast<const void*>(b + uiroot_bound2_rva),
			             sizeof(uiroot_bound2_expected))
				&& std::memcmp(reinterpret_cast<const void*>(b + uiroot_bound2_rva),
				               uiroot_bound2_expected,
				               sizeof(uiroot_bound2_expected)) == 0)
			{
				auto* bound2 = reinterpret_cast<void*>(b + uiroot_bound2_rva + 2);
				const uint8_t old_bound2 = *reinterpret_cast<const uint8_t*>(bound2);
				if (write_bytes(bound2, &four, sizeof(four)))
				{
					uint8_t seen2 = 0;
					std::memcpy(&seen2, bound2, sizeof(seen2));
					if (seen2 != four)
					{
						write_bytes(bound2, &old_bound2, sizeof(old_bound2));
					}
				}
			}

			// Sibling UI loops, same value. `cmp ebx,2` broadcasts "update_safe_area"
			// and steps into no new slot. `cmp edi,2` adds a HUD menu per active
			// controller (PS4 UI_CoD_Init 0xD04C0F). It needs root 2 in use (the count
			// stub no longer holds it at inUse = 0): otherwise HUD(2) lands in
			// "UIRootFull", the primary root, and swallows the other roots'
			// first_snapshot events. The registrar loop is widened later, in
			// widen_ui_registrar_bound.
			struct ui_bound { uint32_t rva; uint8_t modrm; };
			constexpr ui_bound ui_bounds[] = {
				{0x01F26B4D, 0xFB},   // cmp ebx,2
				{0x01F1CD94, 0xFF},   // cmp edi,2  HUD menus
			};
			for (const auto& ub : ui_bounds)
			{
				const uint8_t want[] = {0x83, ub.modrm, 0x02};
				auto* at = reinterpret_cast<uint8_t*>(b + ub.rva);
				if (!readable(at, sizeof(want))
					|| std::memcmp(at, want, sizeof(want)) != 0)
				{
					continue;
				}
				auto* imm = reinterpret_cast<uint8_t*>(b + ub.rva + 2);
				if (write_bytes(imm, &four, sizeof(four)))
				{
					uint8_t back = 0;
					std::memcpy(&back, imm, sizeof(back));
					if (back != four)
					{
						const uint8_t two = 0x02;
						write_bytes(imm, &two, sizeof(two));
					}
				}
			}

			uiroot_new_base_rva = new_base;
			lui_roots_relocated = true;
			return true;
		}

		// ---- s_perController (LUI_CoD) [2] -> [4] --------------------------------
		// Symptom: pane 4 turned grey and blurred mid-round in 4-player MP. PS4
		// CL_IsUIActive 0x415330 checks s_perController[ctrl] byte +1; PS4 has [4] x
		// 0x14, the PC clears only 0x28 bytes (= [2]). Slots 2/3 lie in the
		// button-glyph buffer that follows, so a glyph name marked player 4 as "in
		// a menu". 12 references into slots 0/1 are rewritten; 5 more address the
		// glyph buffer and stay. Moved at startup, so the engine's init clear
		// (widened 0x28 -> 0x50) initialises slots 2/3.
		constexpr uint32_t perctrl_base = 0x16263310;
		constexpr uint32_t perctrl_stride = 0x14;
		constexpr uint32_t perctrl_old_count = 2;
		constexpr uint32_t perctrl_new_count = 4;
		constexpr entcoll_site perctrl_sites[] = {
			{0x01F14F0D, 3, 7, true , 0x8},    // lea rcx,[+8]    subscribers++
			{0x01F1A550, 3, 7, true , 0x4},    // lea rcx,[+4]    float getter
			{0x01F1C68C, 5, 9, false, 0xC},    // mulss xmm1,[rbp+rbx*4+RVA+0xC]
			{0x01F1C6B8, 5, 9, false, 0x10},   // mulss xmm1,[rbp+rbx*4+RVA+0x10]
			{0x01F1CA11, 3, 7, true , 0x0},    // lea r14,[base]  init clear
			{0x01F1D09E, 3, 8, false, 0x0},    // cmp byte [rcx+rax*4+RVA],0
			{0x01F1D0E0, 3, 7, true , 0x1},    // lea rcx,[+1]    UI_CoD_IsUIActive
			{0x01F1F66E, 3, 7, true , 0x0},    // lea rax,[base]  flag setter
			{0x01F21363, 6, 10, false, 0xC},   // movss [r14+rax*4+RVA+0xC],xmm6
			{0x01F2136D, 6, 10, false, 0x10},  // movss [r14+rax*4+RVA+0x10],xmm7
			{0x01F2197D, 3, 7, true , 0x8},    // lea rcx,[+8]    subscribers--
			{0x01F265E7, 3, 7, true , 0x1},    // lea rax,[+1]
		};
		constexpr uint32_t perctrl_clear_rva = 0x01F1CA0D;                    // lea r8d,[rdx+0x28]
		constexpr uint8_t perctrl_clear_stock[] = {0x44, 0x8D, 0x42, 0x28};
		size_t perctrl_new = 0;

		bool relocate_per_controller()
		{
			if (perctrl_new)
			{
				return true;
			}
			const auto b = base();
			auto* clear = reinterpret_cast<uint8_t*>(b + perctrl_clear_rva);
			if (!readable(clear, sizeof(perctrl_clear_stock))
				|| std::memcmp(clear, perctrl_clear_stock, sizeof(perctrl_clear_stock)) != 0)
			{
				note("[splitscreen] s_perController: NOT moved - init clear bytes differ\n");
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(perctrl_new_count * perctrl_stride));
			if (!fresh)
			{
				note("[splitscreen] s_perController: NOT moved - allocation failed\n");
				return false;
			}
			const auto* old = reinterpret_cast<const uint8_t*>(b + perctrl_base);
			std::memcpy(fresh, old, perctrl_old_count * perctrl_stride);   // slots 2/3 stay zero
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);
			static int32_t saved[std::size(perctrl_sites)]{};
			if (!rewrite_entcoll(perctrl_sites, std::size(perctrl_sites), perctrl_base, fresh_abs, saved))
			{
				note("[splitscreen] s_perController: NOT moved - a reference did not match\n");
				return false;
			}
			const uint8_t len = static_cast<uint8_t>(perctrl_new_count * perctrl_stride);   // 0x50
			if (!write_bytes(clear + 3, &len, 1))
			{
				for (size_t j = 0; j < std::size(perctrl_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + perctrl_sites[j].rva);
					write_bytes(insn + perctrl_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				note("[splitscreen] s_perController: NOT moved - init clear write failed (rolled back)\n");
				return false;
			}
			perctrl_new = fresh_abs;
			return true;
		}

		// ============ LUI target tables [2 clients] -> [4] ============
		// Four per-client LUI tables (weakpoints, reticle, rocket launcher, arm blade)
		// are [2] on PC and [4] on PS4. Clients 2/3 overran them into the next table
		// and the UI model globals, crashing 4-player MP with bots. Moves all four
		// (48 refs) and widens two clear-loop bounds, at startup.
		struct lui_table
		{
			const char* name;
			uint32_t base;
			uint32_t stride;
			uint32_t old_count;
			uint32_t new_count;
			const entcoll_site* sites;
			size_t site_count;
		};
		constexpr entcoll_site lui_armblade_sites[] = {
			{0x01FF9FF3, 3, 7, true, 0x10}, {0x01FFA053, 3, 7, true, 0x0}, {0x01FFA0D7, 3, 7, true, 0x0},
			{0x01FFA157, 3, 7, true, 0x0}, {0x01FFBA12, 4, 8, false, 0x4}, {0x01FFEB9D, 5, 9, false, 0x8},
			{0x01FFEBAB, 5, 9, false, 0xC}, {0x01FFEBB4, 5, 9, false, 0x10}, {0x01FFEBC5, 4, 8, false, 0x0},
			{0x01FFEBE7, 3, 7, false, 0x14}, {0x01FFEBEE, 3, 7, false, 0x14}, {0x01FFEC0A, 3, 11, false, 0x4},
			{0x01FFEC15, 3, 11, false, 0x14}, {0x01FFEC20, 4, 8, false, 0x8}, {0x0200C192, 3, 7, true, 0x0},
			{0x0200C3E6, 3, 7, true, 0x0},
		};
		constexpr entcoll_site lui_rocket_sites[] = {
			{0x01FFA1A7, 3, 7, true, 0x0}, {0x0200BFDE, 4, 8, false, 0x0}, {0x0200BFFF, 4, 8, false, 0x2},
			{0x0200C25E, 5, 9, false, 0x0}, {0x0200C343, 3, 7, true, 0x0}, {0x0200C5C9, 3, 7, true, 0x0},
		};
		constexpr entcoll_site lui_reticle_sites[] = {
			{0x01FFA187, 3, 7, true, 0x0}, {0x02000639, 4, 9, false, 0x0}, {0x02000689, 6, 10, false, 0x4},
			{0x020006B7, 4, 8, false, 0x0}, {0x020006C1, 6, 10, false, 0x4}, {0x020006DA, 6, 10, false, 0x4},
			{0x020006E6, 4, 8, false, 0x0}, {0x020006F0, 6, 10, false, 0x4}, {0x0200071B, 6, 10, false, 0x4},
			{0x02000760, 4, 8, false, 0x4}, {0x0200078E, 6, 10, false, 0x4}, {0x020007C1, 6, 10, false, 0x4},
		};
		constexpr entcoll_site lui_weakpoint_sites[] = {
			{0x01FDF700, 3, 7, true, 0x0}, {0x01FED34C, 5, 9, false, 0x4}, {0x01FED355, 4, 8, false, 0x0},
			{0x01FEEDC5, 4, 8, false, 0x0}, {0x01FEEE37, 4, 8, false, 0x0}, {0x01FEEEAD, 6, 10, false, 0x8},
			{0x01FEEEC9, 6, 10, false, 0xC}, {0x01FEEF28, 5, 9, false, 0x4}, {0x01FEEF50, 5, 9, false, 0x4},
			{0x01FEEF65, 4, 8, false, 0x0}, {0x01FF2847, 5, 9, false, 0x10}, {0x01FF2868, 5, 9, false, 0x10},
			{0x01FF96E5, 3, 7, true, 0x6}, {0x01FF9734, 3, 7, true, 0x0},
		};
		constexpr lui_table lui_tables[] = {
			{"weakpoints", 0x1626BDB0, 0x14, 20, 40, lui_weakpoint_sites, std::size(lui_weakpoint_sites)},
			{"reticle", 0x1626BF40, 0x8, 2, 4, lui_reticle_sites, std::size(lui_reticle_sites)},
			{"rocket launcher", 0x1626BF58, 0x4, 2, 4, lui_rocket_sites, std::size(lui_rocket_sites)},
			{"arm blade", 0x1626BF60, 0x18, 8, 16, lui_armblade_sites, std::size(lui_armblade_sites)},
		};
		struct lui_bound
		{
			uint32_t rva;
			uint8_t stock[4];
			uint8_t value;
		};
		constexpr lui_bound lui_bounds[] = {
			{0x01FFA01E, {0x48, 0x83, 0xF8, 0x08}, 0x10},   // arm-blade clear: cmp rax,8
			{0x0200C35C, {0x48, 0x83, 0xF9, 0x02}, 0x04},   // rocket-launcher clear: cmp rcx,2
		};
		bool lui_tables_moved = false;

		bool relocate_lui_target_tables()
		{
			if (lui_tables_moved)
			{
				return true;
			}
			const auto b = base();
			for (const auto& bd : lui_bounds)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + bd.rva);
				if (!readable(p, sizeof(bd.stock)) || std::memcmp(p, bd.stock, sizeof(bd.stock)) != 0)
				{
					note("[splitscreen] LUI target tables: NOT moved - a loop bound differs\n");
					return false;
				}
			}
			// All four or none: a table moved alone is still overrun by its neighbour.
			static int32_t saved[std::size(lui_tables)][16]{};
			size_t moved = 0;
			const auto rollback = [&]
			{
				for (size_t t = 0; t < moved; ++t)
				{
					for (size_t j = 0; j < lui_tables[t].site_count; ++j)
					{
						auto* insn = reinterpret_cast<uint8_t*>(b + lui_tables[t].sites[j].rva);
						write_bytes(insn + lui_tables[t].sites[j].disp_off, &saved[t][j], sizeof(int32_t));
					}
				}
			};
			for (const auto& t : lui_tables)
			{
				const size_t bytes = static_cast<size_t>(t.new_count) * t.stride;
				auto* fresh = static_cast<uint8_t*>(allocate_near_module(bytes));
				if (!fresh || t.site_count > 16)
				{
					rollback();
					note("[splitscreen] LUI target tables: NOT moved - allocation failed\n");
					return false;
				}
				// Start empty; the arm blade marks a free record with entity 0x3FF.
				std::memset(fresh, 0, bytes);
				if (t.base == 0x1626BF60)
				{
					for (uint32_t r = 0; r < t.new_count; ++r)
					{
						const int32_t none = 0x3FF;
						std::memcpy(fresh + r * t.stride + 4, &none, sizeof(none));
					}
				}
				if (!rewrite_entcoll(t.sites, t.site_count, t.base, reinterpret_cast<size_t>(fresh), saved[moved]))
				{
					rollback();
					note("[splitscreen] LUI target tables: NOT moved - a reference did not match (rolled back)\n");
					return false;
				}
				++moved;
			}
			size_t bounds_done = 0;
			for (const auto& bd : lui_bounds)
			{
				if (!write_bytes(reinterpret_cast<uint8_t*>(b + bd.rva + 3), &bd.value, 1))
				{
					for (size_t k = 0; k < bounds_done; ++k)
					{
						write_bytes(reinterpret_cast<uint8_t*>(b + lui_bounds[k].rva + 3), &lui_bounds[k].stock[3], 1);
					}
					rollback();
					note("[splitscreen] LUI target tables: NOT moved - a bound write failed (rolled back)\n");
					return false;
				}
				++bounds_done;
			}
			lui_tables_moved = true;
			return true;
		}

		// ============ per-client marker blocks [2] -> [4] ============
		// 32 records of 0x50 per client at lc*0xA00. Clients 2/3 wrote over the
		// cgame media table that follows and crashed 4-player MP.
		constexpr uint32_t cg_marks_base = 0x04748220;
		constexpr uint32_t cg_marks_block = 0xA00;
		constexpr entcoll_site cg_marks_sites[] = {
			{0x00220447, 3, 7, true, 0x38},   // lea rax,[+0x38]   per-client init
			{0x002287DD, 3, 7, true, 0x8},    // lea rax,[+0x8]
			{0x00228863, 3, 7, true, 0x0},    // lea rcx,[base]    block getter
			{0x00228883, 3, 7, true, 0x0},    // lea rcx,[base]    find by entity
			{0x00233BA4, 3, 7, false, 0x0},   // lea rbx,[rax+RVA]
		};
		bool cg_marks_moved = false;

		bool relocate_cg_marker_blocks()
		{
			if (cg_marks_moved)
			{
				return true;
			}
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * cg_marks_block));
			if (!fresh)
			{
				note("[splitscreen] cg marker blocks: NOT moved - allocation failed\n");
				return false;
			}
			std::memset(fresh, 0, 4 * cg_marks_block);   // blocks 2/3: the per-client init sets them up
			if (!readable(reinterpret_cast<const void*>(b + cg_marks_base), 2 * cg_marks_block))
			{
				note("[splitscreen] cg marker blocks: NOT moved - old blocks unreadable\n");
				return false;
			}
			std::memcpy(fresh, reinterpret_cast<const void*>(b + cg_marks_base), 2 * cg_marks_block);
			static int32_t saved[std::size(cg_marks_sites)]{};
			if (!rewrite_entcoll(cg_marks_sites, std::size(cg_marks_sites), cg_marks_base,
			                     reinterpret_cast<size_t>(fresh), saved))
			{
				note("[splitscreen] cg marker blocks: NOT moved - a reference did not match\n");
				return false;
			}
			cg_marks_moved = true;
			return true;
		}

		// --- cg_localEntities: [2] -> [4], a flat relocation ----------------
		// CG_InitLocalEntities(lc) memsets cg_localEntities[lc] (0x7C00 bytes); for
		// lc 2 that wiped the clientfield system and broke the round start.
		// PS4 CG_InitLocalEntities 0x0021ED10 uses the same length, so 128 entities of
		// 0xF8 bytes hold on PC; only the client dimension differs ([4] vs [2]).
		// The client index is outside the element (lc*0x7C00, lc*0xF8, lc*8), so no
		// field offset changes, and the only client bound reads cl_maxLocalClients.
		// Sites are matched by full expected bytes, never by pattern.
		// History: LOG.md, cg_localEntities
		constexpr uint32_t le_active_rva = 0x0494A900;
		constexpr uint32_t le_free_rva = 0x0494AAF0;
		constexpr uint32_t le_pool_rva = 0x0494AB10;
		constexpr uint32_t le_entity_size = 0xF8;
		constexpr uint32_t le_entities_per_client = 128;
		constexpr uint32_t le_clients = 4;

		constexpr uint32_t le_old_active_size = 2 * le_entity_size;   // 0x1F0
		constexpr uint32_t le_old_free_size = 2 * 8;                  // 0x10
		constexpr uint32_t le_old_pool_size =
			2 * le_entities_per_client * le_entity_size;              // 0xF800

		constexpr uint32_t le_new_active_size = le_clients * le_entity_size; // 0x3E0
		constexpr uint32_t le_new_pool_size =
			le_clients * le_entities_per_client * le_entity_size;            // 0x1F000

		// The pool sits at 0x400 to keep 1024-byte alignment (SIMD fields).
		constexpr uint32_t le_off_active = 0;
		constexpr uint32_t le_off_free = le_new_active_size;           // 0x3E0
		constexpr uint32_t le_off_pool = 0x400;
		constexpr uint32_t le_block_size = le_off_pool + le_new_pool_size; // 0x1F400

		enum le_array_id : uint8_t { le_active = 0, le_free = 1, le_pool = 2 };

		struct le_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint8_t arr;
			uint32_t delta;   // byte offset within that array
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[9];
		};

		constexpr le_ref le_refs[] = {
			{0x0083B986, 7, 3, le_active, 0x0, true, {0x48, 0x8D, 0x05, 0x73, 0xEF, 0x10, 0x04}},
			{0x0083D20B, 8, 4, le_free, 0x0, false, {0x4B, 0x8B, 0x84, 0xE5, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D217, 8, 4, le_free, 0x0, false, {0x4B, 0x89, 0x9C, 0xE5, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D519, 9, 4, le_free, 0x0, false, {0x48, 0x83, 0xBC, 0xFE, 0xF0, 0xAA, 0x94, 0x04, 0x00}},
			{0x0083D52E, 8, 4, le_active, 0x0, false, {0x48, 0x8B, 0x94, 0x32, 0x00, 0xA9, 0x94, 0x04}},
			{0x0083D53B, 8, 4, le_free, 0x0, false, {0x48, 0x8B, 0x9C, 0xFE, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D552, 8, 4, le_free, 0x0, false, {0x48, 0x89, 0x84, 0xFE, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D569, 8, 4, le_active, 0x8, false, {0x48, 0x8B, 0x8C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D575, 7, 3, le_active, 0x0, false, {0x48, 0x8D, 0x8E, 0x00, 0xA9, 0x94, 0x04}},
			{0x0083D582, 8, 4, le_active, 0x8, false, {0x48, 0x8B, 0x8C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D58D, 8, 4, le_active, 0x8, false, {0x48, 0x89, 0x9C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D5F9, 7, 3, le_free, 0x0, true, {0x48, 0x8D, 0x0D, 0xF0, 0xD4, 0x10, 0x04}},
			{0x0083D632, 7, 3, le_pool, 0x0, true, {0x48, 0x8D, 0x35, 0xD7, 0xD4, 0x10, 0x04}},
			{0x0083D667, 7, 3, le_active, 0x0, false, {0x48, 0x8D, 0x82, 0x00, 0xA9, 0x94, 0x04}},
			{0x0083D66E, 8, 4, le_free, 0x0, false, {0x48, 0x89, 0x9C, 0xFA, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D67D, 8, 4, le_active, 0x8, false, {0x48, 0x89, 0x84, 0x11, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D68B, 7, 3, le_pool, 0x8, true, {0x48, 0x8D, 0x0D, 0x86, 0xD4, 0x10, 0x04}},
		};

		bool local_entities_relocated = false;

		bool relocate_local_entities()
		{
			if (local_entities_relocated)
			{
				return true;
			}

			const auto b = base();

			// Every site must match its recorded bytes, or nothing is written.
			for (const auto& r : le_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}

			// cg_freeLocalEntities holds pointers into the pool, so nothing is copied:
			// refuse unless all three arrays are still zero (before any map).
			const auto all_zero = [&](const uint32_t rva, const uint32_t size)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + rva);
				if (!readable(p, size))
				{
					return false;
				}
				for (uint32_t i = 0; i < size; ++i)
				{
					if (p[i] != 0)
					{
						return false;
					}
				}
				return true;
			};
			if (!all_zero(le_active_rva, le_old_active_size)
				|| !all_zero(le_free_rva, le_old_free_size)
				|| !all_zero(le_pool_rva, le_old_pool_size))
			{
				return false;
			}

			auto* destination = allocate_near_module(le_block_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			const auto target_rva = [&](const le_ref& r) -> size_t
			{
				const uint32_t arr_off = r.arr == le_active
					                         ? le_off_active
					                         : (r.arr == le_free ? le_off_free : le_off_pool);
				return new_base + arr_off + r.delta;
			};
			// rip-relative counts from the end of the instruction; the ABS32 form is
			// [reg + disp32] with reg = image base, so its displacement is the RVA.
			const auto field_value = [&](const le_ref& r)
			{
				const auto tgt = target_rva(r);
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			int32_t old_values[std::size(le_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + le_refs[i].rva + le_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			for (size_t i = 0; i < std::size(le_refs); ++i)
			{
				const auto& r = le_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			// Read every field back before declaring success.
			for (size_t i = 0; i < std::size(le_refs); ++i)
			{
				const auto& r = le_refs[i];
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + r.rva + r.disp_off), sizeof(seen));
				if (seen != field_value(r))
				{
					rollback();
					return false;
				}
			}

			local_entities_relocated = true;
			return true;
		}

		// --- RadiantExploderData: [2] -> [4], with a layout change ----------
		// Each 0x3930-byte record holds effectCount[2] at +0x19E8 and effects[2][500]
		// at +0x19F0 with no slack, so writing effectCount[2] crashed a round.
		// New: effectCount[4] at +0x19E8, effects[4][500] at +0x19F8, stride 0x5878.
		// No record transform: the array is per-map data and still empty here.
		// +0x19E8 / +0x19F0 also occur in unrelated structures, so every site is an
		// explicit address with expected bytes.
		constexpr uint32_t exploder_count_rva = 0x043016C4;
		constexpr uint32_t exploder_new_size = 0x587800;
		constexpr uint32_t exploder_new_stride = 0x5878;
		constexpr uint32_t exploder_counts_off = 0x19E8;   // unchanged
		constexpr uint32_t exploder_new_effects_off = 0x19F8;

		// Expected original bytes; a different build fails closed.
		constexpr uint8_t exploder_expect_A49[] = {0x48, 0x69, 0xC9, 0x30, 0x39, 0x00, 0x00};
		constexpr uint8_t exploder_expect_A5C[] = {0x48, 0x81, 0xC6, 0x30, 0x39, 0x00, 0x00};
		constexpr uint8_t exploder_expect_703[] = {0x49, 0x81, 0xC0, 0x30, 0x39, 0x00, 0x00};
		constexpr uint8_t exploder_expect_7A4[] = {0x41, 0xB8, 0x00, 0x30, 0x39, 0x00};
		constexpr uint8_t exploder_expect_AF7[] = {0x48, 0x89, 0xAC, 0xC6, 0xF0, 0x19, 0x00, 0x00};
		constexpr uint8_t exploder_expect_AC9[] = {0x48, 0x8D, 0x48, 0x08};
		constexpr uint8_t exploder_expect_589[] = {0x48, 0x8D, 0x3D, 0x40, 0xDB, 0x0F, 0x04};
		constexpr uint8_t exploder_expect_5D7[] = {0x48, 0x8D, 0x3D, 0xF2, 0xDA, 0x0F, 0x04};
		constexpr uint8_t exploder_expect_6E3[] = {0x48, 0x8D, 0x3D, 0xE6, 0xD9, 0x0F, 0x04};
		constexpr uint8_t exploder_expect_5CF[] = {0x48, 0x63, 0x84, 0x87, 0xC8, 0x30, 0x30, 0x04};
		constexpr uint8_t exploder_expect_79B[] = {0x48, 0x8D, 0x0D, 0x3E, 0x3F, 0x10, 0x04};
		constexpr uint8_t exploder_expect_A38[] = {0x48, 0x8D, 0x35, 0xA1, 0x0C, 0x10, 0x04};

		bool exploders_relocated = false;

		bool relocate_radiant_exploders()
		{
			if (exploders_relocated)
			{
				return true;
			}

			const auto b = base();

			// Verify every original site first; one mismatch and nothing is written.
			const auto expect = [&](const uint32_t rva, const uint8_t* want,
			                        const size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, want, n) == 0;
			};
			if (!expect(0x00200A49, exploder_expect_A49, sizeof(exploder_expect_A49))
				|| !expect(0x00200A5C, exploder_expect_A5C, sizeof(exploder_expect_A5C))
				|| !expect(0x00205703, exploder_expect_703, sizeof(exploder_expect_703))
				|| !expect(0x001FD7A4, exploder_expect_7A4, sizeof(exploder_expect_7A4))
				|| !expect(0x00200AF7, exploder_expect_AF7, sizeof(exploder_expect_AF7))
				|| !expect(0x00200AC9, exploder_expect_AC9, sizeof(exploder_expect_AC9))
				|| !expect(0x00205589, exploder_expect_589, sizeof(exploder_expect_589))
				|| !expect(0x002055D7, exploder_expect_5D7, sizeof(exploder_expect_5D7))
				|| !expect(0x002056E3, exploder_expect_6E3, sizeof(exploder_expect_6E3))
				|| !expect(0x002055CF, exploder_expect_5CF, sizeof(exploder_expect_5CF))
				|| !expect(0x001FD79B, exploder_expect_79B, sizeof(exploder_expect_79B))
				|| !expect(0x00200A38, exploder_expect_A38, sizeof(exploder_expect_A38)))
			{
				return false;
			}

			// A non-zero live count means records exist that a zeroed copy would lose.
			uint32_t live_records = 0;
			std::memcpy(&live_records,
			            reinterpret_cast<const void*>(b + exploder_count_rva),
			            sizeof(live_records));
			if (live_records != 0)
			{
				return false;
			}

			auto* destination = allocate_near_module(exploder_new_size);
			if (!destination)
			{
				// No reachable 5.8 MB hole within a 32-bit displacement.
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;
			// VirtualAlloc zero-fills and the source is all zeros: nothing to copy.

			struct pending
			{
				uint32_t rva;
				uint8_t disp_offset;
				int32_t value;
			};

			const auto rip_to = [&](const size_t insn, const size_t len,
			                        const size_t target)
			{
				return static_cast<int32_t>(target - (insn + len));
			};

			const pending writes[] = {
				// base references
				{0x001FD79B, 3, rip_to(0x001FD79B, 7, new_base)},
				{0x00200A38, 3, rip_to(0x00200A38, 7, new_base)},
				// &effects[0][0] moves to the new field offset, not the old one
				{0x00205589, 3, rip_to(0x00205589, 7, new_base + exploder_new_effects_off)},
				{0x002055D7, 3, rip_to(0x002055D7, 7, new_base + exploder_new_effects_off)},
				{0x002056E3, 3, rip_to(0x002056E3, 7, new_base + exploder_new_effects_off)},
				// &effectCount[0]: ABS32 displacement off rdi, which holds the image base.
				{0x002055CF, 4, static_cast<int32_t>(new_base + exploder_counts_off)},
				// strides and sizes
				{0x00200A49, 3, static_cast<int32_t>(exploder_new_stride)},
				{0x00200A5C, 3, static_cast<int32_t>(exploder_new_stride)},
				{0x00205703, 3, static_cast<int32_t>(exploder_new_stride)},
				{0x001FD7A4, 2, static_cast<int32_t>(exploder_new_size)},
				// the one effects displacement that belongs to this array
				{0x00200AF7, 4, static_cast<int32_t>(exploder_new_effects_off)},
			};

			int32_t old_values[std::size(writes)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + writes[i].rva + writes[i].disp_offset),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			for (size_t i = 0; i < std::size(writes); ++i)
			{
				auto* field = reinterpret_cast<void*>(
					b + writes[i].rva + writes[i].disp_offset);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &writes[i].value, sizeof(writes[i].value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			// `lea rcx,[rax+8]` -> `[rax+0x10]`: the init loop now clears four counts.
			const uint8_t clear_len = 0x10;
			auto* clear_field = reinterpret_cast<void*>(b + 0x00200AC9 + 3);
			const uint8_t old_clear = *reinterpret_cast<const uint8_t*>(clear_field);
			if (!write_bytes(clear_field, &clear_len, sizeof(clear_len)))
			{
				rollback();
				return false;
			}

			// Read every field back before declaring success.
			for (size_t i = 0; i < std::size(writes); ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + writes[i].rva + writes[i].disp_offset),
				            sizeof(seen));
				if (seen != writes[i].value)
				{
					write_bytes(clear_field, &old_clear, sizeof(old_clear));
					rollback();
					return false;
				}
			}

			exploders_relocated = true;
			return true;
		}
