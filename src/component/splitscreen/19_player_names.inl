// Native per-controller names. Included inside splitscreen's anonymous namespace.
// No identity, XUID, seat, userData, or online-profile writes.
		// Verified 2026-11-24 against decrypted checksum 0x06531394 code, objdump
		// and caller_guard_audit.py (including the lookup tail at 0x02260120).
		constexpr uint32_t names_dvar_find_rva = 0x022601B0;
		constexpr uint8_t names_dvar_find_bytes[] = {
			0x48,0x83,0xEC,0x28,0x48,0x85,0xC9,0x74,0x15,0x80,0x39,0x00,0x74,0x10,
			0xE8,0x4D,0xDA,0x0D,0xFF,0x8B,0xC8,0x48,0x83,0xC4,0x28,0xE9,0x52,0xFF,
			0xFF,0xFF,0x33,0xC0,0x48,0x83,0xC4,0x28,0xC3};
		constexpr uint32_t names_lookup_rva = 0x02260120;
		constexpr uint8_t names_lookup_bytes[] = {0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0xD9};
		constexpr uint8_t names_stock_getter_bytes[] = {
			0x48,0x83,0xEC,0x28,0xE8,0x97,0xFB,0xFF,0xFF,
			0x48,0x83,0xC0,0x08,0x48,0x83,0xC4,0x28,0xC3};
		constexpr const char* names_dvars[] = {
			"splitscreen_name_1", "splitscreen_name_2", "splitscreen_name_3", "splitscreen_name_4"};
		constexpr wchar_t names_path[] = L"boiii_players\\splitscreen_names.cfg";
		std::atomic<const names::slots*> names_published{nullptr};
		utils::hook::detour names_stock_hook;
		bool names_hook_ready = false;
		utils::hook::detour names_match_hook;
		bool names_match_ready = false;
		constexpr uint32_t names_match_formatter_rva = 0x013E3160;
		constexpr uint8_t names_match_formatter_bytes[] = {
			0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,
			0x49,0x63,0xF1,0x49,0x8B,0xF8,0x8B,0xDA,0x41,0xC6,0x00,0x00,0xE8,0x50,0xEA,0xC8,0xFE};
		// CG_DrawNames subroutine: allocation count/base/stride and clientNum read.
		constexpr uint8_t names_cg_layout_bytes[] = {
			0x44,0x3B,0x35,0xD8,0x60,0xC8,0x04,0x7C,0x04,0x33,0xDB,0xEB,0x0D,
			0x49,0x8B,0xDE,0x48,0x69,0xDB,0x20,0x27,0x34,0x00,0x48,0x03,0xD8};
		constexpr uint8_t names_cg_client_bytes[] = {0x39,0x3B,0x75,0x38};

		int names_match_controller(const int client)
		{
			if (!signin_relocated || client < 0 || client >= 64) { return -1; }
			uint32_t count{};
			uintptr_t cg{};
			std::memcpy(&count, reinterpret_cast<const void*>(base() + cl_max_local_clients_rva), sizeof(count));
			std::memcpy(&cg, reinterpret_cast<const void*>(base() + 0x04C98C80), sizeof(cg));
			if (count < 1 || count > 4 || !cg) { return -1; }
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
				if (local != lc || controller < 0 || controller >= 4
					|| cg > UINTPTR_MAX - (size_t(lc) * 0x342720 + 0x38)) { return -1; }
				const auto* state = reinterpret_cast<const uint8_t*>(cg + size_t(lc) * 0x342720);
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

		bool names_offline()
		{
			if (!engine_bytes_match(lobby_get_network_mode_rva, lobby_get_network_mode_bytes)) { return false; }
			const int mode = reinterpret_cast<int (*)()>(base() + lobby_get_network_mode_rva)();
			return mode == lobby_network_local || mode == lobby_network_lan;
		}

		const char* player_name_override(const int controller)
		{
			if (controller < 0 || controller >= 4 || !names_offline()) { return nullptr; }
			const auto* snapshot = names_published.load(std::memory_order_acquire);
			return snapshot && !(*snapshot)[controller].empty() ? (*snapshot)[controller].c_str() : nullptr;
		}

		const char* names_stock_stub(const int controller)
		{
			if (const auto* name = player_name_override(controller)) { return name; }
			return names_stock_hook.invoke<const char*>(controller);
		}

		int names_match_stub(const int viewer, const int client, char* buffer, const int capacity, const bool clan)
		{
			// Preserve stock validation and failure results; never write client/session data.
			const int result = names_match_hook.invoke<int>(viewer, client, buffer, capacity, clan);
			if (!result || !buffer || capacity < 2 || capacity > 1024 || viewer < 0 || viewer >= 4
				|| game::Com_IsRunningUILevel() || !names_offline()) { return result; }
			try
			{
				const int controller = names_match_controller(client);
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

		void install_match_names()
		{
			try
			{
				// Cache updater 0x006A7AD0 calls this twice, with capacities 32/40.
				// Guard the formatter prologue separately from existing name hooks.
				if (!engine_bytes_match(names_match_formatter_rva, names_match_formatter_bytes)
					|| !engine_bytes_match(0x0069D641, names_cg_layout_bytes)
					|| !engine_bytes_match(0x0069D7E7, names_cg_client_bytes))
				{
					note("[splitscreen] match names: bytes differ - stock display retained\n");
					return;
				}
				names_match_hook.create(base() + names_match_formatter_rva, reinterpret_cast<void*>(&names_match_stub));
				names_match_ready = true;
			}
			catch (...) { note("[splitscreen] match names: hook failed - stock display retained\n"); }
		}

		void install_player_names()
		{
			install_match_names();
			try
			{
				if ((ezz::chained & 4) != 0)
				{
					ezz::name_provider.store(&player_name_override, std::memory_order_release);
					names_hook_ready = true;
				}
				else if (engine_bytes_match(ezz::get_client_name_rva, names_stock_getter_bytes))
				{
					// Never stack a second detour over ezz or an unrecognised hook.
					names_stock_hook.create(base() + ezz::get_client_name_rva, reinterpret_cast<void*>(&names_stock_stub));
					names_hook_ready = true;
				}
				else { note("[splitscreen] names: getter differs - overrides disabled\n"); }
			}
			catch (...) { note("[splitscreen] names: getter hook failed\n"); }
		}

		bool names_read_dvar(const char* key, std::string& value)
		{
			const auto* dvar = reinterpret_cast<const uint8_t* (*)(const char*)>(
				base() + names_dvar_find_rva)(key);
			if (!dvar || !readable(dvar, 0x30)) { return false; }
			uint32_t type{};
			const char* text{};
			std::memcpy(&type, dvar + 0x1C, sizeof(type));
			std::memcpy(&text, dvar + 0x28, sizeof(text));
			// ezz's encryptedDvar_t retains these fields; STRING (8) is not encrypted.
			if (type != 8 || !text) { return false; }
			value.clear();
			for (size_t i = 0; i < 1024; ++i)
			{
				if (!readable(text + i, 1)) { return false; }
				const char c = text[i];
				if (!c) { return true; }
				value += c;
			}
			return false; // reject unterminated/unbounded engine values
		}

		void names_set_dvar(const char* key, const std::string& value)
		{
			// Keys are native constants; values pass the same command-safe UTF-8 codec.
			const auto command = std::string("set ") + key + " \"" + names::sanitize(value) + "\"";
			reinterpret_cast<void (*)(int, int, const char*, int)>(
				base() + cmd_execute_single_command_rva)(0, 0, command.c_str(), 0);
		}

		bool names_load(names::slots& values)
		{
			const HANDLE file = CreateFileW(names_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				const auto error = GetLastError();
				return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
			}
			char buffer[256];
			DWORD got{};
			const bool ok = ReadFile(file, buffer, sizeof(buffer), &got, nullptr) != FALSE;
			CloseHandle(file);
			return ok && got < sizeof(buffer) && names::decode(std::string_view(buffer, got), values);
		}

		bool names_save(const names::slots& values)
		{
			// Same directory/volume, flush before replace; failure leaves the old file intact.
			const auto bytes = names::encode(values);
			const auto temp = std::wstring(names_path) + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
			if (!CreateDirectoryW(L"boiii_players", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) { return false; }
			const HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE) { return false; }
			DWORD written{};
			bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE
				&& written == bytes.size() && FlushFileBuffers(file) != FALSE;
			if (!CloseHandle(file)) { ok = false; }
			if (ok) { ok = MoveFileExW(temp.c_str(), names_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE; }
			if (!ok) { DeleteFileW(temp.c_str()); }
			return ok;
		}

		// Renderer only, never async. Immutable snapshots are intentionally retained:
		// engine callers may keep returned char pointers after the hook has returned.
		void poll_player_names()
		{
			static bool loaded = false, writable = false, initialized = false, disabled = false;
			static names::slots current, saved;
			static std::string last_status;
			if (disabled || !game::Com_IsRunningUILevel() || !names_offline()) { return; }
			try
			{
				if (!engine_bytes_match(names_dvar_find_rva, names_dvar_find_bytes)
					|| !engine_bytes_match(names_lookup_rva, names_lookup_bytes)
					|| !engine_bytes_match(cmd_execute_single_command_rva, cmd_execute_single_command_prologue))
				{
					disabled = true;
					note("[splitscreen] names: dvar/command bytes differ - bridge disabled\n");
					return;
				}
				if (!loaded)
				{
					loaded = true; // do not retry an uncertain read and then replace the file
					writable = names_load(current);
					saved = current;
					if (!writable) { note("[splitscreen] names: load failed - file protected from writes\n"); }
				}
				if (!initialized)
				{
					for (size_t i = 0; i < 4; ++i) { names_set_dvar(names_dvars[i], current[i]); }
					names_set_dvar("splitscreen_names_match_status", names_match_ready ? "ready" : "hook_error");
					names_published.store(new const names::slots(current), std::memory_order_release);
					initialized = true;
				}
				std::string status = writable ? (names_hook_ready ? "ready" : "hook_error") : "load_error";
				names::slots next;
				bool valid = true;
				for (size_t i = 0; i < 4; ++i)
				{
					std::string raw;
					if (!names_read_dvar(names_dvars[i], raw)) { valid = false; break; }
					next[i] = names::sanitize(raw);
					if (next[i] != raw) { names_set_dvar(names_dvars[i], next[i]); }
				}
				if (!valid) { status = "dvar_error"; }
				else
				{
					if (next != current)
					{
						const auto* snapshot = new const names::slots(next);
						current = next;
						names_published.store(snapshot, std::memory_order_release);
					}
					if (writable && current != saved)
					{
						if (names_save(current)) { saved = current; }
						else { status = "save_error"; }
					}
				}
				if (status != last_status)
				{
					names_set_dvar("splitscreen_names_status", status);
					if (status != "ready") { note("[splitscreen] names: %s\n", status.c_str()); }
					last_status = status;
				}
			}
			catch (...)
			{
				disabled = true;
				note("[splitscreen] names: unexpected failure - polling disabled\n");
			}
		}