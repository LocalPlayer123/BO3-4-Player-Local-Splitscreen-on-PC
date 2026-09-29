// Guest storage: s_targets / s_localFileOpData widening, storage pump for controllers 2/3.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// --- s_targets: the storage pending-vector table ---
		//
		// Needed before controller 2 or 3 can be pumped at all. PS4
		// GetPendingVector (0xF81A40): &s_targets + type*ROW + 0x30
		// + controller*0x410 + operation*0x208, with ROW 0x850 on PC (two
		// controllers) and 0x1070 on PS4 (four). Four target types, so the table
		// grows 0x2140 -> 0x41C0. Without it, storage calls for controller 2
		// overwrite the next target's activeQuery pointer. The row stride changes,
		// so each row is copied on its own.
		//
		// Do not touch the SIB scale in `lea rbx,[rbx + r13*2]`: that 2 is
		// operations per controller (0x410 == 2 * 0x208), not the controller count.
		constexpr size_t targets_rva = 0x033BCDF0;
		constexpr size_t targets_types = 4;
		constexpr size_t targets_old_row = 0x850;
		constexpr size_t targets_new_row = 0x1070;

		struct targets_lea
		{
			uint32_t insn_rva;  // 7-byte rip-relative lea, disp32 at +3
			uint32_t offset;    // offset into the table it points at
		};

		constexpr targets_lea targets_leas[] = {
			{0x0221B146, 0x00}, {0x0221B28F, 0x00}, {0x0221B2E2, 0x00}, {0x0221B50A, 0x00},
			{0x0221B1A4, 0x08},
			{0x0221B223, 0x20}, {0x0221B203, 0x28},
			{0x0221B2BE, 0x30}, {0x0221B5D1, 0x30},
		};

		// Every imm32 in the storage TU that carries the old row stride 0x850 (six,
		// found with tools/pe_find.py). One is not an imul but `add rdi, 0x850`
		// (0x02277CF4) in StorageTarget_GetType (PS4 0xF81890); missing it walks
		// the new table with the old geometry. The old total 0x2140 appears nowhere.
		constexpr uint32_t targets_strides[] = {
			0x0221B152, 0x0221B1C4, 0x0221B20D, 0x0221B22D, 0x0221B28B, 0x0221B5E2,
		};

		// Needed: once s_storage[2] holds an xuid the game does storage work for
		// controller 2 by itself, and every such call reads past a two-controller
		// row. History: LOG.md, "targets_widen".
		bool widen_storage_targets()
		{
			const auto module_base = base();
			const auto old_table = module_base + targets_rva;

			// Verify every site before writing anything: all or nothing.
			for (const auto& l : targets_leas)
			{
				const auto insn = module_base + l.insn_rva;
				const auto* b = reinterpret_cast<const uint8_t*>(insn);
				if (b[1] != 0x8D || (b[0] != 0x48 && b[0] != 0x4A && b[0] != 0x4C && b[0] != 0x4E))
				{
					return false;
				}
				int32_t disp = 0;
				std::memcpy(&disp, reinterpret_cast<const void*>(insn + 3), sizeof(disp));
				if (insn + 7 + disp != old_table + l.offset)
				{
					return false;
				}
			}
			for (const auto rva : targets_strides)
			{
				uint32_t imm = 0;
				std::memcpy(&imm, reinterpret_cast<const void*>(module_base + rva), sizeof(imm));
				if (imm != targets_old_row)
				{
					return false;
				}
			}

			auto* fresh = allocate_near_module(targets_types * targets_new_row);
			if (!fresh)
			{
				return false;
			}
			const auto new_table = reinterpret_cast<size_t>(fresh);

			std::memset(fresh, 0, targets_types * targets_new_row);
			for (size_t t = 0; t < targets_types; ++t)
			{
				std::memcpy(reinterpret_cast<void*>(new_table + t * targets_new_row),
				            reinterpret_cast<const void*>(old_table + t * targets_old_row),
				            targets_old_row);
			}

			for (const auto& l : targets_leas)
			{
				const auto insn = module_base + l.insn_rva;
				const auto disp = static_cast<int32_t>(
					static_cast<int64_t>(new_table + l.offset) - static_cast<int64_t>(insn + 7));
				write_bytes(reinterpret_cast<void*>(insn + 3), &disp, sizeof(disp));
			}
			const uint32_t row = static_cast<uint32_t>(targets_new_row);
			for (const auto rva : targets_strides)
			{
				write_bytes(reinterpret_cast<void*>(module_base + rva), &row, sizeof(row));
			}

			return true;
		}

		// --- s_localFileOpData ---
		//
		// PS4 StartOp (0xF7C740) indexes s_localFileOpData[ci] (stride 0x1820), a
		// four-element global. On PC it is [2] at 0x17908CF0 and element 2 lands
		// on the A/B experiments table, so controller 2's local-file work
		// corrupted it (crash at 0x02275034). Only the count changes, so it is a
		// flat copy with one address-taking reference (0x02274BFE).
		constexpr size_t localfileop_rva = 0x17889DF0;
		constexpr size_t localfileop_elem = 0x1820;
		constexpr uint32_t localfileop_lea = 0x022180CE; // 7-byte lea, disp32 at +3

		bool widen_local_file_ops()
		{
			const auto module_base = base();
			const auto old_array = module_base + localfileop_rva;
			const auto insn = module_base + localfileop_lea;

			const auto* b = reinterpret_cast<const uint8_t*>(insn);
			if (b[0] != 0x48 || b[1] != 0x8D)
			{
				return false;
			}
			int32_t disp = 0;
			std::memcpy(&disp, reinterpret_cast<const void*>(insn + 3), sizeof(disp));
			if (insn + 7 + disp != old_array)
			{
				return false;
			}

			auto* fresh = allocate_near_module(4 * localfileop_elem);
			if (!fresh)
			{
				return false;
			}
			const auto new_array = reinterpret_cast<size_t>(fresh);
			std::memset(fresh, 0, 4 * localfileop_elem);
			std::memcpy(fresh, reinterpret_cast<const void*>(old_array),
			            2 * localfileop_elem);

			const auto new_disp = static_cast<int32_t>(
				static_cast<int64_t>(new_array) - static_cast<int64_t>(insn + 7));
			if (!write_bytes(reinterpret_cast<void*>(insn + 3), &new_disp, sizeof(new_disp)))
			{
				return false;
			}

			return true;
		}

		// Once installed, Storage_Pump starts with a jmp into our stub, and
		// invoke() runs the original.
		utils::hook::detour storage_pump_hook;

		// Com_ControllerIndex_GetLocalClientNum (PC 0x020EF7C0). On PS4 the lobby's
		// gobblegum row reaches BG_UnlockablesGetLocalCACRoot (0xE4130), which
		// asserts that CG_GetLocalClientGlobals(GetLocalClientNum(ci)) is non-null.
		// Retail PC has no asserts, so a -1 silently draws an empty row.
		constexpr uint32_t local_client_num_rva = 0x020E3040;

		// cl_maxLocalClients (old RVA 0x053A2720), stored by the allocator at
		// 0x0135D489.
		constexpr uint32_t cl_max_local_clients_rva = 0x05323720;
		// Enables the count patches and the cl_maxLocalClients hold. The caller
		// applies them only after the container relocations succeeded.
		bool raise_local_client_count = true;
