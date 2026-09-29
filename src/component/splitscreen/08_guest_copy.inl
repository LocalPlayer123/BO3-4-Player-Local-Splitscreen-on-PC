// Guests start from player 1's classes and stats.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Guests start from player 1's classes and stats ----
		// As on console: PS4 Live_HandleClientSplitscreenSignin (0xC16080) calls
		// LiveStats_CopyFromSponsor (0xC6A6B0) for a guest without stats, so the guest plays
		// with the sponsor's level, unlocks and classes and keeps nothing. On PC one function
		// reads every controller's "<name>_<controller>.cgp" (one call site, 0x58-byte file
		// descriptors). The hook runs just before it and, for controllers 1..3, copies player 1's
		// <name>_0.cgp over <name>_N.cgp for the loadout and stats files; the game then reads a
		// real file through its own path.
		constexpr size_t save_desc_stride = 0x58;
		constexpr size_t save_desc_name_max = 0x40;
		constexpr size_t save_desc_other_dir = 0x4C;
		// save_dir_branch_rva: the branch on the descriptor's "other directory" byte. BOIII (patch_players_folder_name)
		// makes it a jmp (0xEB), so every file goes to boiii_players whatever the byte says.
		constexpr const char* sponsor_copy_names[] = {
			"loadouts_zm_offline", "loadouts_mp_offline", "loadouts_cp_offline",
			"stats_zm_offline", "stats_mp_offline", "stats_cp_offline",
			"stats_cp_nightmare_offline", "stats_fr_offline",
		};
		bool guest_copy_installed = false;

		bool is_sponsor_copy_name(const char* name)
		{
			for (const auto* n : sponsor_copy_names)
			{
				if (std::strcmp(name, n) == 0)
				{
					return true;
				}
			}
			return false;
		}

		void save_read_stub(const int controller, uint8_t* files, const int count)
		{
			size_t failed = 0;
			if (controller >= 1 && controller < 4 && files && count > 0)
			{
				const auto b = base();
				const bool dir_forced = *reinterpret_cast<const uint8_t*>(b + save_dir_branch_rva) == 0xEB;
				const auto* dvar = *reinterpret_cast<void* const*>(b + save_base_dvar_rva);
				const auto get_string = reinterpret_cast<const char* (*)(const void*)>(b + dvar_get_string_rva);
				const char* root = dvar ? get_string(dvar) : nullptr;
				for (int i = 0; root && *root && i < count; ++i)
				{
					const auto* desc = files + static_cast<size_t>(i) * save_desc_stride;
					if (strnlen(reinterpret_cast<const char*>(desc), save_desc_name_max) >= save_desc_name_max)
					{
						continue;
					}
					const auto* name = reinterpret_cast<const char*>(desc);
					if (!dir_forced && desc[save_desc_other_dir] != 0)
					{
						continue;
					}
					if (!is_sponsor_copy_name(name))
					{
						continue;
					}
					char src[MAX_PATH]{};
					char dst[MAX_PATH]{};
					const auto ns = std::snprintf(src, sizeof(src), "%s\\boiii_players\\%s_0.cgp", root, name);
					const auto nd = std::snprintf(dst, sizeof(dst), "%s\\boiii_players\\%s_%d.cgp", root, name, controller);
					if (ns <= 0 || nd <= 0 || ns >= MAX_PATH || nd >= MAX_PATH)
					{
						++failed;
						continue;
					}
					if (GetFileAttributesA(src) == INVALID_FILE_ATTRIBUTES)
					{
						continue;   // player 1 has no such file yet - leave the guest's alone
					}
					if (!CopyFileA(src, dst, FALSE))
					{
						++failed;
					}
				}
			}

			reinterpret_cast<void (*)(int, uint8_t*, int)>(base() + save_read_rva)(controller, files, count);

			if (failed)
			{
				note("save read: controller %d, %zu copies from player 1 failed", controller, failed);
			}
		}

		void install_guest_copy()
		{
			if (guest_copy_installed)
			{
				return;
			}
			if (!call_site_targets(save_read_callsite, save_read_rva))
			{
				note("guest copy: NOT installed - 0x%08X is not call 0x%08X", save_read_callsite, save_read_rva);
				return;
			}
			try
			{
				utils::hook::call(base() + save_read_callsite, save_read_stub);
			}
			catch (...)
			{
				note("guest copy: NOT installed - hook failed");
				return;
			}
			guest_copy_installed = true;
		}

		// The game reads the saves only once, at boot, so each join re-reads the eight files for
		// a fresh copy (PS4 CanPerformFileOp 0xF80F80 refuses nothing for a second read).
		// Storage_Read (ecx controller, edx type, r8d slot -> bool) is entered at its own
		// address so storage_read_stub's guest filter still applies. Types (PC property table):
		//   11 loadouts_cp_offline  15 loadouts_mp_offline  20 loadouts_zm_offline
		//    7 stats_cp_offline      9 stats_cp_nightmare_offline
		//   13 stats_mp_offline     18 stats_zm_offline     22 stats_fr_offline
		constexpr int sponsor_copy_types[] = {11, 15, 20, 7, 9, 13, 18, 22};

		void reread_guest_saves(const int controller)
		{
			if (!guest_copy_installed || controller < 1 || controller > 3)
			{
				return;
			}
			const auto read = reinterpret_cast<bool (*)(int, int, int)>(base() + storage_read_rva);
			for (const auto type : sponsor_copy_types)
			{
				read(controller, type, 0);
			}
		}
