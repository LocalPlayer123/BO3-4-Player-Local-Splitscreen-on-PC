// Controllers: rumble on pick, and each pad's real model (from Steam Input) in the split screen menu.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Rumble the controller a player picks in Options > Controls > Split Screen ----
		// With Steam Input every pad reaches the game as an XInput pad, so the menu lists them
		// all alike. Picking one now makes it vibrate briefly, so players see whose pad is whose.
		// The vibration goes straight to XInput (the game's own rumble setting does not apply);
		// Steam forwards it to the real pad (PlayStation, Xbox, Switch). Only the menu's pick
		// (the Lua binding's call, pad_assign_lua_return_rva) rumbles, not the engine's own
		// automatic assignment at startup or hot-plug.
		struct xinput_vibration
		{
			uint16_t left;
			uint16_t right;
		};
		using xinput_set_state_fn = DWORD (WINAPI*)(DWORD, xinput_vibration*);
		xinput_set_state_fn xinput_set_state = nullptr;
		bool xinput_tried = false;

		void load_xinput()
		{
			if (xinput_tried)
			{
				return;
			}
			xinput_tried = true;
			for (const auto* name : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"})
			{
				if (auto* module = LoadLibraryA(name))
				{
					if (auto* fn = GetProcAddress(module, "XInputSetState"))
					{
						xinput_set_state = reinterpret_cast<xinput_set_state_fn>(fn);
						return;
					}
				}
			}
		}

		DWORD WINAPI rumble_thread(void* arg)
		{
			const auto index = static_cast<DWORD>(reinterpret_cast<uintptr_t>(arg));
			xinput_vibration on{pad_rumble_strength, pad_rumble_strength};
			xinput_set_state(index, &on);
			Sleep(pad_rumble_ms);
			xinput_vibration off{0, 0};
			xinput_set_state(index, &off);
			return 0;
		}

		void rumble_xinput_pad(const int index)
		{
			if (index < 0 || index > 3)
			{
				return;
			}
			load_xinput();
			if (!xinput_set_state)
			{
				return;
			}
			if (auto* thread = CreateThread(nullptr, 0, &rumble_thread,
			                                reinterpret_cast<void*>(static_cast<uintptr_t>(index)), 0, nullptr))
			{
				CloseHandle(thread);
			}
		}

		// The engine's device slot `port` (0..7) -> its XInput user index: the device table's
		// first dword where it is 0..3, else the slot itself for 0..3.
		int xinput_index_for_port(const int port)
		{
			if (port < 0 || port > 7)
			{
				return -1;
			}
			const auto v = *reinterpret_cast<const int32_t*>(base() + pad_device_table_rva
			                                                 + static_cast<size_t>(port) * pad_device_stride);
			if (v >= 0 && v <= 3)
			{
				return v;
			}
			return port <= 3 ? port : -1;
		}

		// Entry of the engine's "give controller `ecx` the pad in device slot `edx`".
		void pad_assign_hook(midhook::context& c)
		{
			const auto ret = *reinterpret_cast<const uint64_t*>(midhook::site_rsp(c));
			if (ret != base() + pad_assign_lua_return_rva)
			{
				return;   // the engine's own assignment (startup, hot-plug): no rumble
			}
			const auto controller = static_cast<int>(c.rcx);
			const auto port = static_cast<int>(c.rdx);
			const auto index = xinput_index_for_port(port);
			note("[controllers] menu: controller %d takes device slot %d (XInput %d) - rumble\n", controller, port,
			     index);
			rumble_xinput_pad(index);
		}

		void install_pad_rumble()
		{
			if (!midhook::install(base() + pad_assign_rva, pad_assign_bytes, sizeof(pad_assign_bytes),
			                      &pad_assign_hook, &allocate_near_module))
			{
				note("[controllers] rumble on pick: engine bytes differ - not installed\n");
				return;
			}
			note("[controllers] rumble on pick: installed\n");
		}

		// ---- What Steam says each XInput slot really is ----
		// Steam Input emulates an XInput pad for every controller; the game's own steam_api64.dll
		// (2015-16) has no ISteamInput, and ezz BOIII answers Steam calls itself. This asks the
		// real Steam client directly (steamclient64.dll, ISteamClient SteamClient020 ->
		// ISteamInput SteamInput001), maps each emulated XInput slot to its controller
		// (GetControllerForGamepadIndex) and logs the controller type. If Steam answers, the
		// split screen menu can name the pads; if not, nothing else depends on it.
		// Steam's type per XInput slot (0 = none or unknown), refreshed by the probe thread.
		volatile int32_t steam_pad_type[4] = {};

		const char* steam_input_type_name(const int type)
		{
			switch (type)
			{
			case 1: return "Steam Controller";
			case 2: return "Xbox 360";
			case 3: return "Xbox One/Series";
			case 4: return "generic gamepad";
			case 5: return "PS4";
			case 8: return "Switch Joy-Con pair";
			case 9: return "Switch Joy-Con (single)";
			case 10: return "Switch Pro";
			case 12: return "PS3";
			case 13: return "PS5";
			case 14: return "Steam Deck";
			case 16: return "Switch 2 Pro";
			default: return "unknown";
			}
		}

		template <typename Fn>
		Fn vfunc(void* object, const size_t index)
		{
			return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[index]);
		}

		void* steam_input = nullptr;

		// Steam's calls run behind an SEH guard: a wrong interface must not take the game down.
		bool steam_input_connect()
		{
			if (steam_input)
			{
				return true;
			}
			auto* client_module = GetModuleHandleA("steamclient64.dll");
			if (!client_module)
			{
				note("[controllers] steam probe: steamclient64.dll is not loaded\n");
				return false;
			}
			using create_interface_fn = void* (*)(const char*, int*);
			const auto create = reinterpret_cast<create_interface_fn>(GetProcAddress(client_module, "CreateInterface"));
			if (!create)
			{
				note("[controllers] steam probe: no CreateInterface\n");
				return false;
			}
			void* client = create(steam_client_version, nullptr);
			if (!client)
			{
				note("[controllers] steam probe: %s not available\n", steam_client_version);
				return false;
			}
			const auto pipe = vfunc<int32_t (*)(void*)>(client, steam_client_create_pipe)(client);
			const auto user = pipe ? vfunc<int32_t (*)(void*, int32_t)>(client, steam_client_connect_user)(client, pipe) : 0;
			if (!pipe || !user)
			{
				note("[controllers] steam probe: pipe %d user %d - Steam not reachable\n", pipe, user);
				return false;
			}
			void* input = vfunc<void* (*)(void*, int32_t, int32_t, const char*)>(client, steam_client_generic_interface)(
				client, user, pipe, steam_input_version);
			if (!input)
			{
				note("[controllers] steam probe: %s not available\n", steam_input_version);
				return false;
			}
			const bool ok = vfunc<bool (*)(void*)>(input, steam_input_init)(input);
			note("[controllers] steam probe: %s Init %d\n", steam_input_version, ok);
			steam_input = input;
			return true;
		}

		void steam_input_report()
		{
			if (!steam_input_connect())
			{
				return;
			}
			vfunc<void (*)(void*)>(steam_input, steam_input_run_frame)(steam_input);
			for (int slot = 0; slot < 4; ++slot)
			{
				const auto handle = vfunc<uint64_t (*)(void*, int)>(steam_input, steam_input_controller_for_gamepad)(
					steam_input, slot);
				const int type = handle
					                 ? vfunc<int (*)(void*, uint64_t)>(steam_input, steam_input_type_for_handle)(
						                 steam_input, handle)
					                 : 0;
				if (steam_pad_type[slot] != type)
				{
					note("[controllers] steam: XInput %d -> handle %llX type %d (%s)\n", slot,
					     static_cast<unsigned long long>(handle), type, handle ? steam_input_type_name(type) : "none");
				}
				steam_pad_type[slot] = type;
			}
		}

		bool steam_input_report_guarded()
		{
#ifdef _MSC_VER
			__try
			{
				steam_input_report();
				return true;
			}
			__except (1)
			{
				return false;
			}
#else
			steam_input_report();
			return true;
#endif
		}

		DWORD WINAPI steam_probe_thread(void*)
		{
			// Late enough that ezz has loaded Steam and the pads are enumerated; then every few
			// seconds, so pads plugged in or swapped later are named right.
			Sleep(15000);
			for (;;)
			{
				if (!steam_input_report_guarded())
				{
					note("[controllers] steam: exception inside Steam's interface - names off\n");
					for (auto& t : steam_pad_type)
					{
						t = 0;
					}
					return 0;
				}
				Sleep(5000);
			}
		}

		void install_steam_probe()
		{
			if (auto* thread = CreateThread(nullptr, 0, &steam_probe_thread, nullptr, 0, nullptr))
			{
				CloseHandle(thread);
			}
		}

		// Behind Engine.GamepadType's call: arguments 8..11 ask for XInput slot 0..3's Steam type.
		// The answer is 100 + type, so a stock answer (0..3) can never be mistaken for one.
		void gamepad_type_binding_hook(midhook::context& c)
		{
			const auto arg = static_cast<int32_t>(c.rdi);
			if (arg < pad_type_query_base || arg > pad_type_query_base + 3)
			{
				return;
			}
			const auto type = steam_pad_type[arg - pad_type_query_base];
			if (type > 0)
			{
				c.rax = static_cast<uint32_t>(pad_type_answer_base + type);
			}
		}

		void install_pad_names()
		{
			if (!midhook::install(base() + gamepad_type_binding_rva, gamepad_type_binding_bytes,
			                      sizeof(gamepad_type_binding_bytes), &gamepad_type_binding_hook, &allocate_near_module))
			{
				note("[controllers] pad names: engine bytes differ - not installed\n");
				return;
			}
			install_steam_probe();
			note("[controllers] pad names: installed\n");
		}
