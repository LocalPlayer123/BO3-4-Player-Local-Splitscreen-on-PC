// Update notice: once per start, ask GitHub whether a newer release of the mod exists.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// One HTTPS GET of the public repository's latest release, with a fixed
		// User-Agent: nothing about the player or the PC is sent and nothing is stored.
		// Off with -splitscreen_noupdate on the command line (boiii.exe passes its own).
		// The Nexus variant (release/build.ps1 -Nexus) compiles all of it out, see the end.
		// The request runs on its own thread and never touches the engine. A newer
		// version is handed to the menu Lua (ui_scripts/zz_splitscreen) through two dvars
		// and the LUI event "splitscreen_update", on the game thread:
		// per_controller_update_stub, controller 0, frontend only - it runs every frame
		// until the first split-screen sign-in, and the answer normally comes within a
		// second of the start. Cmd_ExecuteSingleCommand is already called by
		// exec_button_config; PC Cbuf_AddText is not used, it begins with a jump into
		// Arxan code. Both calls pass tools/caller_guard_audit.py.
		// PS4 `set` creates an unknown dvar as a string (Dvar_SetFromStringByNameFromSource
		// 0x10A53C0 -> _Dvar_RegisterString).

		// This build's version. release/build.ps1 refuses a package whose README says otherwise.
		constexpr char mod_version[] = "2.6.9";

#ifndef SS_NO_UPDATE_CHECK
		constexpr wchar_t update_host[] = L"api.github.com";
		constexpr wchar_t update_path[] = L"/repos/LocalPlayer123/BO3-4-Player-Local-Splitscreen-on-PC/releases/latest";
		constexpr wchar_t update_agent[] = L"BO3-Local-Splitscreen";
		constexpr char update_off_switch[] = "-splitscreen_noupdate";
		constexpr size_t update_body_max = 256 * 1024;

		enum update_state : int
		{
			update_pending = 0,
			update_newer = 1,   // update_latest holds the newer version
			update_none = 2,    // up to date, switched off, or no answer
		};
		std::atomic<int> update_result{update_pending};
		char update_latest[16]{};
		bool update_published = false;

		// "2.6.8" / "v2.6" -> {2, 6, 8} / {2, 6, 0}. 1-3 parts of 1-3 digits, nothing else.
		bool parse_version(const char* text, int (&out)[3])
		{
			out[0] = out[1] = out[2] = 0;
			if (*text == 'v' || *text == 'V')
			{
				++text;
			}
			int part = 0;
			int digits = 0;
			for (; *text; ++text)
			{
				if (*text >= '0' && *text <= '9')
				{
					if (++digits > 3)
					{
						return false;
					}
					out[part] = out[part] * 10 + (*text - '0');
				}
				else if (*text == '.' && digits && part < 2)
				{
					++part;
					digits = 0;
				}
				else
				{
					return false;
				}
			}
			return digits > 0;
		}

		bool version_newer(const int (&a)[3], const int (&b)[3])
		{
			for (int i = 0; i < 3; ++i)
			{
				if (a[i] != b[i])
				{
					return a[i] > b[i];
				}
			}
			return false;
		}

		// The value of "tag_name" in the release JSON, at most 15 characters.
		bool tag_name_of(const std::string& body, char (&out)[16])
		{
			const auto key = body.find("\"tag_name\"");
			if (key == std::string::npos)
			{
				return false;
			}
			auto at = body.find(':', key);
			if (at == std::string::npos)
			{
				return false;
			}
			at = body.find('"', at);
			if (at == std::string::npos)
			{
				return false;
			}
			const auto end = body.find('"', at + 1);
			if (end == std::string::npos || end - at - 1 == 0 || end - at - 1 >= sizeof(out))
			{
				return false;
			}
			std::memcpy(out, body.data() + at + 1, end - at - 1);
			out[end - at - 1] = 0;
			return true;
		}

		bool fetch_latest_release(std::string& body)
		{
			const auto session = WinHttpOpen(update_agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!session)
			{
				return false;
			}
			const auto close_session = utils::finally([&] { WinHttpCloseHandle(session); });
			WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

			const auto connection = WinHttpConnect(session, update_host, INTERNET_DEFAULT_HTTPS_PORT, 0);
			if (!connection)
			{
				return false;
			}
			const auto close_connection = utils::finally([&] { WinHttpCloseHandle(connection); });

			const auto request = WinHttpOpenRequest(connection, L"GET", update_path, nullptr,
			                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			                                        WINHTTP_FLAG_SECURE);
			if (!request)
			{
				return false;
			}
			const auto close_request = utils::finally([&] { WinHttpCloseHandle(request); });

			if (!WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1),
			                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
				|| !WinHttpReceiveResponse(request, nullptr))
			{
				return false;
			}

			DWORD status = 0;
			DWORD status_size = sizeof(status);
			if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			                         WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)
				|| status != 200)
			{
				return false;
			}

			char chunk[4096];
			DWORD read = 0;
			while (WinHttpReadData(request, chunk, sizeof(chunk), &read) && read)
			{
				if (body.size() + read > update_body_max)
				{
					return false;
				}
				body.append(chunk, read);
			}
			return !body.empty();
		}

		void run_update_check()
		{
			int current[3];
			int latest[3];
			std::string body;
			char tag[16]{};
			if (parse_version(mod_version, current) && fetch_latest_release(body) && tag_name_of(body, tag)
				&& parse_version(tag, latest) && version_newer(latest, current))
			{
				std::snprintf(update_latest, sizeof(update_latest), "%d.%d.%d", latest[0], latest[1], latest[2]);
				update_result.store(update_newer, std::memory_order_release);
				return;
			}
			update_result.store(update_none, std::memory_order_release);
		}

		void start_update_check()
		{
			const auto* command_line = GetCommandLineA();
			if (command_line && std::strstr(command_line, update_off_switch))
			{
				update_result.store(update_none);
				note("update check: off (%s)", update_off_switch);
				return;
			}
			std::thread(run_update_check).detach();
		}

		// The Lua shows the notice on "splitscreen_update" (and on its own timers, which
		// do not tick on the menu root). Raised every 2 s for the first minute in the
		// menus: the Lua can load after the answer, and it shows the notice only once.
		constexpr uint32_t update_event_repeats = 30;
		constexpr uint64_t update_event_interval_ms = 2000;
		uint32_t update_events_left = 0;
		uint64_t update_next_event = 0;

		// Game thread (per_controller_update_stub). Once: two `set` commands for the Lua,
		// then the event that tells it to look.
		void publish_update_notice(const int controller)
		{
			if (controller != 0 || update_result.load(std::memory_order_acquire) != update_newer
				|| !game::Com_IsRunningUILevel())
			{
				return;
			}
			if (!update_published)
			{
				update_published = true;
				if (!engine_bytes_match(cmd_execute_single_command_rva, cmd_execute_single_command_prologue))
				{
					note("update check: Cmd_ExecuteSingleCommand is not stock - notice skipped");
					return;
				}
				const auto exec = reinterpret_cast<void (*)(int, int, const char*, int)>(
					base() + cmd_execute_single_command_rva);
				char command[64];
				std::snprintf(command, sizeof(command), "set splitscreen_update_current %s", mod_version);
				exec(0, 0, command, 0);
				std::snprintf(command, sizeof(command), "set splitscreen_update_latest %s", update_latest);
				exec(0, 0, command, 0);
				note("update check: %s available (this build %s)", update_latest, mod_version);
				update_events_left = engine_bytes_match(live_raise_lui_event_rva, live_raise_lui_event_prologue)
					? update_event_repeats : 0;
			}
			const auto now = GetTickCount64();
			if (update_events_left && now >= update_next_event)
			{
				--update_events_left;
				update_next_event = now + update_event_interval_ms;
				reinterpret_cast<void (*)(int, const char*)>(base() + live_raise_lui_event_rva)(
					0, "splitscreen_update");
			}
		}
#else
		// Nexus variant (release/build.ps1 -Nexus): no network code at all - Nexus' file
		// submission guidelines prohibit files that connect to the internet unless crucial.
		void start_update_check()
		{
		}

		void publish_update_notice(int)
		{
		}
#endif
