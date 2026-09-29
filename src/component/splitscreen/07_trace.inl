// UI trace (diagnostic build only writes it).
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- UI trace (diagnostic) ----
		// Appends a line to %LOCALAPPDATA%\boiii\splitscreen_ui_trace.txt (tick, call site,
		// client states, root, stack) at both UI_CoD_Init call sites and the three
		// first_snapshot LUIScopedEvent sites. Cold paths only, plain Win32 file calls, no CRT
		// (see note()). UI_CoD_Init is detoured by BOIII: the thunk calls the original address
		// to keep that chain. CG_LUIHUDRestart (PS4 0x29AD90) is kept verified for a future HUD
		// restart. Note: s_rootData+0x84 is RootData.lastSystemUpdateTime, not an element handle.
		constexpr uint32_t ui_cod_init_rva = 0x01F1C890;
		constexpr uint32_t ui_cod_init_callsites[] = {
			0x01F25E5F,   // UI_CoD_RunFrame        (if !UI_IsInitialized)
			0x01F26888,   // UI_CoD_ShutdownAndInit (CL_InitUI, Com_InitUIAndCommonXAssets, devmap)
		};
		constexpr uint32_t lui_scoped_event_rva = 0x02685620;
		constexpr uint32_t first_snapshot_event_callsites[] = {
			0x00F7E9F6,   // CG_LUIHUDRestart
			0x01321058,   // CL_FirstSnapshot
			0x013CFB09,   // SCR_DrawScreenField (CL_CheckKeepDrawingConnectScreen, inlined)
		};
		constexpr uint32_t cg_lui_hud_restart_rva = 0x00F7E970;
		constexpr uint8_t cg_lui_hud_restart_prologue[] = {
			0x48, 0x8B, 0xC4,                            // mov rax, rsp
			0x57,                                        // push rdi
			0x48, 0x81, 0xEC, 0xA0, 0x00, 0x00, 0x00,    // sub rsp, 0xA0
		};
		constexpr uint32_t server_initial_players_connected_rva = 0x0A1C272A;
		constexpr uint32_t connection_state_rva = 0x05359BC8;       // clientUIActives[lc]+8, stride 0x1078
		bool ui_trace_installed = false;

		struct trace_line
		{
			char b[1536];
			size_t n = 0;

			void str(const char* s)
			{
				while (s && *s && n < sizeof(b) - 3)
				{
					b[n++] = *s++;
				}
			}

			void hex(uint64_t v)
			{
				char t[16];
				int k = 0;
				do
				{
					t[k++] = "0123456789ABCDEF"[v & 0xF];
					v >>= 4;
				}
				while (v && k < 16);
				str("0x");
				while (k > 0 && n < sizeof(b) - 3)
				{
					b[n++] = t[--k];
				}
			}

			void dec(uint64_t v)
			{
				char t[20];
				int k = 0;
				do
				{
					t[k++] = static_cast<char>('0' + v % 10);
					v /= 10;
				}
				while (v && k < 20);
				while (k > 0 && n < sizeof(b) - 3)
				{
					b[n++] = t[--k];
				}
			}
		};

		void trace_write(trace_line& l)
		{
			l.b[l.n++] = '\r';
			l.b[l.n++] = '\n';
			char path[MAX_PATH]{};
			const auto len = GetEnvironmentVariableA("LOCALAPPDATA", path, MAX_PATH);
			constexpr char leaf[] = "\\boiii\\splitscreen_ui_trace.txt";
			if (len == 0 || len + sizeof(leaf) > MAX_PATH)
			{
				return;
			}
			std::memcpy(path + len, leaf, sizeof(leaf));
			const auto file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
			                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				return;
			}
			DWORD written{};
			WriteFile(file, l.b, static_cast<DWORD>(l.n), &written, nullptr);
			CloseHandle(file);
		}

		void trace_text(const char* text)
		{
			trace_line l;
			l.str(text);
			trace_write(l);
		}

		void log_gamepad_slots(const char* what)
		{
			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" ");
			l.str(what);
			l.str(" slot device/connected:");
			for (size_t slot = 0; slot < 4; ++slot)
			{
				l.str(" ");
				l.dec(static_cast<uint64_t>(static_cast<uint32_t>(gamepad_device_of(slot))));
				l.str(gamepad_connected(slot) ? "/1" : "/0");
			}
			trace_write(l);
		}

		uint32_t connection_state(const int lc)
		{
			// [0]/[1] stock; [2] is the voice_comm-vacated block the component
			// already uses for clientUIActives[2] (run_cl_init_for_local_client2).
			return *reinterpret_cast<const volatile uint32_t*>(
				base() + connection_state_rva + static_cast<size_t>(lc) * 0x1078);
		}

		void trace_head(trace_line& l, const char* what, const size_t call_site)
		{
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" ");
			l.str(what);
			l.str(" site=");
			l.hex(call_site);
			l.str(" cl_max=");
			l.dec(*reinterpret_cast<const volatile uint32_t*>(base() + cl_max_local_clients_rva));
			l.str(" st=");
			for (int lc = 0; lc < 4; ++lc)   // lc 3: owned head of clientUIActives[3]
			{
				l.dec(connection_state(lc));
				l.str(lc < 3 ? "," : "");
			}
			l.str(" svIPC=");
			l.dec(*reinterpret_cast<const volatile uint8_t*>(base() + server_initial_players_connected_rva));
			l.str(" uiLevel=");
			l.dec(game::Com_IsRunningUILevel() ? 1 : 0);
		}

		void trace_stack(trace_line& l)
		{
			void* frames[20]{};
			const auto count = RtlCaptureStackBackTrace(1, 20, frames, nullptr);
			const auto b = base();
			l.str(" stack:");
			for (USHORT i = 0; i < count; ++i)
			{
				const auto a = reinterpret_cast<size_t>(frames[i]);
				l.str(" ");
				if (a >= b && a < b + 0x20000000)
				{
					l.hex(a - b);
				}
				else
				{
					l.str("x");
					l.hex(a);
				}
			}
		}

		size_t call_site_of(void* return_address)
		{
			return reinterpret_cast<size_t>(return_address) - base() - 5;
		}

		void* lui_event_trace_thunk(void* self, void* lua, const char* root, const char* event)
		{
			trace_line l;
			trace_head(l, "LUIEvent", call_site_of(_ReturnAddress()));
			l.str(" root=");
			l.str(root);
			l.str(" event=");
			l.str(event);
			trace_write(l);
			return reinterpret_cast<void* (*)(void*, void*, const char*, const char*)>(
				base() + lui_scoped_event_rva)(self, lua, root, event);
		}

		void ui_cod_init_trace_thunk(const bool frontend)
		{
			const auto site = call_site_of(_ReturnAddress());
			{
				trace_line l;
				trace_head(l, "UI_CoD_Init", site);
				l.str(" frontend=");
				l.dec(frontend ? 1 : 0);
				trace_stack(l);
				trace_write(l);
			}

			reinterpret_cast<void (*)(bool)>(base() + ui_cod_init_rva)(frontend);
		}

		bool call_site_targets(const uint32_t site, const uint32_t target)
		{
			const auto* p = reinterpret_cast<const uint8_t*>(base() + site);
			if (!readable(p, 5) || p[0] != 0xE8)
			{
				return false;
			}
			int32_t rel{};
			std::memcpy(&rel, p + 1, sizeof(rel));
			return site + 5 + static_cast<int64_t>(rel) == target;
		}

		void install_ui_trace()
		{
			if (ui_trace_installed)
			{
				return;
			}
			const auto b = base();
			if (std::memcmp(reinterpret_cast<const void*>(b + cg_lui_hud_restart_rva),
			                cg_lui_hud_restart_prologue, sizeof(cg_lui_hud_restart_prologue)) != 0)
			{
				return;
			}
			for (const auto site : ui_cod_init_callsites)
			{
				if (!call_site_targets(site, ui_cod_init_rva))
				{
					return;
				}
			}
			for (const auto site : first_snapshot_event_callsites)
			{
				if (!call_site_targets(site, lui_scoped_event_rva))
				{
					return;
				}
			}
			try
			{
				for (const auto site : first_snapshot_event_callsites)
				{
					utils::hook::call(b + site, lui_event_trace_thunk);
				}
				for (const auto site : ui_cod_init_callsites)
				{
					utils::hook::call(b + site, ui_cod_init_trace_thunk);
				}
			}
			catch (...)
			{
				return;
			}
			ui_trace_installed = true;
			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" ---- ui trace installed ----");
			trace_write(l);
		}
