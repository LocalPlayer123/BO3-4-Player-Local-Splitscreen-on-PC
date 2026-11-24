// Panes 3/4 lighting, the 4-player crash at doors / power switches, and restart state for
// clients 2/3. Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Umbra gate guard (user report 2026-11-11) ----
		// With four players, opening the Kino door to the outside and switching the power on
		// in Der Eisendrache crashed at 0x01C8DA0C (AV reading 0xFFFFFFFFFFFFFFFF); three
		// players on the same install did not. An Umbra query tests a gate's bit in the
		// culler's gate bitfield (sUmbra + 0x11CBC0), indexed by (id - 0x50000). It was
		// handed id 0, so the index went negative and the read landed far outside the object.
		// The 0 came from the per-view lighting objects below: client 3 wrote its float over
		// the object's gate id (+0x144). The lighting-object fix removes that source; this
		// guard stays as a backstop - an id below the first gate id skips the bit test and
		// returns 1, the value the stock test gives a gate whose bit is clear.

		std::atomic<uint32_t> umbra_gate_guard_hits{0};

		void umbra_gate_guard(midhook::context& c)
		{
			const auto id = static_cast<int32_t>(static_cast<uint32_t>(c.rax));
			if (id >= static_cast<int32_t>(umbra_gate_first_id))
			{
				return;   // a real gate id: run the stock bytes
			}
			const auto hits = umbra_gate_guard_hits.fetch_add(1, std::memory_order_relaxed) + 1;
#ifdef SS_DIAG
			// First few hits, then every power of two, so a per-frame repeat cannot flood the trace.
			const auto n = hits;
			if (n <= 8 || (n & (n - 1)) == 0)
			{
				uint64_t ret = 0;
				const auto* slot = reinterpret_cast<const void*>(midhook::site_rsp(c) + umbra_gate_ret_off);
				if (readable(slot, sizeof(ret)))
				{
					std::memcpy(&ret, slot, sizeof(ret));
				}
				const auto b = base();
				const auto ret_rva = (ret >= b && ret < b + 0x20000000) ? ret - b : 0;
				note("[splitscreen] umbra gate guard: hit %u, id %d, caller 0x%08llX, rbx 0x%llX, rsi 0x%llX, rdi 0x%llX\n",
				     n, id, static_cast<unsigned long long>(ret_rva), static_cast<unsigned long long>(c.rbx),
				     static_cast<unsigned long long>(c.rsi), static_cast<unsigned long long>(c.rdi));
			}
#endif
			c.rax = (c.rax & ~0xFFull) | 1;              // al = 1: what setae gives a clear bit
			c.rip = base() + umbra_gate_exit_rva;        // the stock path's own jump target
		}

		void install_umbra_gate_guard()
		{
			if (!engine_bytes_match(umbra_gate_exit_rva, umbra_gate_exit_bytes)
			    || !midhook::install(base() + umbra_gate_test_rva, umbra_gate_test_bytes,
			                         sizeof(umbra_gate_test_bytes), &umbra_gate_guard, &allocate_near_module))
			{
				note("[splitscreen] umbra gate guard: engine bytes differ - not installed\n");
				return;
			}
			note("[splitscreen] umbra gate guard: installed\n");
		}

		// ---- Lighting objects: storage for clients 2/3 ----
		// The per-view lighting objects keep a per-client float and skip flag for two clients only
		// (see lightobj_stride in splitscreen_addresses.hpp); clients 2/3 wrote over the flags and
		// the Umbra gate id and read them back as their own values. Clients 2/3 get their own
		// float and flag per object here, and every access indexed by client is redirected for
		// them. Until a client's value is first written, it reads client 0's (same map state).
		// Key by the actual object address, not one current array per client. Switching
		// between two lighting lists must not erase the first list's script-written state.
		// Fixed storage avoids heap allocation/exception paths inside renderer hooks.
		constexpr uint32_t lightobj_max = 32768;

		struct lightobj_side
		{
			uint64_t object = 0;
			uint8_t value_set[2]{};
			uint8_t flag_set[2]{};
			uint8_t flag[2]{};
			float value[2]{};
		};
		lightobj_side lightobj_extra[lightobj_max];
		bool lightobj_capacity_reported = false;
		// Process-lifetime lock: hooks may still run during DLL static teardown.
		std::mutex& lightobj_mutex()
		{
			static auto* mutex = new std::mutex;
			return *mutex;
		}

		void reset_lightobj_state()
		{
			std::lock_guard lock(lightobj_mutex());
			for (auto& s : lightobj_extra)
			{
				s = {};
			}
			lightobj_capacity_reported = false;
		}

		// Caller holds lightobj_mutex across lookup AND access. Reset after client memory
		// is freed: a new map can reuse exactly the previous object address.
		lightobj_side* lightobj_slot(const uint64_t lc, const uint64_t object)
		{
			if (lc < 2 || lc > 3 || !object)
			{
				return nullptr;
			}
			static_assert((lightobj_max & (lightobj_max - 1)) == 0);
			const auto first = ((object >> 3) * 0x9E3779B97F4A7C15ull) & (lightobj_max - 1);
			for (uint32_t probe = 0; probe < lightobj_max; ++probe)
			{
				auto& s = lightobj_extra[(first + probe) & (lightobj_max - 1)];
				if (!s.object || s.object == object)
				{
					s.object = object;
					return &s;
				}
			}
			if (!lightobj_capacity_reported)
			{
				lightobj_capacity_reported = true;
				note("[splitscreen] lighting sidecar full - extra-client values may fall back to client 0\n");
			}
			return nullptr;
		}

		uint8_t lightobj_flag_of(lightobj_side& s, const uint64_t object, const uint64_t lc)
		{
			return s.flag_set[lc - 2] ? s.flag[lc - 2]
			                         : *reinterpret_cast<const uint8_t*>(object + lightobj_flags_off);
		}

		float lightobj_value_of(lightobj_side& s, const uint64_t object, const uint64_t lc)
		{
			return s.value_set[lc - 2] ? s.value[lc - 2]
			                          : *reinterpret_cast<const float*>(object + lightobj_value_off);
		}

		// The trampoline keeps xmm0-5 in its own frame: (rsp at the site & -16) - 0x80 + 0x20 + n*0x10
		// (splitscreen_midhook.hpp); writing there changes the register the game continues with.
		float* saved_xmm(midhook::context& c, const int n)
		{
			const auto frame = (reinterpret_cast<uint64_t>(&c) & ~0xFull) - 0x80;
			return reinterpret_cast<float*>(frame + 0x20 + 0x10 * static_cast<uint64_t>(n));
		}

		// Derive the object from the displaced instruction's effective address, stripping
		// the client offset. This works whether rcx is the list base or an individual object;
		// no dependency on the compiler's representation of object index in r10.
		// movss [rcx+rdx*4+0x138],xmm3 - script sets client r9's value.
		void lightobj_value_set(midhook::context& c)
		{
			const auto lc = c.r9 & 0xFFFFFFFF;
			if (lc < 2)
			{
				return;
			}
			const auto object = c.rcx + c.rdx * 4 - lc * 4;
			std::lock_guard lock(lightobj_mutex());
			if (auto* s = lightobj_slot(lc, object))
			{
				s->value[lc - 2] = *saved_xmm(c, 3);
				s->value_set[lc - 2] = 1;
			}
			c.rip = base() + lightobj_value_set_next_rva;   // never write past the object's [2]
		}

		// movss xmm0,[rbx+rax*4+0x138] - the per-view walk reads client rax's value of object rbx.
		void lightobj_value_get(midhook::context& c)
		{
			const auto lc = c.rax & 0xFFFFFFFF;
			if (lc < 2)
			{
				return;
			}
			std::lock_guard lock(lightobj_mutex());
			auto* s = lightobj_slot(lc, c.rbx);
			*saved_xmm(c, 0) = s ? lightobj_value_of(*s, c.rbx, lc)
			                     : *reinterpret_cast<const float*>(c.rbx + lightobj_value_off);
			c.rip = base() + lightobj_value_get_next_rva;
		}

		// cmp [rax+rsi+0x140],r13b ; jne skip - the per-view walk tests client rax's flag.
		void lightobj_flag_test(midhook::context& c)
		{
			const auto lc = c.rax & 0xFFFFFFFF;
			if (lc < 2)
			{
				return;
			}
			std::lock_guard lock(lightobj_mutex());
			auto* s = lightobj_slot(lc, c.rsi);
			const auto flag = s ? lightobj_flag_of(*s, c.rsi, lc)
			                    : *reinterpret_cast<const uint8_t*>(c.rsi + lightobj_flags_off);
			c.rip = base() + (flag != static_cast<uint8_t>(c.r13) ? lightobj_flag_test_set_rva
			                                                      : lightobj_flag_test_clear_rva);
		}

		// mov [rcx+r10+0x140],dl - script sets client r10's flag of object r11.
		void lightobj_flag_set(midhook::context& c)
		{
			const auto lc = c.r10 & 0xFFFFFFFF;
			if (lc < 2)
			{
				return;
			}
			std::lock_guard lock(lightobj_mutex());
			if (auto* s = lightobj_slot(lc, c.rcx))
			{
				s->flag[lc - 2] = static_cast<uint8_t>(c.rdx);
				s->flag_set[lc - 2] = 1;
			}
			c.rip = base() + lightobj_flag_set_next_rva;
		}

		// mov byte [reg+r14+0x140],0/1 - restore sets client r14's flag of object edi. (rdx holds
		// index * 0x148 at the first site only: `mov edx,4` precedes the second.)
		void lightobj_flag_restore(midhook::context& c, const uint64_t object, const uint8_t value, const uint32_t next)
		{
			const auto lc = c.r14 & 0xFFFFFFFF;
			if (lc < 2)
			{
				return;
			}
			std::lock_guard lock(lightobj_mutex());
			if (auto* s = lightobj_slot(lc, object))
			{
				s->flag[lc - 2] = value;
				s->flag_set[lc - 2] = 1;
			}
			c.rip = base() + next;
		}

		void lightobj_flag_clear(midhook::context& c)
		{
			lightobj_flag_restore(c, c.rcx, 0, lightobj_flag_clear_next_rva);
		}

		void lightobj_flag_mark(midhook::context& c)
		{
			lightobj_flag_restore(c, c.rax, 1, lightobj_flag_mark_next_rva);
		}

		// cmp byte [rcx+rsi+0x140],0 ; je next - save writes the indices of client rsi's set flags.
		void lightobj_flag_save(midhook::context& c)
		{
			const auto lc = c.rsi & 0xFFFFFFFF;
			if (lc < 2)
			{
				return;
			}
			std::lock_guard lock(lightobj_mutex());
			auto* s = lightobj_slot(lc, c.rcx);
			const auto flag = s ? lightobj_flag_of(*s, c.rcx, lc)
			                    : *reinterpret_cast<const uint8_t*>(c.rcx + lightobj_flags_off);
			c.rip = base() + (flag ? lightobj_flag_save_set_rva : lightobj_flag_save_clear_rva);
		}

		bool lightobj_fix_installed = false;

		void install_lightobj_flag_fix()
		{
			if (lightobj_fix_installed)
			{
				return;
			}
			struct site
			{
				uint32_t rva;
				const uint8_t* stock;
				size_t len;
				midhook::callback fn;
			};
			const site sites[] = {
				{lightobj_value_set_rva, lightobj_value_set_bytes, sizeof(lightobj_value_set_bytes), &lightobj_value_set},
				{lightobj_value_get_rva, lightobj_value_get_bytes, sizeof(lightobj_value_get_bytes), &lightobj_value_get},
				{lightobj_flag_test_rva, lightobj_flag_test_bytes, sizeof(lightobj_flag_test_bytes), &lightobj_flag_test},
				{lightobj_flag_set_rva, lightobj_flag_set_bytes, sizeof(lightobj_flag_set_bytes), &lightobj_flag_set},
				{lightobj_flag_clear_rva, lightobj_flag_clear_bytes, sizeof(lightobj_flag_clear_bytes), &lightobj_flag_clear},
				{lightobj_flag_mark_rva, lightobj_flag_mark_bytes, sizeof(lightobj_flag_mark_bytes), &lightobj_flag_mark},
				{lightobj_flag_save_rva, lightobj_flag_save_bytes, sizeof(lightobj_flag_save_bytes), &lightobj_flag_save},
			};
			// All or nothing: a writer redirected without its reader (or the reverse) would split
			// clients 2/3's values between two places.
			uint8_t patches[std::size(sites)][32]{};
			const auto b = base();
			for (size_t i = 0; i < std::size(sites); ++i)
			{
				const auto& st = sites[i];
				if (!midhook::prepare(b + st.rva, st.stock, st.len, st.fn, &allocate_near_module, patches[i]))
				{
					note("[splitscreen] lighting objects 2/3: site 0x%08X differs - nothing installed\n", st.rva);
					return;
				}
			}
			safety::patch transaction[std::size(sites)]{};
			for (size_t i = 0; i < std::size(sites); ++i)
			{
				transaction[i] = {reinterpret_cast<void*>(b + sites[i].rva), sites[i].stock,
				                  patches[i], sites[i].len};
			}
			if (!safety::apply_patches(transaction, std::size(transaction), write_bytes))
			{
				note("[splitscreen] lighting objects: write failed - rollback attempted\n");
				return;
			}
			lightobj_fix_installed = true;
			note("[splitscreen] lighting objects 2/3: own values and flags (7 sites)\n");
		}

		// ---- Restart: per-client table entries 2/3 without their self pointer ----
		// After an allocation that leaves the per-client init to the cgame (flag bit 0), give
		// entries 2/3 whose +0xB60 is still NULL the state the allocator's own init loop gives
		// every entry otherwise (0x014D47D0): +0xB60 = entry + 0xD20 and the side flag = 1. A full
		// per-client init later overwrites both; a restart that skips it for clients > 0 now
		// leaves a valid (empty) entry instead of a NULL. Clients 0/1 are never touched.
		// Gives entry `lc` the allocator loop's state if its self pointer is NULL.
		void client_table_fill_entry(const int32_t lc, const char* where)
		{
			const auto b = base();
			const auto count = *reinterpret_cast<const int32_t*>(b + cltable_count_rva);
			const auto table = *reinterpret_cast<const uint64_t*>(b + cltable_base_rva);
			const auto side = *reinterpret_cast<const uint64_t*>(b + cltable_side_rva);
			if (!table || lc < 2 || lc >= count || lc >= 4)
			{
				return;
			}
			const auto entry = table + static_cast<uint64_t>(lc) * cltable_stride;
			auto* self = reinterpret_cast<uint64_t*>(entry + cltable_self_ptr_off);
			if (*self)
			{
				return;
			}
			*self = entry + cltable_self_target_off;
			if (side)
			{
				*reinterpret_cast<int32_t*>(side + static_cast<uint64_t>(lc) * cltable_side_stride
				                            + cltable_side_flag_off) = 1;
			}
			note("[splitscreen] client table: entry %d had no self pointer (%s) - filled\n", lc, where);
		}

		void client_table_fill(midhook::context&)
		{
			for (int32_t lc = 2; lc < 4; ++lc)
			{
				client_table_fill_entry(lc, "allocation");
			}
		}

		// mov rax,[rcx+0xB60] at the reader that crashed: fill the entry first if it is one of
		// clients 2/3's and still empty, so the stock load then reads a valid pointer.
		void client_table_reader(midhook::context& c)
		{
			const auto b = base();
			const auto table = *reinterpret_cast<const uint64_t*>(b + cltable_base_rva);
			if (!table || c.rcx < table)
			{
				return;
			}
			const auto offset = c.rcx - table;
			if (offset % cltable_stride == 0)
			{
				client_table_fill_entry(static_cast<int32_t>(offset / cltable_stride), "restart");
			}
		}

		void install_client_table_fill()
		{
			if (!midhook::install(base() + cltable_alloc_ret_rva, cltable_alloc_ret_bytes,
			                      sizeof(cltable_alloc_ret_bytes), &client_table_fill, &allocate_near_module))
			{
				note("[splitscreen] client table fill: engine bytes differ at the allocation - not installed\n");
			}
			if (!midhook::install(base() + cltable_reader_rva, cltable_reader_bytes,
			                      sizeof(cltable_reader_bytes), &client_table_reader, &allocate_near_module))
			{
				note("[splitscreen] client table fill: engine bytes differ at the reader - not installed\n");
				return;
			}
			note("[splitscreen] client table fill: installed\n");
		}

		// ---- Match start: local clients initialise in order ----
		// A client's gamestate waits while any lower-numbered client in the match is still
		// loading (connectionState 6..9): client 2's cgame init must not run after client 3's
		// has started (see gamestate_order_rva). Checked at the start of CL_ParseGamestate, so it
		// applies even where the engine's own primary-client check is switched off. Waiting uses
		// the engine's own hold-back path, so the server simply sends the gamestate again.
		void gamestate_order(midhook::context& c)
		{
			const auto lc = static_cast<int32_t>(c.r14);
			if (lc <= 0 || lc >= 4)
			{
				return;
			}
			const auto b = base();
			int32_t holder = -1;
			for (int32_t lower = 0; lower < lc && holder < 0; ++lower)
			{
				const auto slot = b + uia_base_rva + static_cast<size_t>(lower) * uia_stride;
				const auto flags = *reinterpret_cast<const volatile uint32_t*>(slot);
				const auto state = *reinterpret_cast<const volatile int32_t*>(slot + uia_conn_state_off);
				if ((flags & 1) && state >= 6 && state < 10)
				{
					holder = lower;
				}
			}
			if (holder >= 0)
			{
				c.rip = b + gamestate_hold_rva;   // the stock hold-back: drop it, the server resends
			}
		}

		void install_gamestate_order()
		{
			if (!engine_bytes_match(gamestate_hold_rva, gamestate_hold_bytes)
			    || !midhook::install(base() + gamestate_order_rva, gamestate_order_bytes,
			                         sizeof(gamestate_order_bytes), &gamestate_order, &allocate_near_module))
			{
				note("[splitscreen] gamestate order: engine bytes differ - not installed\n");
				return;
			}
			note("[splitscreen] gamestate order: installed\n");
		}
