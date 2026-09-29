// Sun shadow slices per view, pane counts and bounds.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Sun shadow: bound each view's slot ----------------------------------
		// RT 5 (sun shadow depth) and RT 9 (its colour twin) have 3 slices per view
		// slot. The slot is min(splitscreen player count - 1, localClientNum); with
		// the count at 3, player 3 got a slot with no slices and crashed in d3d11
		// OMSetRenderTargets. The cave caps the slot at slices / partitions - 1,
		// read from the verified instructions. 15 straight-line bytes become
		// `jmp cave` + NOPs.
		bool sun_slot_clamped = false;

		// ---- Sun shadow: 12 slices, one slot per view -----------------------------
		// With 2 slots, players 2-4 shared slot 1 and overwrote each other's
		// shadows. PS4 renders views one after another into the same slices; the
		// PC does not, so RT 5 (depth) and RT 9 (colour) grow from 6 to 12 slices.
		// - Descriptors: the two slice stores become calls to caves that store 12
		//   (the `mov r8d,6` stays; it is also RT 6's id).
		// - Depth arrays are sized from the slice count and need nothing else.
		// - Colour views live in 8 inline slots (index clamped to 7). The creation
		//   check admits 2..12, the loop stops at 6 so the inline slots stay stock,
		//   and slices 6..11 get sidecar views (maintain_sun_trans_views) that the
		//   setter and the clear pick; the clear has no clamp of its own.
		// Applied before R_Init, which builds the descriptors. History: LOG.md, 12 slices
		constexpr uint32_t rt_record_stride = 0xAE0;
		constexpr uint32_t sun_trans_rt = 9;
		constexpr uint32_t sun_slices_wanted = 12;
		bool sun_slices_grown = false;
		ID3D11RenderTargetView* sun_trans_extra[6] = {};      // RT 9 slices 6..11 (read by the caves)
		ID3D11RenderTargetView* sun_trans_retired[6] = {};    // released one tick later
		void* sun_trans_seen_v0 = nullptr;

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
				|| !matches(sun_view_check_rva, sun_view_check_stock, sizeof(sun_view_check_stock))
				|| !matches(sun_view_loop_rva, sun_view_loop_stock, sizeof(sun_view_loop_stock))
				|| !matches(sun_setter_rva, sun_setter_stock, sizeof(sun_setter_stock))
				|| !matches(sun_clear_rva, sun_clear_stock, sizeof(sun_clear_stock)))
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - bytes differ\n");
				return false;
			}
			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x180));
			if (!cave)
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - allocation failed\n");
				return false;
			}
			const uint8_t n = static_cast<uint8_t>(sun_slices_wanted);
			// +0x00: mov qword [rbp+0xD04], 12 ; ret      (RT 5 slices, colorFormat 0)
			const uint8_t c_rt5[] = {0x48, 0xC7, 0x85, 0x04, 0x0D, 0x00, 0x00, n, 0x00, 0x00, 0x00, 0xC3};
			// +0x10: mov dword [rbp+0xD5C], 12 ; ret      (RT 9 slices)
			const uint8_t c_rt9[] = {0xC7, 0x85, 0x5C, 0x0D, 0x00, 0x00, n, 0x00, 0x00, 0x00, 0xC3};
			// +0x20: movzx eax,[rsi+0xA86] ; cmp eax,6 ; jbe +5 ; mov eax,6 ; ret
			const uint8_t c_loop[] = {0x0F, 0xB7, 0x86, 0x86, 0x0A, 0x00, 0x00, 0x83, 0xF8, 0x06, 0x76, 0x05,
			                          0xB8, 0x06, 0x00, 0x00, 0x00, 0xC3};
			// +0x40: the setter's colour pick.
			std::vector<uint8_t> s;
			s.insert(s.end(), {0x83, 0xFB, 0x06, 0x7C, 0x00});             // cmp ebx,6 ; jl orig
			const size_t j1 = s.size() - 1;
			s.insert(s.end(), {0x83, 0xFB, 0x0C, 0x73, 0x00});             // cmp ebx,12 ; jae orig
			const size_t j2 = s.size() - 1;
			s.insert(s.end(), {0x66, 0x83, 0xFE, static_cast<uint8_t>(sun_trans_rt), 0x75, 0x00});   // cmp si,9 ; jne orig
			const size_t j3 = s.size() - 1;
			s.insert(s.end(), {0x49, 0xB8});                                 // mov r8, &sun_trans_extra
			{
				const auto a = reinterpret_cast<uint64_t>(&sun_trans_extra[0]);
				const auto* p = reinterpret_cast<const uint8_t*>(&a);
				s.insert(s.end(), p, p + 8);
			}
			s.insert(s.end(), {0x4D, 0x8B, 0x44, 0xD8, 0xD0});             // mov r8,[r8+rbx*8-0x30]
			s.insert(s.end(), {0x4D, 0x85, 0xC0, 0x75, 0x00});             // test r8,r8 ; jnz done
			const size_t j4 = s.size() - 1;
			const size_t orig = s.size();
			s.insert(s.end(), sun_setter_stock, sun_setter_stock + sizeof(sun_setter_stock));
			const size_t done = s.size();
			s.insert(s.end(), {0x0F, 0xB7, 0xD6, 0x33, 0xC9});             // movzx edx,si ; xor ecx,ecx
			s.insert(s.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});       // jmp [rip+0]
			{
				const uint64_t back = b + sun_setter_rva + sizeof(sun_setter_stock);
				const auto* p = reinterpret_cast<const uint8_t*>(&back);
				s.insert(s.end(), p, p + 8);
			}
			s[j1] = static_cast<uint8_t>(orig - (j1 + 1));
			s[j2] = static_cast<uint8_t>(orig - (j2 + 1));
			s[j3] = static_cast<uint8_t>(orig - (j3 + 1));
			s[j4] = static_cast<uint8_t>(done - (j4 + 1));
			// +0x100: the clear's colour pick (rbp = view set, rbx = cmd state):
			// sidecar for RT 9 slices 6..11, otherwise inline[slice], clamped.
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
			if (0x40 + s.size() > 0x100 || 0x100 + k.size() > 0x180
				|| !write_bytes(cave + 0x00, c_rt5, sizeof(c_rt5))
				|| !write_bytes(cave + 0x10, c_rt9, sizeof(c_rt9))
				|| !write_bytes(cave + 0x20, c_loop, sizeof(c_loop))
				|| !write_bytes(cave + 0x40, s.data(), s.size())
				|| !write_bytes(cave + 0x100, k.data(), k.size()))
			{
				note("[splitscreen] sun shadow 12 slices: NOT applied - cave write failed\n");
				return false;
			}

			// All-or-nothing. Setter first: without sidecar views it acts as stock.
			const auto call_site = [&](uint32_t rva, size_t len, size_t cave_off, uint8_t* out)
			{
				std::memset(out, 0x90, len);
				out[0] = 0xE8;
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + cave_off) - (b + rva + 5));
				std::memcpy(out + 1, &rel, sizeof(rel));
			};
			uint8_t p_setter[sizeof(sun_setter_stock)];
			std::memset(p_setter, 0x90, sizeof(p_setter));
			p_setter[0] = 0xE9;
			{
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + 0x40) - (b + sun_setter_rva + 5));
				std::memcpy(p_setter + 1, &rel, sizeof(rel));
			}
			uint8_t p_clear[sizeof(sun_clear_stock)];
			std::memset(p_clear, 0x90, sizeof(p_clear));
			p_clear[0] = 0xE9;
			{
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + 0x100) - (b + sun_clear_rva + 5));
				std::memcpy(p_clear + 1, &rel, sizeof(rel));
			}
			uint8_t p_loop[sizeof(sun_view_loop_stock)];
			call_site(sun_view_loop_rva, sizeof(p_loop), 0x20, p_loop);
			const uint8_t p_check[] = {0x66, 0x83, 0xF8, static_cast<uint8_t>(sun_slices_wanted - 2)};
			uint8_t p_rt5[sizeof(sun_desc_rt5_stock)];
			call_site(sun_desc_rt5_rva, sizeof(p_rt5), 0x00, p_rt5);
			uint8_t p_rt9[sizeof(sun_desc_rt9_stock)];
			call_site(sun_desc_rt9_rva, sizeof(p_rt9), 0x10, p_rt9);

			struct w { uint32_t rva; const uint8_t* stock; const uint8_t* patch; size_t n; };
			const w writes[] = {
				{sun_setter_rva, sun_setter_stock, p_setter, sizeof(p_setter)},
				{sun_clear_rva, sun_clear_stock, p_clear, sizeof(p_clear)},
				{sun_view_loop_rva, sun_view_loop_stock, p_loop, sizeof(p_loop)},
				{sun_view_check_rva, sun_view_check_stock, p_check, sizeof(p_check)},
				{sun_desc_rt5_rva, sun_desc_rt5_stock, p_rt5, sizeof(p_rt5)},
				{sun_desc_rt9_rva, sun_desc_rt9_stock, p_rt9, sizeof(p_rt9)},
			};
			size_t done_w = 0;
			for (const auto& x : writes)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + x.rva), x.patch, x.n))
				{
					for (size_t k = 0; k < done_w; ++k)
					{
						write_bytes(reinterpret_cast<void*>(b + writes[k].rva), writes[k].stock, writes[k].n);
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

		void clamp_sun_shadow_slot()
		{
			if (sun_slot_clamped)
			{
				return;
			}
			const auto b = base();
			const auto* slices_at = reinterpret_cast<const uint8_t*>(b + sun_slices_rva);
			const auto* parts_at = reinterpret_cast<const uint8_t*>(b + sun_partitions_rva);
			auto* site = reinterpret_cast<uint8_t*>(b + sun_slot_site_rva);
			if (!readable(slices_at, 6) || slices_at[0] != 0x41 || slices_at[1] != 0xB8
				|| !readable(parts_at, 3) || parts_at[0] != 0x83 || parts_at[1] != 0xFB
				|| !readable(site, sizeof(sun_slot_site_expected))
				|| std::memcmp(site, sun_slot_site_expected, sizeof(sun_slot_site_expected)) != 0)
			{
				note("[splitscreen] sun shadow slot: bytes differ - not clamped\n");
				return;
			}
			uint32_t slices = 0;
			std::memcpy(&slices, slices_at + 2, sizeof(slices));
			// grow_sun_shadow_slices() leaves that immediate at 6 and stores 12 itself.
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
			const uint32_t max_slot = slices / partitions - 1;

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
			if (!cave)
			{
				return;
			}
			std::vector<uint8_t> c(sun_slot_site_expected, sun_slot_site_expected + 12);
			c.insert(c.end(), {0x83, 0xF9, static_cast<uint8_t>(max_slot)}); // cmp ecx, max_slot
			c.insert(c.end(), {0x7E, 0x05});                                 // jle +5
			c.insert(c.end(), {0xB9});                                       // mov ecx, max_slot
			{
				const auto* p = reinterpret_cast<const uint8_t*>(&max_slot);
				c.insert(c.end(), p, p + 4);
			}
			c.insert(c.end(), {0x89, 0x4D, 0x14});                           // mov [rbp+0x14], ecx
			c.insert(c.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});         // jmp [rip+0]
			const uint64_t back = b + sun_slot_site_rva + sizeof(sun_slot_site_expected);
			const auto* back_bytes = reinterpret_cast<const uint8_t*>(&back);
			c.insert(c.end(), back_bytes, back_bytes + 8);
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return;
			}

			uint8_t patch[sizeof(sun_slot_site_expected)];
			std::memset(patch, 0x90, sizeof(patch));
			patch[0] = 0xE9;
			const auto rel = static_cast<int32_t>(
				reinterpret_cast<size_t>(cave) - (b + sun_slot_site_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return;
			}
			sun_slot_clamped = true;
		}

		bool install_pane_counts_and_bounds()
		{
			if (pane_counts_installed)
			{
				return true;
			}
			// clientUIActives cannot move, so its IsActive gate is caved. Every other
			// array the pane path indexes at 2 must be relocated first, or the wider
			// bounds corrupt foreign globals.
			if (!isactive_caved || !view_params_relocated
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

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x80));
			if (!cave)
			{
				return false;
			}
			const auto cave_addr = reinterpret_cast<size_t>(cave);
			std::vector<uint8_t> c;
			const auto rip32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};
			// CL_LocalClient_GetActiveCount: PS4 (0x1516A20) sums IsActive(i) for
			// i < 4, the PC unrolls two elements. The cave adds elements 2/3 under the
			// IsActive cave's rule (i < cl_maxLocalClients). It counts active flags,
			// not seats: seats stay set in the lobby after GAME OVER, which kept three
			// panes open.
			c.insert(c.end(), {0x33, 0xC0});                   // xor eax, eax
			c.insert(c.end(), {0x48, 0x8D, 0x0D});             // lea rcx, [clientUIActives]
			rip32(b + uia_base_rva);
			for (uint32_t i = 0; i < 4; ++i)
			{
				const uint32_t off = i * uia_stride;
				if (i >= 2)
				{
					c.insert(c.end(), {0x83, 0x3D});           // cmp dword [cl_maxLocalClients], i
					{
						const auto v = static_cast<int32_t>((b + cl_max_local_clients_rva)
							- (cave_addr + c.size() + 4 + 1));
						const auto* p = reinterpret_cast<const uint8_t*>(&v);
						c.insert(c.end(), p, p + 4);
					}
					c.insert(c.end(), {static_cast<uint8_t>(i)});
					c.insert(c.end(), {0x7E, 0x0B});           // jle next (skips the 11 bytes below)
				}
				if (off == 0)
				{
					c.insert(c.end(), {0x8B, 0x11});           // mov edx, [rcx]
				}
				else
				{
					c.insert(c.end(), {0x8B, 0x91});           // mov edx, [rcx+off]
					const auto* p = reinterpret_cast<const uint8_t*>(&off);
					c.insert(c.end(), p, p + 4);
				}
				c.insert(c.end(), {0x83, 0xE2, 0x01});         // and edx, 1
				c.insert(c.end(), {0x03, 0xC2});               // add eax, edx
			}
			c.insert(c.end(), {0xC3});                          // ret
			if (c.size() > 0x80)
			{
				note("[splitscreen] pane count: cave too small (%zu)\n", c.size());
				return false;
			}
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}

			uint8_t patch[5] = {0xE9};
			const auto rel = static_cast<int32_t>(
				cave_addr - (b + get_active_count_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			uint8_t old_head[5] = {};
			std::memcpy(old_head, fn, sizeof(old_head));
			if (!write_bytes(fn, patch, sizeof(patch)))
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
					write_bytes(fn, old_head, sizeof(old_head));
					return false;
				}
				++applied;
			}

			pane_counts_installed = true;
			return true;
		}
