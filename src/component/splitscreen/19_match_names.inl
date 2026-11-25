// Included by 19_player_names.inl. The CG global is encoded, not a raw pointer.
// Obtain the engine-decoded viewer CG from RBX at its overhead updater call.
		utils::hook::detour names_match_hook;
		bool names_match_ready = false;
		constexpr uint32_t names_match_formatter_rva = 0x013E3160;
		constexpr uint32_t names_cache_update_rva = 0x006A7AD0;
		constexpr uint32_t names_cache_call_rva = 0x0069D7D9;
		constexpr size_t names_cg_stride = 0x342720;
		constexpr uint8_t names_match_formatter_bytes[] = {
			0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,
			0x49,0x63,0xF1,0x49,0x8B,0xF8,0x8B,0xDA,0x41,0xC6,0x00,0x00,0xE8,0x50,0xEA,0xC8,0xFE};
		constexpr uint8_t names_cg_layout_bytes[] = {
			0x44,0x3B,0x35,0xD8,0x60,0xC8,0x04,0x7C,0x04,0x33,0xDB,0xEB,0x0D,
			0x49,0x8B,0xDE,0x48,0x69,0xDB,0x20,0x27,0x34,0x00,0x48,0x03,0xD8};
		constexpr uint8_t names_cg_client_bytes[] = {0x39,0x3B,0x75,0x38};
		constexpr uint8_t names_cache_call_bytes[] = {
			0x8B,0x4D,0x9C,0x8B,0xD7,0x48,0x89,0x45,0x88,0xE8,0xF2,0xA2,0x00,0x00};
		// Entire unprotected updater, except its already-relocated LEA displacement
		// at offsets 0x1e..0x21 (validated against both other playerDetails LEAs).
		constexpr uint8_t names_cache_update_bytes[] = {
			0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x41,
			0x56,0x48,0x83,0xEC,0x30,0x4C,0x63,0xF1,0x48,0x63,0xEA,0x48,0x8D,0x05,0x9E,0xD5,
			0x29,0x04,0x4F,0x8D,0x04,0xF6,0x41,0xB9,0x20,0x00,0x00,0x00,0x8B,0xD5,0x4A,0x8D,
			0x3C,0x45,0x00,0x00,0x00,0x00,0x41,0x8B,0xCE,0xC6,0x44,0x24,0x20,0x00,0x48,0x03,
			0xFD,0x48,0x6B,0xFF,0x68,0x48,0x03,0xF8,0x4C,0x8D,0x47,0x2F,0xE8,0x3F,0xB6,0xD3,
			0x00,0x85,0xC0,0x74,0x7D,0x48,0x89,0x5C,0x24,0x40,0x48,0x83,0xCB,0xFF,0x48,0x8B,
			0xC3,0x48,0xFF,0xC0,0x80,0x7C,0x07,0x2F,0x00,0x75,0xF6,0x41,0xB9,0x28,0x00,0x00,
			0x00,0x4C,0x8B,0xC7,0x8B,0xD5,0x41,0x8B,0xCE,0x89,0x47,0x58,0xC6,0x44,0x24,0x20,
			0x01,0xE8,0x0A,0xB6,0xD3,0x00,0x85,0xC0,0x74,0x43,0x48,0x8B,0xC3,0x0F,0x1F,0x00,
			0x48,0xFF,0xC0,0x80,0x3C,0x07,0x00,0x75,0xF7,0x89,0x47,0x50,0x48,0x8D,0x05,0x3D,
			0x1F,0x88,0x02,0x4C,0x8D,0x47,0x28,0x41,0xB9,0x07,0x00,0x00,0x00,0x8B,0xD5,0x41,
			0x8B,0xCE,0x48,0x89,0x44,0x24,0x20,0xE8,0x64,0xB4,0xD3,0x00,0x85,0xC0,0x74,0x0D,
			0x48,0xFF,0xC3,0x80,0x7C,0x1F,0x28,0x00,0x75,0xF6,0x89,0x5F,0x54,0x48,0x8B,0x5C,
			0x24,0x40,0x48,0x8B,0x6C,0x24,0x48,0x48,0x8B,0x74,0x24,0x50,0x48,0x8B,0x7C,0x24,
			0x58,0x48,0x83,0xC4,0x30,0x41,0x5E,0xC3};

		struct names_draw_state { int viewer = -1; uintptr_t cg = 0; };
		thread_local names_draw_state names_draw_context;
		using names_cache_function = void (*)(int, int);
		names_cache_function names_cache_update = nullptr;

		int names_match_controller(const int viewer, const int client)
		{
			if (!signin_relocated || client < 0 || client >= 64 || viewer != names_draw_context.viewer
				|| viewer < 0 || viewer >= 4 || !names_draw_context.cg) { return -1; }
			uint32_t count{};
			const auto* count_at = reinterpret_cast<const void*>(base() + cl_max_local_clients_rva);
			if (!readable(count_at, sizeof(count))) { return -1; }
			std::memcpy(&count, count_at, sizeof(count));
			const size_t offset = size_t(viewer) * names_cg_stride;
			if (count < 1 || count > 4 || uint32_t(viewer) >= count || names_draw_context.cg < offset) { return -1; }
			const auto cg = names_draw_context.cg - offset;
			if (!cg || cg > UINTPTR_MAX - (size_t(count - 1) * names_cg_stride + 0x38)) { return -1; }
			std::array<names::match_seat, 4> seats{};
			for (uint32_t lc = 0; lc < count; ++lc)
			{
				const auto* record = reinterpret_cast<const uint8_t*>(base() + signin_new_base + lc * signin_stride);
				if (!readable(record, 12)) { return -1; }
				uint32_t flags{}, local{};
				int controller{};
				std::memcpy(&flags, record + cgs_flags, sizeof(flags));
				std::memcpy(&local, record + cgs_local_client_num, sizeof(local));
				std::memcpy(&controller, record + cgs_controller_index, sizeof(controller));
				if (!(flags & 1)) { continue; }
				if (local != lc || controller < 0 || controller >= 4) { return -1; }
				const auto* state = reinterpret_cast<const uint8_t*>(cg + size_t(lc) * names_cg_stride);
				if (!readable(state, 0x38)) { return -1; }
				int number{};
				uintptr_t snapshot{};
				std::memcpy(&number, state, sizeof(number));
				std::memcpy(&snapshot, state + 0x30, sizeof(snapshot));
				if (!snapshot || number < 0 || number >= 64) { return -1; }
				seats[lc] = {true, controller, number};
			}
			return names::match_controller(seats, client);
		}

		int names_match_stub(const int viewer, const int client, char* buffer, const int capacity, const bool clan)
		{
			const int result = names_match_hook.invoke<int>(viewer, client, buffer, capacity, clan);
			if (!result || !buffer || capacity < 2 || capacity > 1024 || viewer < 0 || viewer >= 4
				|| viewer != names_draw_context.viewer || !names_draw_context.cg
				|| game::Com_IsRunningUILevel() || !names_offline()) { return result; }
			try
			{
				const int controller = names_match_controller(viewer, client);
				const auto* snapshot = names_published.load(std::memory_order_acquire);
				if (controller < 0 || !snapshot || (*snapshot)[controller].empty()
					|| !readable(buffer, capacity)) { return result; }
				const auto* end = static_cast<const char*>(std::memchr(buffer, 0, capacity));
				if (!end) { return result; }
				const std::string_view stock(buffer, static_cast<size_t>(end - buffer));
				char plain[32]{};
				std::string_view original = stock;
				if (clan)
				{
					if (!names_match_hook.invoke<int>(viewer, client, plain, 32, false)) { return result; }
					const auto* plain_end = static_cast<const char*>(std::memchr(plain, 0, sizeof(plain)));
					if (!plain_end) { return result; }
					original = std::string_view(plain, static_cast<size_t>(plain_end - plain));
				}
				std::string formatted;
				if (names::format_match_name(stock, original, (*snapshot)[controller], capacity, formatted))
				{
					std::memcpy(buffer, formatted.c_str(), formatted.size() + 1);
				}
			}
			catch (...) { /* allocation failure keeps stock output intact */ }
			return result;
		}

		void names_cache_stub(const int viewer, const int client, const uintptr_t viewer_cg)
		{
			// Restore even on unwinding/nested calls. No persistent CG pointer or
			// guessed decoding of the protected global survives this stock update.
			const auto previous = names_draw_context;
			struct restore_context
			{
				names_draw_state previous;
				~restore_context() { names_draw_context = previous; }
			} restore{previous};
			names_draw_context = {viewer, viewer_cg};
			names_cache_update(viewer, client);
		}

		bool names_cache_bytes_match()
		{
			const auto* at = reinterpret_cast<const uint8_t*>(base() + names_cache_update_rva);
			if (!readable(at, sizeof(names_cache_update_bytes))) { return false; }
			for (size_t i = 0; i < sizeof(names_cache_update_bytes); ++i)
			{
				if ((i < 0x1e || i >= 0x22) && at[i] != names_cache_update_bytes[i]) { return false; }
			}
			uintptr_t target = 0;
			for (const auto rva : {0x00677B94u, 0x0069D795u, 0x006A7AEBu})
			{
				const auto* lea = reinterpret_cast<const uint8_t*>(base() + rva);
				if (!readable(lea, 7) || lea[0] != 0x48 || lea[1] != 0x8D
					|| lea[2] != (rva == 0x00677B94 ? 0x0D : 0x05)) { return false; }
				int32_t disp{};
				std::memcpy(&disp, lea + 3, sizeof(disp));
				const auto current = base() + rva + 7 + static_cast<intptr_t>(disp);
				if (target && current != target) { return false; }
				target = current;
			}
			return target && readable(reinterpret_cast<const void*>(target), 4 * 18 * 0x68);
		}

		void install_match_names()
		{
			if (names_match_ready) { return; }
			try
			{
				if (!engine_bytes_match(names_match_formatter_rva, names_match_formatter_bytes)
					|| !engine_bytes_match(0x0069D641, names_cg_layout_bytes)
					|| !engine_bytes_match(0x0069D7E7, names_cg_client_bytes)
					|| !engine_bytes_match(names_cache_call_rva - 9, names_cache_call_bytes)
					|| !names_cache_bytes_match())
				{
					note("[splitscreen] match names: bytes differ - stock display retained\n");
					return;
				}
				// The original CALL supplies ECX/EDX. Relay adds decoded viewer CG
				// in R8 without changing RBX, the stack, or the original return address.
				uint8_t code[] = {0x49,0x89,0xD8,0xFF,0x25,0,0,0,0,0,0,0,0,0,0,0,0};
				const auto destination = reinterpret_cast<uintptr_t>(&names_cache_stub);
				std::memcpy(code + 9, &destination, sizeof(destination));
				auto* relay = static_cast<uint8_t*>(allocate_near_module(sizeof(code)));
				if (!relay) { return; }
				const auto distance = static_cast<int64_t>(reinterpret_cast<uintptr_t>(relay))
					- static_cast<int64_t>(base() + names_cache_call_rva + 5);
				if (distance < INT32_MIN || distance > INT32_MAX || !write_bytes(relay, code, sizeof(code))) { return; }
				uint8_t patch[] = {0xE8,0,0,0,0};
				const auto rel32 = static_cast<int32_t>(distance);
				std::memcpy(patch + 1, &rel32, sizeof(rel32));
				names_cache_update = reinterpret_cast<names_cache_function>(base() + names_cache_update_rva);
				names_match_hook.create(base() + names_match_formatter_rva, reinterpret_cast<void*>(&names_match_stub));
				if (!write_bytes(reinterpret_cast<void*>(base() + names_cache_call_rva), patch, sizeof(patch)))
				{
					names_match_hook.clear();
					return;
				}
				names_match_ready = true;
			}
			catch (...) { note("[splitscreen] match names: hook failed - stock display retained\n"); }
		}