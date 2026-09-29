// Relocations, part A: completion fixes, per-client array engine (relocate_perclient), batches 1-11.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ============ Completion of earlier relocations ============
		// Sites that tools/audit_reloc_tables.py found still pointing at the old
		// array of playersKb (key-state bytes) and s_gamePads while the engine
		// used the moved one (data/reloc_sites/completion_2026-09-28.txt).
		// Not completed: numdestructibles' hits are the previous array's end
		// markers (rewriting them crashed at boot); read the loop before
		// rewriting a "still targets the old base" hit.
		constexpr entcoll_site players_kb_completion_sites[] = {
			{0x012F3574, 4, 9, false, 0x01A8}, // cmp byte ptr [rbx + r14 + 0x52f2b98], 0
			{0x012F357F, 4, 9, false, 0x01A9}, // cmp byte ptr [rbx + r14 + 0x52f2b99], 0
			{0x012F3591, 4, 9, false, 0x01A9}, // mov byte ptr [rbx + r14 + 0x52f2b99], 0
			{0x012F359A, 4, 9, false, 0x01C0}, // cmp byte ptr [rbx + r14 + 0x52f2bb0], 0
			{0x012F35A5, 4, 9, false, 0x01C1}, // cmp byte ptr [rbx + r14 + 0x52f2bb1], 0
			{0x012F35B7, 4, 9, false, 0x01C1}, // mov byte ptr [rbx + r14 + 0x52f2bb1], 0
			{0x012F35C0, 4, 9, false, 0x0220}, // cmp byte ptr [rbx + r14 + 0x52f2c10], 0
			{0x012F35CB, 4, 9, false, 0x0221}, // cmp byte ptr [rbx + r14 + 0x52f2c11], 0
			{0x012F35DD, 4, 9, false, 0x0221}, // mov byte ptr [rbx + r14 + 0x52f2c11], 0
			{0x012F35E6, 4, 9, false, 0x0238}, // cmp byte ptr [rbx + r14 + 0x52f2c28], 0
			{0x012F35F1, 4, 9, false, 0x0239}, // cmp byte ptr [rbx + r14 + 0x52f2c29], 0
			{0x012F3603, 4, 9, false, 0x0239}, // mov byte ptr [rbx + r14 + 0x52f2c29], 0
			{0x012F360C, 4, 9, false, 0x0250}, // cmp byte ptr [rbx + r14 + 0x52f2c40], 0
			{0x012F3617, 4, 9, false, 0x0251}, // cmp byte ptr [rbx + r14 + 0x52f2c41], 0
			{0x012F3629, 4, 9, false, 0x0251}, // mov byte ptr [rbx + r14 + 0x52f2c41], 0
			{0x012F3632, 4, 9, false, 0x0268}, // cmp byte ptr [rbx + r14 + 0x52f2c58], 0
			{0x012F363D, 4, 9, false, 0x0269}, // cmp byte ptr [rbx + r14 + 0x52f2c59], 0
			{0x012F364F, 4, 9, false, 0x0269}, // mov byte ptr [rbx + r14 + 0x52f2c59], 0
			{0x012F3658, 4, 9, false, 0x0280}, // cmp byte ptr [rbx + r14 + 0x52f2c70], 0
			{0x012F3663, 4, 9, false, 0x0281}, // cmp byte ptr [rbx + r14 + 0x52f2c71], 0
			{0x012F3675, 4, 9, false, 0x0281}, // mov byte ptr [rbx + r14 + 0x52f2c71], 0
			{0x012F367E, 4, 9, false, 0x0298}, // cmp byte ptr [rbx + r14 + 0x52f2c88], 0
			{0x012F3689, 4, 9, false, 0x0299}, // cmp byte ptr [rbx + r14 + 0x52f2c89], 0
			{0x012F369B, 4, 9, false, 0x0299}, // mov byte ptr [rbx + r14 + 0x52f2c89], 0
			{0x012F36A4, 4, 9, false, 0x02B0}, // cmp byte ptr [rbx + r14 + 0x52f2ca0], 0
			{0x012F36AF, 4, 9, false, 0x02B1}, // cmp byte ptr [rbx + r14 + 0x52f2ca1], 0
			{0x012F36C1, 4, 9, false, 0x02B1}, // mov byte ptr [rbx + r14 + 0x52f2ca1], 0
			{0x012F36CA, 4, 9, false, 0x02C8}, // cmp byte ptr [rbx + r14 + 0x52f2cb8], 0
			{0x012F36D5, 4, 9, false, 0x02C9}, // cmp byte ptr [rbx + r14 + 0x52f2cb9], 0
			{0x012F36E7, 4, 9, false, 0x02C9}, // mov byte ptr [rbx + r14 + 0x52f2cb9], 0
			{0x012F36F0, 4, 9, false, 0x0100}, // cmp byte ptr [rbx + r14 + 0x52f2af0], 0
			{0x012F36FB, 4, 9, false, 0x0101}, // cmp byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F370D, 4, 9, false, 0x0101}, // mov byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F3716, 4, 9, false, 0x02E0}, // cmp byte ptr [rbx + r14 + 0x52f2cd0], 0
			{0x012F3721, 4, 9, false, 0x02E1}, // cmp byte ptr [rbx + r14 + 0x52f2cd1], 0
			{0x012F3733, 4, 9, false, 0x02E1}, // mov byte ptr [rbx + r14 + 0x52f2cd1], 0
			{0x012F373C, 4, 9, false, 0x0328}, // cmp byte ptr [rbx + r14 + 0x52f2d18], 0
			{0x012F3747, 4, 9, false, 0x0329}, // cmp byte ptr [rbx + r14 + 0x52f2d19], 0
			{0x012F3789, 4, 9, false, 0x0329}, // mov byte ptr [rbx + r14 + 0x52f2d19], 0
			{0x012F3792, 4, 9, false, 0x0340}, // cmp byte ptr [rbx + r14 + 0x52f2d30], 0
			{0x012F379D, 4, 9, false, 0x0341}, // cmp byte ptr [rbx + r14 + 0x52f2d31], 0
			{0x012F37AF, 4, 9, false, 0x0341}, // mov byte ptr [rbx + r14 + 0x52f2d31], 0
			{0x012F37B8, 4, 9, false, 0x0358}, // cmp byte ptr [rbx + r14 + 0x52f2d48], 0
			{0x012F37C3, 4, 9, false, 0x0359}, // cmp byte ptr [rbx + r14 + 0x52f2d49], 0
			{0x012F37D5, 4, 9, false, 0x0359}, // mov byte ptr [rbx + r14 + 0x52f2d49], 0
			{0x012F37DE, 4, 9, false, 0x0370}, // cmp byte ptr [rbx + r14 + 0x52f2d60], 0
			{0x012F37E9, 4, 9, false, 0x0371}, // cmp byte ptr [rbx + r14 + 0x52f2d61], 0
			{0x012F37F8, 4, 9, false, 0x0371}, // mov byte ptr [rbx + r14 + 0x52f2d61], 0
			{0x012F3801, 4, 9, false, 0x0388}, // cmp byte ptr [rbx + r14 + 0x52f2d78], 0
			{0x012F380C, 4, 9, false, 0x0389}, // cmp byte ptr [rbx + r14 + 0x52f2d79], 0
			{0x012F381E, 4, 9, false, 0x0389}, // mov byte ptr [rbx + r14 + 0x52f2d79], 0
			{0x012F3827, 4, 9, false, 0x03A0}, // cmp byte ptr [rbx + r14 + 0x52f2d90], 0
			{0x012F3832, 4, 9, false, 0x03A1}, // cmp byte ptr [rbx + r14 + 0x52f2d91], 0
			{0x012F3844, 4, 9, false, 0x03A1}, // mov byte ptr [rbx + r14 + 0x52f2d91], 0
			{0x012F384D, 4, 9, false, 0x03B8}, // cmp byte ptr [rbx + r14 + 0x52f2da8], 0
			{0x012F3858, 4, 9, false, 0x03B9}, // cmp byte ptr [rbx + r14 + 0x52f2da9], 0
			{0x012F386A, 4, 9, false, 0x03B9}, // mov byte ptr [rbx + r14 + 0x52f2da9], 0
			{0x012F3873, 4, 9, false, 0x03D0}, // cmp byte ptr [rbx + r14 + 0x52f2dc0], 0
			{0x012F387E, 4, 9, false, 0x03D1}, // cmp byte ptr [rbx + r14 + 0x52f2dc1], 0
			{0x012F3890, 4, 9, false, 0x03D1}, // mov byte ptr [rbx + r14 + 0x52f2dc1], 0
			{0x012F3899, 4, 9, false, 0x03E8}, // cmp byte ptr [rbx + r14 + 0x52f2dd8], 0
			{0x012F38A4, 4, 9, false, 0x03E9}, // cmp byte ptr [rbx + r14 + 0x52f2dd9], 0
			{0x012F38B3, 4, 9, false, 0x03E9}, // mov byte ptr [rbx + r14 + 0x52f2dd9], 0
			{0x012F38BC, 4, 9, false, 0x0460}, // cmp byte ptr [rbx + r14 + 0x52f2e50], 0
			{0x012F38C7, 4, 9, false, 0x0461}, // cmp byte ptr [rbx + r14 + 0x52f2e51], 0
			{0x012F38D9, 4, 9, false, 0x0461}, // mov byte ptr [rbx + r14 + 0x52f2e51], 0
			{0x012F38E2, 4, 9, false, 0x02F8}, // cmp byte ptr [rbx + r14 + 0x52f2ce8], 0
			{0x012F38ED, 4, 9, false, 0x02F9}, // cmp byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F38FC, 4, 9, false, 0x02F9}, // mov byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F3905, 4, 9, false, 0x0400}, // cmp byte ptr [rbx + r14 + 0x52f2df0], 0
			{0x012F3910, 4, 9, false, 0x0401}, // cmp byte ptr [rbx + r14 + 0x52f2df1], 0
			{0x012F3922, 4, 9, false, 0x0401}, // mov byte ptr [rbx + r14 + 0x52f2df1], 0
			{0x012F392B, 4, 9, false, 0x0418}, // cmp byte ptr [rbx + r14 + 0x52f2e08], 0
			{0x012F3936, 4, 9, false, 0x0419}, // cmp byte ptr [rbx + r14 + 0x52f2e09], 0
			{0x012F3948, 4, 9, false, 0x0419}, // mov byte ptr [rbx + r14 + 0x52f2e09], 0
			{0x012F3951, 4, 9, false, 0x0430}, // cmp byte ptr [rbx + r14 + 0x52f2e20], 0
			{0x012F395C, 4, 9, false, 0x0431}, // cmp byte ptr [rbx + r14 + 0x52f2e21], 0
			{0x012F396E, 4, 9, false, 0x0431}, // mov byte ptr [rbx + r14 + 0x52f2e21], 0
			{0x012F3977, 4, 9, false, 0x0448}, // cmp byte ptr [rbx + r14 + 0x52f2e38], 0
			{0x012F3982, 4, 9, false, 0x0449}, // cmp byte ptr [rbx + r14 + 0x52f2e39], 0
			{0x012F3996, 4, 9, false, 0x0449}, // mov byte ptr [rbx + r14 + 0x52f2e39], 0
			{0x012F39A4, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F39AF, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F39C1, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F39CA, 4, 9, false, 0x02F8}, // cmp byte ptr [rbx + r14 + 0x52f2ce8], 0
			{0x012F39D5, 4, 9, false, 0x02F9}, // cmp byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F39E7, 4, 9, false, 0x02F9}, // mov byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F39F0, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F39FB, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3A0D, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3A16, 4, 9, false, 0x0310}, // cmp byte ptr [rbx + r14 + 0x52f2d00], 0
			{0x012F3A21, 4, 9, false, 0x0311}, // cmp byte ptr [rbx + r14 + 0x52f2d01], 0
			{0x012F3A33, 4, 9, false, 0x0311}, // mov byte ptr [rbx + r14 + 0x52f2d01], 0
			{0x012F3A3C, 4, 9, false, 0x0100}, // cmp byte ptr [rbx + r14 + 0x52f2af0], 0
			{0x012F3A47, 4, 9, false, 0x0101}, // cmp byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F3A5E, 4, 9, false, 0x0101}, // mov byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F3AB2, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F3ABD, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3ACF, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3AE1, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F3AEC, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3AFE, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012FEEE7, 3, 8, false, 0x0160}, // cmp byte ptr [rdi + rax + 0x52f2b50], 0
			{0x012FEEF1, 3, 8, false, 0x0161}, // cmp byte ptr [rdi + rax + 0x52f2b51], 0
			{0x012FEF41, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FEF4C, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FEF65, 4, 9, false, 0x0161}, // mov byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FEF83, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FEF8E, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF000, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FF00B, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF0E3, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FF0EE, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF101, 4, 9, false, 0x0161}, // mov byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF114, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
			{0x012FF11F, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF130, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
			{0x012FF13B, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF178, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
			{0x012FF183, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF196, 4, 9, false, 0x0191}, // mov byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF19F, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
			{0x012FF1AA, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF1BD, 4, 9, false, 0x0179}, // mov byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF1EA, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
			{0x012FF1F5, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF208, 4, 9, false, 0x0191}, // mov byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF211, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
			{0x012FF21C, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF22F, 4, 9, false, 0x0179}, // mov byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x01306C23, 3, 8, false, 0x0208}, // cmp byte ptr [rbx + rsi + 0x52f2bf8], 0
			{0x01306C2D, 3, 8, false, 0x0209}, // cmp byte ptr [rbx + rsi + 0x52f2bf9], 0
			{0x01306C45, 3, 8, false, 0x0209}, // mov byte ptr [rbx + rsi + 0x52f2bf9], 0
			{0x01306C50, 3, 8, false, 0x01D8}, // cmp byte ptr [rbx + rsi + 0x52f2bc8], 0
			{0x01306C5A, 3, 8, false, 0x01D9}, // cmp byte ptr [rbx + rsi + 0x52f2bc9], 0
			{0x01306C71, 3, 8, false, 0x01F0}, // cmp byte ptr [rbx + rsi + 0x52f2be0], 0
			{0x01306C7B, 3, 8, false, 0x01F1}, // cmp byte ptr [rbx + rsi + 0x52f2be1], 0
			{0x01306CBF, 3, 8, false, 0x020A}, // cmp byte ptr [rbx + rsi + 0x52f2bfa], 0
			{0x01306CD0, 3, 8, false, 0x020A}, // mov byte ptr [rbx + rsi + 0x52f2bfa], 1
			{0x01306CF0, 4, 10, false, 0x01D9}, // mov word ptr [rbx + rsi + 0x52f2bc9], 0x100
			{0x01306CFA, 4, 10, false, 0x01F1}, // mov word ptr [rbx + rsi + 0x52f2be1], 0x100
			{0x01306D11, 3, 8, false, 0x020A}, // mov byte ptr [rbx + rsi + 0x52f2bfa], 0
			{0x01306D19, 3, 8, false, 0x01DA}, // cmp byte ptr [rbx + rsi + 0x52f2bca], 0
			{0x01306D28, 3, 8, false, 0x01D8}, // cmp byte ptr [rbx + rsi + 0x52f2bc8], 0
			{0x01306D32, 3, 8, false, 0x01D9}, // cmp byte ptr [rbx + rsi + 0x52f2bc9], 0
			{0x01306D44, 3, 8, false, 0x01D9}, // mov byte ptr [rbx + rsi + 0x52f2bc9], 0
			{0x01306D59, 3, 8, false, 0x01F2}, // cmp byte ptr [rbx + rsi + 0x52f2be2], 0
			{0x01306D68, 3, 8, false, 0x01F0}, // cmp byte ptr [rbx + rsi + 0x52f2be0], 0
			{0x01306D72, 3, 8, false, 0x01F1}, // cmp byte ptr [rbx + rsi + 0x52f2be1], 0
			{0x01306D84, 3, 8, false, 0x01F1}, // mov byte ptr [rbx + rsi + 0x52f2be1], 0
			{0x01308DD0, 4, 9, false, 0x01A8}, // mov byte ptr [rax + r14 + 0x52f2b98], 1
			{0x01308DD9, 4, 9, false, 0x01A9}, // mov byte ptr [rax + r14 + 0x52f2b99], 1
			{0x0131B73B, 4, 9, false, 0x02B0}, // cmp byte ptr [rdx + r8 + 0x52f2ca0], 0
			{0x0131B746, 4, 9, false, 0x0118}, // cmp byte ptr [rdx + r8 + 0x52f2b08], 0
			{0x0131B8BB, 4, 9, false, 0x02B0}, // cmp byte ptr [rdx + r8 + 0x52f2ca0], 0
			{0x0131B8C6, 4, 9, false, 0x0118}, // cmp byte ptr [rdx + r8 + 0x52f2b08], 0
			{0x0131BC5E, 4, 9, false, 0x02F8}, // cmp byte ptr [rdi + r15 + 0x52f2ce8], 0
			{0x0131BC71, 5, 11, false, 0x02F8}, // mov word ptr [rdi + r15 + 0x52f2ce8], 0x101
			{0x0131BCDA, 4, 9, false, 0x0118}, // cmp byte ptr [rdi + r15 + 0x52f2b08], 0
			{0x0131BCF1, 5, 11, false, 0x0118}, // mov word ptr [rdi + r15 + 0x52f2b08], 0x101
			{0x0131BD01, 4, 9, false, 0x02B0}, // cmp byte ptr [rdi + r15 + 0x52f2ca0], 0
			{0x0131BD10, 4, 9, false, 0x0118}, // cmp byte ptr [rdi + r15 + 0x52f2b08], 0
		};
		constexpr entcoll_site gamepads_completion_sites[] = {
			{0x02284AF2, 2, 7, true , 0x0074}, // cmp dword ptr [rip + 0x15b7c7bb], 8
			{0x022861A1, 4, 9, false, 0x0000}, // cmp byte ptr [rcx + r9 + 0x17e6e310], 0
		};

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

		void trace_text(const char* text);   // defined after trace_write

		void complete_relocation(const char* name, const entcoll_site& moved_site,
		                         const uint32_t old_base, const entcoll_site* sites,
		                         const size_t count, int32_t* saved, bool& done)
		{
			if (done)
			{
				return;
			}
			const auto fresh = moved_base_from_site(moved_site, old_base);
			char line[160]{};
			if (!fresh)
			{
				std::snprintf(line, sizeof(line), "%s completion: base not moved - skipped", name);
			}
			else if (!rewrite_entcoll(sites, count, old_base, fresh, saved))
			{
				std::snprintf(line, sizeof(line), "%s completion: a site did not match - NOTHING written", name);
			}
			else
			{
				done = true;
				std::snprintf(line, sizeof(line), "%s completion: %zu remaining references moved to the new array",
				              name, count);
			}
			trace_text(line);
		}

		bool players_kb_completed = false;
		bool gamepads_completed = false;

		// Probe site the playersKb relocation always rewrites (IN_Attack_Up's
		// kbutton read). s_gamePads uses its own verified destination instead.
		constexpr entcoll_site players_kb_probe = {0x0131B2B1, 4, 8, false, 0x0198};   // reloc.hpp players_kb row

		void complete_players_kb()
		{
			static int32_t saved[std::size(players_kb_completion_sites)]{};
			complete_relocation("playersKb", players_kb_probe, 0x052739F0,
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
			                    0x17DEF3E0, destination_abs, saved))
			{
				gamepads_completed = true;
				trace_text("s_gamePads completion: 2 remaining references moved");
			}
			else
			{
				trace_text("s_gamePads completion: a site did not match - NOTHING written");
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

			entcoll_world_new = reinterpret_cast<size_t>(world);
			entcoll_nodes_new = reinterpret_cast<size_t>(nodes);
			entcoll_relocated = true;
			note("[splitscreen] entity collision [2]->[4]: world RVA 0x%08X,"
			     " nodes RVA 0x%08X (%zu + %zu sites)\n",
			     static_cast<uint32_t>(entcoll_world_new - b),
			     static_cast<uint32_t>(entcoll_nodes_new - b),
			     std::size(entcoll_world_sites), std::size(entcoll_node_sites));
			return true;
		}

		// Clientfield pending-callback buffer, [2] -> [4] clients. Client 2 faulted
		// at 0x00132F94. Layout: entries 2 x 2048 x 32 bytes, then counts [2] x 4
		// at +0x20000 (total 0x20008). Growing it alone would put lc 2's entries on
		// the counts, so the count offset moves to 0x40000 (6 sites) and the size
		// to 0x40010 (3 sites). Consumers read the buffer through cf_pointer_rva.
		// All or nothing with rollback; gated on BO3_CG_FRAME.
		struct cf_imm
		{
			uint32_t rva;
			uint8_t off;
			uint32_t was;
			uint32_t want;
		};
		constexpr uint32_t cf_buffer_rva = 0x049925A0;
		constexpr uint32_t cf_pointer_rva = 0x04992590;
		constexpr size_t cf_new_bytes = 0x40010;   // 4 * 0x10000 + 4 * 4
		constexpr uint32_t cf_lea_sites[] = {0x008F26E3, 0x008F2DB0}; // 7 B, disp @3
		constexpr cf_imm cf_imms[] = {
			{0x00132F59, 3, 0x20000, 0x40000}, // mov r8d,[r9+0x20000]
			{0x00132F7F, 3, 0x20000, 0x40000}, // mov [r9+0x20000],eax
			{0x0013304D, 3, 0x20000, 0x40000}, // mov r8d,[r10+0x20000]
			{0x00133079, 3, 0x20000, 0x40000}, // mov [r10+0x20000],eax
			{0x001337F6, 4, 0x20000, 0x40000}, // lea r15,[r8*4+0x20000]
			{0x00136BD4, 4, 0x20000, 0x40000}, // lea r14,[rbx*4+0x20000]
			{0x0013444B, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (clear)
			{0x00137160, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (clear)
			{0x008F2DB9, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (init memset)
		};
		bool cf_relocated = false;
		size_t cf_new_buffer = 0;

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

			cf_new_buffer = reinterpret_cast<size_t>(fresh);
			cf_relocated = true;
			note("[splitscreen] clientfield callbacks [2]->[4]: buffer RVA 0x%08X,"
			     " 2 leas + %zu immediates\n",
			     static_cast<uint32_t>(cf_new_buffer - b), std::size(cf_imms));
			return true;
		}

		// clientObjMap: per-client DObj handle table, PC [2][0x702] -> PS4 layout
		// [4][0x704] (PS4 0x101D3470; viewmodel handle = 0x700 + lc). Client 2 hit
		// MSVC's range check (fail-fast 0xC0000409, so no crash dialog), and with
		// 0x702-word rows lc 2's viewmodel aliased lc 3's entity 0. The table cannot
		// grow in place, so it moves, and every row-dependent constant changes with
		// it: stride, row count, byte size 0x1C08 -> 0x3820 (memset and /GS bound)
		// and the client count 2 -> 4 of the PC-only free-all / rebuild-all pair.
		constexpr uint32_t entword_base = 0x16D545D0;
		constexpr uint32_t entword_old_row = 0x702;             // PC handles per client
		constexpr uint32_t entword_row = 0x704;                 // PS4 handles per client
		constexpr uint32_t entword_client_bytes = entword_row * 2;   // 0xE08
		constexpr size_t entword_slots = 4;
		constexpr entcoll_site entword_sites[] = {
			{0x020F3CF4, 3, 7, true,  0}, // lea rcx,[rip+..]
			{0x020F55FD, 4, 8, false, 0}, // mov word [rax+rdx*2+0x16DD3540],di
			{0x020F5689, 3, 7, true,  0}, // lea rcx,[rip+..]
			{0x020F5858, 3, 7, true,  0}, // lea rcx,[rip+..]   (the clear)
			{0x020F5967, 5, 9, false, 0}, // movsx rcx,word [rax+r10+..]  (crash site)
			{0x020F5999, 5, 9, false, 0}, // mov word [rax+r10+..],dx
			{0x020F5CB3, 5, 9, false, 0}, // movzx ecx,word cs:[rcx+rax*2+..]
			{0x020F8F4C, 3, 7, true,  0}, // lea rsi,[rip+..]
		};
		// size = immediate width in bytes (4 = imm32, 1 = imm8)
		struct entword_imm { uint32_t rva; uint8_t off; uint8_t size; uint32_t was; uint32_t want; };
		constexpr entword_imm entword_imms[] = {
			{0x020F5861, 2, 4, 0x1C08, 0x3820}, // mov r8d,0x1c08  - memset size
			{0x020F598F, 2, 4, 0x1C08, 0x3820}, // cmp rax,0x1c08  - the /GS bound
			{0x020F3CDF, 2, 4, 0x702, 0x704},   // imul ecx,ecx,0x702   ClearAllSkel(lc)
			{0x020F3CE5, 1, 4, 0x702, 0x704},   // mov edi,0x702        ClearAllSkel count
			{0x020F55EC, 3, 4, 0x702, 0x704},   // imul rdx,rdx,0x702   Com_ClientDObjCreate
			{0x020F5680, 2, 4, 0x702, 0x704},   // imul edx,edx,0x702   Com_GetClientDObj
			{0x020F58DE, 2, 4, 0x702, 0x704},   // imul edi,edi,0x702   Com_SafeClientDObjFree
			{0x020F5CA9, 3, 4, 0x702, 0x704},   // imul r13,r13,0x702   rebuild-all row
			{0x020F8D60, 2, 4, 0x702, 0x704},   // cmp esi,0x702        rebuild-all count
			{0x020F8F9F, 2, 4, 0x702, 0x704},   // cmp ebx,0x702        free-all count
			{0x020F8D7D, 3, 1, 0x02, 0x04},     // cmp r12d,2           rebuild-all clients
			{0x020F8FA9, 2, 1, 0x02, 0x04},     // cmp ebp,2            free-all clients
		};
		bool entword_relocated = false;
		size_t entword_new = 0;
		const char* entword_result = "clientObjMap: not attempted";

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
					entword_result = "clientObjMap: NOT moved - an immediate did not match";
					return false;
				}
			}

			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(entword_slots * entword_client_bytes));
			if (!fresh)
			{
				entword_result = "clientObjMap: NOT moved - allocation failed";
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
					entword_result = "clientObjMap: NOT moved - an immediate write failed (rolled back)";
					return false;
				}
				++done;
			}
			entword_new = reinterpret_cast<size_t>(fresh);
			entword_relocated = true;
			entword_result = "clientObjMap [2][0x702] -> [4][0x704] (8 sites, 12 immediates)";
			note("[splitscreen] clientObjMap [2][0x702] -> [4][0x704] at RVA 0x%08X (8 sites, %zu imms)\n",
			     static_cast<uint32_t>(entword_new - b), std::size(entword_imms));
			return true;
		}

		// s_exposureAdaptions [3] -> [5]: one auto-exposure buffer per local client
		// plus the extra cam, as on PS4 (0xAE89550; RB_FxBloomLDRColorGrade picks
		// `isExtraCam ? 4 : localClientNum`). The PC picked `extraCam ? 2 : lc`, so
		// player 3 shared the extra cam's buffer and player 4 read
		// exposureOutputBuffer (pane 4 overexposed). The slots after [3] are
		// foreign, so the array moves and the selector's 30 bytes are rewritten to
		// PS4's rule. Runs at post_unpack, before R_InitLightingData.
		constexpr uint32_t exposure_base = 0x0F64EBB0;
		constexpr uint32_t exposure_stride = 0x110;
		constexpr uint32_t exposure_old_count = 3;
		constexpr uint32_t exposure_new_count = 5;
		constexpr uint32_t exposure_texture_off = 0x108;
		constexpr entcoll_site exposure_base_sites[] = {
			{0x01CBF7C0, 3, 7, true, 0},   // lea rbx,[base]  free loop
			{0x01CC028F, 3, 7, true, 0},   // lea rcx,[base]  table fill
			{0x01CC06C4, 3, 7, true, 0},   // lea rbx,[base]  create loop
		};
		constexpr entcoll_site exposure_fill_end_site[] = {
			{0x01CBFF0C, 3, 7, true, exposure_old_count * exposure_stride},   // lea r13,[end]
		};
		constexpr entcoll_site exposure_all_end_sites[] = {
			{0x01CBF7C7, 3, 7, true, exposure_old_count * exposure_stride},   // lea rdi,[end] free
			{0x01CC06D2, 3, 7, true, exposure_old_count * exposure_stride},   // lea rdi,[end] create
		};
		constexpr uint32_t exposure_select_rva = 0x01C5FC0C;
		constexpr uint8_t exposure_select_stock[] = {
			0xB8, 0x02, 0x00, 0x00, 0x00,                   // mov eax,2
			0x75, 0x06,                                     // jne +6
			0x8B, 0x82, 0x98, 0x03, 0x00, 0x00,             // mov eax,[rdx+0x398]
			0x8B, 0xC8,                                     // mov ecx,eax
			0x48, 0x8B, 0x82, 0xB0, 0x03, 0x00, 0x00,       // mov rax,[rdx+0x3B0]
			0x48, 0x8B, 0xB4, 0xC8, 0xF0, 0x10, 0x00, 0x00, // mov rsi,[rax+rcx*8+0x10F0]
		};
		constexpr uint32_t exposure_select_lea_off = 19;   // lea rsi,[rip+d] inside the patch
		const char* exposure_result = "exposure adaptions: not attempted";
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
				exposure_result = "exposure adaptions: NOT moved - selector bytes differ at 0x01C6BFDC";
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(exposure_new_count * exposure_stride));
			if (!fresh)
			{
				exposure_result = "exposure adaptions: NOT moved - allocation failed";
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
				exposure_result = "exposure adaptions: NOT moved - new block out of rip range";
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
				exposure_result = "exposure adaptions: NOT moved - a base lea did not match";
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
				exposure_result = "exposure adaptions: NOT moved - the fill end lea did not match";
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
				exposure_result = "exposure adaptions: NOT moved - a create/free end lea did not match";
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
				exposure_result = "exposure adaptions: NOT moved - selector write failed (rolled back)";
				return false;
			}
			exposure_new = fresh_abs;
			exposure_result = "exposure adaptions [3] -> [5] (PS4 MAX_LOCAL_CLIENTS+1), selector extraCam ? 4 : lc";
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
		constexpr uint32_t model_pool_base = 0x16293160;
		constexpr uint32_t model_pool_stride = 0x28;
		constexpr uint32_t model_pool_old_count = 0x9000;
		constexpr uint32_t model_pool_new_count = 0xFFFF;
		constexpr uint32_t model_pool_sentinel = 0x9000;
		constexpr uint32_t model_pool_self_off = 0x1C;
		constexpr uint32_t model_pool_next_off = 0x1E;
		constexpr entcoll_site model_pool_sites[] = {
			{0x0200C70E, 3, 7, true , 0x00},   // lea rdx,[base]            AllocateNode
			{0x0200C75F, 3, 7, true , 0x00},   // lea rdx,[base]            AllocateNode
			{0x0200CA81, 3, 7, true , 0x00},   // lea rbx,[base]            FreeModel
			{0x0200CA9E, 3, 7, true , 0x00},   // lea rsi,[base]            FreeModel
			{0x0200CAD3, 3, 7, true , 0x00},   // lea rbx,[base]            FreeModel
			{0x0200CC9F, 3, 7, true , 0x00},   // lea rax,[base]            GetBool   (leaf, no .pdata)
			{0x0200CCCF, 3, 7, true , 0x08},   // lea rax,[base+8]          GetDataType (leaf)
			{0x0200CCEF, 3, 7, true , 0x00},   // lea rax,[base]            GetFunction (leaf)
			{0x0200CD2F, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
			{0x0200CE4D, 3, 7, true , 0x00},   // lea r11,[base]            GetModel
			{0x0200CFA0, 3, 7, true , 0x00},   // lea rax,[base]            GetReal   (leaf)
			{0x0200CFD4, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
			{0x0200CFFF, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
			{0x0200D20F, 5, 9, false, 0x20},   // movzx ebx,[r13+rax*8+base+0x20]  notify
			{0x0200D413, 3, 7, true , 0x20},   // lea rax,[base+0x20]       Reset (subscription heads)
			{0x0200D438, 3, 7, true , 0x22},   // lea rdi,[base+0x22]       Reset (persistent)
			{0x0200D498, 3, 7, true , 0x00},   // lea r9,[base]             typed get/set
			{0x0200D4FC, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D555, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D5AC, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D5F7, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D661, 3, 7, true , 0x00},   // lea r15,[base]            SetString
			{0x0200D74C, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D7F1, 4, 8, false, 0x20},   // lea rdx,[rcx*8+base+0x20] Subscribe
			{0x0200D850, 3, 7, false, 0x20},   // lea rdx,[r10+base+0x20]   (leaf)
			{0x0200D97F, 4, 8, false, 0x20},   // movzx ecx,[rax+rbp+base+0x20]  unsubscribe
			{0x0200D9C7, 4, 8, false, 0x1A},   // movzx ecx,[rax+rbp+base+0x1A]
			{0x0200D9E7, 4, 8, false, 0x1E},   // movzx ebx,[rdi+rdx*8+base+0x1E]
		};
		// Command buffers for local clients 2/3. MP players 3/4 never spawned: their
		// class choice (a client command, Cbuf_AddText(lc)) was dropped because cbuf
		// records 2/3 were empty, and Com_Frame executed only lc < 2. As PS4
		// Cbuf_Init 0xE2FB20 does, give records 2/3 a 64 KB buffer each (the records
		// are already [4], reloc_tables "cbuf"), then widen Cbuf_Execute's range
		// check (the bytes it guards for 2/3 are padding) and Com_Frame's loop,
		// both 2 -> 4. Runs at post_unpack, before Cbuf_Init; all or nothing.
		constexpr uint32_t cbuf_old_records_rva = 0x1681EFB8;
		constexpr uint32_t cbuf_exec_lea_rva = 0x020DFBC8;   // lea rax,[records] in Cbuf_ExecuteInternal
		constexpr uint8_t cbuf_exec_lea_head[] = {0x48, 0x8D, 0x05};
		constexpr uint32_t cbuf_range_check_rva = 0x020DFA2D;
		constexpr uint8_t cbuf_range_check_stock[] = {0x48, 0x83, 0xFB, 0x02, 0x73, 0x13};
		constexpr uint32_t cbuf_frame_bound_rva = 0x020ECDB3;
		constexpr uint8_t cbuf_frame_bound_stock[] = {0x83, 0xFE, 0x02, 0x7C, 0xE9};
		constexpr uint32_t cbuf_text_size = 0x10000;
		constexpr size_t cbuf_record_stride = 0x10;
		const char* cbuf34_result = "command buffers 2/3: not attempted";

		bool install_cbuf_for_players34()
		{
			const auto b = base();
			const auto* lea = reinterpret_cast<const uint8_t*>(b + cbuf_exec_lea_rva);
			if (!readable(lea, 7) || std::memcmp(lea, cbuf_exec_lea_head, sizeof(cbuf_exec_lea_head)) != 0)
			{
				cbuf34_result = "command buffers 2/3: NOT installed - Cbuf_ExecuteInternal lea differs";
				return false;
			}
			int32_t disp = 0;
			std::memcpy(&disp, lea + 3, sizeof(disp));
			auto* records = reinterpret_cast<uint8_t*>(b + cbuf_exec_lea_rva + 7 + static_cast<int64_t>(disp));
			if (records == reinterpret_cast<uint8_t*>(b + cbuf_old_records_rva))
			{
				cbuf34_result = "command buffers 2/3: NOT installed - the cbuf records were not relocated";
				return false;
			}
			if (!readable(records, 4 * cbuf_record_stride))
			{
				cbuf34_result = "command buffers 2/3: NOT installed - records unreadable";
				return false;
			}
			for (size_t i = 2 * cbuf_record_stride; i < 4 * cbuf_record_stride; ++i)
			{
				if (records[i] != 0)
				{
					cbuf34_result = "command buffers 2/3: NOT installed - records 2/3 are not empty";
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
				cbuf34_result = "command buffers 2/3: NOT installed - range check or Com_Frame bound bytes differ";
				return false;
			}
			auto* text = static_cast<uint8_t*>(
				VirtualAlloc(nullptr, 2 * cbuf_text_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
			if (!text)
			{
				cbuf34_result = "command buffers 2/3: NOT installed - allocation failed";
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
				cbuf34_result = "command buffers 2/3: NOT installed - range check write failed";
				return false;
			}
			if (!write_bytes(bound + 2, &four, 1))
			{
				write_bytes(range + 3, &two, 1);
				std::memset(records + 2 * cbuf_record_stride, 0, 2 * cbuf_record_stride);
				cbuf34_result = "command buffers 2/3: NOT installed - Com_Frame bound write failed (rolled back)";
				return false;
			}
			cbuf_range_resting = 0x04;
			cbuf34_result = "command buffers 2/3: 64 KB each, Cbuf_Execute range 2 -> 4, Com_Frame Cbuf loop 2 -> 4";
			return true;
		}

		// Lobby join clients [2] -> [4] and LobbyMsgTransport_Update 2 -> 4. With
		// 3-4 players seated, "Failed to host lobby": the party join waits for every
		// member to agree, and controllers 2/3 were never polled (PS4 0xCD2100 loops
		// c < 4). The agreement request handler indexes s_joinClient (PC [2] x
		// 0xB0, PS4 [4] x 0xB8), whose slot 2 is foreign, so it moves first (9 sites
		// + 2 end markers). Slots 2/3 start as copies of slot 1 with state (+0) 0 and
		// controller index (+0xAC) 2/3.
		constexpr uint32_t joinclient_base = 0x156CB4B0;
		constexpr uint32_t joinclient_stride = 0xB0;
		constexpr uint32_t joinclient_old_count = 2;
		constexpr uint32_t joinclient_new_count = 4;
		constexpr uint32_t joinclient_ci_off = 0xAC;
		constexpr entcoll_site joinclient_sites[] = {
			{0x01ED81CF, 3, 7, true , 0x0},     // lea rbx,[base]          reset loop
			{0x01ED8223, 3, 7, true , 0x0},     // lea rcx,[base]          getter (leaf)
			{0x01ED825C, 2, 6, true , 0x0},     // mov [base],eax          init (leaf)
			{0x01ED8262, 3, 7, true , 0xAC},    // mov qword [base+0xAC]   init: slot0 ci, slot1 state
			{0x01ED8252, 2, 10, true, 0x15C},   // mov dword [base+0x15C],1  init: slot1 ci
			{0x01ED8298, 3, 7, true , 0x0},     // lea rax,[base]          agreement request handler
			{0x01ED8438, 3, 7, true , 0x0},     // lea rax,[base]
			{0x01ED86D5, 3, 7, true , 0xA8},    // lea rbx,[base+0xA8]     update loop start
			{0x02E904FF, 3, 7, true , 0x6E},    // lea rbx,[base+0x6E]     static ctor (ran already)
		};
		constexpr entcoll_site joinclient_end_sites[] = {
			{0x01ED81D8, 3, 7, true , 0x160},   // lea rsi,[base+2*0xB0]        reset loop end
			{0x01ED86DE, 3, 7, true , 0x208},   // lea r14,[base+0xA8+2*0xB0]   update loop end
		};
		constexpr uint32_t lobbymsg_bound_rva = 0x01EEC68E;
		constexpr uint8_t lobbymsg_bound_stock[] = {0x83, 0xFB, 0x02, 0x7C, 0xC5};   // cmp ebx,2 / jl
		constexpr uint32_t netchan_get_lea_rva = 0x0211BF51;                          // lea rax,[s_netchan]
		constexpr uint32_t netchan_old_base = 0x16DEAEB0;
		const char* joinclient_result = "lobby join clients: not attempted";
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
				joinclient_result = "lobby join clients: NOT moved - LobbyMsgTransport_Update bytes differ";
				return false;
			}
			// The widened loop reads controllers 2/3 through the netchan table, so
			// it must already be the relocated [4] one (reloc_tables "netchan").
			const auto* nlea = reinterpret_cast<const uint8_t*>(b + netchan_get_lea_rva);
			int32_t nd = 0;
			if (!readable(nlea, 7))
			{
				joinclient_result = "lobby join clients: NOT moved - netchan lea unreadable";
				return false;
			}
			std::memcpy(&nd, nlea + 3, sizeof(nd));
			if (netchan_get_lea_rva + 7 + static_cast<int64_t>(nd) == netchan_old_base)
			{
				joinclient_result = "lobby join clients: NOT moved - the netchan table is still [2]";
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(joinclient_new_count * joinclient_stride));
			if (!fresh)
			{
				joinclient_result = "lobby join clients: NOT moved - allocation failed";
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
				joinclient_result = "lobby join clients: NOT moved - a reference did not match";
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
				joinclient_result = "lobby join clients: NOT moved - an end marker did not match (rolled back)";
				return false;
			}
			const uint8_t four = 0x04;
			if (!write_bytes(bound + 2, &four, 1))
			{
				joinclient_result = "lobby join clients [2] -> [4] moved, but the message loop widen FAILED";
				joinclient_new = fresh_abs;
				return false;
			}
			joinclient_new = fresh_abs;
			joinclient_result = "lobby join clients [2] -> [4] (9 sites + 2 end markers), LobbyMsgTransport_Update 2 -> 4";
			note("[splitscreen] lobby join clients [2] -> [4] at RVA 0x%08X, lobby message loop 2 -> 4\n",
			     static_cast<uint32_t>(fresh_abs - b));
			return true;
		}

		// Lua Engine.GetClientNum / GetPredictedClientNum returned -1 for controllers
		// 2/3 (no MP HUD in panes 3/4): both start with `cmp ecx,1 / ja -> -1`, while
		// PS4 0xD18000 accepts < 4. Behind the check the PC only indexes cg globals
		// while lc < cl_maxLocalClients, so widening to 3 is safe. Eight other Lua
		// bindings with this check are frontend/online and stay.
		struct ctrl_check_patch
		{
			uint32_t rva;           // the cmp
			uint8_t stock[9];       // cmp ecx,1 ; ja rel32
			const char* what;
		};
		constexpr ctrl_check_patch lua_ctrl_checks[] = {
			{0x01F427FE, {0x83, 0xF9, 0x01, 0x0F, 0x87, 0x12, 0x18, 0x00, 0x00}, "Engine.GetClientNum"},
			{0x01F4FC2E, {0x83, 0xF9, 0x01, 0x0F, 0x87, 0x17, 0x18, 0x00, 0x00}, "Engine.GetPredictedClientNum"},
		};
		const char* lua_ctrl_result = "lua controller checks: not attempted";

		bool widen_lua_controller_checks()
		{
			const auto b = base();
			for (const auto& p : lua_ctrl_checks)
			{
				const auto* site = reinterpret_cast<const uint8_t*>(b + p.rva);
				if (!readable(site, sizeof(p.stock)) || std::memcmp(site, p.stock, sizeof(p.stock)) != 0)
				{
					lua_ctrl_result = "lua controller checks: NOT widened - bytes differ";
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
			lua_ctrl_result = done == std::size(lua_ctrl_checks)
				                  ? "lua controller checks 1 -> 3: Engine.GetClientNum, GetPredictedClientNum"
				                  : "lua controller checks: a write FAILED";
			return done == std::size(lua_ctrl_checks);
		}

		// UI model string hunk "UIModelAllocator" 0xC0000 -> 4 MB (PS4 0x80000). With
		// four class lists Hunk_UserAlloc returned NULL and UI_Model_SetString
		// crashed. The hidden UI_Model_Init creates the hunk through the visible
		// Hunk_UserCreateFromBuffer (PS4 0x10D1C90); this detour swaps in a bigger
		// buffer for exactly that call. Nothing else references the static buffer.
		constexpr uint32_t hunk_create_rva = 0x02276DA0;
		constexpr uint8_t hunk_create_prologue[] = {
			0x49, 0x63, 0xC0,                          // movsxd rax,r8d
			0x4C, 0x8D, 0x1D, 0x46, 0xBB, 0x14, 0x01,  // lea r11,[rip+0x0114B486] (scheme table)
		};
		constexpr size_t model_string_stock_size = 0xC0000;
		constexpr size_t model_string_new_size = 0x400000;
		utils::hook::detour hunk_create_hook;
		void* model_string_buffer = nullptr;
		const char* model_string_result = "ui model string hunk: not installed";

		// "ClientCache_ClientPool" hunk 0x3880 -> 0x7100. Each player centity takes
		// two blocks per local client from it; with four players it ran full and the
		// ET_PLAYER handler did memset(NULL). The PS4 pool is sized for 4 local
		// clients, the PC one for two, so it doubles.
		constexpr size_t client_cache_stock_size = 0x3880;
		constexpr size_t client_cache_new_size = 0x7100;
		void* client_cache_buffer = nullptr;
		const char* client_cache_result = "client cache pool: not seen";

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
					client_cache_result = "client cache pool 0x3880 -> 0x7100 (ClientCache_ClientPool)";
					note("[splitscreen] client cache pool 0x3880 -> 0x7100 (ClientCache_ClientPool)\n");
				}
				else
				{
					client_cache_result = "client cache pool: allocation failed - stock 0x3880 kept";
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
					model_string_result = "ui model string hunk 0xC0000 -> 0x400000 (UIModelAllocator)";
				}
				else
				{
					model_string_result = "ui model string hunk: allocation failed - stock 0xC0000 kept";
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
				model_string_result = "ui model string hunk: NOT hooked - Hunk_UserCreateFromBuffer bytes differ";
				return false;
			}
			hunk_create_hook.create(reinterpret_cast<void*>(base() + hunk_create_rva), hunk_create_stub);
			model_string_result = "ui model string hunk: hooked, waiting for UI_Model_Init";
			return true;
		}

		constexpr uint32_t model_pool_bound_rva = 0x0200D40E;
		constexpr uint8_t model_pool_bound_stock[] = {0xBE, 0x00, 0x90, 0x00, 0x00};   // mov esi,0x9000
		constexpr uint8_t model_pool_bound_new[] = {0xBE, 0xFF, 0xFF, 0x00, 0x00};     // mov esi,0xFFFF
		const char* model_pool_result = "ui model pool: not attempted";
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
				model_pool_result = "ui model pool: NOT moved - reset bound bytes differ at 0x0200DACE";
				return false;
			}
			// The old array must still be all zero: if UI_Model_Init had already
			// run, live nodes would be left behind.
			const auto* old_nodes = reinterpret_cast<const uint8_t*>(b + model_pool_base);
			if (!readable(old_nodes, 16 * model_pool_stride))
			{
				model_pool_result = "ui model pool: NOT moved - old array unreadable";
				return false;
			}
			for (size_t i = 0; i < 16 * model_pool_stride; ++i)
			{
				if (old_nodes[i] != 0)
				{
					model_pool_result = "ui model pool: NOT moved - old array already initialised";
					return false;
				}
			}
			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(static_cast<size_t>(model_pool_new_count) * model_pool_stride));
			if (!fresh)
			{
				model_pool_result = "ui model pool: NOT moved - allocation failed";
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
				model_pool_result = "ui model pool: NOT moved - a reference did not match";
				return false;
			}
			if (!write_bytes(bound, model_pool_bound_new, sizeof(model_pool_bound_new)))
			{
				for (size_t j = 0; j < std::size(model_pool_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + model_pool_sites[j].rva);
					write_bytes(insn + model_pool_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				model_pool_result = "ui model pool: NOT moved - reset bound write failed (rolled back)";
				return false;
			}
			model_pool_new = fresh_abs;
			model_pool_result = "ui model pool 0x9000 -> 0xFFFF nodes (28 sites, reset bound, 0x9000 kept as end sentinel)";
			install_model_string_hunk();
			note("[splitscreen] ui model pool 0x9000 -> 0xFFFF nodes at RVA 0x%08X\n",
			     static_cast<uint32_t>(fresh_abs - b));
			return true;
		}

		// Per-view sun-shadow (SST) ring 4 -> 8 entries. Each view takes a record
		// (0x21F0 bytes of GPU buffers) from a 4-entry ring; with four views a record
		// is reused every frame while the GPU may still draw from it (pane 4 shadow
		// flicker). 8 = 4 views x 2 frames, the stock margin. Patched: base leas,
		// mask 3 -> 7, free count 4 -> 8, static constructor count 3 -> 7. The
		// constructors only write zeros, so a zeroed block is constructed. Runs at
		// post_unpack, before the renderer creates the buffers.
		constexpr uint32_t sst_base = 0x10B21260;
		constexpr uint32_t sst_stride = 0x21F0;
		constexpr uint32_t sst_old_count = 4;
		constexpr uint32_t sst_new_count = 8;
		constexpr entcoll_site sst_sites[] = {
			{0x01D0E059, 3, 7, true, 0},            // lea rdx,[base]        alloc
			{0x01D0D598, 3, 7, true, sst_stride},   // lea rbp,[base+0x21F0] buffer creation
			{0x01D0DF0A, 3, 7, true, sst_stride},   // lea rbp,[base+0x21F0] free loop
			{0x02E8A83A, 3, 7, true, 0},            // lea rbx,[base]        static constructor
		};
		constexpr entcoll_site sst_end_site[] = {
			{0x01D0D5A6, 3, 7, true, sst_old_count * sst_stride},   // lea r14,[end] creation loop end
		};
		struct sst_imm { uint32_t rva; uint8_t off; uint8_t size; uint32_t was; uint32_t want; };
		constexpr sst_imm sst_imms[] = {
			{0x01D0E056, 2, 1, 3, sst_new_count - 1},   // and eax,3 -> 7
			{0x01D0DF11, 2, 4, 4, sst_new_count},       // mov r14d,4 -> 8
			{0x02E8A841, 1, 4, 3, sst_new_count - 1},   // mov edi,3 (dec/jns) -> 7
		};
		const char* sst_result = "sun-shadow ring: not attempted";
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
					sst_result = "sun-shadow ring: NOT moved - immediate unreadable";
					return false;
				}
				std::memcpy(&cur, at, s.size);
				if (cur != s.was)
				{
					sst_result = "sun-shadow ring: NOT moved - an immediate differs";
					return false;
				}
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(sst_new_count * sst_stride));
			if (!fresh)
			{
				sst_result = "sun-shadow ring: NOT moved - allocation failed";
				return false;
			}
			std::memset(fresh, 0, sst_new_count * sst_stride);
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			static int32_t saved[std::size(sst_sites)]{};
			static int32_t saved_end[std::size(sst_end_site)]{};
			if (!rewrite_entcoll(sst_sites, std::size(sst_sites), sst_base, fresh_abs, saved))
			{
				sst_result = "sun-shadow ring: NOT moved - a base lea did not match";
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
				sst_result = "sun-shadow ring: NOT moved - the end lea did not match";
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
					sst_result = "sun-shadow ring: NOT moved - an immediate write failed (rolled back)";
					return false;
				}
				++done;
			}
			sst_new = fresh_abs;
			sst_result = "sun-shadow ring [4] -> [8] (per-view records, 5 leas + 3 immediates)";
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
		// Slots 2..3 are foreign, so each is relocated. Gated on BO3_CG_FRAME.
		// cgDC - the per-client display context (CG_Init memsets cgDC[lc])
		constexpr uint32_t cgdc_base = 0x049B2CD0;
		constexpr uint32_t cgdc_stride = 0x1838;
		constexpr entcoll_site cgdc_sites[] = {
			{0x008F0ABC, 3, 7, false, 0x0000}, // lea rbx, [rbx + 0x4a31cd0]
			{0x010AAC43, 5, 9, false, 0x002C}, // movss xmm0, dword ptr [rax + rcx + 0x4a31cfc]
		};
		bool cgdc_relocated = false;

		bool relocate_cgdc()
		{
			if (cgdc_relocated) { return true; }
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * cgdc_stride));
			if (!fresh) { return false; }
			std::memset(fresh, 0, 4 * cgdc_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + cgdc_base), 2 * cgdc_stride);
			static int32_t saved[std::size(cgdc_sites)]{};
			if (!rewrite_entcoll(cgdc_sites, std::size(cgdc_sites), cgdc_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			cgdc_relocated = true;
			note("[splitscreen] cgdc [2]->[4] at RVA 0x%08X (%zu sites)\n",
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), std::size(cgdc_sites));
			return true;
		}

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
		constexpr uint32_t playerkeys_base = 0x0531D850;
		constexpr uint32_t playerkeys_stride = 0x1940;
		constexpr entcoll_site playerkeys_sites[] = {
			{0x012F2B9B, 3, 7, false, 0x1938}, // mov esi, dword ptr [rax + rsi + 0x539d988]
			{0x0133A477, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x4061bf2]
			{0x0133DE94, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405e1d5]
			{0x0133F27A, 3, 7, false, 0x0138}, // lea r15, [r8 + 0x539c188]
			{0x0133F2C6, 4, 8, false, 0x0130}, // inc dword ptr [r8 + rbp + 0x539c180]
			{0x0133F2D5, 4, 8, false, 0x0130}, // dec dword ptr [r8 + rbp + 0x539c180]
			{0x0133F2DD, 4, 8, false, 0x0130}, // mov eax, dword ptr [r8 + rbp + 0x539c180]
			{0x0133F2E8, 4, 8, false, 0x0130}, // mov dword ptr [r8 + rbp + 0x539c180], eax
			{0x0133F668, 3, 7, false, 0x0138}, // lea r12, [r8 + 0x539c188]
			{0x0133F6C1, 4, 8, false, 0x0130}, // inc dword ptr [rbx + r8 + 0x539c180]
			{0x0133F6D0, 4, 8, false, 0x0130}, // dec dword ptr [rbx + r8 + 0x539c180]
			{0x0133F6D8, 4, 8, false, 0x0130}, // mov eax, dword ptr [rbx + r8 + 0x539c180]
			{0x0133F6E3, 4, 8, false, 0x0130}, // mov dword ptr [rbx + r8 + 0x539c180], eax
			{0x0133FEC2, 4, 8, false, 0x0138}, // lea r12, [r12 + 0x539c188]
			{0x0133FF22, 3, 7, false, 0x0130}, // inc dword ptr [rdi + rax + 0x539c180]
			{0x013405A4, 3, 7, true , 0x1938}, // lea rax, [rip + 0x405d3fd]
			{0x01340E1F, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405b24a]
			{0x01341D93, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x405a40e]
			{0x01341E42, 3, 7, true , 0x0000}, // lea r12, [rip + 0x405a227]
			{0x01341ED9, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a190]
			{0x01341F93, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a0d6]
			{0x01342029, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a040]
			{0x01342261, 3, 7, false, 0x0138}, // lea r13, [rdx + 0x539c188]
			{0x013422C6, 3, 7, false, 0x0130}, // inc dword ptr [rsi + rdx + 0x539c180]
			{0x013422D4, 3, 7, false, 0x0130}, // dec dword ptr [rsi + rdx + 0x539c180]
			{0x013422DB, 3, 7, false, 0x0130}, // mov eax, dword ptr [rsi + rdx + 0x539c180]
			{0x013422E6, 3, 7, false, 0x0130}, // mov dword ptr [rsi + rdx + 0x539c180], eax
			{0x01343E68, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], eax
			{0x01343E7E, 4, 12, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], 3
			{0x01343E9D, 4, 9, false, 0x1938}, // cmp dword ptr [rsi + r9 + 0x539d988], 2
			{0x01343EB8, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], r8d
			{0x01343EC2, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], r12d
			{0x0134499C, 4, 8, false, 0x1038}, // mov ebp, dword ptr [rax + r15 + 0x539d088]
			{0x013449A4, 4, 8, false, 0x1020}, // mov edi, dword ptr [rax + r15 + 0x539d070]
			{0x013449AC, 4, 8, false, 0x1008}, // mov r14d, dword ptr [rax + r15 + 0x539d058]
			{0x01345375, 3, 7, true , 0x012C}, // lea rax, [rip + 0x4056e20]
			{0x01345650, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056a19]
			{0x013456F0, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056979]
			{0x013457EF, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405687a]
			{0x013459E0, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056689]
			{0x01345DC2, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x40562a7]
			{0x01345F97, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x40560d2]
			{0x013462E1, 4, 8, false, 0x0140}, // cmp r14d, dword ptr [rax + r11 + 0x539c190]
			{0x0134638D, 4, 8, false, 0x0144}, // cmp r11d, dword ptr [rax + r14 + 0x539c194]
			{0x013463D8, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r15d
			{0x013463E0, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x0134640C, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r15d
			{0x01346414, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x01346468, 3, 7, false, 0x0148}, // lea rcx, [rax + 0x539c198]
			{0x0134648B, 4, 8, false, 0x0140}, // mov dword ptr [rsi + rax + 0x539c190], r14d
			{0x0134649A, 4, 12, false, 0x193C}, // mov dword ptr [rdi + r14 + 0x539d98c], 1
			{0x013464F5, 4, 8, false, 0x0140}, // cmp r10d, dword ptr [rcx + r14 + 0x539c190]
			{0x01346537, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r11d
			{0x0134653F, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x01346566, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r11d
			{0x0134656E, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x0134674A, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x4055a57]
			{0x01346917, 3, 7, false, 0x0148}, // lea rcx, [rsi + 0x539c198]
			{0x01346928, 4, 8, false, 0x0140}, // mov qword ptr [rbx + rsi + 0x539c190], rax
			{0x01346930, 3, 11, false, 0x193C}, // mov dword ptr [rdi + rsi + 0x539d98c], 1
			{0x01346B24, 3, 7, true , 0x0144}, // lea rax, [rip + 0x4055689]
			{0x01346BD2, 3, 7, true , 0x0138}, // lea rax, [rip + 0x40555cf]
			{0x01346C62, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4055407]
			{0x01346D0B, 3, 7, true , 0x0140}, // lea rcx, [rip + 0x405549e]
			{0x01346E37, 3, 7, true , 0x0138}, // lea rax, [rip + 0x405536a]
			{0x01346EE2, 3, 7, true , 0x0140}, // lea r13, [rip + 0x40552c7]
			{0x0134700D, 3, 7, true , 0x0140}, // lea rax, [rip + 0x405519c]
			{0x013470A0, 3, 7, true , 0x0138}, // lea rax, [rip + 0x4055101]
			{0x0134724D, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x4054f54]
			{0x013475E6, 3, 7, true , 0x193C}, // lea rdi, [rip + 0x40563bf]
			{0x013478B4, 3, 7, true , 0x0000}, // lea r15, [rip + 0x40547b5]
			{0x01347993, 3, 7, true , 0x193C}, // lea rcx, [rip + 0x4056012]
			{0x013479FA, 3, 7, true , 0x0148}, // lea rsi, [rip + 0x40547b7]
			{0x01347C68, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4054401]
			{0x01347D9C, 3, 7, true , 0x0000}, // lea r10, [rip + 0x40542cd]
			{0x01347E2F, 3, 7, true , 0x0000}, // lea r12, [rip + 0x405423a]
			{0x01347E3B, 3, 7, true , 0x0148}, // lea rax, [rip + 0x4054376]
			{0x01347E77, 3, 7, true , 0x0148}, // lea rax, [rip + 0x405433a]
			{0x01347F7F, 3, 7, true , 0x0138}, // lea rax, [rip + 0x4054222]
			{0x013481CE, 3, 7, true , 0x0000}, // lea r8, [rip + 0x4053e9b]
			{0x01DDE2EA, 3, 7, true , 0x0000}, // lea rax, [rip + 0x35b12cf]
			// Not a site: 0x0219DA4D is unreachable Arxan filler whose bytes happen
			// to equal playerKeys+0x2994. Never write unproven bytes.
		};
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
			if (!retarget_end_marker(0x01347A03, 3, 7, playerkeys_base + 2 * playerkeys_stride + 0x148,
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
			note("[splitscreen] playerKeys [2]->[4] at RVA 0x%08X (%zu sites)\n",
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), std::size(playerkeys_sites));
			return true;
		}

		// g_notetrackLerps: PS4 [LOCAL_CLIENT_COUNT][16] x 0x34 (CG_InitNotetrackLerps
		// 0x13C6A0), [2] on PC. For lc 2, CG_UpdateNotetrackLerps read an entity number
		// from foreign memory and wrote through a wild pointer. 34 sites (5 RIP,
		// 29 ABS32), data/reloc_sites/sites_notetracklerps.txt.
		// CG_InitNotetrackLerps(2) initializes the new row.
		constexpr uint32_t notetracklerps_base = 0x0474B130;
		constexpr uint32_t notetracklerps_stride = 0x340;
		constexpr entcoll_site notetracklerps_sites[] = {
			{0x00248E3F, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x45812ea]
			{0x00249355, 3, 7, true , 0x0034}, // lea rcx, [rip + 0x4580e08]
			{0x0024E039, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x457c0f0]
			{0x0026EE8A, 4, 9, false, 0x0000}, // cmp dword ptr [rdi + r10 + 0x47ca130], 3
			{0x0026EE99, 4, 8, false, 0x002C}, // movsxd r8, dword ptr [rdi + r10 + 0x47ca15c]
			{0x002735C3, 4, 8, false, 0x0000}, // mov eax, dword ptr [rdi + r10 + 0x47ca130]
			{0x002735CF, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
			{0x002735D7, 3, 7, true , 0x0014}, // lea rax, [rip + 0x4556b66]
			{0x00273605, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027361B, 4, 8, false, 0x0014}, // mov eax, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027366C, 4, 12, false, 0x0000}, // mov dword ptr [rdi + r10 + 0x47ca130], 3
			{0x00275949, 4, 9, false, 0x0000}, // cmp dword ptr [rdi + r10 + 0x47ca130], 3
			{0x0027595F, 4, 8, false, 0x002C}, // movsxd r8, dword ptr [rdi + r10 + 0x47ca15c]
			{0x0027A063, 4, 8, false, 0x0024}, // mov edx, dword ptr [rdi + r10 + 0x47ca154]
			{0x0027A06B, 4, 8, false, 0x0028}, // mov r8d, dword ptr [rdi + r10 + 0x47ca158]
			{0x0027A07F, 4, 8, false, 0x0000}, // mov eax, dword ptr [rdi + r10 + 0x47ca130]
			{0x0027A0AA, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
			{0x0027A0B4, 6, 10, false, 0x0008}, // movss xmm2, dword ptr [rdi + r10 + 0x47ca138]
			{0x0027A0BE, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A0C8, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
			{0x0027A0E7, 6, 10, false, 0x0018}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca148]
			{0x0027A0FA, 6, 10, false, 0x000C}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca13c]
			{0x0027A10C, 6, 10, false, 0x001C}, // movss xmm2, dword ptr [rdi + r10 + 0x47ca14c]
			{0x0027A11F, 6, 10, false, 0x0010}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca140]
			{0x0027A131, 6, 10, false, 0x0020}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca150]
			{0x0027A167, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
			{0x0027A171, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A1A8, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
			{0x0027A1B2, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A1D9, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
			{0x0027A1F5, 3, 7, true , 0x0014}, // lea rax, [rip + 0x454ff48]
			{0x0027A216, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A22C, 4, 8, false, 0x0014}, // mov eax, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A273, 4, 12, false, 0x0000}, // mov dword ptr [rdi + r10 + 0x47ca130], 3
		};
		bool notetracklerps_relocated = false;

		bool relocate_notetracklerps()
		{
			if (notetracklerps_relocated) { return true; }
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * notetracklerps_stride));
			if (!fresh) { return false; }
			std::memset(fresh, 0, 4 * notetracklerps_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + notetracklerps_base), 2 * notetracklerps_stride);
			static int32_t saved[std::size(notetracklerps_sites)]{};
			if (!rewrite_entcoll(notetracklerps_sites, std::size(notetracklerps_sites),
			                     notetracklerps_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			notetracklerps_relocated = true;
			note("[splitscreen] g_notetrackLerps [2]->[4] at RVA 0x%08X (%zu sites)\n",
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), std::size(notetracklerps_sites));
			return true;
		}

		// DWARF-map batch 1b: PS4 [4] globals the cgame frame touches, matched to the
		// PC by stride and confirmed by the index register (lc, or the cg_t index).
		// Sites from tools/gen_reloc_sites.py v2.2 (data/reloc_sites/sites_<name>.txt),
		// each verified against the old address before anything is written.
		struct perclient_array
		{
			const char* name;
			uint32_t base;              // old RVA of slot 0
			uint32_t stride;            // bytes per local client
			const entcoll_site* sites;
			size_t count;
			uint32_t ctor_rva;          // 0: zero-fill IS the initial state
			uint8_t ctor_sig[8];        // the constructor's first bytes, verified
		};

		// cg_pmove - pmove_t[LOCAL_CLIENT_COUNT] (PS4 0x0451EFE0, 0x1660), used every
		// frame by CG_PredictPlayerState_Internal. The element constructor stores a
		// vtable at +0x2C0 (a zeroed slot would call through NULL), so it runs on 2/3.
		constexpr entcoll_site cg_pmove_sites[] = {
			{0x00926DC4, 2, 6, true , 0x0340}, // mov dword ptr [rip + 0x43f1cb6], esi
			{0x00926DCA, 3, 7, true , 0x0344}, // mov byte ptr [rip + 0x43f1cb3], sil
			{0x00926DD1, 2, 10, true , 0x02D0}, // mov dword ptr [rip + 0x43f1c35], 0x7e967699
			{0x00926DDB, 2, 10, true , 0x02D4}, // mov dword ptr [rip + 0x43f1c2f], 0x7e967699
			{0x00926DE5, 2, 10, true , 0x02D8}, // mov dword ptr [rip + 0x43f1c29], 0x7e967699
			{0x00926DEF, 2, 10, true , 0x02E0}, // mov dword ptr [rip + 0x43f1c27], 0xfe967699
			{0x00926DF9, 2, 10, true , 0x02E4}, // mov dword ptr [rip + 0x43f1c21], 0xfe967699
			{0x00926E03, 2, 10, true , 0x02E8}, // mov dword ptr [rip + 0x43f1c1b], 0xfe967699
			{0x00926E0D, 2, 6, true , 0x19A0}, // mov dword ptr [rip + 0x43f32cd], esi
			{0x00926E14, 2, 6, true , 0x19A4}, // mov byte ptr [rip + 0x43f32ca], dh
			{0x00926E1A, 2, 10, true , 0x1930}, // mov dword ptr [rip + 0x43f324c], 0x7e967699
			{0x00926E24, 2, 10, true , 0x1934}, // mov dword ptr [rip + 0x43f3246], 0x7e967699
			{0x00926E2E, 2, 10, true , 0x1938}, // mov dword ptr [rip + 0x43f3240], 0x7e967699
			{0x00926E38, 2, 10, true , 0x1940}, // mov dword ptr [rip + 0x43f323e], 0xfe967699
			{0x00926E42, 2, 10, true , 0x1944}, // mov dword ptr [rip + 0x43f3238], 0xfe967699
			{0x00926E4C, 2, 10, true , 0x1948}, // mov dword ptr [rip + 0x43f3232], 0xfe967699
			{0x009D18A0, 4, 8, false, 0x02B0}, // mov dword ptr [rsi + r15 + 0x4d189f0], ebx
			{0x009D18A8, 3, 7, false, 0x00A8}, // lea rbx, [r15 + 0x4d187e8]
			{0x009D18AF, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi + 0x4d18740], r13
			{0x009D18BA, 4, 9, false, 0x02AC}, // mov byte ptr [rsi + r15 + 0x4d189ec], 0
			{0x009D1915, 4, 12, false, 0x0294}, // mov dword ptr [rsi + r15 + 0x4d189d4], 0
			{0x009D1927, 4, 8, false, 0x0290}, // mov dword ptr [rsi + r15 + 0x4d189d0], eax
			{0x009D1C16, 3, 7, false, 0x0008}, // lea rbx, [r15 + 0x4d18748]
			{0x009D1C6D, 3, 7, false, 0x0000}, // lea rcx, [r15 + 0x4d18740]
			{0x009D1CD0, 3, 7, false, 0x0008}, // lea r13, [rax + 0x4d18748]
			{0x009D1CF4, 3, 7, false, 0x0008}, // mov eax, dword ptr [rsi + rcx + 0x4d18748]
			{0x009D1D10, 3, 7, false, 0x0058}, // lea r8, [rcx + 0x4d18798]
			{0x009D1E56, 4, 8, false, 0x0000}, // mov rax, qword ptr [rsi + rcx + 0x4d18740]
			{0x009D1E5E, 3, 7, false, 0x0000}, // lea rcx, [rcx + 0x4d18740]
			{0x009D1E8D, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rax + 0x4d18740]
			{0x009D1F36, 4, 8, false, 0x0000}, // mov rax, qword ptr [rsi + rax + 0x4d18740]
			{0x009D1F7A, 3, 7, false, 0x0000}, // lea rcx, [r14 + 0x4d18740]
			{0x009D1FE1, 6, 10, false, 0x0294}, // movss xmm8, dword ptr [rsi + r14 + 0x4d189d4]
			{0x009D2000, 4, 8, false, 0x0290}, // cmp dword ptr [rsi + r14 + 0x4d189d0], eax
			{0x009D2086, 4, 8, false, 0x0290}, // mov eax, dword ptr [rsi + r14 + 0x4d189d0]
			{0x009D2106, 4, 8, false, 0x0290}, // mov eax, dword ptr [rsi + r14 + 0x4d189d0]
			{0x010BBA0C, 4, 9, false, 0x1618}, // cmp dword ptr [r14 + rax + 0x4d19d58], 0
			{0x010BBA17, 3, 7, false, 0x161C}, // lea rbx, [rax + 0x4d19d5c]
			{0x010BBA53, 4, 8, false, 0x1618}, // cmp edi, dword ptr [r14 + r15 + 0x4d19d58]
			{0x010C153D, 4, 9, false, 0x1618}, // cmp dword ptr [r12 + rax + 0x4d19d58], 0
			{0x010C1548, 3, 7, false, 0x161C}, // lea rdi, [rax + 0x4d19d5c]
			{0x010C1583, 4, 8, false, 0x1618}, // cmp r14d, dword ptr [r12 + r13 + 0x4d19d58]
			{0x023A0ACE, 3, 7, true , 0x0000}, // mov rdx, qword ptr [rip + 0x28feafb]
			{0x023AAE5A, 4, 9, false, 0x0000}, // cmp qword ptr [rax + r15 + 0x4d18740], 0
			{0x02621DD8, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x267d7f1]
			{0x02CCE142, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x1fd0ff7]
			{0x02EF9657, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x1da0082]
		};

		// s_cameraShakeSet - CameraShakeSet[4] (PS4 0x03F61EF0, 0x104). CG_ClearCameraShakes
		// (0x005830A0) memsets [lc]; CG_ShakeCamera reads it every frame.
		constexpr entcoll_site camerashake_sites[] = {
			{0x005830A3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42608e6]
			{0x0058491C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425f06d]
			{0x0058651E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425d46b]
		};

		// moverInfos - mover_info_t[4] (PS4 0x03F60F50, 0x390), camera-tween mover records.
		// Its constructor is a no-op, so zero is the initial state.
		constexpr entcoll_site moverinfos_sites[] = {
			{0x004CB9D4, 3, 7, true , 0x0000}, // lea rax, [rip + 0x43177d5]
			{0x004F0FDB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42f21ce]
			{0x02CCD75A, 3, 7, true , 0x0000}, // lea rbx, [rip + 0x1a9c44f]
		};

		// moveInfoEntNum - int[4] (PS4 0x03F61D90), read next to moverInfos at 0x004F0FCA.
		// Its slot 2 is used by 3 foreign leas.
		constexpr entcoll_site moveinfoentnum_sites[] = {
			{0x004F0FCA, 3, 7, false, 0x0000}, // lea rsi, [r11 + 0x47e3140]
		};

		// rumbleGlobArray - RumbleGlobals[4] (PS4 0x045269F0, 0x410). GetRumbleGlobals is
		// inlined 8 times; every one indexes with the lc argument.
		constexpr entcoll_site rumble_sites[] = {
			{0x009E6E50, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336619]
			{0x009E6E99, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x43365d0]
			{0x009E6F03, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4336566]
			{0x009E6F98, 3, 7, true , 0x0000}, // lea rax, [rip + 0x43364d1]
			{0x009E7033, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336436]
			{0x009E70AB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x43363be]
			{0x009E7112, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336357]
			{0x009EF6A1, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x432ddc8]
		};

		// atGlobArray - AimTargetGlob[4] (PS4 0x03159380, 0x1604). AimTarget_GetGlobArray
		// (0x000771E0) and the clear (0x0007E100) take lc; used per frame by aim assist.
		constexpr entcoll_site atglob_sites[] = {
			{0x000771E3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36085d6]
			{0x0007AA64, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3604d55]
			{0x0007E103, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36016b6]
			{0x00086724, 4, 8, false, 0x1600}, // mov dword ptr [rax + rdx + 0x3680dc0], r14d
		};

		// g_aimtarget_cmd - AimTarget_Cmd[4] (PS4 0x031592E0, 0x10), indexed lc*16 next to
		// atGlobArray at 0x0008672C. Slot 2 lands on a foreign global.
		constexpr entcoll_site aimtargetcmd_sites[] = {
			{0x0007A991, 3, 7, true , 0x0000}, // lea r12, [rip + 0x3604de8]
			{0x0008672C, 4, 8, false, 0x0004}, // mov dword ptr [rdx + r11*8 + 0x367f784], r14d
			{0x0008978A, 4, 8, false, 0x0004}, // movsxd rdx, dword ptr [rax + r11*8 + 0x367f784]
			{0x000897BA, 4, 8, false, 0x0004}, // mov dword ptr [rdx + r11*8 + 0x367f784], eax
		};

		// gArcData - ARC_DATA[4] (PS4 0x03FF7A10, 0xEEC), grenade arc prediction
		// (CG_ArcPrediction_Update/Render). Indexed by lc next to cg_t (0x342720).
		constexpr entcoll_site arcdata_sites[] = {
			{0x005F11E7, 3, 7, true , 0x0000}, // lea r14, [rip + 0x42270c2]
			{0x005F66A0, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221c09]
			{0x005F6852, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221a57]
			{0x005F6981, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221928]
			{0x005F8562, 3, 7, true , 0x0000}, // lea r9, [rip + 0x421fd47]
			{0x005F86BE, 3, 7, true , 0x0000}, // lea r9, [rip + 0x421fbeb]
			{0x005FBB81, 4, 8, false, 0x0E60}, // mov dword ptr [rsi + r12 + 0x4819110], r15d
			{0x005FBBBD, 4, 8, false, 0x0E60}, // mov dword ptr [rsi + r12 + 0x4819110], r15d
			{0x005FBBC5, 5, 11, false, 0x0EE8}, // mov word ptr [rsi + r12 + 0x4819198], 0x100
			{0x005FF962, 3, 8, false, 0x0EE8}, // mov byte ptr [rsi + rax + 0x4819198], 1
			{0x005FF97B, 3, 8, false, 0x0EE9}, // mov byte ptr [rsi + rax + 0x4819199], 1
			{0x005FF98C, 3, 8, false, 0x0E60}, // cmp dword ptr [rsi + rax + 0x4819110], 1
			{0x005FF996, 3, 8, false, 0x0EE9}, // mov byte ptr [rsi + rax + 0x4819199], 0
		};

		// cg_zbarriers - cgZBarrier_t[4][128] (PS4 0x03F2FE90, row 0xC400 = 128 x 0x188),
		// handed out per lc by CG_InitZBarrier. PC row 2 was foreign memory. The
		// map-start clear is widened in widen_zbarrier_clear().
		constexpr entcoll_site zbarriers_sites[] = {
			{0x00461048, 3, 7, true , 0x0000}, // lea r10, [rip + 0x43698a1]
			{0x004616C4, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4369225]
		};

		constexpr perclient_array batch1b[] = {
			{"cg_pmove", 0x04C99740, 0x1660, cg_pmove_sites, std::size(cg_pmove_sites), 0x009B4720, {0x33, 0xD2, 0x0F, 0x57, 0xC0, 0x48, 0x8D, 0x05}},
			{"camerashake", 0x04764990, 0x104, camerashake_sites, std::size(camerashake_sites), 0x00000000, {}},
			{"moverinfos", 0x047641B0, 0x390, moverinfos_sites, std::size(moverinfos_sites), 0x00000000, {}},
			{"moveinfoentnum", 0x04764140, 0x4, moveinfoentnum_sites, std::size(moveinfoentnum_sites), 0x00000000, {}},
			{"rumble", 0x04C9E470, 0x410, rumble_sites, std::size(rumble_sites), 0x00000000, {}},
			{"atglob", 0x036007C0, 0x1604, atglob_sites, std::size(atglob_sites), 0x00000000, {}},
			{"aimtargetcmd", 0x03600780, 0x10, aimtargetcmd_sites, std::size(aimtargetcmd_sites), 0x00000000, {}},
			{"arcdata", 0x047992B0, 0xEEC, arcdata_sites, std::size(arcdata_sites), 0x00000000, {}},
			{"zbarriers", 0x0474B8F0, 0xC400, zbarriers_sites, std::size(zbarriers_sites), 0x00000000, {}},
		};
		size_t batch1b_new[std::size(batch1b)] = {};

		// Per-local-client [2][18] x 0x132 array: the game session's 18 member slots
		// for each local client. lc 2 ran past it into a static cmd_function_t node.
		// Moved to [4]; its clear (memset 0x2B08) widens to four rows.
		constexpr entcoll_site session_member_sites[] = {
			{0x020E4180, 3, 7, true , 0x0000}, // lea rcx, [arr]           clear
			{0x020E6224, 3, 7, false, 0x0000}, // lea rdx, [rax + arr]     rax = image base
			{0x020E6260, 3, 7, true , 0x0021}, // lea rax, [arr + 0x21]
			{0x020E6271, 3, 7, false, 0x0001}, // lea rsi, [rsi + arr + 1] image-base relative
		};
		constexpr uint32_t session_member_base = 0x1684FAA0;
		constexpr uint32_t session_member_stride = 18 * 0x132;   // 0x1584
		constexpr uint32_t session_member_clear_rva = 0x020E4189;
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
			note("[splitscreen] %s [2]->[4] at RVA 0x%08X (%zu sites)\n", a.name,
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), a.count);
			return reinterpret_cast<size_t>(fresh);
		}

		// aaGlobArray ([2] x 0x4E30), complete table: 50 sites (28 rip, 22 abs,
		// data/reloc_sites/sites_aimglob.txt). Moving only the `lea reg,[base]`
		// sites left field accessors on the old array (player 4 crashed aiming).
		// 0x00039BB9 is a site: rcx holds the module base there. Zero-fill is the
		// initial state (AimTarget_Init memsets each slot).
		constexpr entcoll_site aaglob_v2_sites[] = {
			{0x0002D7B6, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3647a13]
			{0x0002DAD3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36476f6]
			{0x0002F70F, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3645aba]
			{0x0002FC45, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3645584]
			{0x0002FFD1, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36451f8]
			{0x00034D16, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36404b3]
			{0x000369C8, 3, 7, true , 0x0000}, // lea rax, [rip + 0x363e801]
			{0x00039BB9, 3, 7, false, 0x0000}, // lea rsi, [rcx + 0x36751d0]
			{0x0003FF57, 3, 7, false, 0x4E18}, // mov ebx, dword ptr [rax + rbx + 0x3679fe8]
			{0x00043773, 3, 7, true , 0x4E20}, // lea rdx, [rip + 0x3636876]
			{0x00043793, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3631a36]
			{0x00043E7D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x363134c]
			{0x000457A3, 3, 7, true , 0x4E20}, // lea rcx, [rip + 0x3634846]
			{0x0004EDEE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36263db]
			{0x000520D3, 3, 7, true , 0x2814}, // lea rcx, [rip + 0x362590a]
			{0x00052104, 3, 7, true , 0x0214}, // lea rcx, [rip + 0x36232d9]
			{0x0005219A, 3, 7, true , 0x0000}, // lea rax, [rip + 0x362302f]
			{0x00056D33, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x361e496]
			{0x0005B969, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3619860]
			{0x0005BA60, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3619769]
			{0x0005D383, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3617e46]
			{0x000604DE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3614ceb]
			{0x000639B3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3611816]
			{0x00065353, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360fe76]
			{0x00066CC7, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360e502]
			{0x00066D50, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360e479]
			{0x0006DC2A, 3, 7, false, 0x00C8}, // lea rsi, [r13 + 0x3675298]
			{0x0006DC47, 6, 10, false, 0x01C0}, // movss dword ptr [rdi + r13 + 0x3675390], xmm6
			{0x0006DC6E, 3, 7, false, 0x00CC}, // lea r12, [r13 + 0x367529c]
			{0x0006DC8E, 6, 10, false, 0x01C4}, // movss dword ptr [rdi + r13 + 0x3675394], xmm6
			{0x0006DC98, 4, 9, false, 0x018D}, // cmp byte ptr [rdi + r13 + 0x367535d], 0
			{0x0006DCA3, 6, 10, false, 0x0194}, // movss xmm6, dword ptr [rdi + r13 + 0x3675364]
			{0x0006DCE4, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DCEE, 6, 10, false, 0x01C8}, // movss dword ptr [rdi + r13 + 0x3675398], xmm6
			{0x0006DD24, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DD2E, 6, 10, false, 0x01CC}, // movss dword ptr [rdi + r13 + 0x367539c], xmm6
			{0x0006DD64, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DD6E, 6, 10, false, 0x01D0}, // movss dword ptr [rdi + r13 + 0x36753a0], xmm6
			{0x0006DDA7, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DDB1, 6, 10, false, 0x01D4}, // movss dword ptr [rdi + r13 + 0x36753a4], xmm6
			{0x0006DDE7, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DDF1, 6, 10, false, 0x01D8}, // movss dword ptr [rdi + r13 + 0x36753a8], xmm6
			{0x0006DE64, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DE6E, 6, 10, false, 0x01DC}, // movss dword ptr [rdi + r13 + 0x36753ac], xmm6
			{0x0006F5EB, 5, 9, false, 0x01E0}, // movss dword ptr [rax + rdx + 0x36753b0], xmm6
			{0x00070E6D, 5, 9, false, 0x01E4}, // movss dword ptr [rax + rcx + 0x36753b4], xmm6
			{0x00071B48, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3603681]
			{0x00073644, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3601b85]
			{0x000753E0, 3, 7, true , 0x0000}, // lea rax, [rip + 0x35ffde9]
			{0x02C6A7A5, 3, 7, true , 0x01B4}, // lea rax, [rip + 0x9915d8]
		};
		constexpr perclient_array aaglob_array = {
			"aaGlobArray", 0x035F61D0, 0x4E30, aaglob_v2_sites, std::size(aaglob_v2_sites), 0, {}};

		// CG_InitZBarriers (PC 0x004616C0, PS4 0x1697B0): PS4 clears four rows and
		// four counts. Widen the memset 0x18800 -> 0x31000, and turn the 11-byte
		// qword store to numcgZBarriers into xorps/movups/nop, a 16-byte store
		// (its slots 2..3 are padding; xmm0 is volatile). Otherwise player 3's
		// count never resets and passes 128 on the next map.
		bool widen_zbarrier_clear(const size_t zb_new)
		{
			const auto b = base();
			auto* imm = reinterpret_cast<uint8_t*>(b + 0x004616CD);
			auto* clr = reinterpret_cast<uint8_t*>(b + 0x004616D8);
			constexpr uint8_t imm_old[] = {0x41, 0xB8, 0x00, 0x88, 0x01, 0x00};
			constexpr uint8_t clr_old[] = {0x48, 0xC7, 0x05, 0x1D, 0x2A, 0x30, 0x04, 0x00, 0x00, 0x00, 0x00};
			// the memset's rcx must already point at the moved rows
			const auto* lea = reinterpret_cast<const uint8_t*>(b + 0x004616C4);
			int32_t lea_disp = 0;
			std::memcpy(&lea_disp, lea + 3, sizeof(lea_disp));
			if (!zb_new || b + 0x004616CB + lea_disp != zb_new
			    || !readable(imm, sizeof(imm_old)) || std::memcmp(imm, imm_old, sizeof(imm_old)) != 0
			    || !readable(clr, sizeof(clr_old)) || std::memcmp(clr, clr_old, sizeof(clr_old)) != 0)
			{
				note("[splitscreen] zbarrier clear: bytes differ - not widened\n");
				return false;
			}
			const auto disp = static_cast<int32_t>(0x04764100 - (0x004616DB + 7));
			uint8_t clr_new[11] = {0x0F, 0x57, 0xC0, 0x0F, 0x11, 0x05, 0, 0, 0, 0, 0x90};
			std::memcpy(clr_new + 6, &disp, sizeof(disp));
			constexpr uint8_t imm_new[] = {0x41, 0xB8, 0x00, 0x10, 0x03, 0x00};
			if (!write_bytes(clr, clr_new, sizeof(clr_new)))
			{
				return false;
			}
			if (!write_bytes(imm, imm_new, sizeof(imm_new)))
			{
				write_bytes(clr, clr_old, sizeof(clr_old));
				return false;
			}
			return true;
		}

		void relocate_batch1b()
		{
			for (size_t i = 0; i < std::size(batch1b); ++i)
			{
				if (!batch1b_new[i])
				{
					batch1b_new[i] = relocate_perclient(batch1b[i]);
				}
				if (batch1b_new[i] && std::strcmp(batch1b[i].name, "zbarriers") == 0)
				{
					widen_zbarrier_clear(batch1b_new[i]);
				}
			}
		}

		// DWARF-map batch 2.
		// totalCoverageArea_s - totalCoverageArea_t[4][18] (PS4 0x04E73820, row 0x360),
		// CG_TotalCoverage_Frame(lc). Slot 2 overlaps foreign globals.
		constexpr entcoll_site totalcoverage_sites[] = {
			{0x0125F881, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3ae2bb8]
			{0x01262A2B, 3, 7, true , 0x0008}, // lea rax, [rip + 0x3adfa16]
			{0x01262B66, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x3adf8d3]
			{0x01262B71, 3, 7, true , 0x0018}, // lea rax, [rip + 0x3adf8e0]
			{0x012633A3, 3, 7, true , 0x0018}, // lea rcx, [rip + ..]  (missed by both scans)
			{0x01264CA6, 3, 7, true , 0x0004}, // lea r10, [rip + 0x3add797]
		};
		// gaGlobs - GpadAxesGlob[4] x 0x48 (PS4 0x05A4F090): per-client gamepad axis
		// bindings, read by CL_GamepadAxisValue(lc, axis). Slot 2 is foreign.
		// CL_InitGamepadAxisBindings loops up to an end marker, &gaGlobs[2] + 0x1C;
		// it moves to &new[4] + 0x1C, else the loop stops after client 0.
		constexpr entcoll_site gaglobs_sites[] = {
			{0x0133F00B, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405c6de]
			{0x0133F085, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405c664]
			{0x0133F5AA, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405c13f]
			{0x0133FB06, 3, 7, false, 0x0000}, // lea r14, [r13 + 0x539b6d0]
			{0x0133FD4E, 3, 7, false, 0x0000}, // lea r9, [r10 + 0x539b6d0]
			{0x01340140, 3, 7, true , 0x001C}, // lea r8, [rip + 0x405b5c5]
			{0x0134067B, 3, 7, false, 0x0000}, // lea rdi, [r15 + 0x539b6d0]
		};
		// s_rightStickModels (word[5] per controller, stride 0xA) overlaps
		// s_gamepadButtons[0] with its slot 3, so it moves. s_gamepadButtons (PC
		// stride 0x2E, PS4 0x2A) moves too: slots 2..3 have no code refs but hold
		// static list nodes reached through links (a widen without the move crashed).
		constexpr entcoll_site gamepadbuttons_sites[] = {
			{0x013402B6, 4, 8, false, 0x0002}, // lea rdi, [r12 + 0x539b782]           init
			{0x013402C7, 5, 9, false, 0x0000}, // mov word [r14 + r12 + 0x539b780], r13w
			{0x01340323, 5, 9, false, 0x002C}, // mov word [r14 + r12 + 0x539b7ac], ax  KeyPressBits
			{0x0134036D, 3, 7, true , 0x0000}, // lea rcx, [rip -> 0x0539B780]        CL_ModelForButton
			{0x01340383, 3, 7, true , 0x002C}, // lea rcx, [rip -> 0x0539B7AC]        KeyPressBits getter
			{0x013403D2, 3, 7, true , 0x0002}, // lea rax, [rip -> 0x0539B782]        per-controller reset
		};
		constexpr entcoll_site rightstick_sites[] = {
			{0x01340235, 5, 9, false, 0x0000}, // mov word ptr [r12 + rbx*2 + 0x539b760], ax
			{0x01340243, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
			{0x01340253, 5, 9, false, 0x0002}, // mov word ptr [r12 + rbx*2 + 0x539b762], ax
			{0x01340261, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
			{0x01340271, 5, 9, false, 0x0004}, // mov word ptr [r12 + rbx*2 + 0x539b764], ax
			{0x0134027F, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
			{0x0134028F, 5, 9, false, 0x0006}, // mov word ptr [r12 + rbx*2 + 0x539b766], ax
			{0x013402A8, 5, 9, false, 0x0008}, // mov word ptr [r12 + rbx*2 + 0x539b768], ax
			{0x013403FB, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x405b37e]
			{0x013404F8, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x405b281]
		};
		size_t gaglobs_new = 0;

		bool relocate_gaglobs()
		{
			if (gaglobs_new)
			{
				return true;
			}
			constexpr uint32_t ga_base = 0x0531C6D0;
			constexpr uint32_t ga_stride = 0x48;
			constexpr uint32_t end_rva = 0x0134014A; // lea r10, [rip + d32], 7 bytes
			const auto b = base();
			auto* end_disp = reinterpret_cast<uint8_t*>(b + end_rva + 3);
			int32_t end_old = 0;
			if (!readable(end_disp, sizeof(end_old)))
			{
				return false;
			}
			std::memcpy(&end_old, end_disp, sizeof(end_old));
			const auto* end_insn = reinterpret_cast<const uint8_t*>(b + end_rva);
			if (end_insn[0] != 0x4C || end_insn[1] != 0x8D || end_insn[2] != 0x15
			    || static_cast<int64_t>(end_rva) + 7 + end_old != static_cast<int64_t>(ga_base) + 2 * ga_stride + 0x1C)
			{
				note("[splitscreen] gaGlobs: end marker bytes differ - nothing moved\n");
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * ga_stride));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, 4 * ga_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + ga_base), 2 * ga_stride);
			std::vector<int32_t> saved(std::size(gaglobs_sites));
			if (!rewrite_entcoll(gaglobs_sites, std::size(gaglobs_sites), ga_base,
			                     reinterpret_cast<size_t>(fresh), saved.data()))
			{
				return false;
			}
			const auto end_new = static_cast<int64_t>(reinterpret_cast<size_t>(fresh) + 4 * ga_stride + 0x1C)
			                     - static_cast<int64_t>(b + end_rva + 7);
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

		constexpr perclient_array batch2[] = {
			{"totalcoverage", 0x04CC3420, 0x360, totalcoverage_sites, std::size(totalcoverage_sites), 0, {}},
			{"rightstick", 0x0531C760, 0xA, rightstick_sites, std::size(rightstick_sites), 0, {}},
			{"gamepadbuttons", 0x0531C780, 0x2E, gamepadbuttons_sites, std::size(gamepadbuttons_sites), 0, {}},
		};
		size_t batch2_new[std::size(batch2)] = {};

		// cgExploderTriggers (1000 x 0x30 per client, row 0xBB80) and cgExploderTriggerCount
		// (int per client): [4] on PS4 (0x03E9C550), [2] on PC. Trigger row 2 is foreign data
		// and count slot 3 is written by a static initializer. CG_ExplodersInit clears the
		// counts with one 7-byte qword store and memsets the triggers (0x17700), so both move
		// into one block, the counts right behind the four rows, and the memset length becomes
		// 0x2EE10: one clear covers everything, as on PS4.
		// Sites: exploder_triggers_reloc.validated.txt, not the generator's output.
		constexpr entcoll_site exploder_trig_sites[] = {
			{0x001FD4B8, 3, 7, true , 0x0010}, // lea rax,[rip+..]  +0x10   CG_ExploderUpdate walk, imul lc,0xBB80
			{0x001FD776, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          CG_ExplodersInit memset (length widened below)
			{0x002008EA, 3, 7, true , 0x0018}, // lea rdi,[rip+..]  +0x18   CG_FindTrigger(lc, ...)
			{0x00205679, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
			{0x002070EA, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
			{0x00208B38, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
		};
		constexpr entcoll_site exploder_count_sites[] = {
			{0x001FD466, 3, 7, false, 0x0000}, // lea r12, [r10 + 0x43806c8]
			{0x001FD78F, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x4182f32], rax
			{0x0020090A, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x417fdb7]
			{0x00205612, 3, 7, false, 0x0000}, // lea rax, [rax + 0x43806c8]
			{0x0020706C, 4, 8, false, 0x0000}, // lea rcx, [rax*4 + 0x43806c8]
			{0x00208AAC, 4, 8, false, 0x0000}, // lea rcx, [rax*4 + 0x43806c8]
		};
		constexpr uint32_t exploder_trig_base = 0x046946E0;
		constexpr uint32_t exploder_trig_stride = 0xBB80;
		constexpr uint32_t exploder_count_base = 0x043016C8;
		size_t exploder_trig_new = 0;

		bool relocate_exploder_triggers()
		{
			if (exploder_trig_new)
			{
				return true;
			}
			const auto b = base();
			auto* len = reinterpret_cast<uint8_t*>(b + 0x001FD77F);
			constexpr uint8_t len_old[] = {0x41, 0xB8, 0x00, 0x77, 0x01, 0x00}; // mov r8d, 0x17700
			constexpr uint8_t len_new[] = {0x41, 0xB8, 0x10, 0xEE, 0x02, 0x00}; // mov r8d, 0x2EE10
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
			note("[splitscreen] cgExploderTriggers + counts [2]->[4] at RVA 0x%08X\n",
			     static_cast<uint32_t>(exploder_trig_new - b));
			return true;
		}

		void relocate_batch2()
		{
			for (size_t i = 0; i < std::size(batch2); ++i)
			{
				if (!batch2_new[i])
				{
					batch2_new[i] = relocate_perclient(batch2[i]);
				}
			}
			relocate_exploder_triggers();
			relocate_gaglobs();
		}

		// ---- Batch 3: screen effects and compass ----
		// s_screenBlur (x 0x1C), s_screenElectrified and s_screenBurn (x 0xC) are [2] arrays
		// packed back to back (PS4 0x04001E50 / EC0 / EF0), so each slot 2 is the next base.
		// CG_CompassUpdateActors(lc) runs every frame, so player 3 wrote 0x2C00 bytes past
		// s_compassActors across the other compass tables. CG_ClearCompassPingData clears each
		// table with a two-row length (batch3_clear_len); it becomes four rows only for a table
		// that actually moved.
		constexpr entcoll_site screenblur_sites[] = {
			{0x0060136E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x421c09b]
			{0x0060C333, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42110d6]
			{0x00641853, 3, 7, true , 0x0018}, // lea rcx, [rip + 0x41dbbce]
			{0x006467B2, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41d6c57]
			{0x00652BA0, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41ca869]
		};
		constexpr entcoll_site screenelec_sites[] = {
			{0x00602C3B, 4, 8, false, 0x0000}, // lea rdx, [rcx*4 + 0x481d448]
			{0x0060C363, 3, 7, true , 0x0000}, // lea r8, [rip + 0x42110de]
			{0x0061136D, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d44c], r12d
			{0x0061137F, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d44c], eax
			{0x00611394, 4, 8, false, 0x0008}, // mov dword ptr [r13 + rdi*4 + 0x481d450], eax
			{0x0061139C, 4, 8, false, 0x0000}, // mov qword ptr [r13 + rdi*4 + 0x481d448], r12
		};
		constexpr entcoll_site screenburn_sites[] = {
			{0x0060C393, 3, 7, true , 0x0000}, // lea r8, [rip + 0x42110c6]
			{0x006113A9, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d464], r12d
			{0x006113BB, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d464], eax
			{0x006113D0, 4, 8, false, 0x0008}, // mov dword ptr [r13 + rdi*4 + 0x481d468], eax
			{0x006113D8, 4, 8, false, 0x0000}, // mov qword ptr [r13 + rdi*4 + 0x481d460], r12
			{0x0063FECB, 4, 8, false, 0x0000}, // lea rdx, [rcx*4 + 0x481d460]
		};
		constexpr entcoll_site compass_actors_sites[] = {
			{0x00598884, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426bcf5]   CG_ClearCompassPingData
			{0x005A1EC3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42626b6]
			{0x005A1FA3, 3, 7, true , 0x0000}, // lea rcx,[array] / imul rax,rax,0x2C00
			{0x005A3B0D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4260a6c]
			{0x005A5343, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425f236]
			{0x005A6CFF, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425d87a]
			{0x005B3452, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4251127]
			{0x005B4DB4, 3, 7, true , 0x0000}, // lea r11, [rip + 0x424f7c5]
			{0x005B7FDB, 3, 7, true , 0x0000}, // lea r11, [rip + 0x424c59e]
			{0x005CC464, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4238115]
			{0x005D2B8D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42319ec]
			{0x005D2BBE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42319bb]
			{0x005D445E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x423011b]
		};
		constexpr entcoll_site compass_vehicles_sites[] = {
			{0x005988AC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42735cd]
			{0x005A20A3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4269dd6]
			{0x005AB961, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4260518]
			{0x005D95D5, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42328a4]
		};
		constexpr entcoll_site compass_artillery_sites[] = {
			{0x00593D7F, 3, 7, true , 0x0010}, // lea rax, [rip + 0x427950a]
			{0x00593DD2, 4, 8, false, 0x0010}, // mov dword ptr [rsi + rbx*4 + 0x480d290], r9d
			{0x00593DDE, 3, 7, false, 0x0000}, // mov dword ptr [rsi + rbx*4 + 0x480d280], eax
			{0x00593DE8, 3, 7, false, 0x0004}, // mov dword ptr [rsi + rbx*4 + 0x480d284], eax
			{0x00593DF1, 3, 7, false, 0x0008}, // mov dword ptr [rsi + rbx*4 + 0x480d288], eax
			{0x00593DFB, 3, 7, false, 0x000C}, // mov dword ptr [rsi + rbx*4 + 0x480d28c], eax
			{0x00593E18, 5, 9, false, 0x0000}, // addss xmm0, dword ptr [rsi + rbx*4 + 0x480d280]
			{0x00593E21, 5, 9, false, 0x0000}, // movss dword ptr [rsi + rbx*4 + 0x480d280], xmm0
			{0x00593E36, 5, 9, false, 0x0004}, // addss xmm0, dword ptr [rsi + rbx*4 + 0x480d284]
			{0x00593E3F, 5, 9, false, 0x0004}, // movss dword ptr [rsi + rbx*4 + 0x480d284], xmm0
			{0x005988E8, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274991]
			{0x005A1FC3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b2b6]
		};
		constexpr entcoll_site compass_heli_sites[] = {
			{0x005988FC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274a6d]
			{0x005A2043, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b326]
			{0x005D9285, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42340e4]
		};
		constexpr entcoll_site compass_0240_sites[] = {
			{0x00598910, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274c19]
			{0x005A2023, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b506]
			{0x005D9145, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42343e4]
		};
		constexpr entcoll_site compass_0120_sites[] = {
			{0x00598924, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4275085]
			{0x005A2063, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b946]
			{0x005D8DC8, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4234be1]
		};
		constexpr entcoll_site compass_0500_sites[] = {
			{0x00598938, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42762b1]
			{0x005A1FE3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426cc06]
			{0x005CC5B0, 3, 7, true , 0x0004}, // lea rax, [rip + 0x424263d]
		};
		constexpr entcoll_site compass_0400_sites[] = {
			{0x0059894C, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4276c9d]
			{0x005A2083, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426d566]
			{0x005D94A5, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x4236144]
		};
		constexpr perclient_array batch3[] = {
			{"screenblur", 0x0479E410, 0x1C, screenblur_sites, std::size(screenblur_sites), 0, {}},
			{"screenelec", 0x0479E448, 0xC, screenelec_sites, std::size(screenelec_sites), 0, {}},
			{"screenburn", 0x0479E460, 0xC, screenburn_sites, std::size(screenburn_sites), 0, {}},
			{"compass_actors", 0x04785580, 0x2C00, compass_actors_sites, std::size(compass_actors_sites), 0, {}},
			{"compass_vehicles", 0x0478CE80, 0x900, compass_vehicles_sites, std::size(compass_vehicles_sites), 0, {}},
			{"compass_artillery", 0x0478E280, 0x78, compass_artillery_sites, std::size(compass_artillery_sites), 0, {}},
			{"compass_heli", 0x0478E370, 0xE0, compass_heli_sites, std::size(compass_heli_sites), 0, {}},
			{"compass_0240", 0x0478E530, 0x240, compass_0240_sites, std::size(compass_0240_sites), 0, {}},
			{"compass_0120", 0x0478E9B0, 0x120, compass_0120_sites, std::size(compass_0120_sites), 0, {}},
			{"compass_0500", 0x0478FBF0, 0x500, compass_0500_sites, std::size(compass_0500_sites), 0, {}},
			{"compass_0400", 0x047905F0, 0x400, compass_0400_sites, std::size(compass_0400_sites), 0, {}},
		};
		constexpr uint32_t batch3_clear_len[] = {
			0x00000000, // screenblur (no clear site)
			0x00000000, // screenelec (no clear site)
			0x00000000, // screenburn (no clear site)
			0x0059888D, // compass_actors
			0x005988B5, // compass_vehicles
			0x005988F1, // compass_artillery
			0x00598905, // compass_heli
			0x00598919, // compass_0240
			0x0059892D, // compass_0120
			0x00598941, // compass_0500
			0x00598955, // compass_0400
		};
		size_t batch3_new[std::size(batch3)] = {};

		void relocate_batch3()
		{
			const auto b = base();
			for (size_t i = 0; i < std::size(batch3); ++i)
			{
				if (batch3_new[i])
				{
					continue;
				}
				const auto& a = batch3[i];
				auto* len = batch3_clear_len[i] ? reinterpret_cast<uint8_t*>(b + batch3_clear_len[i]) : nullptr;
				const uint32_t len_old = 2 * a.stride;
				const uint32_t len_new = 4 * a.stride;
				if (len)
				{
					uint32_t cur = 0;
					if (!readable(len, 6) || len[0] != 0x41 || len[1] != 0xB8)
					{
						continue;
					}
					std::memcpy(&cur, len + 2, sizeof(cur));
					if (cur != len_old)
					{
						note("[splitscreen] %s: clear length differs - not moved\n", a.name);
						continue;
					}
				}
				batch3_new[i] = relocate_perclient(a);
				if (batch3_new[i] && len)
				{
					write_bytes(len + 2, &len_new, sizeof(len_new));
				}
			}
		}

		// ---- Batch 4: CG_AllocateClientMemory's pointer tables and the destructibles ----
		// CG_AllocateClientMemory (PS4 0x21FD70) allocates the cg_weaponsArray, cg_destructibles
		// and cg_ikBuf buffers of every local client, client 2 included, but the pointer tables
		// are [2], so slot 2 was stored over foreign globals (weapons[2] replaced a weapon-info
		// pointer used by everyone). Moving the tables is the whole fix. cg_numDestructibles ->
		// cg_updateTime and s_destructible_gamestates ([2][32] x 0x84, row 0x1080) ->
		// s_num_destructible_gamestates are packed the same way: each slot 2 is the next global.
		constexpr entcoll_site cg_weaponsarray_sites[] = {
			{0x0044D1AA, 4, 8, false, 0x0000}, // add r13, qword ptr [rax + rcx*8 + 0x49d9410]
			{0x00843ACF, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x49d9410], rax
			{0x00853DE9, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x49d9410]
			{0x00856EE2, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x4182527], rax
			{0x00856EE9, 3, 7, true , 0x0008}, // mov qword ptr [rip + 0x4182528], rax
			{0x008F258B, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rcx + r14*8 + 0x49d9410]
			{0x0119A273, 4, 8, false, 0x0000}, // add r15, qword ptr [rdx + r14*8 + 0x49d9410]
			{0x011CA1FD, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x380f22c]
			{0x0122C649, 4, 8, false, 0x0000}, // add rdi, qword ptr [rbp + r14*8 + 0x49d9410]
			{0x0126671A, 4, 8, false, 0x0000}, // add rdi, qword ptr [r10 + rsi*8 + 0x49d9410]
			{0x026D13F4, 4, 8, false, 0x0000}, // add r13, qword ptr [rcx + rax*8 + 0x49d9410]
			{0x0271F6BE, 4, 8, false, 0x0000}, // add rbx, qword ptr [r14 + rax*8 + 0x49d9410]
		};
		constexpr entcoll_site cg_ikbuf_sites[] = {
			{0x00843B03, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x4a315c0], rax
			{0x00853DC0, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x4a315c0]
		};
		// ikStates is not in this batch: see relocate_ikstates and relocate_batch4.
		constexpr entcoll_site cg_destructibles_sites[] = {
			{0x00843AF1, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x17f00ff0], rax
			{0x00853DD9, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x17f00ff0]
			{0x00856F0C, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x176aa0dd], rax
			{0x00856F13, 3, 7, true , 0x0008}, // mov qword ptr [rip + 0x176aa0de], rax
			{0x022F28C0, 4, 8, false, 0x0000}, // mov rax, qword ptr [r13 + rdi*8 + 0x17f00ff0]
			{0x022F5C96, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b921e3]
			{0x022F5CB3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b921c6]
			{0x022F5CF3, 4, 8, false, 0x0000}, // mov r10, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5D7D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5D8D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5D9D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5DAA, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5DB7, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5DDB, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5EF1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rsi*8 + 0x17f00ff0]
			{0x022F5F10, 4, 8, false, 0x0000}, // mov rdi, qword ptr [r14 + rsi*8 + 0x17f00ff0]
			{0x022F6067, 4, 8, false, 0x0000}, // add r8, qword ptr [rdi + r12*8 + 0x17f00ff0]
			{0x022F60B4, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r12*8 + 0x17f00ff0]
			{0x022F60C8, 4, 8, false, 0x0000}, // mov r8, qword ptr [rdx + r12*8 + 0x17f00ff0]
			{0x022F60EA, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r12*8 + 0x17f00ff0]
			{0x022F6110, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + r12*8 + 0x17f00ff0]
			{0x022F6154, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdi + r12*8 + 0x17f00ff0]
			{0x022F6161, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r12*8 + 0x17f00ff0]
			{0x022FAB99, 4, 8, false, 0x0000}, // add rdx, qword ptr [r12 + r15*8 + 0x17f00ff0]
			{0x022FAC3A, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b8d23f]
			{0x022FC8DF, 4, 8, false, 0x0000}, // lea rsi, [rdi*8 + 0x17f00ff0]
			{0x022FD47C, 4, 8, false, 0x0000}, // add rdi, qword ptr [rsi + r15*8 + 0x17f00ff0]
			{0x0230064F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b8782a]
		};
		constexpr entcoll_site numdestructibles_sites[] = {
			// Three leas that point at this base are deliberately not listed: they are end markers
			// of loops over s_destructibles (0x80 x 0x108), which ends exactly where this array
			// begins. Moving them made the destructible walk run off the end (tools/sentinel_check.py).
			{0x022F5A58, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15bd1511]
			{0x022F5D62, 4, 8, false, 0x0000}, // inc dword ptr [rdi + r11*4 + 0x17f400e0]
			{0x022F5F55, 4, 12, false, 0x0000}, // mov dword ptr [r14 + rsi*4 + 0x17f400e0], 0
			{0x022F601F, 4, 8, false, 0x0000}, // mov dword ptr [rdi + r14*4 + 0x17f400e0], eax
			{0x022F91CB, 4, 8, false, 0x0000}, // cmp r13d, dword ptr [rdi + r12*4 + 0x17f400e0]
			{0x022FAB84, 4, 8, false, 0x0000}, // cmp dword ptr [r12 + r15*4 + 0x17f400e0], ebx
			{0x022FABC2, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r12 + r15*4 + 0x17f400e0]
			{0x022FC94D, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + r15 + 0x17f400e0], ebx
			{0x022FC98F, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r14 + r15 + 0x17f400e0]
		};
		constexpr entcoll_site cg_updatetime_sites[] = {
			{0x022F5F61, 4, 12, false, 0x0000}, // mov dword ptr [r14 + rsi*4 + 0x17f400e8], 0
			{0x022FC920, 4, 8, false, 0x0000}, // mov eax, dword ptr [r14 + r15 + 0x17f400e8]
			{0x022FC92A, 4, 8, false, 0x0000}, // mov dword ptr [r14 + r15 + 0x17f400e8], eax
			{0x022FC945, 4, 8, false, 0x0000}, // mov dword ptr [r14 + r15 + 0x17f400e8], eax
		};
		constexpr entcoll_site destr_gamestates_sites[] = {
			{0x0230200C, 3, 7, true , 0x0002}, // lea rax, [rip + 0x15b85e7f]
			{0x02302043, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b85e46]
			{0x02302B07, 3, 7, true , 0x0000}, // lea r15, [rip + 0x15b85382]
			{0x02302BB1, 3, 7, true , 0x0002}, // lea rax, [rip + 0x15b852da]
			{0x02302BE3, 3, 7, false, 0x0000}, // lea rdx, [r11 + 0x17f01000]
			{0x02302BEA, 3, 7, false, 0x0000}, // lea r9, [r11 + 0x17f01000]
		};
		constexpr entcoll_site destr_numgamestates_sites[] = {
			{0x02301FF3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b87f96]
			{0x02302ACD, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b874bc]
			{0x02302BA1, 4, 8, false, 0x0000}, // mov r9d, dword ptr [r11 + r10*4 + 0x17f03100]
			{0x02302BF1, 4, 8, false, 0x0000}, // mov dword ptr [r11 + r10*4 + 0x17f03100], eax
		};
		constexpr perclient_array batch4[] = {
			{"cg_weaponsarray", 0x0495A410, 0x8, cg_weaponsarray_sites, std::size(cg_weaponsarray_sites), 0, {}},
			{"cg_ikbuf", 0x049B25C0, 0x8, cg_ikbuf_sites, std::size(cg_ikbuf_sites), 0, {}},
			{"cg_destructibles", 0x17E820C0, 0x8, cg_destructibles_sites, std::size(cg_destructibles_sites), 0, {}},
			{"numdestructibles", 0x17EC11B0, 0x4, numdestructibles_sites, std::size(numdestructibles_sites), 0, {}},
			{"cg_updatetime", 0x17EC11B8, 0x4, cg_updatetime_sites, std::size(cg_updatetime_sites), 0, {}},
			{"destr_gamestates", 0x17E820D0, 0x1080, destr_gamestates_sites, std::size(destr_gamestates_sites), 0, {}},
			{"destr_numgamestates", 0x17E841D0, 0x4, destr_numgamestates_sites, std::size(destr_numgamestates_sites), 0, {}},
		};
		size_t batch4_new[std::size(batch4)] = {};

		bool ik_reset_widened = false;

		// ikStates: PS4 `IKState* ikStates[5]` (0x120DB5D0), the server's state plus one per
		// local client. The PC table has three slots ([0] server, [1 + lc]), walked by the IK
		// reset loop up to its end marker. Client 2's slot is that unreferenced end address,
		// but client 3's is a foreign byte flag, so the table moves to [5] and the reset loop's
		// end marker (lea r14) moves with it.
		constexpr entcoll_site ikstates_sites[] = {
			{0x023F7B43, 3, 7, true , 0x0008}, // lea rdx, [ikStates+8]   IK_AllocateLocalClientMemory
			{0x023F7D0D, 3, 7, true , 0x0000}, // lea rdx, [ikStates]
			{0x023F7D3F, 3, 7, true , 0x0000}, // lea rdx, [ikStates]
			{0x023F7E15, 3, 7, true , 0x0008}, // lea rax, [ikStates+8]
			{0x023F8200, 3, 7, true , 0x0000}, // lea rsi, [ikStates]
			{0x023F84AC, 3, 7, true , 0x0000}, // lea rsi, [ikStates]      reset loop start
			{0x023F8594, 3, 7, true , 0x0000}, // lea rsi, [ikStates]
			{0x023F9260, 3, 7, true , 0x0008}, // lea rcx, [ikStates+8]
			{0x0245A539, 4, 9, false, 0x0008}, // cmp qword [rbx+rcx*8+ikStates+8], 0
		};
		constexpr uint32_t ikstates_base = 0x17F297C0;
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
			if (!retarget_end_marker(0x023F84B3, 3, 7, ikstates_base + ikstates_old_slots * 8,
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

		void relocate_batch4()
		{
			for (size_t i = 0; i < std::size(batch4); ++i)
			{
				if (!batch4_new[i])
				{
					batch4_new[i] = relocate_perclient(batch4[i]);
				}
			}
			// Move ikStates to [5]. If a reference fails to verify, fall back to moving the reset
			// loop's end marker one slot, which covers client 2 (three players); with two players
			// that slot is NULL and the loop skips it.
			if (!ik_reset_widened)
			{
				ik_reset_widened = relocate_ikstates()
					|| retarget_end_marker(0x023F84B3, 3, 7, 0x17F297D8, base() + 0x17F297E0);
			}
		}

		// ---- Batch 5: two unnamed per-client cgame arrays (no PS4 [4] global has either shape) ----
		// cg_clientents30: 30 entries of 0x11E0 per client (stride 0x21840). Row 2 holds foreign
		// pointer globals, so player 3's entity interpolation read a NULL pointer and crashed.
		// The static initializer only zeroes a field, so zero-fill is the initial state.
		constexpr entcoll_site cg_clientents30_sites[] = {
			{0x0019891D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2bdc]
			{0x001989B0, 3, 7, true , 0x0080}, // lea rcx, [rip + 0x40c2bc9]
			{0x001989EB, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2b0e]
			{0x00198A89, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2a70]
			{0x0019A3BC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c113d]
			{0x0019A462, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c1097]
			{0x0019A63D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0ebc]
			{0x0019A754, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0da5]
			{0x0019A8A4, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0c55]
			{0x0019A9D6, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0b23]
			{0x0019B831, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x40bfcc8]
			{0x02CA73F8, 3, 7, true , 0x00F0}, // lea rax, [rip + 0x153abf1]
		};
		// cg_perclient_3c0: 8 entries of 0x78 per client, right before s_screenBlur; row 2 ran
		// over the screen-effect arrays and foreign globals.
		constexpr entcoll_site cg_perclient_3c0_sites[] = {
			{0x0060F6B5, 3, 7, true , 0x0000}, // lea rax, [rip + 0x420d5c4]
			{0x0060F844, 3, 7, true , 0x0000}, // lea rax, [rip + 0x420d435]
			{0x00641878, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41db401]
			{0x00643596, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41d96e3]
			{0x0065785F, 3, 7, false, 0x0000}, // lea rcx, [rcx + 0x481cc80]
			{0x00662085, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x41babf4]
		};
		constexpr perclient_array batch5[] = {
			{"cg_clientents30", 0x041DC500, 0x21840, cg_clientents30_sites, std::size(cg_clientents30_sites), 0, {}},
			{"cg_perclient_3c0", 0x0479DC80, 0x3C0, cg_perclient_3c0_sites, std::size(cg_perclient_3c0_sites), 0, {}},
		};
		size_t batch5_new[std::size(batch5)] = {};

		void relocate_batch5()
		{
			for (size_t i = 0; i < std::size(batch5); ++i)
			{
				if (!batch5_new[i])
				{
					batch5_new[i] = relocate_perclient(batch5[i]);
				}
			}
		}

		// ---- Batch 6: cgame threaded-notify queues ----
		// PS4 CG_ThreadedNotifyList_* (Init 0x2956D0): per local client 100 items plus
		// s_processQueueHead/Tail and s_firstFree, all [4]. On PC (items 0x50, stride 0x1F40) all
		// four are [2] and packed back to back, so CG_Init(2) at map load linked 100 items over
		// player 1's queue pointers and ~8 KB of live globals: a wild writer consistent with the
		// Arxan faults, the lost default_aitype and "Data is corrupt" in 3-player rounds.
		// Runs with CG_FRAME on and off, so it is not gated on BO3_CG_FRAME.
		constexpr entcoll_site tnotify_list_sites[] = {
			{0x00A18777, 3, 7, true , 0x0044}, // lea rcx, [rip + 0x430c0e6]
			{0x00A2179C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x430307d]
			{0x00A217DC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x430303d]
			{0x00A2181C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302ffd]
			{0x00A21865, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302fb4]
			{0x00A218BC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302f5d]
			{0x00A21A0B, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302e0e]
			{0x00A21B09, 3, 7, true , 0x0000}, // lea r11, [rip + 0x4302d10]
			{0x00A21B10, 3, 7, true , 0x0048}, // lea rdx, [rip + 0x4302d51]
			{0x00A21B69, 3, 7, true , 0x0044}, // lea rsi, [rip + 0x4302cf4]
			{0x02D2C855, 3, 7, true , 0x0040}, // lea rax, [rip + 0x1f7ea04]
		};
		constexpr entcoll_site tnotify_head_sites[] = {
			{0x00A219F1, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286c0], rbx
			{0x00A21B22, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286c0], r10
			{0x00A21CC4, 4, 8, false, 0x0000}, // mov r15, qword ptr [r9 + rbx*8 + 0x4d286c0]
			{0x00A21CDB, 4, 12, false, 0x0000}, // mov qword ptr [r9 + rbx*8 + 0x4d286c0], 0
			{0x00A21FA2, 4, 8, false, 0x0000}, // mov rax, qword ptr [r12 + rdi*8 + 0x4d286c0]
			{0x00A21FBB, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286c0], rbx
		};
		constexpr entcoll_site tnotify_tail_sites[] = {
			{0x00A219DE, 4, 8, false, 0x0000}, // mov rax, qword ptr [r12 + rdi*8 + 0x4d286d0]
			{0x00A219F9, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286d0], rbx
			{0x00A21B31, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286d0], r10
			{0x00A21CE7, 4, 12, false, 0x0000}, // mov qword ptr [r9 + rbx*8 + 0x4d286d0], 0
			{0x00A21FB2, 4, 9, false, 0x0000}, // cmp qword ptr [r12 + rdi*8 + 0x4d286d0], 0
			{0x00A21FC5, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286d0], r13
		};
		constexpr entcoll_site tnotify_free_sites[] = {
			{0x00A21915, 4, 9, false, 0x0000}, // cmp qword ptr [r12 + rdi*8 + 0x4d286e0], 0
			{0x00A219A9, 4, 8, false, 0x0000}, // mov rbx, qword ptr [r12 + rdi*8 + 0x4d286e0]
			{0x00A219D2, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286e0], rax
			{0x00A21B5E, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286e0], rax
			{0x00A21EEE, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + rax*8 + 0x4d286e0]
			{0x00A21F02, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286e0], rsi
		};
		constexpr perclient_array batch6[] = {
			{"tnotify_list", 0x04CA5820, 0x1F40, tnotify_list_sites, std::size(tnotify_list_sites), 0, {}},
			{"tnotify_head", 0x04CA96C0, 0x8, tnotify_head_sites, std::size(tnotify_head_sites), 0, {}},
			{"tnotify_tail", 0x04CA96D0, 0x8, tnotify_tail_sites, std::size(tnotify_tail_sites), 0, {}},
			{"tnotify_free", 0x04CA96E0, 0x8, tnotify_free_sites, std::size(tnotify_free_sites), 0, {}},
		};
		size_t batch6_new[std::size(batch6)] = {};

		// The engine's static initializer sets every item of both clients to zero except
		// +0x04 = 0x3FF. Its lea is in the site table, so it fills slots 0/1 of the new block;
		// relocate_batch6 gives slots 2/3 the same values. If its bytes differ, nothing moves.
		constexpr uint8_t tnotify_static_init[] = {
			0xB9, 0xC7, 0x00, 0x00, 0x00,                         // mov ecx, 0xC7
			0x48, 0x8D, 0x05,                                     // lea rax, [rip+..]
		};
		constexpr uint8_t tnotify_static_init_body[] = {
			0x89, 0x50, 0xC0,                                     // mov [rax-0x40], edx
			0xC7, 0x40, 0xC4, 0xFF, 0x03, 0x00, 0x00,             // mov dword [rax-0x3C], 0x3FF
		};

		void relocate_batch6()
		{
			const auto b = base();
			const auto bytes_at = [b](const uint32_t rva, const uint8_t* expect, const size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, expect, n) == 0;
			};
			if (!batch6_new[0])
			{
				if (!bytes_at(0x02D2C850, tnotify_static_init, sizeof(tnotify_static_init))
					|| !bytes_at(0x02D2C862, tnotify_static_init_body, sizeof(tnotify_static_init_body)))
				{
					note("[splitscreen] tnotify: static initializer differs - queues not moved\n");
					return;
				}
				batch6_new[0] = relocate_perclient(batch6[0]);
				if (!batch6_new[0])
				{
					return;   // the pointer arrays only make sense with the list moved
				}
				auto* items = reinterpret_cast<uint8_t*>(batch6_new[0]);
				for (size_t lc = 2; lc < 4; ++lc)
				{
					for (size_t i = 0; i < 100; ++i)
					{
						*reinterpret_cast<uint32_t*>(items + lc * 0x1F40 + i * 0x50 + 0x04) = 0x3FF;
					}
				}
			}
			for (size_t i = 1; i < std::size(batch6); ++i)
			{
				if (!batch6_new[i])
				{
					batch6_new[i] = relocate_perclient(batch6[i]);
				}
			}
		}

		// ---- Batch 7: renderer [2] x 0x240 array that slid the image by 8 bytes ----
		// An element holds 16 {int id, int age} entries, a qword count at +0x80, an id bitmask
		// at +0xC0 and a flag at +0x238. A routine drops stale entries by shifting the list down
		// 8 bytes up to &e[count-1]. With index 2 the element was foreign and the count garbage,
		// so ~190 MB of the image through .idata slid down by 8: Arxan faults, "Cannot find AI
		// Type" and "Data is corrupt". The initializer writes only zeros. History: LOG.md, "190 MB".
		constexpr entcoll_site fxgpu_client_sites[] = {
			{0x01CBD588, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd841ea1]
			{0x02E89DB4, 3, 7, true , 0x00C0}, // lea rbx, [rip + 0xc608505]
		};
		constexpr perclient_array batch7[] = {
			{"view_idsets_240", 0x0F48C880, 0x240, fxgpu_client_sites, std::size(fxgpu_client_sites), 0, {}},
		};
		size_t batch7_new[std::size(batch7)] = {};

		void relocate_batch7()
		{
			for (size_t i = 0; i < std::size(batch7); ++i)
			{
				if (!batch7_new[i])
				{
					batch7_new[i] = relocate_perclient(batch7[i]);
				}
			}
		}

		// ---- Batch 8: renderer scene buffers ----
		// scene_pc480: 4 entries of 0x120 per client, sized for two. Client 2's slot lies over
		// the dpvs globals (PS4 GfxSceneDpvs: entVisData[4] etc.), so renderer workers crashed
		// using a float as entVisData[lc].
		// scene_c: the per-client pointer table R_InitSceneBuffers fills (PS4 dpvsGlob); slot 2
		// is the base of another array. The allocator's store is among its sites, so C[0]/C[1]
		// land in the new block too.
		constexpr entcoll_site scene_pc480_sites[] = {
			{0x01C84F2F, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9201136]
			{0x01C85077, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200fee]
			{0x01C850AE, 3, 7, true , 0x001C}, // lea r8, [rip + 0x9200fb7]
			{0x01C854D8, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200b8d]
			{0x01C855E7, 3, 7, true , 0x0138}, // lea rdi, [rip + 0x9200b9a]
			{0x01C856A2, 3, 7, true , 0x001C}, // lea rax, [rip + 0x92009c3]
			{0x01C858A7, 3, 7, true , 0x001C}, // lea rax, [rip + 0x92007be]
			{0x01C8593E, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200727]
			{0x01C87AF0, 4, 8, false, 0x001C}, // mov ecx, dword ptr [r8 + rbx + 0xae9243c]
			{0x01C87B9A, 4, 8, false, 0x001C}, // mov ecx, dword ptr [r8 + rbx + 0xae9243c]
			{0x01C87C2E, 3, 7, true , 0x0020}, // lea rax, [rip + 0x91fe43b]
			{0x01C87C6A, 3, 7, true , 0x0034}, // lea rax, [rip + 0x91fe413]
			{0x01C87CAA, 3, 7, true , 0x0048}, // lea rax, [rip + 0x91fe3e7]
			{0x01C87CEA, 3, 7, true , 0x005C}, // lea rax, [rip + 0x91fe3bb]
			{0x01C87D80, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fe2e5]
			{0x01C8A9D8, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fb68d]
			{0x01C8AF48, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fb11d]
			{0x01C8B094, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fafd1]
			{0x01CE1DA1, 3, 7, true , 0x037C}, // lea rax, [rip + 0x91a4624]
			{0x01D0DB6F, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91784f6]
		};
		constexpr entcoll_site scene_c_sites[] = {
			{0x01C85A0C, 4, 8, false, 0x0000}, // mov rax, qword ptr [r9 + r10 + 0x10615a60]
			{0x01C85A4E, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + rcx*8 + 0x10615a60]
			{0x01C8745D, 4, 8, false, 0x0000}, // mov qword ptr [rbx + rsi + 0x10615a60], rax
			{0x01C8752F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + rdi + 0x10615a60]
		};
		constexpr perclient_array batch8[] = {
			{"scene_pc480", 0x0AE134A0, 0x480, scene_pc480_sites, std::size(scene_pc480_sites), 0, {}},
			{"scene_c", 0x10596AE0, 0x8, scene_c_sites, std::size(scene_c_sites), 0, {}},
		};
		size_t batch8_new[std::size(batch8)] = {};

		void relocate_batch8()
		{
			for (size_t i = 0; i < std::size(batch8); ++i)
			{
				if (!batch8_new[i])
				{
					batch8_new[i] = relocate_perclient(batch8[i]);
				}
			}
			scene_c_new = batch8_new[1];
		}

		// ---- Batch 9: renderer per-client array (x 0xA24) ----
		// Client 2's slot covers the frame-limiter target and the globals around it (the
		// limiter's actual writer is batch 10). The static initializer only writes zero
		// fields, so zero-filled slots 2/3 are the constructed state.
		constexpr entcoll_site rview_a24_sites[] = {
			{0x01C9C189, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd83b9ec]
			{0x01C9C3C6, 3, 7, true , 0x0A20}, // lea rdi, [rip + 0xd83c1cf]
			{0x01C9C3CD, 3, 7, true , 0x0000}, // lea rbp, [rip + 0xd83b7a8]
			{0x01C9C440, 4, 8, true , 0x01C0}, // movss xmm0, dword ptr [rip + 0xd83b8f4]
			{0x01C9C457, 4, 8, true , 0x01C4}, // divss xmm0, dword ptr [rip + 0xd83b8e1]
			{0x01C9C464, 4, 8, true , 0x01C8}, // movss xmm0, dword ptr [rip + 0xd83b8d8]
			{0x01C9C471, 4, 8, true , 0x01CC}, // divss xmm1, dword ptr [rip + 0xd83b8cf]
			{0x01C9C47E, 4, 8, true , 0x01B0}, // movss xmm0, dword ptr [rip + 0xd83b8a6]
			{0x01C9C48B, 4, 8, true , 0x01B4}, // movss xmm1, dword ptr [rip + 0xd83b89d]
			{0x01C9C498, 4, 8, true , 0x01B8}, // movss xmm0, dword ptr [rip + 0xd83b894]
			{0x01C9C4A5, 4, 8, true , 0x01BC}, // movss xmm1, dword ptr [rip + 0xd83b88b]
			{0x01C9C4B2, 4, 8, true , 0x01D8}, // movss xmm0, dword ptr [rip + 0xd83b89a]
			{0x01C9C4BF, 4, 8, true , 0x01DC}, // movss xmm1, dword ptr [rip + 0xd83b891]
			{0x01C9C4CC, 4, 8, true , 0x01E0}, // movss xmm0, dword ptr [rip + 0xd83b888]
			{0x01C9C4D9, 4, 8, true , 0x01E8}, // movss xmm1, dword ptr [rip + 0xd83b883]
			{0x01C9C4E6, 4, 8, true , 0x01EC}, // movss xmm0, dword ptr [rip + 0xd83b87a]
			{0x01C9C4F3, 4, 8, true , 0x01F0}, // movss xmm1, dword ptr [rip + 0xd83b871]
			{0x01C9C500, 4, 8, true , 0x01F4}, // movss xmm0, dword ptr [rip + 0xd83b868]
			{0x01C9C50D, 4, 8, true , 0x01F8}, // movss xmm1, dword ptr [rip + 0xd83b85f]
			{0x01C9C51A, 4, 8, true , 0x01E4}, // movss xmm0, dword ptr [rip + 0xd83b83e]
			{0x01C9C85F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd83b316]
			{0x01C9CA22, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd83b153]
			{0x01C9CB0F, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd83b066]
			{0x02E89C1A, 3, 7, true , 0x0000}, // lea rbx, [rip + 0xc5e0d2b]
		};
		constexpr perclient_array batch9[] = {
			{"rview_a24", 0x0F464FCC, 0xA24, rview_a24_sites, std::size(rview_a24_sites), 0, {}},
		};
		size_t batch9_new[std::size(batch9)] = {};

		void relocate_batch9()
		{
			for (size_t i = 0; i < std::size(batch9); ++i)
			{
				if (!batch9_new[i])
				{
					batch9_new[i] = relocate_perclient(batch9[i]);
				}
			}
		}

		// ---- Batch 10: frame-limiter stall (renderer per-view array, x 0x30) ----
		// With three views the main thread sat in the frame limiter (`while (Sys_Milliseconds()
		// < target) Sys_Sleep(1)`) because the target held a float: no frames, no LUI tick,
		// frozen players. The writer is this array, indexed by the view's local client (`lea
		// r,[i+i*2]; shl r,4`, so perclient_sweep missed it); element 2 covers a pointer global
		// and the limiter target. On PS4 it is a member of a larger renderer struct. No static
		// initializer references it. History: LOG.md, "frame limiter".
		constexpr entcoll_site rview_org30_sites[] = {
			{0x01C73DFD, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd8651dc]
			{0x01C747DB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd8647fe]
			{0x01C74EDC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd8640fd]
			{0x01C75012, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd863fc7]
			{0x01C7AB76, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85e463]
			{0x01C7AFB0, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85e029]
			{0x01C7C8CC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85c70d]
			{0x01C7D057, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85bf82]
			{0x01C7E2E4, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85acf5]
			{0x01C7E4E5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85aaf4]
			{0x01C7E8BA, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85a71f]
			{0x01CB2BC5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd826414]
			{0x01CDD3EC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd7fbbed]
		};
		constexpr perclient_array batch10[] = {
			{"rview_org30", 0x0F466430, 0x30, rview_org30_sites, std::size(rview_org30_sites), 0, {}},
		};
		size_t batch10_new[std::size(batch10)] = {};

		void relocate_batch10()
		{
			for (size_t i = 0; i < std::size(batch10); ++i)
			{
				if (!batch10_new[i])
				{
					batch10_new[i] = relocate_perclient(batch10[i]);
				}
			}
		}

		// ---- Batch 11: aim-target actor lists ----
		// PS4 aim_target_actors [4]: 64 centity pointers per client (base + lc*0x200), built by
		// the aim_target submitter and read by a worker job. [2] on PC; slot 2 was
		// AimTarget_Cmd's old home (moved in batch 1b), so player 3's pass read stale bytes as
		// entity pointers and crashed. Zero-filled slots mean "no actors".
		constexpr entcoll_site aimactors_sites[] = {
			{0x0007A998, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36049e1]
			{0x000897A8, 4, 8, false, 0x0000}, // mov qword ptr [rax + rcx*8 + 0x367f380], r8
		};
		constexpr perclient_array batch11[] = {
			{"aimactors", 0x03600380, 0x200, aimactors_sites, std::size(aimactors_sites), 0, {}},
		};
		size_t batch11_new[std::size(batch11)] = {};

		void relocate_batch11()
		{
			for (size_t i = 0; i < std::size(batch11); ++i)
			{
				if (!batch11_new[i])
				{
					batch11_new[i] = relocate_perclient(batch11[i]);
				}
			}
		}
