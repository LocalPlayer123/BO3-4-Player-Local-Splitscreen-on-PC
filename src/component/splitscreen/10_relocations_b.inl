// Relocations, part B: batches 12-18, light queue, the per-client relocation table (perclient_rows),
// Umbra, lens flares, controller UI models, lobby max players.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Batch 12: zombies HUD player list ----
		// The PlayerList HUD update keeps six per-client arrays, all [2] and packed back to back:
		// scores, a second score, shown flags, client ids (-1 = none), icon names (char*) and the
		// own index. For lc 2 the icon row is the own-index array, so ints were read as string
		// pointers (crash in strcmp from the UI model string setter). The engine's reset leaves
		// set icons and ids per lc before use, so zero-filled slots 2/3 are fine.
		constexpr entcoll_site hudpl_score_sites[] = {
			{0x026A4375, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + rsi + 0x1a8759d0], eax
			{0x026A4388, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a8759d0], eax
			{0x026A74F2, 3, 7, false, 0x0000}, // mov dword ptr [rcx + rsi + 0x1a8759d0], eax
			{0x026C6E0B, 3, 7, false, 0x0000}, // lea rcx, [r10 + 0x1a8759d0]
			{0x026CC141, 4, 8, false, 0x0000}, // mov edx, dword ptr [r14 + rax + 0x1a8759d0]
		};
		constexpr entcoll_site hudpl_gap_sites[] = {
			{0x026A4396, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a10], eax
		};
		constexpr entcoll_site hudpl_flags_sites[] = {
			{0x026A4349, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + rsi + 0x1a875a50], eax
			{0x026A4362, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a50], eax
			{0x026A74D7, 3, 8, false, 0x0000}, // cmp dword ptr [rcx + rsi + 0x1a875a50], 1
			{0x026A74F9, 3, 11, false, 0x0000}, // mov dword ptr [rcx + rsi + 0x1a875a50], 0
			{0x026C6DDC, 4, 8, false, 0x0000}, // mov qword ptr [rax + r10 + 0x1a875a50], r9
			{0x026C6DE4, 4, 8, false, 0x0008}, // mov qword ptr [rax + r10 + 0x1a875a58], r9
			{0x026C6DEC, 4, 8, false, 0x0010}, // mov qword ptr [rax + r10 + 0x1a875a60], r9
			{0x026C6DFB, 4, 8, false, 0x0018}, // mov qword ptr [rax + r10 + 0x1a875a68], r9
			{0x026CC0D5, 4, 9, false, 0x0000}, // cmp dword ptr [r14 + rdx + 0x1a875a50], 0
		};
		constexpr entcoll_site hudpl_ids_sites[] = {
			{0x026A7434, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r14 + rax + 0x1a875a90]
			{0x026A74A6, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a90], ebx
			{0x026C6D58, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x18135bc1]
			{0x026C6DD2, 3, 7, false, 0x0000}, // lea rdx, [r10 + 0x1a875a90]
		};
		constexpr entcoll_site hudpl_icons_sites[] = {
			{0x026A43A2, 3, 8, false, 0x0000}, // cmp qword ptr [rsi + 0x1a875ad0], 0
			{0x026A43AA, 3, 7, false, 0x0000}, // lea rsi, [rsi + 0x1a875ad0]
			{0x026A74E1, 4, 8, false, 0x0000}, // mov qword ptr [rsi + rax*8 + 0x1a875ad0], rbx
			{0x026C6DC0, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x18135b99]
			{0x026CC0A9, 3, 7, true , 0x0000}, // lea rax, [rip + 0x181308b0]
			{0x026C6D43, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x18135c16]  (reset leaf; the generator misses it)
		};
		constexpr entcoll_site hudpl_self_sites[] = {
			{0x026A42DB, 4, 8, false, 0x0000}, // mov dword ptr [rsi + r11*4 + 0x1a875b50], eax
			{0x026CC08D, 3, 7, false, 0x0000}, // lea rax, [rdx + 0x1a875b50]
		};

		// ---- Batch 13: previous-frame view ----
		// Pane 3 drew a white void: two [2] per-view renderer arrays have a foreign slot 2.
		// The other one, s_sunVolumeTransitions (PS4 [4] x 0x2A28), is moved by
		// relocate_percg_context (same six sites). Slots 2/3 need no static init: the
		// per-client view init resets each element, -1 at +0x2BB0 included (store at
		// 0x010CD118, after FX_SetNextUpdateCamera(lc, 2) as in PS4 CG_InitView 0x2CB240).
		// g_prevFrameViewParmsDraw (PS4 GfxViewParms[4] x 0x290): R_RenderScene copies each
		// frame's view parms to prev[localClientNum]; slot 2 covered another renderer object.
		constexpr entcoll_site prevview_sites[] = {
			{0x01CDF3D5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xe15ffd4]
		};

		// ---- Batch 14: LiveStats per-controller stat-change cache ----
		// PS4 LiveStats_SetStatChanged (0xC63D00) decodes change messages into
		// s_cachedStatsChanges[controller], [4]. The PC cache (0x100 entries, stride 0x4404,
		// count at +0x4400) is [2], so controller 2 overwrote the statics behind it, including
		// the "statReadDDLExt" cmd node (crash in Cmd_RemoveCommand at game over).
		// LiveStats_ResetCache clears all four slots on PS4; the PC memset (0x8808) clears two
		// and is widened to 0x11010 after the move (an uncleared count reaching 0x100 is
		// EXE_PATCH_STATSOVERFLOW).
		constexpr entcoll_site statscache_sites[] = {
			{0x01E94E8F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xf578eaa]  (LiveStats_ResetCache memset)
			{0x01E9893F, 2, 6, true , 0x4400}, // mov edx, dword ptr [rip + 0xf5797fb]
			{0x01E9894F, 2, 6, true , 0x4400}, // mov eax, dword ptr [rip + 0xf5797eb]
			{0x01E98959, 3, 7, true , 0x0040}, // lea r14, [rip + 0xf575420]
			{0x01E98960, 3, 7, true , 0x0000}, // lea rbp, [rip + 0xf5753d9]
			{0x01E989A3, 2, 6, true , 0x4400}, // mov eax, dword ptr [rip + 0xf579797]
			{0x01E989AD, 3, 7, true , 0x4400}, // mov dword ptr [rip + 0xf57978c], r15d
			{0x01E9952B, 3, 7, true , 0x0000}, // lea rax, [rip + 0xf57480e]  (LiveStats_SetStatChanged)
		};

		// ---- Batch 15: per-client UI visibility bits ----
		// The zombie HUD shows its widgets through the "UIVisibilityBit.<n>" models. PS4 keeps
		// the bits as u64[4] (sharedUiInfo +0x14240), the PC as u64[2]. Client 2's u64 lies on
		// the per-client visibility-bit model handles and the scoreboard team-model handles,
		// which CL_UpdateUIVisibilityBits(2) overwrote every frame: no HUD in panes 1 and 2.
		// Known and left: one routine clears match bits for lc 0/1 with a single 16-byte
		// and/andn; client 2's bits are recomputed every frame anyway.
		constexpr entcoll_site visbits_sites[] = {
			{0x0061D911, 3, 7, false, 0x0000}, // mov ebx, dword ptr [rbx + rsi*8 + 0x179dbdc8]
			{0x006D3CAD, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r9 + rbx*8 + 0x179dbdc8]
			{0x008635A1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rsi*8 + 0x179dbdc8]
			{0x00A141F4, 4, 8, false, 0x0000}, // mov eax, dword ptr [r13 + r15*8 + 0x179dbdc8]
			{0x00E477F9, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + rsi*8 + 0x179dbdc8]
			{0x00FB5B24, 3, 7, false, 0x0000}, // mov ecx, dword ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x0135FAF1, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x013D2030, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + rdx + 0x179dbdc8]
			{0x013D2047, 4, 12, false, 0x0000}, // mov qword ptr [r14 + rdx + 0x179dbdc8], 0
			{0x013D38B2, 4, 8, false, 0x0000}, // or qword ptr [r14 + rdx + 0x179dbdc8], rax
			{0x013D38CB, 4, 8, false, 0x0000}, // or rcx, qword ptr [r14 + rdx + 0x179dbdc8]
			{0x013D38D3, 4, 8, false, 0x0000}, // mov qword ptr [r14 + rdx + 0x179dbdc8], rcx
			{0x013D38FA, 4, 8, false, 0x0000}, // mov qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3919, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x40000000
			{0x013D3946, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3972, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3991, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x40000000
			{0x013D39A6, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x800000
			{0x013D39CD, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x1000000
			{0x013D3A12, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x2000000
			{0x013D3A54, 4, 8, false, 0x0000}, // or qword ptr [rcx + rdx + 0x179dbdc8], rax
			{0x013D3A81, 4, 8, false, 0x0000}, // or qword ptr [rcx + rdx + 0x179dbdc8], rax
			{0x013D3AAA, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3ADA, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B03, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B22, 4, 12, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], 0x4000000
			{0x013D3B3B, 4, 12, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], 0x8000000
			{0x013D3B5E, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B79, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B94, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3BBD, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3BF1, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x10000000
			{0x013D6D42, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x20000000
			{0x013D6DDF, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E04, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E26, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E44, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E7E, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6EDC, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6EFD, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6F78, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6F80, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x013D6FE1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x013D7033, 4, 8, false, 0x0000}, // mov qword ptr [r15 + r13 + 0x179dbdc8], rcx
			{0x013D7055, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D709D, 4, 8, false, 0x0000}, // and qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D70BF, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D70DD, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D7103, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D711E, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D7139, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D714A, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x200000
			{0x013D715F, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x400000
			{0x013D7173, 4, 8, false, 0x0000}, // mov rax, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x013D71D5, 4, 8, false, 0x0000}, // mov qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D72CC, 4, 8, false, 0x0000}, // mov r8, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x01F23CB7, 4, 8, false, 0x0000}, // mov esi, dword ptr [rax + r12*8 + 0x179dbdc8]
			{0x01F24013, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + r12*8 + 0x179dbdc8]
			{0x01F26735, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x15aa8f0c]
			{0x01FDAA77, 3, 7, true , 0x0000}, // lea r8, [rip + 0x159f4bca]
			{0x01FF7347, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdx*8 + 0x179dbdc8]
			{0x01FF7537, 4, 8, false, 0x0000}, // mov r9, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x01FF770F, 4, 8, false, 0x0000}, // mov r9, qword ptr [rdx + rdi*8 + 0x179dbdc8]
			{0x01FF785E, 4, 8, false, 0x0000}, // mov r9, qword ptr [rcx + rdi*8 + 0x179dbdc8]
			{0x01FF79B7, 4, 8, false, 0x0000}, // mov r9, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x0200C283, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rbx*8 + 0x179dbdc8]
			{0x0201127F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x159be3c2]
			{0x0201153E, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x159be103]
			{0x0201C4B7, 5, 9, false, 0x0000}, // movzx eax, byte ptr [r13 + rdi*8 + 0x179dbdc8]
			{0x02035F70, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r11 + rsi*8 + 0x179dbdc8]
			{0x02037FE1, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x02038163, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x0203838E, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x0203A820, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdx + r12*8 + 0x179dbdc8]
			{0x020462F0, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x02054DD8, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r15*8 + 0x179dbdc8]
			{0x0205BF00, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + r13*8 + 0x179dbdc8]
			{0x020668E6, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x0206F333, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdi*8 + 0x179dbdc8]
			{0x0206F711, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x02074E6E, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + rbx*8 + 0x179dbdc8]
			{0x0207824F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r14*8 + 0x179dbdc8]
			{0x02079CBC, 4, 8, false, 0x0000}, // mov r9, qword ptr [r12 + r14*8 + 0x179dbdc8]
			{0x0207B8ED, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r12*8 + 0x179dbdc8]
			{0x02080A84, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r14*8 + 0x179dbdc8]
			{0x02084436, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x02087672, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r10 + rdi*8 + 0x179dbdc8]
			{0x020877EE, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + rdi*8 + 0x179dbdc8]
			{0x02092456, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rax + rbx*8 + 0x179dbdc8]
			{0x02098F85, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r12 + rsi*8 + 0x179dbdc8]
			{0x0209920F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r13*8 + 0x179dbdc8]
			{0x020A0DDF, 3, 7, true , 0x0000}, // lea r9, [rip + 0x1592e862]
			{0x020A0ED7, 3, 7, true , 0x0000}, // lea r9, [rip + 0x1592e76a]
			{0x025B1223, 4, 8, true , 0x0000}, // movdqu xmm0, xmmword ptr [rip + 0x153b1a2d]
			{0x025B126B, 4, 8, true , 0x0000}, // movdqu xmmword ptr [rip + 0x153b19e5], xmm0
			{0x025D17BE, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15391493]
			{0x025D40C8, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rbx*8 + 0x179dbdc8]
			{0x026B72CE, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdi*8 + 0x179dbdc8]
			{0x026EBC6A, 4, 8, false, 0x0000}, // movzx ecx, byte ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x026EF0C3, 4, 8, false, 0x0000}, // mov eax, dword ptr [rax + r13*8 + 0x179dbdc8]
			{0x026EF10A, 4, 8, false, 0x0000}, // mov eax, dword ptr [rax + r13*8 + 0x179dbdc8]
		};

		// Post-step of the visbits row: widen the per-client reset loop that zeroes
		// bits[lc] from 2 to 4.
		bool widen_visbits_reset(const perclient_array&, size_t)
		{
			auto* bound = reinterpret_cast<uint8_t*>(base() + 0x01F26750);
			constexpr uint8_t bound_old[] = {0x83, 0xFF, 0x02};
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
		constexpr uint32_t conmsgbuf_base = 0x052F87F8;
		constexpr uint32_t conmsgbuf_stride = 0x2BC0;

		constexpr entcoll_site conmsgbuf_sites[] = {
			{0x0133930F, 4, 8, false, 0x2030}, // mov qword ptr [rax + rdi + 0x5379828], rcx
			{0x01339317, 4, 8, false, 0x2038}, // mov qword ptr [rax + rdi + 0x5379830], rcx
			{0x0133931F, 4, 8, false, 0x2070}, // mov qword ptr [rax + rdi + 0x5379868], rcx
			{0x01339327, 4, 8, false, 0x2078}, // mov qword ptr [rax + rdi + 0x5379870], rcx
			{0x0133932F, 4, 8, false, 0x20B0}, // mov qword ptr [rax + rdi + 0x53798a8], rcx
			{0x01339337, 4, 8, false, 0x20B8}, // mov qword ptr [rax + rdi + 0x53798b0], rcx
			{0x0133933F, 4, 8, false, 0x20F0}, // mov qword ptr [rax + rdi + 0x53798e8], rcx
			{0x01339347, 4, 8, false, 0x20F8}, // mov qword ptr [rax + rdi + 0x53798f0], rcx
			{0x0133934F, 4, 8, false, 0x2B30}, // mov qword ptr [rbx + rdi + 0x537a328], rcx
			{0x01339357, 4, 8, false, 0x2B38}, // mov qword ptr [rbx + rdi + 0x537a330], rcx
			{0x0133A666, 3, 7, true , 0x201C}, // lea rcx, [rip + 0x403f1c7]
			{0x0133A7E0, 3, 7, true , 0x2000}, // lea rcx, [rip + 0x403f031]  Con_GetGameMsgWindow
			{0x0133AA39, 3, 7, true , 0x2B10}, // lea rdi, [rip + 0x403f8e8]  Con_InitMessageBuffer
			{0x0133D8E4, 3, 7, true , 0x2000}, // lea r13, [rip + 0x403bf2d]
			{0x0133DA8E, 3, 7, true , 0x2000}, // lea rax, [rip + 0x403bd83]
		};

		struct con_rel_site
		{
			uint32_t rva;
			uint8_t off;         // where the disp32 / imm32 sits in the instruction
			uint32_t old_value;  // offset from con it holds
		};

		constexpr con_rel_site conmsgbuf_con_rel[] = {
			{0x0133D107, 3, 0x13094}, // lea rcx, [rsi + 0x13094]            rsi = con (0x0133D0D8)
			{0x0133D172, 4, 0x13BB0}, // cmp dword ptr [rdi + rsi + 0x13bb0], r11d
			{0x0133D180, 3, 0x13BAC}, // mov eax, dword ptr [rdi + rsi + 0x13bac]
			{0x0133D18E, 3, 0x13B94}, // idiv dword ptr [rdi + rsi + 0x13b94]
			{0x0133D19C, 4, 0x13B78}, // mov rax, qword ptr [rdi + rsi + 0x13b78]
			{0x0133D1A8, 4, 0x13B80}, // mov rax, qword ptr [rdi + rsi + 0x13b80]
			{0x0133D1C3, 4, 0x13BB0}, // cmp r11d, dword ptr [rdi + rsi + 0x13bb0]
			{0x0133D22C, 3, 0x13078}, // lea rbx, [r15 + 0x13078]            r15 = con (0x0133D1FE)
			{0x0133D256, 3, 0x13B78}, // lea rcx, [r15 + 0x13b78]
			{0x0133D930, 4, 0x13B78}, // lea rdx, [r12 + 0x13b78]            r12 = con (0x0133D8CB)
			{0x0133D987, 4, 0x13B78}, // lea rdx, [r12 + 0x13b78]
			{0x0133DABC, 3, 0x13B78}, // add r8, 0x13b78                     r8 = con (0x0133DA95)
			{0x0133DB8B, 4, 0x13BB4}, // mov eax, dword ptr [rcx + r8 + 0x13bb4]   r8 = con (0x0133DB56)
			{0x0133DB93, 4, 0x13B80}, // mov rdi, qword ptr [rcx + r8 + 0x13b80]
			{0x0133DB9E, 4, 0x13B94}, // idiv dword ptr [rcx + r8 + 0x13b94]
			{0x0133DBA6, 4, 0x13BB4}, // mov dword ptr [rcx + r8 + 0x13bb4], edx
			// Con_ClearNotify (PS4 0x3F1FE0), a leaf without .pdata: clears the four game-message
			// windows, rcx = con. Missing, it cleared parts of the print queue for lc 2/3.
			{0x01339223, 4, 0x130A8}, // mov qword ptr [rax + rcx + 0x130a8], rdx
			{0x0133922B, 4, 0x130B0},
			{0x01339233, 4, 0x130E8},
			{0x0133923B, 4, 0x130F0},
			{0x01339243, 4, 0x13128},
			{0x0133924B, 4, 0x13130},
			{0x01339253, 4, 0x13168},
			{0x0133925B, 4, 0x13170},
		};

		constexpr uint32_t conmsgbuf_end_marker_rva = 0x0133AB7E; // lea rcx, [&messageBuffer[2]+0x2B10]
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
		constexpr entcoll_site uiinfo_sites[] = {
			{0x022304A1, 3, 7, true , 0x0000},
			{0x0223085C, 3, 7, true , 0x184C},
			{0x0223088C, 3, 7, true , 0x002C},
			{0x02230BC9, 3, 7, true , 0x0000}, // UI_UIContext_GetInfo
			{0x02231087, 3, 7, true , 0x001C}, // UI_InitUIInfos
			{0x022312C9, 3, 7, true , 0x184C},
			{0x0223157F, 3, 7, true , 0x0000},
			{0x0223268B, 3, 7, true , 0x0030},
			{0x022328F5, 3, 7, true , 0x0000},
			{0x022329A0, 3, 7, true , 0x0000},
		};
		constexpr uint32_t uiinfo_init_bound_rva = 0x02231110;
		constexpr uint8_t uiinfo_init_bound_old[] = {0x83, 0xFD, 0x02};   // cmp ebp, 2

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
		constexpr entcoll_site ui3d_windows_sites[] = {
			{0x01D0FD15, 3, 7, true , 0x0000}, // lea rbx, [saved]         init, slot 0
			{0x01D0FDB0, 3, 7, true , 0x0438}, // lea rcx, [saved + 0x438] init, slot 1
			{0x01D0FF2F, 3, 7, true , 0x0000}, // lea rdx, [saved]         R_UI3D_PerframeInit
			{0x01D10373, 3, 7, true , 0x0000}, // lea rcx, [saved]         R_UI3D_SetupBackendData
		};

		// ---- Light queue: records [2][1024] + counters [2] -> [4] ----------------
		// Per-client ring of light records (1024 x 0x28, stride 0xA000) with
		// read/write counters in two int[2] arrays A and B, 8 bytes apart. Client
		// 2's records overlay the counters, so on Revelations the consumer read a
		// garbage pointer. New block: records[4], A[4] at +0x28000, B[4] at +0x28010.
		// The reset's two `mov qword [rip+d],rax` become `movups [rip+d],xmm0` (same
		// length, xmm0 already zero) so all four clients' counters clear.
		constexpr uint32_t lightq_base = 0x10598670;
		constexpr uint32_t lightq_stride = 0xA000;
		constexpr uint32_t lightq_a = 0x105AC670;
		constexpr uint32_t lightq_b = 0x105AC678;
		constexpr size_t lightq_records_new = 4 * lightq_stride;   // 0x28000

		constexpr entcoll_site lightq_sites[] = {
			{0x000B15FE, 3, 7, true , 0x0000}, // restore: memset
			{0x000B1665, 3, 7, true , 0x0000}, // restore
			{0x000B1E7B, 3, 7, false, 0x0000}, // save
			{0x000B1EDE, 4, 9, false, 0x0020},
			{0x000B1EF4, 4, 8, false, 0x0000},
			{0x000B1F73, 4, 8, false, 0x0008},
			{0x000B1F8B, 4, 8, false, 0x0008},
			{0x000B1F93, 4, 8, false, 0x0010},
			{0x000B2008, 4, 8, false, 0x001C},
			{0x00436EF7, 4, 8, false, 0x0008}, // consumer
			{0x00436EFF, 4, 8, false, 0x0020},
			{0x00436F07, 4, 8, false, 0x0010},
			{0x00436F0F, 4, 8, false, 0x0000},
			{0x00436F1C, 4, 8, false, 0x0018},
			{0x01CEDEFE, 4, 8, false, 0x0010}, // producer
			{0x01CEDF09, 4, 8, false, 0x0008},
			{0x01CEDF1F, 5, 9, false, 0x0020},
			{0x01CEDF2E, 4, 8, false, 0x001C},
			{0x01CEDF3A, 4, 8, false, 0x0018},
			{0x01CEDF52, 5, 9, false, 0x0000},
			{0x01CEDF5B, 5, 10, false, 0x0020},
			{0x01CEDF6B, 4, 8, false, 0x0000},
			{0x01CEE6EE, 3, 7, false, 0x0000},
		};
		constexpr entcoll_site lightq_a_sites[] = {
			{0x000B174B, 4, 8, false, 0},
			{0x000B1DB0, 4, 8, false, 0},
			{0x00436E47, 4, 8, false, 0},
			{0x0043A80D, 4, 8, false, 0},
			{0x01CEDECE, 4, 8, false, 0},
			{0x01CEE158, 3, 7, true , 0}, // reset
			{0x01CEE6C5, 4, 8, false, 0},
		};
		constexpr entcoll_site lightq_b_sites[] = {
			{0x000B1753, 4, 8, false, 0},
			{0x000B1DB8, 3, 7, false, 0},
			{0x00436E3C, 4, 8, false, 0},
			{0x0043A7DE, 4, 8, false, 0},
			{0x0043A805, 4, 8, false, 0},
			{0x01CEDEE5, 4, 8, false, 0},
			{0x01CEE15F, 3, 7, true , 0}, // reset
			{0x01CEE6DA, 3, 7, false, 0},
		};
		bool lightq_relocated = false;

		bool relocate_lightq()
		{
			if (lightq_relocated)
			{
				return true;
			}
			const auto b = base();
			struct fixed_bytes { uint32_t rva; uint8_t len; uint8_t old_bytes[6]; uint8_t new_bytes[6]; };
			constexpr fixed_bytes extras[] = {
				{0x01CEE155, 3, {0x0F, 0x57, 0xC0}, {0x0F, 0x57, 0xC0}},   // xorps xmm0,xmm0 - must be there
				{0x01CEE158, 3, {0x48, 0x89, 0x05}, {0x0F, 0x11, 0x05}},   // mov qword -> movups (A)
				{0x01CEE15F, 3, {0x48, 0x89, 0x05}, {0x0F, 0x11, 0x05}},   // mov qword -> movups (B)
				{0x000B15F8, 6, {0x41, 0xB8, 0x00, 0x40, 0x01, 0x00}, {0x41, 0xB8, 0x00, 0x80, 0x02, 0x00}}, // restore memset
				{0x000B176E, 4, {0x41, 0x83, 0xFF, 0x02}, {0x41, 0x83, 0xFF, 0x03}},   // restore loop
				{0x000B2060, 4, {0x41, 0x83, 0xFD, 0x02}, {0x41, 0x83, 0xFD, 0x03}},   // save loop
			};
			for (const auto& e : extras)
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
			for (const auto& e : extras)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + e.rva), e.new_bytes, e.len))
				{
					note("[splitscreen] lightq [2]->[4]: reset/memset/loop patch at 0x%08X FAILED\n", e.rva);
				}
			}
			lightq_relocated = true;
			return true;
		}

		// ---- Umbra occlusion culling: per-client state [2] -> [4] ---------------
		// Symptom: pane 3 drew no world geometry. The heap object sUmbra holds
		// per-client arrays (PS4: UmbraQueryParameters[4], R_Umbra_SelectTome
		// 0x945980). The PC keeps [2], and client 2 aliases the next fields:
		//   +0x12DC0C  params[2] x 0x14
		//   +0x12DC48  tome trigger[2] x 4   (-1 = none)
		//   +0x12DC50  persistent tome trigger[2] x 4
		// Fix: grow the allocation 0x470210 -> 0x470300 and put [4] copies at the
		// new tail; params at a multiple of 0x14, since the distance-scale setter
		// indexes (lc + 0xF167) * 0x14. Only possible while sUmbra is still NULL.
		struct umbra_disp
		{
			uint32_t rva;
			uint8_t off;       // where the imm32/disp32 sits in the instruction
			uint32_t old_value;
			uint32_t new_value;
		};

		constexpr uint32_t umbra_params_new = 0x470220;
		constexpr uint32_t umbra_trig_new = 0x470270;
		constexpr uint32_t umbra_ptrig_new = 0x470280;
		constexpr uint32_t umbra_params_delta = umbra_params_new - 0x12DC0C;
		static_assert(umbra_params_new % 0x14 == 0, "the distance-scale setter indexes params as (lc + bias) * 0x14");

		constexpr umbra_disp umbra_disps[] = {
			// params (field offsets 0x00..0x10 of each 0x14 entry)
			{0x01C8D417, 5, 0x12DC0C, 0x12DC0C + umbra_params_delta},
			{0x01C8D420, 5, 0x12DC10, 0x12DC10 + umbra_params_delta},
			{0x01C8D42F, 5, 0x12DC14, 0x12DC14 + umbra_params_delta},
			{0x01C8D43E, 5, 0x12DC18, 0x12DC18 + umbra_params_delta},
			{0x01C8D44D, 5, 0x12DC1C, 0x12DC1C + umbra_params_delta},
			{0x01C8DDDE, 5, 0x12DC10, 0x12DC10 + umbra_params_delta},   // SetAccurateOcclusionThreshold
			{0x01C8E045, 5, 0x12DC14, 0x12DC14 + umbra_params_delta},   // SetMinimumContributionThreshold
			{0x01C8EC55, 4, 0x12DC10, 0x12DC10 + umbra_params_delta},
			{0x01C8EFF6, 3, 0x12DC0C, umbra_params_new},                // defaults init
			{0x01C8F084, 3, 0x12DC0C, umbra_params_new},                // UmbraLevel settings x5
			{0x01C8F0D0, 3, 0x12DC0C, umbra_params_new},
			{0x01C8F11C, 3, 0x12DC0C, umbra_params_new},
			{0x01C8F16C, 3, 0x12DC0C, umbra_params_new},
			{0x01C8F1B3, 3, 0x12DC0C, umbra_params_new},
			{0x01C8E013, 2, 0xF167, umbra_params_new / 0x14},           // SetDistanceScale index bias
			// tome trigger
			{0x01C8C81D, 2, 0x12DC48, umbra_trig_new},
			{0x01C8CAFA, 3, 0x12DC48, umbra_trig_new},
			{0x01C8CBFE, 3, 0x12DC48, umbra_trig_new},
			{0x01C8CC72, 3, 0x12DC48, umbra_trig_new},
			{0x01C8DB20, 4, 0x12DC48, umbra_trig_new},
			{0x01C8DB68, 4, 0x12DC48, umbra_trig_new},
			{0x01C8F3FC, 3, 0x12DC48, umbra_trig_new},
			// persistent tome trigger
			{0x01C8CB3E, 3, 0x12DC50, umbra_ptrig_new},
			{0x01C8CBB2, 3, 0x12DC50, umbra_ptrig_new},
			{0x01C8DA64, 4, 0x12DC50, umbra_ptrig_new},
			{0x01C8F41F, 3, 0x12DC50, umbra_ptrig_new},
			// the allocation and its memset
			{0x01C8D345, 1, 0x470210, 0x470300},
			{0x01C8D35E, 2, 0x470210, 0x470300},
		};

		// init-loop end bounds: `lea reg, [base + disp8]`, disp8 at +3
		struct umbra_bound
		{
			uint32_t rva;
			uint8_t old_value;
			uint8_t new_value;
		};

		constexpr umbra_bound umbra_bounds[] = {
			{0x01C8EFFD, 0x28, 0x50},   // defaults: 2 x 0x14 -> 4 x 0x14
			{0x01C8F08B, 0x28, 0x50},
			{0x01C8F0D7, 0x28, 0x50},
			{0x01C8F123, 0x28, 0x50},
			{0x01C8F173, 0x28, 0x50},
			{0x01C8F1BA, 0x28, 0x50},
			{0x01C8F403, 0x08, 0x10},   // tome triggers: 2 x 4 -> 4 x 4
			{0x01C8F426, 0x08, 0x10},
		};

		bool umbra_grown = false;

		bool grow_umbra_client_arrays()
		{
			if (umbra_grown)
			{
				return true;
			}
			const auto b = base();
			const auto* object = reinterpret_cast<const uint64_t*>(b + 0x0AE15BF8);
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
			auto* imm = reinterpret_cast<uint8_t*>(b + 0x01E94E98);
			constexpr uint8_t imm_old[] = {0x41, 0xB8, 0x08, 0x88, 0x00, 0x00};
			constexpr uint8_t imm_new[] = {0x41, 0xB8, 0x10, 0x10, 0x01, 0x00};
			const auto* lea = reinterpret_cast<const uint8_t*>(b + 0x01E94E8F);
			int32_t lea_disp = 0;
			std::memcpy(&lea_disp, lea + 3, sizeof(lea_disp));
			if (!cache_new || b + 0x01E94E96 + lea_disp != cache_new
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
					|| retarget_end_marker(0x023F84B3, 3, 7, 0x17F297D8, base() + 0x17F297E0);
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
			{{"cgdc", 0x049B2CD0, 0x1838, cgdc_sites, std::size(cgdc_sites), 0, {}}},
			{.array = {"playerKeys"}, .own = relocate_playerkeys},
			{{"g_notetrackLerps", 0x0474B130, 0x340, notetracklerps_sites, std::size(notetracklerps_sites), 0, {}}},
			// batch 1b
			{{"cg_pmove", 0x04C99740, 0x1660, cg_pmove_sites, std::size(cg_pmove_sites), 0x009B4720, {0x33, 0xD2, 0x0F, 0x57, 0xC0, 0x48, 0x8D, 0x05}}},
			{{"camerashake", 0x04764990, 0x104, camerashake_sites, std::size(camerashake_sites), 0, {}}},
			{{"moverinfos", 0x047641B0, 0x390, moverinfos_sites, std::size(moverinfos_sites), 0, {}}},
			{{"moveinfoentnum", 0x04764140, 0x4, moveinfoentnum_sites, std::size(moveinfoentnum_sites), 0, {}}},
			{{"rumble", 0x04C9E470, 0x410, rumble_sites, std::size(rumble_sites), 0, {}}},
			{{"atglob", 0x036007C0, 0x1604, atglob_sites, std::size(atglob_sites), 0, {}}},
			{{"aimtargetcmd", 0x03600780, 0x10, aimtargetcmd_sites, std::size(aimtargetcmd_sites), 0, {}}},
			{{"arcdata", 0x047992B0, 0xEEC, arcdata_sites, std::size(arcdata_sites), 0, {}}},
			{.array = {"zbarriers", 0x0474B8F0, 0xC400, zbarriers_sites, std::size(zbarriers_sites), 0, {}},
			 .post = widen_zbarrier_clear},
			// batch 2
			{{"totalcoverage", 0x04CC3420, 0x360, totalcoverage_sites, std::size(totalcoverage_sites), 0, {}}},
			{{"rightstick", 0x0531C760, 0xA, rightstick_sites, std::size(rightstick_sites), 0, {}}},
			{{"gamepadbuttons", 0x0531C780, 0x2E, gamepadbuttons_sites, std::size(gamepadbuttons_sites), 0, {}}},
			{.array = {"cgExploderTriggers"}, .own = relocate_exploder_triggers},
			{.array = {"gaGlobs"}, .own = relocate_gaglobs},
			// batch 3
			{{"screenblur", 0x0479E410, 0x1C, screenblur_sites, std::size(screenblur_sites), 0, {}}},
			{{"screenelec", 0x0479E448, 0xC, screenelec_sites, std::size(screenelec_sites), 0, {}}},
			{{"screenburn", 0x0479E460, 0xC, screenburn_sites, std::size(screenburn_sites), 0, {}}},
			{.array = {"compass_actors", 0x04785580, 0x2C00, compass_actors_sites, std::size(compass_actors_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x0059888D>, .post = compass_clear_to_four_rows<0x0059888D>},
			{.array = {"compass_vehicles", 0x0478CE80, 0x900, compass_vehicles_sites, std::size(compass_vehicles_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x005988B5>, .post = compass_clear_to_four_rows<0x005988B5>},
			{.array = {"compass_artillery", 0x0478E280, 0x78, compass_artillery_sites, std::size(compass_artillery_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x005988F1>, .post = compass_clear_to_four_rows<0x005988F1>},
			{.array = {"compass_heli", 0x0478E370, 0xE0, compass_heli_sites, std::size(compass_heli_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x00598905>, .post = compass_clear_to_four_rows<0x00598905>},
			{.array = {"compass_0240", 0x0478E530, 0x240, compass_0240_sites, std::size(compass_0240_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x00598919>, .post = compass_clear_to_four_rows<0x00598919>},
			{.array = {"compass_0120", 0x0478E9B0, 0x120, compass_0120_sites, std::size(compass_0120_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x0059892D>, .post = compass_clear_to_four_rows<0x0059892D>},
			{.array = {"compass_0500", 0x0478FBF0, 0x500, compass_0500_sites, std::size(compass_0500_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x00598941>, .post = compass_clear_to_four_rows<0x00598941>},
			{.array = {"compass_0400", 0x047905F0, 0x400, compass_0400_sites, std::size(compass_0400_sites), 0, {}},
			 .pre = compass_clear_is_two_rows<0x00598955>, .post = compass_clear_to_four_rows<0x00598955>},
			// batch 4
			{{"cg_weaponsarray", 0x0495A410, 0x8, cg_weaponsarray_sites, std::size(cg_weaponsarray_sites), 0, {}}},
			{{"cg_ikbuf", 0x049B25C0, 0x8, cg_ikbuf_sites, std::size(cg_ikbuf_sites), 0, {}}},
			{{"cg_destructibles", 0x17E820C0, 0x8, cg_destructibles_sites, std::size(cg_destructibles_sites), 0, {}}},
			{{"numdestructibles", 0x17EC11B0, 0x4, numdestructibles_sites, std::size(numdestructibles_sites), 0, {}}},
			{{"cg_updatetime", 0x17EC11B8, 0x4, cg_updatetime_sites, std::size(cg_updatetime_sites), 0, {}}},
			{{"destr_gamestates", 0x17E820D0, 0x1080, destr_gamestates_sites, std::size(destr_gamestates_sites), 0, {}}},
			{{"destr_numgamestates", 0x17E841D0, 0x4, destr_numgamestates_sites, std::size(destr_numgamestates_sites), 0, {}}},
			{.array = {"ikStates"}, .own = ikstates_step},
			// batch 5
			{{"cg_clientents30", 0x041DC500, 0x21840, cg_clientents30_sites, std::size(cg_clientents30_sites), 0, {}}},
			{{"cg_perclient_3c0", 0x0479DC80, 0x3C0, cg_perclient_3c0_sites, std::size(cg_perclient_3c0_sites), 0, {}}},
			{.array = {"session_members"}, .own = session_members_step},
			// batch 6
			{.array = {"tnotify_list", 0x04CA5820, 0x1F40, tnotify_list_sites, std::size(tnotify_list_sites), 0, {}},
			 .pre = tnotify_init_matches, .post = tnotify_init_items},
			{.array = {"tnotify_head", 0x04CA96C0, 0x8, tnotify_head_sites, std::size(tnotify_head_sites), 0, {}},
			 .pre = tnotify_list_moved},
			{.array = {"tnotify_tail", 0x04CA96D0, 0x8, tnotify_tail_sites, std::size(tnotify_tail_sites), 0, {}},
			 .pre = tnotify_list_moved},
			{.array = {"tnotify_free", 0x04CA96E0, 0x8, tnotify_free_sites, std::size(tnotify_free_sites), 0, {}},
			 .pre = tnotify_list_moved},
			// batch 7 (ungated: the 190 MB slide happened with the third pane off too)
			{{"view_idsets_240", 0x0F48C880, 0x240, fxgpu_client_sites, std::size(fxgpu_client_sites), 0, {}}},
			// batch 8 (scene_c before install_perclient_buffer_guard: its cave bakes C's base)
			{{"scene_pc480", 0x0AE134A0, 0x480, scene_pc480_sites, std::size(scene_pc480_sites), 0, {}}},
			{.array = {"scene_c", 0x10596AE0, 0x8, scene_c_sites, std::size(scene_c_sites), 0, {}},
			 .post = publish_scene_c},
			// batches 9-13
			{{"rview_a24", 0x0F464FCC, 0xA24, rview_a24_sites, std::size(rview_a24_sites), 0, {}}},
			{{"rview_org30", 0x0F466430, 0x30, rview_org30_sites, std::size(rview_org30_sites), 0, {}}},
			{{"aimactors", 0x03600380, 0x200, aimactors_sites, std::size(aimactors_sites), 0, {}}},
			{{"hudpl_score", 0x1A7F6A50, 0x20, hudpl_score_sites, std::size(hudpl_score_sites), 0, {}}},
			{{"hudpl_gap", 0x1A7F6A90, 0x20, hudpl_gap_sites, std::size(hudpl_gap_sites), 0, {}}},
			{{"hudpl_flags", 0x1A7F6AD0, 0x20, hudpl_flags_sites, std::size(hudpl_flags_sites), 0, {}}},
			{{"hudpl_ids", 0x1A7F6B10, 0x20, hudpl_ids_sites, std::size(hudpl_ids_sites), 0, {}}},
			{{"hudpl_icons", 0x1A7F6B50, 0x40, hudpl_icons_sites, std::size(hudpl_icons_sites), 0, {}}},
			{{"hudpl_self", 0x1A7F6BD0, 0x4, hudpl_self_sites, std::size(hudpl_self_sites), 0, {}}},
			{{"prevview", 0x0FDCC800, 0x290, prevview_sites, std::size(prevview_sites), 0, {}}},
			// batches 14-18 and the light queue
			{.array = {"statscache", 0x1139B860, 0x4404, statscache_sites, std::size(statscache_sites), 0, {}},
			 .post = widen_statscache_reset},
			{.array = {"visbits", 0x1795CEC8, 0x8, visbits_sites, std::size(visbits_sites), 0, {}},
			 .post = widen_visbits_reset},
			{.array = {"conmsgbuf", conmsgbuf_base, conmsgbuf_stride, conmsgbuf_sites, std::size(conmsgbuf_sites), 0, {}},
			 .pre = conmsgbuf_refs_match, .post = retarget_conmsgbuf_refs},
			{.array = {"uiinfo", 0x1795D270, 0x1B68, uiinfo_sites, std::size(uiinfo_sites), 0, {}},
			 .pre = uiinfo_bound_matches, .post = widen_uiinfo_init},
			{{"ui3d_windows", 0x10B2F2F0, 0x438, ui3d_windows_sites, std::size(ui3d_windows_sites), 0, {}}},
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

		// ---- Lens flares: disabled for local clients >= 2 ------------------------
		// PS4 FxLensFlaresManager has eight per-client arrays of 4; on the PC they
		// are arrays of 2 inside one static object, so lc 2 hits the neighbouring
		// members. The object cannot grow (the fix would re-lay the whole class),
		// so for now clients >= 2 get no lens flares. Each entry point below takes
		// lc in edx and gets a cave: `cmp edx,2 / jl original`, else return
		// (SpawnInstance: -1, its own failure value).
		struct lc_gate
		{
			uint32_t rva;
			uint8_t prologue[9];
			uint8_t len;
			bool returns_minus_one;
		};
		constexpr lc_gate lensflare_gates[] = {
			{0x014BA810, {0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08}, 9, false}, // per-client pool setup
			{0x014BAC40, {0x40, 0x55, 0x56, 0x57, 0x41, 0x54}, 6, false}, // SetPersistentData
			{0x014BB9B0, {0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54}, 6, false}, // per-client update
			{0x014BBD40, {0x48, 0x89, 0x4C, 0x24, 0x08}, 5, true},        // SpawnInstance
			{0x014BC9E0, {0x48, 0x8B, 0xC4, 0x55, 0x53}, 5, false},       // per-view render
		};
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
		constexpr uint32_t lensflare_exit_thunk_rva = 0x02EF9840;
		constexpr uint8_t lensflare_exit_thunk_expected[] = {
			0x48, 0x8D, 0x0D, 0xC9, 0x53, 0x3B, 0x00,   // lea rcx, [FxLensFlaresManager]
			0xE9, 0x24, 0x24, 0x5C, 0xFE,               // jmp Shutdown
		};

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
		constexpr uint32_t lastinput_init_rva = 0x020E32E0;       // Com_LocalClient_LastInput_Init
		constexpr uint8_t lastinput_init_expected[] = {
			0x48, 0x89, 0x5C, 0x24, 0x10,                         // mov [rsp+0x10], rbx
		};
		constexpr uint32_t ui_controller_model_getter_rva = 0x0200CEE0;
		constexpr uint8_t ui_controller_model_getter_expected[] = {
			0x48, 0x63, 0xC1,                                     // movsxd rax, ecx
			0x48, 0x8D, 0x0D, 0x52, 0xF1, 0x25, 0x14,             // lea rcx, [s_controllerModel]
			0x0F, 0xB7, 0x04, 0x41,                               // movzx eax, word [rcx+rax*2]
			0xC3,
		};
		constexpr uint32_t ui_global_model_getter_rva = 0x0200CD10;
		constexpr uint8_t ui_global_model_getter_expected[] = {
			0x0F, 0xB7, 0x05, 0x21, 0xF3, 0x25, 0x14,             // movzx eax, word [global model]
			0xC3,
		};
		constexpr uint32_t ui_create_persistent_rva = 0x0200C900;
		constexpr uint8_t ui_create_persistent_expected[] = {
			0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, // prologue
			0x57, 0x48, 0x83, 0xEC, 0x70,
		};
		constexpr uint32_t ui_create_persistent_alloc_rva = 0x0200C965;
		constexpr uint8_t ui_create_persistent_alloc_expected[] = {
			0x48, 0x8D, 0x54, 0x24, 0x20,                         // lea rdx, [rsp+0x20]  (key)
			0x41, 0xB0, 0x01,                                     // mov r8b, 1           (persistent)
			0x0F, 0xB7, 0xCF,                                     // movzx ecx, di        (parent)
			0xE8, 0xCB, 0xFC, 0xFF, 0xFF,                         // call UI_Model_AllocateNode
		};
		constexpr uint32_t ui_global_model_rva = 0x1626C038;
		constexpr uint32_t ui_controller_model_rva = 0x1626C03C;  // uint16[2] on the PC
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
		constexpr uint32_t gamepad_models_bound_rva = 0x01340339;
		constexpr uint8_t gamepad_models_bound_expected[] = {
			0x83, 0xFE, 0x02,                                     // cmp esi, 2
			0x0F, 0x8C, 0xBE, 0xFE, 0xFF, 0xFF,                   // jl loop head
		};

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
		constexpr uint32_t lobby_max_local_reg_rva = 0x01EDBF02;
		constexpr uint8_t lobby_max_local_reg_expected[] = {
			0xB9, 0x69, 0xF0, 0xD2, 0x44,                         // mov ecx, hash
			0x89, 0x5C, 0x24, 0x28,                               // mov [rsp+0x28], ebx  flags
			0x48, 0x89, 0x05, 0x2E, 0x25, 0x7F, 0x13,             // mov [rip+..], rax
			0xC7, 0x44, 0x24, 0x20, 0x02, 0x00, 0x00, 0x00,       // mov dword [rsp+0x20], 2  max
		};
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
