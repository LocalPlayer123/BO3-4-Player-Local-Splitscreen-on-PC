// Client-script local-client bound (follows cl_maxLocalClients).
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Client-script local-client bound ----
		// PS4 CScr_GetLocalClientNum (0x1D926A0) accepts local clients 0..3; the PC inlines the
		// check into every client-script builtin with the bound 2:
		//   call Scr_GetInt / cmp reg, 1 / jbe ok
		// For lc 2 the script error aborted the clientfield callback dispatcher before client 2's
		// queue was cleared, so it re-dispatched forever (main-thread hang). The table holds the
		// imm8 of each `cmp reg, 1` (tools/gen_csc_lc_checks.py). All-or-nothing: every byte
		// must read 0x01 before any is written.
		bool csc_lc_widened = false;

		// The bound follows cl_maxLocalClients: sync_csc_lc_bound applies min(3, cl_max - 1).
		// A fixed 3 let lc 2/3 into these builtins in the lobby (cl_max 2), where they get a NULL
		// cg and crash (CScr_SetShowcaseWeaponPaintshopXUID). The sync runs right after the only
		// two writers of cl_maxLocalClients, on the main thread. Do not use an async loop: the
		// async pipelines stop during a map load and the 4-player load hung.
		void widen_csc_lc_checks()
		{
			if (csc_lc_widened)
			{
				return;
			}
			const auto b = base();
			for (const auto rva : csc_lc_check_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + rva);
				if (!readable(at, 1) || *at != 0x01)
				{
					note("[splitscreen] csc lc bound: 0x%X is not stock - nothing written\n", rva);
					return;
				}
			}
			// Slot-3 audit (tools/audit_csc_slot3.py): only clientUIActives' owned slot-3 head is
			// left. The bytes stay stock here; sync_csc_lc_bound writes the bound.
			csc_lc_widened = true;
		}

		bool odd_lc_owned[std::size(odd_lc_checks)]{};
		uint8_t odd_lc_applied[std::size(odd_lc_checks)]{};

		void widen_odd_lc_checks()
		{
			for (size_t i = 0; i < std::size(odd_lc_checks); ++i)
			{
				const auto& c = odd_lc_checks[i];
				auto* at = reinterpret_cast<uint8_t*>(base() + c.rva);
				odd_lc_owned[i] = readable(at, c.len) && std::memcmp(at, c.stock, c.len) == 0
					&& write_bytes(at, c.patched, c.len);
				odd_lc_applied[i] = 1;
				if (!odd_lc_owned[i])
				{
					note("[splitscreen] %s: lc check bytes differ - not widened\n", c.name);
				}
			}
		}

		uint8_t csc_lc_bound_applied = 1;

		void sync_csc_lc_bound()
		{
			if (!csc_lc_widened)
			{
				return;
			}
			const auto b = base();
			const auto max_local = *reinterpret_cast<const volatile uint32_t*>(b + cl_max_local_clients_rva);
			const uint8_t bound = max_local >= 4 ? 3 : max_local == 3 ? 2 : 1;
			if (bound != csc_lc_bound_applied)
			{
				for (const auto rva : csc_lc_check_imms)
				{
					write_bytes(reinterpret_cast<uint8_t*>(b + rva), &bound, 1);
				}
				csc_lc_bound_applied = bound;
			}
			for (size_t i = 0; i < std::size(odd_lc_checks); ++i)
			{
				const auto& c = odd_lc_checks[i];
				const bool held = (c.needs_signin && !signin_relocated)
					|| (c.needs_lui_roots && !lui_roots_relocated);
				const uint8_t want = held ? 1 : bound;
				if (odd_lc_owned[i] && want != odd_lc_applied[i])
				{
					write_bytes(reinterpret_cast<uint8_t*>(b + c.rva + c.imm), &want, 1);
					odd_lc_applied[i] = want;
				}
			}
		}

		// AllocatePerLocalClientMemory stores cl_maxLocalClients as its last act and
		// CL_FreePerLocalClientMemory zeroes it; nothing else writes it. Neither has an Arxan
		// caller guard and ezz hooks neither, so both are detoured to resync the bound.
		utils::hook::detour alloc_per_lc_hook;
		utils::hook::detour free_per_lc_hook;

		uint64_t alloc_per_lc_stub(const int a, const int b, const int c)
		{
			const auto r = alloc_per_lc_hook.invoke<uint64_t>(a, b, c);
			sync_csc_lc_bound();
			return r;
		}

		uint64_t free_per_lc_stub(const bool a)
		{
			const auto r = free_per_lc_hook.invoke<uint64_t>(a);
			sync_csc_lc_bound();
			return r;
		}

		void install_lc_bound_hooks()
		{
			if (!csc_lc_widened)
			{
				return;
			}
			const auto b = base();
			if (!readable(reinterpret_cast<const void*>(b + alloc_per_lc_rva), sizeof(alloc_per_lc_head))
				|| std::memcmp(reinterpret_cast<const void*>(b + alloc_per_lc_rva), alloc_per_lc_head, sizeof(alloc_per_lc_head)) != 0
				|| !readable(reinterpret_cast<const void*>(b + free_per_lc_rva), sizeof(free_per_lc_head))
				|| std::memcmp(reinterpret_cast<const void*>(b + free_per_lc_rva), free_per_lc_head, sizeof(free_per_lc_head)) != 0)
			{
				csc_lc_widened = false;   // no trigger - the bound stays stock
				note("[splitscreen] csc lc bound: allocator prologues differ - not widened\n");
				return;
			}
			alloc_per_lc_hook.create(b + alloc_per_lc_rva, reinterpret_cast<void*>(&alloc_per_lc_stub));
			free_per_lc_hook.create(b + free_per_lc_rva, reinterpret_cast<void*>(&free_per_lc_stub));
			sync_csc_lc_bound();
		}
