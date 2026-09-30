// LiveStats stat-change cache: LiveStats_SetStatChanged and LiveStats_ResetCache over four slots.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// PS4 live_stats.cpp: static cachedStats_t s_cachedStatsChanges[4], 0x1984 each (0x60
		// changes of {u8 data[0x40]; int size}, int count at 0x1980). The PC keeps 0x100 changes
		// (stride 0x4404, count at +0x4400) and only two slots at statscache_rva; slot 2 would be
		// SetStatChanged's hwm statics and the statReadDDLExt command node (crash in
		// Cmd_RemoveCommand at game over). Controllers 0/1 keep the engine's slots - the PC-only
		// statsHash command flushes slot 0 in place - 2/3 use these.
		struct stats_change
		{
			uint8_t data[0x40];
			int32_t size;
		};

		struct stats_cache
		{
			stats_change changes[0x100];
			int32_t count;
		};
		static_assert(sizeof(stats_change) == 0x44 && sizeof(stats_cache) == statscache_stride);

		stats_cache statscache_extra[2]{};
		bool statscache_installed = false;
		utils::hook::detour set_stat_changed_hook;

		stats_cache* stats_cache_for(const int ctrl)
		{
			if (ctrl == 0 || ctrl == 1)
			{
				return reinterpret_cast<stats_cache*>(base() + statscache_rva) + ctrl;
			}
			if (ctrl == 2 || ctrl == 3)
			{
				return &statscache_extra[ctrl - 2];
			}
			return nullptr;
		}

		// LiveStats_SetStatChanged (PS4 0xC63D00). The PC inlines SetStatChangedInternal into
		// the flush; that copy makes the same calls with the same arguments as the out-of-line
		// one (its write test is the predicate 0x01E93F90 inlined), which the statsHash command
		// and SetStatChangedNoCache call. `hwm` is false only for the DIAG dry run.
		void set_stat_changed(stats_cache& c, const int mode, const int ctrl, const char* msg,
		                      const int cache_only, const bool hwm)
		{
			const auto b = base();
			if (c.count < 0x100)
			{
				auto& change = c.changes[c.count++];   // the count is stored before the decode, as stock
				change.size = reinterpret_cast<int (*)(const char*, uint8_t*, int)>(b + com_decode_yenc_rva)(
					msg + 1, change.data, static_cast<int>(sizeof(change.data)));
			}
			else
			{
				reinterpret_cast<void (*)(const char*, int, int, const char*, ...)>(b + com_error_rva)(
					reinterpret_cast<const char*>(b + str_empty_rva), 0, 2 /* ERR_DROP */,
					reinterpret_cast<const char*>(b + str_statsoverflow_rva));
			}
			if (cache_only)
			{
				return;
			}
			if (hwm)
			{
				auto& guard = *reinterpret_cast<uint32_t*>(b + statscache_hwm_guard_rva);
				auto& id = *reinterpret_cast<int32_t*>(b + statscache_hwm_id_rva);
				if (!(guard & 1))
				{
					guard |= 1;
					id = reinterpret_cast<int (*)(const char*)>(b + bb_register_hwm_rva)(
						reinterpret_cast<const char*>(b + str_statscache2_rva));
				}
				reinterpret_cast<void (*)(int, uint64_t)>(b + bb_set_hwm_rva)(id, static_cast<uint32_t>(c.count));
			}
			const auto internal = reinterpret_cast<void (*)(int, int, const uint8_t*, int, int)>(
				b + set_stat_changed_internal_rva);
			for (int i = 0; i < c.count; ++i)   // count re-read every pass, as stock
			{
				internal(mode, ctrl, c.changes[i].data, c.changes[i].size, i != c.count - 1);
			}
			c.count = 0;
		}

		void set_stat_changed_stub(const int mode, const int ctrl, const char* msg, const int cache_only)
		{
			if (auto* c = stats_cache_for(ctrl))
			{
				note("[statscache] ctrl %d cache_only %d count %d\n", ctrl, cache_only, c->count);
				set_stat_changed(*c, mode, ctrl, msg, cache_only, true);
				return;
			}
			note("[splitscreen] statscache: controller %d out of range - change dropped\n", ctrl);
		}

		// LiveStats_ResetCache (PS4 0xC63CD0, clears all four) is inlined in LiveStats_PreGame;
		// its memset call becomes this: the engine's two slots as before, then ours.
		void* stats_cache_reset_stub(void* dst, const int value, const size_t size)
		{
			std::memset(dst, value, size);
			std::memset(statscache_extra, 0, sizeof(statscache_extra));
			note("[statscache] reset %s 0x%zX\n",
			     dst == reinterpret_cast<void*>(base() + statscache_rva) ? "engine" : "?", size);
			return dst;
		}

		// Every slot-0 reference of the statsHash command still addresses the engine's slot 0.
		bool stats_hash_on_engine_slot0()
		{
			const auto b = base();
			for (const auto& s : stats_hash_slot0_sites)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + s.rva);
				if (!readable(p, s.insn_len))
				{
					return false;
				}
				int32_t disp = 0;
				std::memcpy(&disp, p + s.disp_off, sizeof(disp));
				if (b + s.rva + s.insn_len + static_cast<ptrdiff_t>(disp) != b + statscache_rva + s.target_off)
				{
					return false;
				}
			}
			return true;
		}

		// All or nothing, and only over an engine that still addresses its own two slots
		// (the reset's lea/len/call and all six slot-0 references of the statsHash command).
		bool install_stats_cache()
		{
			if (statscache_installed)
			{
				return true;
			}
			uint8_t reset_call[5]{};
			if (!engine_bytes_match(statscache_reset_rva, statscache_reset_bytes)
			    || !stats_hash_on_engine_slot0()
			    || !call_site_bytes(statscache_reset_call_rva, sizeof(reset_call),
			                        reinterpret_cast<const void*>(&stats_cache_reset_stub), reset_call))
			{
				note("[splitscreen] statscache: engine bytes differ - not installed\n");
				return false;
			}
			try
			{
				if (!hook_if_stock(set_stat_changed_hook, set_stat_changed_rva, set_stat_changed_prologue,
				                   set_stat_changed_stub))
				{
					return false;
				}
			}
			catch (...)
			{
				set_stat_changed_hook.clear();
				note("[splitscreen] statscache: hook failed - not installed\n");
				return false;
			}
			if (!write_bytes(reinterpret_cast<void*>(base() + statscache_reset_call_rva), reset_call, sizeof(reset_call)))
			{
				set_stat_changed_hook.clear();
				note("[splitscreen] statscache: reset call not written - detour removed\n");
				return false;
			}
			statscache_installed = true;
			note("[splitscreen] statscache: installed\n");
			return true;
		}
