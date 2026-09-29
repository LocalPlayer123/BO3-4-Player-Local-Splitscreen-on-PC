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
		constexpr uint32_t csc_lc_check_imms[] = {
			0x0028A308, 0x00293969, 0x00293A1A, 0x00293A7C, 0x002B41CB, 0x002DDF33,
			0x002E5B63, 0x002E7413, 0x002E8D03, 0x002EC2A6, 0x002EC2F3, 0x002EF4FA,
			0x002EF59C, 0x002F0E6A, 0x002F0EE3, 0x003284FA, 0x0032855A, 0x00368C1A,
			0x00368FE8, 0x003694DC, 0x0036C73C, 0x0036FB2A, 0x0036FBA1, 0x00372E1A,
			0x0037AAC4, 0x0037AB6A, 0x0039294A, 0x003929AA, 0x00392A0A, 0x00395CAF,
			0x00395D2A, 0x00395D91, 0x003AC01A, 0x003AC0DA, 0x003AC13A, 0x003B24BA,
			0x003B3E16, 0x003B3F6F, 0x003B592F, 0x003B8B8A, 0x003B8C16, 0x003B8FAF,
			0x003BA90A, 0x003BC21A, 0x003BC27A, 0x003BC308, 0x003BC463, 0x003D8293,
			0x003DE4EF, 0x003DE564, 0x003DE5AA, 0x003DE60A, 0x003DE66A, 0x003DE6CA,
			0x003DE75A, 0x003DE82F, 0x003DE8E4, 0x003DE9AF, 0x003DEA54, 0x003E984C,
			0x003ED234, 0x003EEDD9, 0x003EF00A, 0x003FF0D2, 0x00412A33, 0x004209C2,
			0x00422265, 0x00423CA5, 0x004255B4, 0x004257FA, 0x00425925, 0x00425A7D,
			0x00425B61, 0x004274EA, 0x004275D1, 0x00428FE4, 0x0042924C, 0x0042AAE4,
			0x0042AB66, 0x00435E84, 0x00A3630C, 0x00A3F43C, 0x00A8444B, 0x00A8DA73,
			0x00A9A624, 0x00AA0A94, 0x00AAD334, 0x00AB686A, 0x00AB9E71, 0x00ACCA84,
			0x00ACFBB4, 0x00B582E8, 0x00B583B9, 0x00B61869, 0x00B7A497, 0x00B7EF11,
			0x00B824E1, 0x00BC53D4, 0x00BC542C, 0x00BC6CDC, 0x00BE8296, 0x00BF095B,
			0x00C07E54, 0x00C224DC, 0x00C30536, 0x00C383D1, 0x00C4041F, 0x00C6728A,
			0x00C7BC62, 0x00C7EDE2, 0x00C82289, 0x00C8252A, 0x00C86DFE, 0x00C8D28C,
			0x00C8EB2C, 0x00C9FBB8, 0x00C9FCAC, 0x00CA5F19, 0x00CA909A, 0x00CBA12E,
			0x00CBA24D, 0x00CBA304, 0x00CBA3E4, 0x00CBA4B4, 0x00CBA58E, 0x00CBA6B4,
			0x00CBA76A, 0x00CBA84F, 0x00CBA8CF, 0x00CBA93F, 0x00CBA9E3, 0x00CBCC90,
			0x00CC1873, 0x00CC3123, 0x00CCADCB, 0x00CCB38C, 0x00CE56B4, 0x00CE5723,
			0x00CE57E4, 0x00CE5834, 0x00CE588C, 0x00CE715C, 0x00CF86AB, 0x00CFD1CB,
			0x00D254CA, 0x00D2E913, 0x00D301E1, 0x00D31AB3, 0x00D333B8, 0x00D46288,
			0x00D46318, 0x00D463A8, 0x00D46488, 0x00D46518, 0x00D4DF98, 0x00D4E07C,
			0x00D4F938, 0x00D4F9F8, 0x00D4FA78, 0x00D4FAEB, 0x00D5136D, 0x00D53289,
			0x00D57C31, 0x00D5C6A8, 0x00D5C714, 0x00D5C78B, 0x00D5E1EA, 0x00D6475B,
			0x00D647DC, 0x00D6C1CC, 0x00D6DA64, 0x00D73DBC, 0x00D97681, 0x00D9F424,
			0x00D9F484, 0x00D9F504, 0x00DF9DA4, 0x00DF9E0C, 0x00DFE8AC, 0x00E00144,
			0x00E001A4, 0x00E001F4, 0x00E0025C, 0x00E04C2F, 0x00E2183E, 0x00E2946F,
			0x00E2AEA1, 0x00E2C83A, 0x00E2E186, 0x00E2FB6E, 0x00E314CD, 0x00E32DFD,
			0x00E34711, 0x00E360AA, 0x00E36226, 0x00E363D7, 0x00E36469, 0x00E44376,
			0x00E45F85, 0x00E63981, 0x00E65289, 0x00E69CFC, 0x00E6B5DC, 0x00E6CECC,
			0x00E6E7BC, 0x00E93371, 0x00EA12F3, 0x00EE3D51, 0x00EE560A, 0x00F0175C,
			0x00F0C47E, 0x00F0DD86, 0x00F29D51, 0x00F2B7D0, 0x00F2B8D5, 0x00F41B44,
			0x00F5101B,
		};
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

		// ---- Builtins with their own form of the check ----
		// These compare lc against a register holding 1, or use `cmp eax,2 / jl`, so they are not
		// in the table above and raised a script error for players 3/4 (no screen filters, ...).
		// PS4 accepts 0..3 in all of them (CScr_GetLocalClientNum 0x1D926A0, CPlayerCmd_HasPerk
		// 0x28C830, CPlayerCmd_GetPerks 0x28C930). Each window is rewritten once to
		// `cmp eax,1 / jbe|ja` - same length, same instruction boundaries and jump targets, stock
		// bytes verified first - and sync_csc_lc_bound owns the imm8 from then on. Every array
		// they index for lc 2/3 is per cl_maxLocalClients (cgArray, entity pools) or moved by
		// reloc_tables, which cl_maxLocalClients > 2 already requires; the two flags below name
		// the relocations that are not part of that (LOG.md 2026-09-29, "odd lc checks").
		struct odd_lc_check
		{
			const char* name;
			uint32_t rva;             // first byte of the rewritten window
			uint8_t len;
			uint8_t imm;              // offset of the bound imm8 in the window
			uint8_t stock[15];
			uint8_t patched[15];
			bool needs_signin;        // reads clientGameStates[lc] (signin_relocated)
			bool needs_lui_roots;     // reads s_rootData[controller] (lui_roots_relocated)
		};
		constexpr odd_lc_check odd_lc_checks[] = {
			// movsxd r9,eax / cmp r9d,edi / ja  ->  movsxd r9,eax / cmp eax,1 / ja
			{"SetFilterPassEnabled", 0x0039DB48, 8, 5,
			 {0x4C, 0x63, 0xC8, 0x44, 0x3B, 0xCF, 0x0F, 0x87},
			 {0x4C, 0x63, 0xC8, 0x83, 0xF8, 0x01, 0x0F, 0x87}, false, false},
			// mov ebx,eax / cmp eax,edi / jbe / lea rcx,msg / mov edx,eax
			//   ->  cmp eax,1 / xchg ebx,eax / jbe / lea rcx,msg / mov edx,ebx
			{"LUIDisable", 0x004259CD, 15, 2,
			 {0x8B, 0xD8, 0x3B, 0xC7, 0x76, 0x1B, 0x48, 0x8D, 0x0D, 0x56, 0xFD, 0xAF, 0x02, 0x8B, 0xD0},
			 {0x83, 0xF8, 0x01, 0x93, 0x76, 0x1B, 0x48, 0x8D, 0x0D, 0x56, 0xFD, 0xAF, 0x02, 0x8B, 0xD3},
			 true, true},
			// mov r14d,eax / cmp eax,edi  ->  cmp eax,1 / xchg r14d,eax. A rejected lc's error text
			// then shows r14's old value (no room for mov edx,r14d); only a bad script sees it.
			{"GetDStat", 0x00A187BF, 7, 2,
			 {0x44, 0x8B, 0xF0, 0x3B, 0xC7, 0x76, 0x1B},
			 {0x83, 0xF8, 0x01, 0x41, 0x96, 0x76, 0x1B}, true, false},
			// cmp eax,2 / jl  ->  cmp eax,1 / jbe (unsigned: a negative lc is rejected now)
			{"GetPerks", 0x00A22072, 5, 2,
			 {0x83, 0xF8, 0x02, 0x7C, 0x1B}, {0x83, 0xF8, 0x01, 0x76, 0x1B}, false, false},
			{"HasPerk", 0x00A22137, 5, 2,
			 {0x83, 0xF8, 0x02, 0x7C, 0x1B}, {0x83, 0xF8, 0x01, 0x76, 0x1B}, false, false},
			// mov r9d,eax / mov [rbp+48h],eax / cmp eax,ebx / jbe
			//   ->  mov [rbp+48h],eax / cmp eax,1 / nop / jbe to the stock `mov r9d,[rbp+48h]`
			{"IsInHelicopter", 0x00D8B0B9, 10, 5,
			 {0x44, 0x8B, 0xC8, 0x89, 0x45, 0x48, 0x3B, 0xC3, 0x76, 0x1F},
			 {0x89, 0x45, 0x48, 0x83, 0xF8, 0x01, 0x66, 0x90, 0x76, 0x1B}, false, false},
		};
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
			// Expected first bytes: alloc `mov [rsp+8],rbx`, free `push rbx; sub rsp,20h`
			static constexpr uint8_t alloc_head[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10};
			static constexpr uint8_t free_head[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0xE8};
			const auto b = base();
			if (!readable(reinterpret_cast<const void*>(b + 0x0135D330), sizeof(alloc_head))
				|| std::memcmp(reinterpret_cast<const void*>(b + 0x0135D330), alloc_head, sizeof(alloc_head)) != 0
				|| !readable(reinterpret_cast<const void*>(b + 0x0135DC20), sizeof(free_head))
				|| std::memcmp(reinterpret_cast<const void*>(b + 0x0135DC20), free_head, sizeof(free_head)) != 0)
			{
				csc_lc_widened = false;   // no trigger - the bound stays stock
				note("[splitscreen] csc lc bound: allocator prologues differ - not widened\n");
				return;
			}
			alloc_per_lc_hook.create(b + 0x0135D330, reinterpret_cast<void*>(&alloc_per_lc_stub));
			free_per_lc_hook.create(b + 0x0135DC20, reinterpret_cast<void*>(&free_per_lc_stub));
			sync_csc_lc_bound();
		}
