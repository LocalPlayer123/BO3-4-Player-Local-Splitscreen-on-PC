// Renderer scene buffers and entity collision.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ============ R_InitSceneBuffers: per-client renderer scene buffers ====
		// The allocator fills A[lc], B[lc] (N bytes) and C[lc] (N*8) for lc < 2
		// (PS4 R_InitSceneBuffers 0x0093B060 loops to 4). Do not raise its bound:
		// it also bounds a release loop over another array, and A[2] is B[0].
		// Instead B is relocated to free A[2]/A[3], and fill_scene_buffers()
		// allocates the skipped slots at the engine's sizes.

		// Move B (one store, disp32 at +4, [rbx + rsi + disp] with rsi = module
		// base, so the new array must be within disp32 of the module base).
		bool relocate_scene_buffer_b()
		{
			if (scene_b_relocated)
			{
				return true;
			}
			const auto b = base();
			auto* insn = reinterpret_cast<uint8_t*>(b + scene_b_store_rva);
			if (!readable(insn, sizeof(scene_b_store_expect))
				|| std::memcmp(insn, scene_b_store_expect,
				               sizeof(scene_b_store_expect)) != 0)
			{
				note("[splitscreen] scene B store mismatch, not relocating\n");
				return false;
			}

			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(scene_slots * sizeof(void*)));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, scene_slots * sizeof(void*));

			const auto delta = reinterpret_cast<size_t>(fresh) - b;
			if (delta > 0x7FFFFFFF)
			{
				note("[splitscreen] scene B cave out of disp32 range\n");
				return false;
			}
			const auto new_rva = static_cast<uint32_t>(delta);

			// carry the two live pointers across before anything reads them
			std::memcpy(fresh, reinterpret_cast<const void*>(b + scene_b_rva),
			            2 * sizeof(void*));

			if (!write_bytes(insn + 4, &new_rva, sizeof(new_rva)))
			{
				return false;
			}
			scene_b_new_rva = new_rva;
			scene_b_relocated = true;
			note("[splitscreen] scene buffer B [2]->[4] at RVA 0x%08X\n", new_rva);
			return true;
		}

		// Allocate the slots the count-2 loop skips. Runs on the renderer
		// pipeline and does nothing once the slots are filled.
		void fill_scene_buffers()
		{
			if (scene_buffers_filled || !scene_b_relocated
				|| !raise_local_client_count)
			{
				return;
			}
			const auto b = base();
			uint32_t elems = 0;
			std::memcpy(&elems, reinterpret_cast<const void*>(b + scene_size_rva),
			            sizeof(elems));
			if (elems == 0 || elems > 0x100000)
			{
				return; // config not copied yet
			}

			auto** a = reinterpret_cast<void**>(b + scene_a_rva);
			// Only the relocated C has real slots 2/3 (old C[2] is a foreign array).
			if (!scene_c_new)
			{
				return;
			}
			auto** c = reinterpret_cast<void**>(scene_c_new);
			auto** bb = reinterpret_cast<void**>(b + scene_b_new_rva);
			if (!readable(a, scene_slots * sizeof(void*))
				|| !readable(c, scene_slots * sizeof(void*)))
			{
				return;
			}
			if (a[0] == nullptr || c[0] == nullptr)
			{
				return; // R_InitSceneBuffers has not run yet
			}
			if (a[2] != nullptr && c[2] != nullptr)
			{
				scene_buffers_filled = true;
				return;
			}

			const auto grab = [](const size_t bytes) -> void*
			{
				// zero-filled by the OS, which is the state the engine's own
				// allocator hands back here
				return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
				                    PAGE_READWRITE);
			};

			for (size_t lc = 2; lc < scene_slots; ++lc)
			{
				if (a[lc] == nullptr)
				{
					auto* p = grab(elems);
					if (!p) { return; }
					a[lc] = p;
				}
				if (bb[lc] == nullptr)
				{
					auto* p = grab(elems);
					if (!p) { return; }
					bb[lc] = p;
				}
				if (c[lc] == nullptr)
				{
					auto* p = grab(static_cast<size_t>(elems) * 8);
					if (!p) { return; }
					c[lc] = p;
				}
			}
			scene_buffers_filled = true;
			note("[splitscreen] scene buffers allocated for clients 2/3"
			     " (%u elements)\n", elems);
		}

		// ============ cgEntCollWorld / cgEntCollNodes: entity collision ========
		// Both are [2] with foreign slots 2/3, cleared for lc 2 by the inlined
		// CG_ClearEntityCollWorld, so both are relocated. That function (PS4
		// 0x189890, called from CG_SetInitialSnapshot) builds the free-list, so
		// the engine initializes slots 2/3 itself.
		// The site tables come from tools/gen_entcoll_sites.py; never edit them by
		// hand (a hand-built table missed 8 field accessors and hung the game).
		// target_off: a site may point at a field of element 0.
		// History: LOG.md, ae57c92
		struct entcoll_site
		{
			uint32_t rva;         // instruction start
			uint8_t disp_off;     // byte offset of the disp32 inside it
			uint8_t insn_len;     // total instruction length
			bool rip;             // true: disp is rip-relative; false: absolute RVA
			uint32_t target_off;  // target's offset inside element 0
		};

		constexpr uint32_t entcoll_world_base = 0x04764BA0;
		constexpr uint32_t entcoll_world_stride = 0x401C;
		constexpr uint32_t entcoll_nodes_base = 0x032608B0;
		constexpr uint32_t entcoll_node_bytes = 0xC400;
		constexpr size_t entcoll_slots = 4;

		constexpr entcoll_site entcoll_world_sites[] = {
			{0x0058B173, 3, 7, true,  0x0028}, // lea rcx,[rip+..]
			{0x0058B1F6, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0058B2DE, 3, 7, false, 0x0000}, // lea rdi,[rsi+0x047E3BA0]
			{0x0058B445, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0058B60C, 3, 7, false, 0x0000}, // lea rdi,[rsi+0x047E3BA0]
			{0x0058B710, 3, 7, true,  0x0028}, // lea rax,[rip+..]
			{0x0058B886, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0129DE34, 3, 7, true,  0x001C}, // lea rcx,[rip+..]
			{0x0129DF28, 3, 7, true,  0x001C}, // lea rcx,[rip+..]
			{0x0129E784, 3, 7, true,  0x001C}, // lea r10,[rip+..]
			{0x0129EA61, 3, 7, true,  0x001C}, // lea r10,[rip+..]
			{0x012AEC27, 3, 7, true,  0x001C}, // lea rdi,[rip+..]
			{0x012AEE47, 3, 7, true,  0x001C}, // lea rdi,[rip+..]
		};

		constexpr entcoll_site entcoll_node_sites[] = {
			{0x0058B17A, 3, 7, true,  0x0000}, // lea r10,[rip+..]
			{0x0058B2FD, 4, 8, false, 0x0000}, // mov rcx,[rsi+rbx*8+0x032DF8B0]
			{0x0058B44F, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0058B660, 4, 8, false, 0x0000}, // mov rsi,[rsi+rdx*8+..]
			{0x0058B721, 4, 8, false, 0x0000}, // mov r9,[rsi+r9*8+..]
			{0x0058B853, 4, 8, false, 0x0000}, // lea rdx,[r12+..]
			{0x0070DE25, 4, 8, false, 0x0000}, // mov rax,[rcx+rax*8+..]
			{0x0070FB49, 4, 8, false, 0x0000}, // mov rax,[rdx+rax*8+..]
			{0x0072E424, 4, 8, false, 0x0000}, // mov rax,[r8+r14*8+..]
			{0x0072E607, 4, 8, false, 0x0000}, // mov rax,[rcx+r14*8+..]
			{0x00731AA3, 4, 8, false, 0x0000}, // mov rax,[r12+r14*8+..]
			{0x00FEEED2, 3, 7, true,  0x0000}, // lea rsi,[rip+..]
			{0x0129DE45, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129DECC, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129DF21, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129E78B, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129E820, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129EA5A, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x012AEC63, 3, 7, true,  0x0000}, // lea rax,[rip+..]
		};

		bool entcoll_relocated = false;
		size_t entcoll_world_new = 0;
		size_t entcoll_nodes_new = 0;

		// Point a site table at new_abs, all or nothing. Every site must first
		// resolve to old_rva + target_off, or nothing is written; the original
		// disps go to `saved` for rollback.
		bool rewrite_entcoll(const entcoll_site* sites, const size_t count,
		                     const uint32_t old_rva, const size_t new_abs, int32_t* saved)
		{
			const auto b = base();

			for (size_t i = 0; i < count; ++i)
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + sites[i].rva);
				if (!readable(insn, sites[i].insn_len))
				{
					note("[splitscreen] entcoll site 0x%08X unreadable\n", sites[i].rva);
					return false;
				}
				std::memcpy(&saved[i], insn + sites[i].disp_off, sizeof(int32_t));
				const auto want = static_cast<int64_t>(b) + old_rva + sites[i].target_off;
				const auto have = sites[i].rip
					? static_cast<int64_t>(b + sites[i].rva + sites[i].insn_len) + saved[i]
					: static_cast<int64_t>(b) + saved[i];
				if (have != want)
				{
					note("[splitscreen] site 0x%08X does not reference 0x%08X+0x%X - "
					     "nothing written\n", sites[i].rva, old_rva, sites[i].target_off);
					return false;
				}
			}

			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t j = 0; j < done; ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + sites[j].rva);
					write_bytes(insn + sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
			};

			for (size_t i = 0; i < count; ++i)
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + sites[i].rva);
				const auto target = new_abs + sites[i].target_off;
				int32_t disp = 0;

				if (sites[i].rip)
				{
					const auto end = static_cast<int64_t>(b + sites[i].rva
					                                      + sites[i].insn_len);
					const auto delta = static_cast<int64_t>(target) - end;
					if (delta > INT32_MAX || delta < INT32_MIN)
					{
						note("[splitscreen] entcoll 0x%08X out of rip range\n",
						     sites[i].rva);
						rollback();
						return false;
					}
					disp = static_cast<int32_t>(delta);
				}
				else
				{
					const auto rva = static_cast<int64_t>(target)
					                 - static_cast<int64_t>(b);
					if (rva < 0 || rva > INT32_MAX)
					{
						note("[splitscreen] entcoll 0x%08X out of abs range\n",
						     sites[i].rva);
						rollback();
						return false;
					}
					disp = static_cast<int32_t>(rva);
				}

				if (!write_bytes(insn + sites[i].disp_off, &disp, sizeof(disp)))
				{
					note("[splitscreen] entcoll 0x%08X write failed\n", sites[i].rva);
					rollback();
					return false;
				}
				++done;
			}
			return true;
		}
