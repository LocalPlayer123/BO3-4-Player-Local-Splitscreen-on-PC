// Sun shadow slices per view, pane counts and bounds.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Sun shadow: bound each view's slot ----------------------------------
		// RT 5 (sun shadow depth) and RT 9 (its colour twin) have 3 slices per view
		// slot. The slot is min(splitscreen player count - 1, localClientNum); with
		// the count at 3, player 3 got a slot with no slices and crashed in d3d11
		// OMSetRenderTargets. The view setup's count is capped at the slots that have
		// slices (slices / partitions, read from the verified instructions).
		bool sun_slot_clamped = false;
		uint32_t sun_max_slot = 1;

		// ---- Sun shadow: 12 slices, one slot per view -----------------------------
		// With 2 slots, players 2-4 shared slot 1 and overwrote each other's
		// shadows. PS4 renders views one after another into the same slices; the
		// PC does not, so RT 5 (depth) and RT 9 (colour) grow from 6 to 12 slices.
		// - Descriptors: R_InitRenderTargetsPlatform receives 12 for both (the stores
		//   of 6 stay; that `mov r8d,6` is also RT 6's id).
		// - Depth arrays are sized from the slice count and need nothing else.
		// - Colour views live in 8 inline slots (index clamped to 7). The creation
		//   check admits 2..12, the loop stops at 6 so the inline slots stay stock,
		//   and slices 6..11 get sidecar views (maintain_sun_trans_views) that the
		//   setter's view lookup and the clear pick; the clear has no clamp of its own.
		// Applied before R_Init, which builds the descriptors. History: LOG.md, 12 slices
		constexpr uint32_t rt_record_stride = 0xAE0;
		constexpr uint32_t rt_config_stride = 0x2C;   // GfxRenderTargetConfiguration (PS4 r_rendertarget.cpp:74)
		constexpr uint32_t rt_config_slices = 0x14;
		constexpr uint32_t sun_depth_rt = 5;
		constexpr uint32_t sun_trans_rt = 9;
		constexpr uint32_t sun_slices_stock = 6;
		constexpr uint32_t sun_slices_wanted = 12;
		bool sun_slices_grown = false;
		ID3D11RenderTargetView* sun_trans_extra[sun_slices_wanted - sun_slices_stock] = {};    // RT 9 slices 6..11
		ID3D11RenderTargetView* sun_trans_retired[sun_slices_wanted - sun_slices_stock] = {};  // released one tick later
		void* sun_trans_seen_v0 = nullptr;

		// R_InitRenderTargetsPlatform(configs, count, allocator, flag), the one reader of
		// R_InitRenderTargets' descriptor table: RT 5 and RT 9 are created with 12
		// slices, every other target as the table says.
		void init_rt_platform_stub(uint8_t* configs, const int count, void* allocator, const bool flag)
		{
			for (int i = 0; i < count; ++i)
			{
				auto* c = configs + static_cast<size_t>(i) * rt_config_stride;
				const auto id = *reinterpret_cast<const uint16_t*>(c);
				auto* slices = reinterpret_cast<uint32_t*>(c + rt_config_slices);
				if ((id == sun_depth_rt || id == sun_trans_rt) && *slices == sun_slices_stock)
				{
					*slices = sun_slices_wanted;
				}
			}
			using init_rt_platform_t = void (*)(uint8_t*, int, void*, bool);
			reinterpret_cast<init_rt_platform_t>(base() + init_rt_platform_rva)(configs, count, allocator, flag);
		}

		// The setter's view-set lookup. The setter reads only the colour slot
		// min(slice, 7) and the depth-array pointer at +0x40 of the set, then overwrites
		// rax; so for RT 9 slices 6..11 it gets a per-thread copy of the set whose slot
		// holds the sidecar view. Without a sidecar it gets the stock set.
		uint64_t* setter_view_set_stub(void* state, const uint16_t rt, const uint32_t slice)
		{
			using view_set_t = uint64_t* (*)(void*, uint16_t);
			auto* set = reinterpret_cast<view_set_t>(base() + rt_view_set_rva)(state, rt);
			if (rt != sun_trans_rt || slice < sun_slices_stock || slice >= sun_slices_wanted)
			{
				return set;
			}
			auto* extra = sun_trans_extra[slice - sun_slices_stock];
			if (!extra)
			{
				return set;
			}
			thread_local uint64_t shadow[9];   // 8 inline colour views + the depth-array pointer
			std::memcpy(shadow, set, sizeof(shadow));
			shadow[slice < 7 ? slice : 7] = reinterpret_cast<uint64_t>(extra);
			return shadow;
		}

		bool grow_sun_shadow_slices()
		{
			if (sun_slices_grown)
			{
				return true;
			}
			const auto b = base();
			const auto matches = [&](uint32_t rva, const uint8_t* stock, size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, stock, n) == 0;
			};
			if (!matches(sun_desc_rt5_rva, sun_desc_rt5_stock, sizeof(sun_desc_rt5_stock))
				|| !matches(sun_desc_rt9_rva, sun_desc_rt9_stock, sizeof(sun_desc_rt9_stock))
				|| !matches(init_rt_platform_call_rva, init_rt_platform_call_stock, sizeof(init_rt_platform_call_stock))
				|| !matches(sun_view_check_rva, sun_view_check_stock, sizeof(sun_view_check_stock))
				|| !matches(sun_view_loop_rva, sun_view_loop_stock, sizeof(sun_view_loop_stock))
				|| !matches(sun_setter_premise_rva, sun_setter_premise, sizeof(sun_setter_premise))
				|| !matches(sun_clear_rva, sun_clear_stock, sizeof(sun_clear_stock)))
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - bytes differ\n");
				return false;
			}
			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x80));
			if (!cave)
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - allocation failed\n");
				return false;
			}
			// Two picks have no call to hook, so they stay machine code:
			// +0x00 the colour-view loop cap (the bound is reloaded from the record after
			// a 4-byte indirect call): movzx eax,[rsi+0xA86] ; cmp eax,6 ; jbe +5 ; mov eax,6 ; ret
			const uint8_t c_loop[] = {0x0F, 0xB7, 0x86, 0x86, 0x0A, 0x00, 0x00, 0x83, 0xF8, 0x06, 0x76, 0x05,
			                          0xB8, 0x06, 0x00, 0x00, 0x00, 0xC3};
			// +0x20 the clear's colour pick (rbp = view set, also walked as the depth
			// array, rbx = cmd state): sidecar for RT 9 slices 6..11, else inline[slice],
			// clamped.
			std::vector<uint8_t> k;
			k.insert(k.end(), {0x8B, 0x93, 0xC8, 0x80, 0x00, 0x00});       // mov edx,[rbx+0x80C8]
			k.insert(k.end(), {0x83, 0xFA, 0x06, 0x7C, 0x00});             // cmp edx,6 ; jl orig
			const size_t k1 = k.size() - 1;
			k.insert(k.end(), {0x83, 0xFA, 0x0C, 0x73, 0x00});             // cmp edx,12 ; jae clamp
			const size_t k2 = k.size() - 1;
			k.insert(k.end(), {0x66, 0x83, 0xBB, 0xC0, 0x80, 0x00, 0x00, static_cast<uint8_t>(sun_trans_rt)});
			k.insert(k.end(), {0x75, 0x00});                                 // cmp word [rbx+0x80C0],9 ; jne clamp
			const size_t k3 = k.size() - 1;
			k.insert(k.end(), {0x48, 0xB8});                                 // mov rax, &sun_trans_extra
			{
				const auto a = reinterpret_cast<uint64_t>(&sun_trans_extra[0]);
				const auto* p = reinterpret_cast<const uint8_t*>(&a);
				k.insert(k.end(), p, p + 8);
			}
			k.insert(k.end(), {0x48, 0x8B, 0x54, 0xD0, 0xD0});             // mov rdx,[rax+rdx*8-0x30]
			k.insert(k.end(), {0x48, 0x85, 0xD2, 0x75, 0x00});             // test rdx,rdx ; jnz done
			const size_t k4 = k.size() - 1;
			const size_t kclamp = k.size();
			k.insert(k.end(), {0xBA, 0x05, 0x00, 0x00, 0x00});             // mov edx,5 (last inline view)
			const size_t korig = k.size();
			k.insert(k.end(), {0x48, 0x8B, 0x54, 0xD5, 0x00});             // mov rdx,[rbp+rdx*8]
			const size_t kdone = k.size();
			k.insert(k.end(), {0x48, 0x8B, 0x07, 0x4C, 0x8B, 0xC6, 0x48, 0x8B, 0xCF});   // rax, r8, rcx as stock
			k.insert(k.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});       // jmp [rip+0]
			{
				const uint64_t back = b + sun_clear_rva + sizeof(sun_clear_stock);
				const auto* p = reinterpret_cast<const uint8_t*>(&back);
				k.insert(k.end(), p, p + 8);
			}
			k[k1] = static_cast<uint8_t>(korig - (k1 + 1));
			k[k2] = static_cast<uint8_t>(kclamp - (k2 + 1));
			k[k3] = static_cast<uint8_t>(kclamp - (k3 + 1));
			k[k4] = static_cast<uint8_t>(kdone - (k4 + 1));
			if (0x20 + k.size() > 0x80
				|| !write_bytes(cave + 0x00, c_loop, sizeof(c_loop))
				|| !write_bytes(cave + 0x20, k.data(), k.size()))
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - cave write failed\n");
				return false;
			}

			// The site bytes, all prepared before the first write.
			const auto* setter_call_stock = sun_setter_premise + (sun_setter_call_rva - sun_setter_premise_rva);
			uint8_t p_setter[5];
			uint8_t p_rt[sizeof(init_rt_platform_call_stock)];
			if (!call_site_bytes(sun_setter_call_rva, sizeof(p_setter),
			                     reinterpret_cast<const void*>(&setter_view_set_stub), p_setter)
				|| !call_site_bytes(init_rt_platform_call_rva, sizeof(p_rt),
				                    reinterpret_cast<const void*>(&init_rt_platform_stub), p_rt))
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - relay allocation failed\n");
				return false;
			}
			const auto jump_or_call = [&](uint8_t op, uint32_t rva, size_t len, size_t cave_off, uint8_t* out)
			{
				std::memset(out, 0x90, len);
				out[0] = op;
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + cave_off) - (b + rva + 5));
				std::memcpy(out + 1, &rel, sizeof(rel));
			};
			uint8_t p_clear[sizeof(sun_clear_stock)];
			jump_or_call(0xE9, sun_clear_rva, sizeof(p_clear), 0x20, p_clear);
			uint8_t p_loop[sizeof(sun_view_loop_stock)];
			jump_or_call(0xE8, sun_view_loop_rva, sizeof(p_loop), 0x00, p_loop);
			const uint8_t p_check[] = {0x66, 0x83, 0xF8, static_cast<uint8_t>(sun_slices_wanted - 2)};

			// All-or-nothing. Setter first: without sidecar views it acts as stock.
			struct w { uint32_t rva; const uint8_t* stock; const uint8_t* patch; size_t n; };
			const w writes[] = {
				{sun_setter_call_rva, setter_call_stock, p_setter, sizeof(p_setter)},
				{sun_clear_rva, sun_clear_stock, p_clear, sizeof(p_clear)},
				{sun_view_loop_rva, sun_view_loop_stock, p_loop, sizeof(p_loop)},
				{sun_view_check_rva, sun_view_check_stock, p_check, sizeof(p_check)},
				{init_rt_platform_call_rva, init_rt_platform_call_stock, p_rt, sizeof(p_rt)},
			};
			size_t done_w = 0;
			for (const auto& x : writes)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + x.rva), x.patch, x.n))
				{
					for (size_t i = 0; i < done_w; ++i)
					{
						write_bytes(reinterpret_cast<void*>(b + writes[i].rva), writes[i].stock, writes[i].n);
					}
					note("[splitscreen] sun shadow 12 slices: NOT applied - a site write failed (rolled back)\n");
					return false;
				}
				++done_w;
			}
			sun_slices_grown = true;
			return true;
		}

		// Renderer loop: remakes RT 9's views for slices 6..11 whenever the target is
		// recreated. Old views are released one tick later, never while in use.
		void maintain_sun_trans_views()
		{
			for (auto*& r : sun_trans_retired)
			{
				if (r)
				{
					r->Release();
					r = nullptr;
				}
			}
			if (!sun_slices_grown)
			{
				return;
			}
			const auto b = base();
			const auto recs = *reinterpret_cast<const uint64_t*>(b + rt_records_ptr_rva);
			if (!recs)
			{
				return;
			}
			const auto* rec = reinterpret_cast<const uint8_t*>(recs + sun_trans_rt * rt_record_stride);
			if (!readable(rec, rt_record_stride))
			{
				return;
			}
			const uint16_t slices = *reinterpret_cast<const uint16_t*>(rec + 0xA86);
			auto* v0 = *reinterpret_cast<ID3D11RenderTargetView* const*>(rec + 8);
			if (v0 == sun_trans_seen_v0)
			{
				return;
			}
			for (size_t i = 0; i < std::size(sun_trans_extra); ++i)
			{
				sun_trans_retired[i] = sun_trans_extra[i];
				sun_trans_extra[i] = nullptr;
			}
			sun_trans_seen_v0 = v0;
			if (!v0 || slices <= 6)
			{
				return;
			}
			D3D11_RENDER_TARGET_VIEW_DESC d{};
			v0->GetDesc(&d);
			if (d.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
			{
				return;
			}
			ID3D11Resource* res = nullptr;
			ID3D11Device* dev = nullptr;
			v0->GetResource(&res);
			v0->GetDevice(&dev);
			if (res && dev)
			{
				for (uint32_t sl = 6; sl < slices && sl < 6 + std::size(sun_trans_extra); ++sl)
				{
					d.Texture2DArray.FirstArraySlice = sl;
					d.Texture2DArray.ArraySize = 1;
					ID3D11RenderTargetView* v = nullptr;
					if (SUCCEEDED(dev->CreateRenderTargetView(res, &d, &v)) && v)
					{
						sun_trans_extra[sl - 6] = v;
					}
				}
			}
			if (res)
			{
				res->Release();
			}
			if (dev)
			{
				dev->Release();
			}
		}

		// CL_SplitscreenPlayerCount as the sun-shadow view setup sees it, capped at the
		// slots that have slices: its slot min(max(n, 1) - 1, lc) becomes
		// min(max(n, 1) - 1, sun_max_slot, lc). It answers what the game's own call
		// reaches - the detour, else the stock dvar read; never the original, whose dvar
		// getter has an Arxan caller check.
		int splitscreen_player_count_stub();   // 14_player4_fixes.inl
		int stock_splitscreen_player_count();  // 14_player4_fixes.inl

		int sun_slot_player_count()
		{
			const int n = player_count_detoured ? splitscreen_player_count_stub() : stock_splitscreen_player_count();
			return std::min(n, static_cast<int>(sun_max_slot) + 1);
		}

		void clamp_sun_shadow_slot()
		{
			if (sun_slot_clamped)
			{
				return;
			}
			const auto b = base();
			const auto* slices_at = reinterpret_cast<const uint8_t*>(b + sun_slices_rva);
			const auto* parts_at = reinterpret_cast<const uint8_t*>(b + sun_partitions_rva);
			const auto* premise = reinterpret_cast<const uint8_t*>(b + sun_slot_count_call_rva);
			if (!readable(slices_at, 6) || slices_at[0] != 0x41 || slices_at[1] != 0xB8
				|| !readable(parts_at, 3) || parts_at[0] != 0x83 || parts_at[1] != 0xFB
				|| !readable(premise, sizeof(sun_slot_premise))
				|| std::memcmp(premise, sun_slot_premise, sizeof(sun_slot_premise)) != 0)
			{
				note("[splitscreen] sun shadow slot: bytes differ - not clamped\n");
				return;
			}
			uint32_t slices = 0;
			std::memcpy(&slices, slices_at + 2, sizeof(slices));
			// grow_sun_shadow_slices() leaves that immediate at 6 and makes it 12 itself.
			if (sun_slices_grown)
			{
				slices = sun_slices_wanted;
			}
			const uint32_t partitions = parts_at[2];
			if (partitions == 0 || slices < partitions || slices % partitions != 0
				|| slices / partitions > 0x80)
			{
				note("[splitscreen] sun shadow slot: %u slices / %u partitions - not clamped\n",
				     slices, partitions);
				return;
			}
			sun_max_slot = slices / partitions - 1;
			sun_slot_clamped = call_site_to(sun_slot_count_call_rva, 5,
			                                reinterpret_cast<const void*>(&sun_slot_player_count));
		}

		// CL_LocalClient_GetActiveCount: PS4 (0x1516A20) sums IsActive(i) for i < 4, the PC
		// unrolls two elements. Its replacement adds elements 2/3 under the IsActive rule
		// (i < cl_maxLocalClients). It counts active flags, not seats: seats stay set in the
		// lobby after GAME OVER, which kept three panes open. Entered through
		// preserving_thunk; the original is never called.
		utils::hook::detour pane_count_hook;

		uint32_t active_count_for_panes()
		{
			const auto b = base();
			const auto* ui = reinterpret_cast<const volatile uint8_t*>(b + uia_base_rva);
			const auto max_local = *reinterpret_cast<const volatile int32_t*>(b + cl_max_local_clients_rva);
			uint32_t count = 0;
			for (int lc = 0; lc < 4; ++lc)
			{
				if (lc >= 2 && max_local <= lc)
				{
					continue;
				}
				count += *reinterpret_cast<const volatile uint32_t*>(ui + lc * uia_stride) & 1;
			}
			return count;
		}

		bool install_pane_counts_and_bounds()
		{
			if (pane_counts_installed)
			{
				return true;
			}
			// clientUIActives cannot move, so its IsActive gate is replaced. Every other
			// array the pane path indexes at 2 must be relocated first, or the wider
			// bounds corrupt foreign globals.
			if (!isactive_hooked || !view_params_relocated
				|| !scrplace_relocated || !perclient54_relocated
				|| !aaglob_relocated)
			{
				return false;
			}
			const auto b = base();

			auto* fn = reinterpret_cast<uint8_t*>(b + get_active_count_rva);
			if (std::memcmp(fn, get_active_count_expected,
			                sizeof(get_active_count_expected)) != 0)
			{
				return false;
			}
			for (const auto& f : pane_bounds)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + f.rva);
				if (!readable(p, f.expect_len)
					|| std::memcmp(p, f.expect, f.expect_len) != 0)
				{
					return false;
				}
			}

			auto* thunk = preserving_thunk(reinterpret_cast<const void*>(&active_count_for_panes));
			if (!thunk || !hook_if_stock(pane_count_hook, get_active_count_rva, get_active_count_expected,
			                             static_cast<void*>(thunk)))
			{
				return false;
			}

			uint32_t applied = 0;
			for (const auto& f : pane_bounds)
			{
				auto* site = reinterpret_cast<uint8_t*>(b + f.rva + f.offset);
				if (*site != f.from || !write_bytes(site, &f.to, sizeof(f.to)))
				{
					// roll the whole transaction back
					for (uint32_t i = 0; i < applied; ++i)
					{
						auto* undo = reinterpret_cast<uint8_t*>(
							b + pane_bounds[i].rva + pane_bounds[i].offset);
						write_bytes(undo, &pane_bounds[i].from, 1);
					}
					pane_count_hook.clear();
					return false;
				}
				++applied;
			}

			pane_counts_installed = true;
			return true;
		}
