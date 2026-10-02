// Lobby: guests into the real lobby, deactivate/leave, SwapClients, player 3 and 4 input.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// --- Put the native guest into the real lobby -----------------------
		// Repeating the plural LobbyHost_AddLocalClients late is unsafe (it bumps
		// session+0xF0 again for an existing XUID), so the single-client
		// LobbyHost_AddLocalClient(actionId, ci, type) is used (PS4 0xCA5C70).
		// actionId 0 only tags the UI result.
		constexpr int game_lobby_type = 1;

		template <size_t N>
		bool engine_bytes_match(const uint32_t rva, const uint8_t (&expected)[N])
		{
			const auto* p = reinterpret_cast<const void*>(base() + rva);
			return readable(p, N) && std::memcmp(p, expected, N) == 0;
		}

		// LiveUser_GetXuid is safe for controllers 2/3 when it is the engine's code,
		// or when a host client replaced it and splitscreen_ezz.hpp restored it for 2/3.
		bool live_user_get_xuid_callable()
		{
			return engine_bytes_match(live_user_get_xuid_rva, live_user_get_xuid_bytes)
				|| (ezz::get_xuid_chained() && live_user_get_xuid_rva == ezz::get_xuid_rva);
		}

		bool lobby_enrollment_api_matches()
		{
			return engine_bytes_match(lobby_host_add_local_rva, lobby_host_add_local_bytes)
				&& engine_bytes_match(lobby_get_session_rva, lobby_get_session_bytes)
				&& engine_bytes_match(lobby_get_client_by_xuid_rva, lobby_get_client_bytes)
				&& live_user_get_xuid_callable();
		}

		// LobbyBase_GetNetworkMode (PS4 0xCBFB40): 0 LOCAL, 1 LAN, 2 LIVE
		// (PS4 LobbyTypes_GetLobbyNetworkModeName 0xCC55B0).
		constexpr int lobby_network_local = 0;
		constexpr int lobby_network_lan = 1;

		// Join order: every seated controller below `below` (the host always) must
		// already be in the game lobby. A guest added before the host gets joinOrder 0
		// and is never acked by the launch pump (PS4 HasAllClientsGotLatestStateMsg
		// 0xCAF3B0), so START GAME hangs on a black screen.
		bool host_and_guest_in_game_lobby(void* session, const int below = 2)
		{
			using get_xuid_fn = uint64_t (*)(int);
			using get_client_fn = void* (*)(void*, uint64_t);
			const auto get_xuid = reinterpret_cast<get_xuid_fn>(base() + live_user_get_xuid_rva);
			const auto get_client = reinterpret_cast<get_client_fn>(
				base() + lobby_get_client_by_xuid_rva);
			for (int controller = 0; controller < below; ++controller)
			{
				if (controller >= 1 && !controller_seated(controller))
				{
					continue;
				}
				const auto xuid = get_xuid(controller);
				if (!xuid || !get_client(session, xuid))
				{
					return false;
				}
			}
			return true;
		}

		// Player 3 may be seated only in an offline (LOCAL/LAN) lobby whose game
		// lobby is up (session+0x40 != 0) and holds the host and the native guest.
		// Unverified bytes answer "no".
		bool offline_lobby_ready_for_player3()
		{
			if (!engine_bytes_match(lobby_get_network_mode_rva, lobby_get_network_mode_bytes)
				|| !lobby_enrollment_api_matches())
			{
				return false;
			}

			const auto network_mode = reinterpret_cast<int (*)()>(
				base() + lobby_get_network_mode_rva)();
			if (network_mode != lobby_network_local && network_mode != lobby_network_lan)
			{
				return false;
			}

			auto* session = reinterpret_cast<void* (*)(int)>(
				base() + lobby_get_session_rva)(game_lobby_type);
			return session
				&& *reinterpret_cast<const uint32_t*>(
					reinterpret_cast<size_t>(session) + 0x40) != 0
				&& host_and_guest_in_game_lobby(session);
		}

		bool profile_publish_api_matches()
		{
			return engine_bytes_match(lobby_get_session_rva, lobby_get_session_bytes)
				&& engine_bytes_match(lobby_get_client_by_xuid_rva, lobby_get_client_bytes)
				&& live_user_get_xuid_callable()
				&& engine_bytes_match(mutable_client_info_rva, mutable_client_info_bytes)
				&& engine_bytes_match(lobby_update_client_rva, lobby_update_client_bytes);
		}

		bool lobby_enrollment_in_progress = false;
		bool guest2_lobby_enrolled = false;
		uint32_t lobby_enrollment_attempts = 0;
		constexpr uint32_t lobby_enrollment_max_attempts = 16;

		// Per-guest lobby state for controller 3; controller 2 keeps its own globals.
		struct guest_lobby_state
		{
			bool join_done = false;
			bool enrolled = false;
			uint32_t enroll_attempts = 0;
			bool published = false;
		};
		guest_lobby_state guest3_lobby{};

		bool ensure_guest_game_lobby(int controller, bool join_done, bool& enrolled,
		                             uint32_t& attempts);

		bool ensure_guest2_game_lobby()
		{
			return ensure_guest_game_lobby(2, guest_join_done, guest2_lobby_enrolled,
			                               lobby_enrollment_attempts);
		}

		bool ensure_guest_game_lobby(const int controller, const bool join_done, bool& enrolled,
		                             uint32_t& attempts)
		{
			if (enrolled)
			{
				return true;
			}
			if (!join_done || lobby_enrollment_in_progress
				|| attempts >= lobby_enrollment_max_attempts)
			{
				return false;
			}
			if (!lobby_enrollment_api_matches())
			{
				return false;
			}

			using get_session_fn = void* (*)(int);
			using get_xuid_fn = uint64_t (*)(int);
			using get_client_fn = void* (*)(void*, uint64_t);
			using add_local_fn = void (*)(int, int, int);

			const auto get_session = reinterpret_cast<get_session_fn>(
				base() + lobby_get_session_rva);
			const auto get_xuid = reinterpret_cast<get_xuid_fn>(
				base() + live_user_get_xuid_rva);
			const auto get_client = reinterpret_cast<get_client_fn>(
				base() + lobby_get_client_by_xuid_rva);
			const auto add_local = reinterpret_cast<add_local_fn>(
				base() + lobby_host_add_local_rva);

			const auto xuid = get_xuid(controller);
			auto* session = get_session(game_lobby_type);
			// LobbyHost_AddLocalClient rejects session+0x40 == 0: check first, retry later.
			if (!xuid || !session
				|| *reinterpret_cast<const uint32_t*>(
					reinterpret_cast<size_t>(session) + 0x40) == 0)
			{
				return false;
			}

			if (get_client(session, xuid))
			{
				enrolled = true;
				return true;
			}

			// Never ahead of the host and native guest (join order); waiting costs no attempt.
			if (!host_and_guest_in_game_lobby(session, controller))
			{
				return false;
			}

			const in_progress_guard guard(lobby_enrollment_in_progress);
			++attempts;
			add_local(0, controller, game_lobby_type);

			enrolled = get_client(session, xuid) != nullptr;
			return enrolled;
		}

		bool guest2_profile_published = false;
		bool profile_publish_in_progress = false;

		bool refresh_guest_lobby_profile(int controller, bool enrolled, bool& published);

		bool refresh_guest2_lobby_profile()
		{
			return refresh_guest_lobby_profile(2, guest2_lobby_enrolled, guest2_profile_published);
		}

		bool refresh_guest_lobby_profile(const int controller, const bool enrolled, bool& published)
		{
			if (published)
			{
				return true;
			}
			if (!enrolled || profile_publish_in_progress)
			{
				return false;
			}
			// Throttled: until the CAC is ready this is reached from several per-frame
			// paths (Storage_Pump for every controller, both guests' update, the lobby
			// add) - up to six GetMutableClientInfo calls a frame, with no end if the
			// gum IDs stay zero. One try per 250 ms is enough to catch readiness.
			static uint64_t next_try[4]{};
			const auto now = GetTickCount64();
			if (now < next_try[controller & 3])
			{
				return false;
			}
			next_try[controller & 3] = now + 250;
			if (!profile_publish_api_matches())
			{
				return false;
			}

			using mutable_info_fn = void (*)(int, void*);
			using get_session_fn = void* (*)(int);
			using get_xuid_fn = uint64_t (*)(int);
			using get_client_fn = void* (*)(void*, uint64_t);
			using update_client_fn = bool (*)(void*, uint64_t, const void*, bool*);

			const in_progress_guard guard(profile_publish_in_progress);
			alignas(16) std::array<uint8_t, 0x410> info{};
			reinterpret_cast<mutable_info_fn>(base() + mutable_client_info_rva)(
				controller, info.data());

			// GetMutableClientInfo puts the five equipped gum IDs at +0x20..+0x24. All
			// zero: CAC/storage not ready, retry later instead of copying another player's.
			bool gums_ready = false;
			for (size_t i = 0x20; i < 0x25; ++i)
			{
				gums_ready = gums_ready || info[i] != 0;
			}
			if (!gums_ready)
			{
				return false;
			}

			const auto get_session = reinterpret_cast<get_session_fn>(
				base() + lobby_get_session_rva);
			const auto get_xuid = reinterpret_cast<get_xuid_fn>(
				base() + live_user_get_xuid_rva);
			const auto get_client = reinterpret_cast<get_client_fn>(
				base() + lobby_get_client_by_xuid_rva);
			const auto update_client = reinterpret_cast<update_client_fn>(
				base() + lobby_update_client_rva);

			const auto xuid = get_xuid(controller);
			if (!xuid)
			{
				return false;
			}

			bool updated_any = false;
			for (int lobby_type = 0; lobby_type < 2; ++lobby_type)
			{
				auto* session = get_session(lobby_type);
				if (!session || !get_client(session, xuid))
				{
					continue;
				}
				bool changed = false;
				updated_any = update_client(session, xuid, info.data(), &changed)
				              || updated_any;
			}

			if (updated_any)
			{
				published = true;
			}
			return published;
		}

		// Player 4: controller 3 joins through the stock Lua (A); the lobby entry and
		// profile publish are then done here, as for controller 2.
		void guest3_input_frame();

		void advance_guest3_join()
		{
			guest3_input_frame();
			auto& g = guest3_lobby;
			if (g.join_done
				&& ensure_guest_game_lobby(3, g.join_done, g.enrolled, g.enroll_attempts))
			{
				refresh_guest_lobby_profile(3, g.enrolled, g.published);
			}
		}

		// Defined further down: player 3's own controller (A joins, B leaves).
		void guest2_input_frame();

		// Every frame (per_controller_update_stub, controller 2).
		void advance_guest2_join()
		{
			// Until done: the host alone in an offline lobby is enough to activate pads.
			activate_gamepads_in_lobby();
			guest2_input_frame();
			if (guest_join_done && ensure_guest2_game_lobby())
			{
				refresh_guest2_lobby_profile();
			}
		}

		// Hooked only to sign in as early as possible; a guest the loop misses is
		// added later by the single-client path above.
		utils::hook::detour lobby_add_all_hook;

		bool lobby_add_all_stub(const int lobby_type)
		{
			// Relocate the gamepad table as soon as player 2 is seated.
			if (lobby_type == game_lobby_type
				&& (seat_flags(0) & 1) && (seat_flags(1) & 1))
			{
				activate_gamepads_in_lobby();
			}

			const auto result = lobby_add_all_hook.invoke<bool>(lobby_type);

			if (lobby_type == game_lobby_type
				&& (seat_flags(0) & 1) && (seat_flags(1) & 1))
			{
				activate_gamepads_in_lobby();
			}

			if (guest_join_done && ensure_guest2_game_lobby())
			{
				refresh_guest2_lobby_profile();
			}
			return result;
		}

		// --- Deactivate splitscreen / guest leave --------------------------
		// PS4 Lua_CoD_LuaCall_SetLocalClientToInactive 0xCF8070, per controller:
		//   if LobbyClient_IsActive(GAME)  LobbyVM_OnLocalClientLeave(ci)
		//   elif LobbyHost_IsHost(PARTY)   LobbyHost_RemoveClient(PARTY, xuid, reason)
		//   Live_HandleClientSplitscreenSignin(ci, false, false)   always
		// Lua decides which controllers leave, and the stock LobbySplitscreenToggle
		// only touches controller 1. Now each local player leaves on his own, as on
		// console: ui_scripts/zz_splitscreen removes every extra player, the LobbyVM
		// leave below repairs a refused leave, and guest2_input_frame handles player
		// 3's own controller. The sign-in function is detoured (guest_signin_stub) to
		// follow every splitscreen sign-in/out.
		// History: LOG.md, DEACTIVATE SPLITSCREEN
		constexpr char local_client_left_reason[] = "Local Client Left.";
		utils::hook::detour lobbyvm_local_leave_hook;
		utils::hook::detour guest_signin_hook;

		// Player 3 is gone: the next ACTIVATE must seat and enrol him afresh.
		void reset_guest2_join()
		{
			guest_join_done = false;
			guest_join_attempts = 0;
			guest2_lobby_enrolled = false;
			lobby_enrollment_attempts = 0;
			guest2_profile_published = false;
		}

		// Take a component-seated controller out of every lobby we host. Only when
		// no lobby lists him may he be signed out: a seat without a lobby entry, or
		// the reverse, is the "Failed to host lobby" state.
		bool remove_guest_from_lobbies(const int controller)
		{
			const auto* reason = reinterpret_cast<const char*>(
				base() + local_client_left_reason_rva);
			if (!lobby_enrollment_api_matches()
				|| !engine_bytes_match(lobby_get_network_mode_rva, lobby_get_network_mode_bytes)
				|| !engine_bytes_match(lobby_host_is_host_rva, lobby_host_is_host_bytes)
				|| !engine_bytes_match(lobby_host_remove_client_rva,
				                       lobby_host_remove_client_bytes)
				|| !readable(reason, sizeof(local_client_left_reason))
				|| std::memcmp(reason, local_client_left_reason,
				               sizeof(local_client_left_reason)) != 0)
			{
				note("[splitscreen] guest %d leave: lobby bytes mismatch - not removed from lobbies\n",
				     controller);
				return false;
			}

			const auto network_mode = reinterpret_cast<int (*)()>(
				base() + lobby_get_network_mode_rva)();
			const auto xuid = reinterpret_cast<uint64_t (*)(int)>(
				base() + live_user_get_xuid_rva)(controller);
			if ((network_mode != lobby_network_local && network_mode != lobby_network_lan)
				|| !xuid)
			{
				return false;
			}

			using get_session_fn = void* (*)(int);
			using get_client_fn = void* (*)(void*, uint64_t);
			using is_host_fn = bool (*)(int);
			using remove_fn = bool (*)(int, uint64_t, const char*);
			const auto get_session = reinterpret_cast<get_session_fn>(
				base() + lobby_get_session_rva);
			const auto get_client = reinterpret_cast<get_client_fn>(
				base() + lobby_get_client_by_xuid_rva);
			const auto is_host = reinterpret_cast<is_host_fn>(
				base() + lobby_host_is_host_rva);
			const auto remove = reinterpret_cast<remove_fn>(
				base() + lobby_host_remove_client_rva);

			// Game lobby first, then the party; lobbies that do not list him are skipped.
			bool failed = false;
			for (const int lobby_type : {game_lobby_type, 0})
			{
				auto* session = get_session(lobby_type);
				const bool member = session && get_client(session, xuid);
				if (!member)
				{
					continue;
				}
				if (!is_host(lobby_type))
				{
					failed = true;
					continue;
				}
				const bool removed = remove(lobby_type, xuid, reason);
				failed = failed || !removed;
			}
			return !failed;
		}

		// If the LobbyVM refuses to let controller 2/3 leave, remove him from the
		// lobbies ourselves and let the caller sign him out.
		bool lobbyvm_local_leave_stub(const int controller, const uint64_t client_xuid)
		{
			const auto lua_result = lobbyvm_local_leave_hook.invoke<bool>(controller, client_xuid);
			if (controller < 2 || controller >= 4)
			{
				return lua_result;
			}

			const bool result = lua_result || remove_guest_from_lobbies(controller);
			if (result && controller == 2)
			{
				reset_guest2_join();
			}
			return result;
		}

		uint32_t seat_bits()
		{
			uint32_t bits = 0;
			for (uint32_t lc = 0; lc < signin_new_slots; ++lc)
			{
				bits |= static_cast<uint32_t>(seat_flags(lc) & 1) << lc;
			}
			return bits;
		}

		void guest_signin_stub(const int controller, const bool signin, const bool arg3)
		{
			const auto before = seat_bits();
			guest_signin_hook.invoke<void>(controller, signin, arg3);
			const auto after = seat_bits();

			// Controller 2 signed out by any path: the next A press starts afresh.
			if (!signin && controller == 2 && !controller_seated(2))
			{
				reset_guest2_join();
			}
			// Controller 2 seated by the stock join (Engine.SigninLocalClient reaches this
			// function): latch the same flag as our own join, so lobby entry and profile follow.
			if (signin && controller == 2 && controller_seated(2))
			{
				guest_join_done = true;
			}
			if (controller == 3)
			{
				if (signin && controller_seated(3))
				{
					guest3_lobby.join_done = true;
				}
				else if (!signin && !controller_seated(3))
				{
					guest3_lobby = {};
				}
			}

			// Run CL_Init(2) as soon as seat 2 is real (PS4 Com_Init inits lc 0..3 at
			// boot, the PC only 0..1). Waiting for three seats failed when player 3 joined
			// first: CompressClients (PS4 0xE35450) moved his uninitialised lc 2 into lc 1,
			// which then stuck at CA_CONFIRMLOADING.
			if (signin && (seat_flags(2) & 1) && raise_local_client_count && signin_relocated
				&& !lc2_fully_done())
			{
				run_cl_init_for_local_client2();
			}
			// Player 4: the same for local client 3 once seat record 3 is in use.
			if (signin && (seat_flags(3) & 1) && raise_local_client_count && signin_relocated)
			{
				run_cl_init_for_local_client3();
			}
			// Every added controller starts from player 1's classes and stats
			// (see reread_guest_saves) - only on a real seat add.
			if (signin && after != before && controller_seated(controller))
			{
				reread_guest_saves(controller);
			}
		}

		// ---- SwapClients (PS4 0xE356F0): slot-3 guard ----
		utils::hook::detour swap_clients_hook;

		void swap_clients_stub(const int a, const int b)
		{
			// clientUIActives[3] owns only its first 0x3F0 bytes; the rest overlaps live
			// client globals and cls, which cannot be moved (CLAUDE.md dead end). On PS4
			// everything past +0x18 is online host migration data and voice counters, so
			// a swap involving slot 3 keeps the foreign tail [+0x3F0, +0x1078) in place
			// on both sides.
			constexpr size_t window_off = 0x3F0, window_len = uia_stride - 0x3F0;
			const bool guard = (a == 3 || b == 3) && a >= 0 && b >= 0 && a <= 3 && b <= 3;
			static uint8_t keep_a[window_len], keep_b[window_len];
			auto* win_a = reinterpret_cast<uint8_t*>(base() + uia_base_rva + a * uia_stride + window_off);
			auto* win_b = reinterpret_cast<uint8_t*>(base() + uia_base_rva + b * uia_stride + window_off);
			if (guard)
			{
				std::memcpy(keep_a, win_a, window_len);
				std::memcpy(keep_b, win_b, window_len);
			}
			swap_clients_hook.invoke<void>(a, b);
			if (guard)
			{
				std::memcpy(win_a, keep_a, window_len);
				std::memcpy(win_b, keep_b, window_len);
			}
		}

		// --- Player 3's own controller: A joins, B leaves -----------------------
		// On PC the lobby menu only listens to controllers below
		// GetMaxLocalControllers() (2), so the component reads controller 2's buttons
		// itself, frontend only. Console runs the same join/leave from Lua.
		// Button bits: gamepad record +0x08; A = 0x100, B = 0x200.
		// Unplugging keeps the seat, as on console: CL_ControllerRemoved (PS4 0x415D80)
		// only raises the LUI event, and no stock Lua signs a player out on it (only
		// the two lobby button widgets listen, to refresh their label). The engine's
		// device assignment (0x022849F0) gives the next new pad to the lowest empty
		// slot - the waiting seat. Until 2.6.7 a guest left after 30 frames unplugged.
		constexpr size_t gamepad_buttons = 0x08;
		constexpr uint64_t guest2_join_request_ms = 3000;
		uint32_t guest2_prev_buttons = 0;
		bool guest2_join_requested = false;
		uint64_t guest2_join_request_tick = 0;
		bool guest2_leave_in_progress = false;

		uint32_t gamepad_buttons_of(const size_t slot)
		{
			return *reinterpret_cast<const volatile uint32_t*>(
				base() + gamepads_reserved_rva + slot * gamepad_stride + gamepad_buttons);
		}

		bool guest_listed_in_game_lobby(int controller);

		bool guest2_listed_in_game_lobby()
		{
			return guest_listed_in_game_lobby(2);
		}

		bool guest_listed_in_game_lobby(const int controller)
		{
			if (!lobby_enrollment_api_matches())
			{
				return false;
			}
			const auto xuid = reinterpret_cast<uint64_t (*)(int)>(
				base() + live_user_get_xuid_rva)(controller);
			auto* session = reinterpret_cast<void* (*)(int)>(
				base() + lobby_get_session_rva)(game_lobby_type);
			return xuid && session
				&& reinterpret_cast<void* (*)(void*, uint64_t)>(
					base() + lobby_get_client_by_xuid_rva)(session, xuid);
		}

		// Leave every lobby we host, then sign out through the hooked sign-in function.
		void guest_leave(int controller);

		void guest2_leave()
		{
			guest_leave(2);
		}

		void guest_leave(const int controller)
		{
			if (guest2_leave_in_progress)
			{
				return;
			}
			const in_progress_guard guard(guest2_leave_in_progress);
			if (remove_guest_from_lobbies(controller))
			{
				if (controller_seated(controller))
				{
					reinterpret_cast<void (*)(int, bool, bool)>(base() + guest_signin_rva)(
						controller, false, false);
				}
				const bool out = !controller_seated(controller);
				if (out)
				{
					if (controller == 2)
					{
						reset_guest2_join();
					}
					else
					{
						guest3_lobby = {};
					}
				}
			}
		}

		void guest2_input_frame()
		{
			if (!gamepads_activated)
			{
				return;
			}
			const bool connected = gamepad_connected(2);
			const uint32_t buttons = connected ? gamepad_buttons_of(2) : 0;
			const uint32_t pressed = buttons & ~guest2_prev_buttons;
			guest2_prev_buttons = buttons;

			if (!game::Com_IsRunningUILevel())
			{
				guest2_join_requested = false;
				return;
			}

			if (!controller_seated(2))
			{
				// Seat lost but still listed: finish the leave (no member without a seat).
				if (guest2_lobby_enrolled && guest2_listed_in_game_lobby())
				{
					guest2_leave();
					return;
				}
				// Once controller 2's ButtonBits models exist (widen_gamepad_button_models),
				// the stock Lua handles A and B as on console; only the cleanup here remains.
				if (gamepad_models_widened)
				{
					return;
				}
				if (pressed & game_button_a)
				{
					guest2_join_requested = true;
					guest2_join_request_tick = GetTickCount64();
					guest_join_attempts = 0;
				}
				if (guest2_join_requested)
				{
					if (guest_join_done
						|| GetTickCount64() - guest2_join_request_tick > guest2_join_request_ms)
					{
						guest2_join_requested = false;
					}
					else
					{
						try_join_guest2();
					}
				}
				return;
			}

			guest2_join_requested = false;
			if ((pressed & game_button_b) && !gamepad_models_widened)
			{
				guest2_leave();
			}
		}

		// Player 4's controller: A and B go through the stock Lua. Only the cleanup is
		// done here: the seat is gone but the lobby still lists him.
		void guest3_input_frame()
		{
			if (!gamepads_activated || !game::Com_IsRunningUILevel())
			{
				return;
			}
			if (!controller_seated(3) && guest3_lobby.enrolled && guest_listed_in_game_lobby(3))
			{
				guest_leave(3);
			}
		}

		void cl_init_watch()
		{
			// Not cl_init2_done alone: the widens are deferred until the allocation is
			// real, so keep re-entering until both halves are done.
			if (lc2_fully_done() || !signin_relocated || !raise_local_client_count)
			{
				return;
			}

			uint32_t seats = 0;
			for (uint32_t lc = 0; lc < signin_new_slots; ++lc)
			{
				uint8_t flags = 0;
				std::memcpy(&flags,
				            reinterpret_cast<const void*>(
					            base() + signin_new_base + lc * signin_stride),
				            sizeof(flags));
				if (flags & 1)
				{
					++seats;
				}
			}

			if (seats >= 3)
			{
				run_cl_init_for_local_client2();
			}
		}

		// Trigger for CL_Init(2): CL_LocalClients_SetAllUsedActive. It sets the
		// active bit inline and is known to run, while the connect loop's
		// CL_LocalClient_SetActive call never runs before the crash. PS4
		// CL_SetupClientsForIngame calls it right before the per-client allocation
		// and the connect loop. History: LOG.md, SetAllUsedActive
		utils::hook::detour set_active_hook;

		void run_cl_init_for_local_client2();

		// ---- Count per match (A9; docs/SIGNIN_REDESIGN.md D3) ----
		// PS4 CL_SetupClientsForIngame (0x40B870: CompressClients, AssignUIContextsForInGame,
		// SetAllUsedActive) runs right before each match's CL_AllocatePerLocalClientMemory
		// (CL_ConnectFromLobby 0x4154DB -> 0x4155A0, LobbyLaunch_PreloadMap; SV_SpawnServer
		// only for a map not preloaded from the menus, i.e. devmap). Every match is sized for
		// the clients in use at its start, smaller or larger than the last. The PC has the
		// same sequence (0x0135CD10, an inline copy at 0x0134C5DB). set_active_stub latches
		// that count; the allocator's next count call consumes it (splitscreen_player_count_stub).
		uint32_t match_latch = 0;
		bool match_latch_pending = false;
		bool alloc_count_site_ok = false;   // the allocator's count call is stock (install)

		// SetAllUsedActive takes no arguments (PS4 0x1517020: for i in 0..3
		// SetActive(i, IsBeingUsed(i))). Run the engine's pass first, then ours.
		void set_active_stub()
		{
			set_active_hook.invoke<void>();

			// The clients this match starts with, by the predicate the engine pass just used.
			// Seat records 2/3 exist only once relocated.
			if (signin_relocated)
			{
				const auto used = reinterpret_cast<bool (*)(int)>(base() + is_being_used_rva);
				uint32_t players = 0;
				for (int lc = 0; lc < 4; ++lc)
				{
					players += used(lc) ? 1 : 0;
				}
				match_latch = players;
				match_latch_pending = players > 0;
			}

			// Seat check: without it the first call at boot ran CL_Init(2) for a client
			// that did not exist yet and killed startup. Three used seats (bit 0 of each
			// relocated seat record) mean local client 2 is real.
			if (lc2_fully_done() || !raise_local_client_count || !signin_relocated)
			{
				return;
			}

			uint32_t seats = 0;
			for (uint32_t lc = 0; lc < signin_new_slots; ++lc)
			{
				uint8_t flags = 0;
				std::memcpy(&flags,
				            reinterpret_cast<const void*>(
					            base() + signin_new_base + lc * signin_stride),
				            sizeof(flags));
				if (flags & 1)
				{
					++seats;
				}
			}

			if (seats >= 3)
			{
				run_cl_init_for_local_client2();
			}
		}
