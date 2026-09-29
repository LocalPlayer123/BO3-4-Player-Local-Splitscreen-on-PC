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
		}

		constexpr uint32_t entcoll_world_stride = 0x401C;
		constexpr uint32_t entcoll_node_bytes = 0xC400;
		constexpr size_t entcoll_slots = 4;

		bool entcoll_relocated = false;

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
