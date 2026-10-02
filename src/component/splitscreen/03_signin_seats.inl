// Sign-in and seats: player count, seat records, CL_Init for local clients 2/3, gamepad activation, guest 2 join.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// splitscreen_playerCount. This slot holds the dvar pointer; the current int
		// is at +0x28. CL_SplitscreenPlayerCount (PS4 0x1516BE0) returns it, and
		// CL_AllocatePerLocalClientMemory allocates max(count, 2) of the whole
		// per-local-client family (clients, clientConnections, snapshots,
		// parseEntities, the 0x1E940 block).
		// Raise the dvar, not the allocator's floor of 2: patching only the floor made
		// the allocator disagree with every other reader and was the round-start crash.
		constexpr uint32_t dvar_current_offset = 0x28;

		bool active_count_installed = false;

		// The count must already be right in the lobby: PS4 CL_ConnectFromLobby
		// allocates (0x4155A0) before it activates the clients (0x41566C). Calling
		// CL_LocalClients_SetAllUsedActive (0x027C19C0) is not enough: its early-out
		// skips unless a client's active state changes, and local client 2 is
		// outside its loop bound of 2.
		bool set_splitscreen_player_count(const uint32_t value)
		{
			uint64_t dvar = 0;
			std::memcpy(&dvar,
			            reinterpret_cast<const void*>(base() + splitscreen_player_count_dvar_rva),
			            sizeof(dvar));
			if (dvar == 0)
			{
				return false;
			}
			// Write the value directly. Never call Dvar_SetInt from this DLL: its Arxan
			// return-address check loops forever for a caller outside the game image while
			// holding the dvar lock, and the main thread freezes (not on every launch).
			// History: LOG.md, "e085b71"
			auto* current = reinterpret_cast<uint32_t*>(dvar + dvar_current_offset);
			return readable(current, sizeof(uint32_t)) && write_bytes(current, &value, sizeof(value));
		}

		uint32_t last_active_refresh = 0;

		// Com_LocalClient_IsBeingUsed, the local client's controller index and
		// LiveUser_IsSignedIn: the conditions PS4 GetCountUsedAndSignedInLocalClients
		// (0xD2EE40) counts over lc 0..3. The PC bounds that loop at 2.

		uint32_t true_local_client_count()
		{
			const auto used = reinterpret_cast<bool (*)(int)>(base() + is_being_used_rva);
			const auto ctrl = reinterpret_cast<int (*)(int)>(base() + lc_controller_index_rva);
			const auto signed_in = reinterpret_cast<bool (*)(int)>(base() + live_user_is_signed_in_rva);

			uint32_t n = 0;
			for (int lc = 0; lc < 4; ++lc)
			{
				if (!used(lc))
				{
					continue;
				}
				const int ci = ctrl(lc);
				if (ci >= 0 && signed_in(ci))
				{
					++n;
				}
			}
			return n;
		}

		// Defined next to signin_relocated, which it reads.
		uint32_t seat_count();

		// The mod raises splitscreen_playerCount only for 3-4 players, which the PC
		// cannot count. 1-2 players are the engine's own business (stock
		// CL_LocalClient_SetActive sets 2 when player 2 activates). Raising it at 2
		// caught a seat the boot marks in use for a moment and left the count at 2 in
		// the main menu; the game's device assignment (0x022849F0: "count > 1 and
		// slot 1 empty -> the new device goes to controller 1") then gave a pad
		// plugged in by a lone player to player 2 (measured 2026-09-29).
		constexpr uint32_t first_raised_player_count = 3;

		// Hold splitscreen_playerCount at the real number of local clients.
		// PS4 CL_LocalClient_SetActive (0x15167D0) sets it from
		// CL_LocalClient_GetActiveCount (0x1516A20, i < 4). The PC unrolls that count
		// to two elements, so the engine alone never writes more than 2, and it writes
		// its value back on every activation toggle, so a one-shot set does not hold.
		// Only ever raised; lowering it would fight the engine when a player leaves.
		void hold_splitscreen_player_count()
		{
			if (!raise_local_client_count)
			{
				return;
			}

			uint64_t dvar = 0;
			std::memcpy(&dvar,
			            reinterpret_cast<const void*>(base() + splitscreen_player_count_dvar_rva),
			            sizeof(dvar));
			if (dvar == 0)
			{
				return; // not registered yet - CL_SplitscreenPlayerCount returns 1
			}

			auto* current = reinterpret_cast<uint32_t*>(dvar + dvar_current_offset);
			if (!readable(current, sizeof(uint32_t)))
			{
				return;
			}

			// Widen the dvar's domain too. It is registered as 1..4 but reads {1, 2} at
			// runtime, so the engine rejects its own Dvar_SetInt(3) ("not a valid value").
			// The domain is {min, max} at +0x88. Only max is raised (2 -> 4), so a stock
			// 2-player session is unaffected.
			constexpr size_t dvar_domain_offset = 0x88;
			auto* domain_max = reinterpret_cast<uint32_t*>(
				dvar + dvar_domain_offset + sizeof(uint32_t));
			if (readable(domain_max, sizeof(uint32_t)) && *domain_max == 2)
			{
				const uint32_t four = 4;
				write_bytes(domain_max, &four, sizeof(four));
			}

			// Take the larger of the predicate count and the seat count. In a round the
			// guests read as not signed in, so the predicate count alone drops to 1 and
			// the map-load reallocation shrank cl_maxLocalClients back to 2.
			const auto want = std::max(true_local_client_count(), seat_count());

			if (want >= first_raised_player_count && *current < want)
			{
				write_bytes(current, &want, sizeof(want));
			}
		}

		// splitscreen_playerCount is not a saved setting on console: PS4 CL_RegisterDvars
		// registers it with flags 0 (0x414191). The PC dvar carries DVAR_ARCHIVE (flags
		// at +0x18 read 0x1 live, 2026-10-01), so ezz writes it to
		// boiii_players/user/config.cfg and runs that file at the next start: a session
		// that ended with two players began the next one at 2 with one player signed in,
		// and the first pad plugged in became player 2 (see assign_player_count). Clear
		// the flag as on console; ezz's next config write drops the line. Checked on
		// every tick: a one-shot clear ran before the flag was set and never stuck
		// (measured 2026-10-01; an external clear then held).
		constexpr size_t dvar_flags_offset = 0x18;
		constexpr uint32_t dvar_archive = 1;

		void unarchive_player_count()
		{
			uint64_t dvar = 0;
			std::memcpy(&dvar,
			            reinterpret_cast<const void*>(base() + splitscreen_player_count_dvar_rva),
			            sizeof(dvar));
			if (dvar == 0)
			{
				return;
			}
			auto* flags = reinterpret_cast<uint32_t*>(dvar + dvar_flags_offset);
			if (!readable(flags, sizeof(uint32_t)))
			{
				return;
			}
			if (*flags & dvar_archive)
			{
				const uint32_t cleared = *flags & ~dvar_archive;
				write_bytes(flags, &cleared, sizeof(cleared));
			}
		}

		// Keeps splitscreen_playerCount at the real local-client count; every 50 ms
		// from mirror_signin_state. Set in the lobby: allocation happens before
		// activation, and per_controller_update_stub stops running after a
		// splitscreen sign-in. max_local >= 2 is the "game is up" test; touching the
		// dvar system during early startup black-screened the client.
		void sync_player_count()
		{
			unarchive_player_count();

			uint32_t max_local = 0;
			std::memcpy(&max_local,
			            reinterpret_cast<const void*>(base() + cl_max_local_clients_rva),
			            sizeof(max_local));
			if (active_count_installed && max_local >= 2)
			{
				const auto live = true_local_client_count();
				if (live >= first_raised_player_count && live != last_active_refresh
				    && set_splitscreen_player_count(live))
				{
					last_active_refresh = live;
				}
			}
			hold_splitscreen_player_count();
		}

		// userData_t (PS4 DWARF, dwMessaging.cpp). The PC record is packed differently
		// but keeps the field order; the live records show +0x28 = isActive and
		// +0x29 = isGuest. The guests were copied from the donor while it was still
		// signing in, so isActive stayed 0 and SigninLocalClient(2) never returned.
		// Mirror the donor's current signInState and isActive into guests 2/3.
		constexpr size_t userdata_is_active = 0x28;

		void mirror_signin_state()
		{
			// Before the guest check: the count is held with or without guests.
			sync_player_count();

			if (!guests_filled || !guest_array_rva)
			{
				return;
			}

			const auto array = base() + guest_array_rva;
			const auto donor = array + client_ui_stride;
			uint32_t state = 0;
			std::memcpy(&state, reinterpret_cast<const void*>(donor + userdata_signedin),
			            sizeof(state));
			if (!state)
			{
				return;
			}

			uint8_t active = 0;
			std::memcpy(&active, reinterpret_cast<const void*>(donor + userdata_is_active),
			            sizeof(active));

			for (size_t i = 2; i < 4; ++i)
			{
				const auto slot = array + i * client_ui_stride;

				uint32_t have = 0;
				std::memcpy(&have, reinterpret_cast<const void*>(slot + userdata_signedin),
				            sizeof(have));
				if (have != state)
				{
					std::memcpy(reinterpret_cast<void*>(slot + userdata_signedin), &state,
					            sizeof(state));
				}

				uint8_t have_active = 0;
				std::memcpy(&have_active,
				            reinterpret_cast<const void*>(slot + userdata_is_active),
				            sizeof(have_active));
				if (have_active != active)
				{
					std::memcpy(reinterpret_cast<void*>(slot + userdata_is_active), &active,
					            sizeof(active));
				}
			}
		}

		// Guest storage reads are filtered (storage_read_stub). Completing a guest's
		// settings read runs configs with localClient = -1 (PS4 SettingsReadResult
		// 0x6F5C60) and kills the renderer. The lobby's ready[ci] flag comes from the
		// stats records' callback instead, so guests may always read the stats files.
		// Refusing a read is a state the game already handles.
		//
		// Always allowed: the ten stats file types (table at RVA 0x0340D840) plus
		// type 20, the zombie loadout file the gobblegum icons are drawn from.
		constexpr uint32_t guest_allowed_file_types[] = {
			12, 13, 21, 22, 6, 7, 8, 9, 17, 18,   // stats records
			20,                                    // STORAGE_ZM_LOADOUTS_OFFLINE
		};

		// Allowed only for a guest with a local-client seat. These complete into
		// callbacks that exec configs with localClient =
		// Com_ControllerIndex_GetLocalClientNum(ci); without a seat that is -1 and
		// the process dies. Files 15 and 11 are offline loadout-reset records the
		// sign-in gate checks for ready[ci] (file 20, the third, is always allowed);
		// the online ones (14, 10, 19) stay blocked.
		constexpr uint32_t guest_seated_file_types[] = {
			3, 5,                                  // required by the sign-in gate
			15, 11,                                // mp/cp offline loadout resets
			0x1B, 0x1C,                            // also required by the sign-in gate
			0,                                     // user_settings, see storage_read_stub
			1,                                     // shoutcaster_settings, likewise
		};

		// SettingsReadResult (PS4 0x6F5C60), read callback of storage file 0
		// (user_settings). On success it runs Com_RunAutoExec, Com_RunUserConfig and
		// Settings_RunCallbacks; on failure Storage_Reset(ci, 0, 0),
		// Settings_ResetCommonVarsToDefault, SaveChanges and Settings_RunCallbacks.
		// For a guest only Storage_Reset runs: it leaves a valid default DDL context
		// (PS4 0xF7DAC0). The rest writes [2]-sized per-controller settings arrays,
		// execs configs and writes a guest profile, which wiped controller 2's storage
		// and blacked out the renderer.
		// The file still becomes ready: the storage completion (PS4 0xF7DF1A) marks it
		// ready before calling this callback and clears that only if it returns false.

		utils::hook::detour settings_read_result_hook;

		// File 0 is allowed only while this is true. If the hook is not installed,
		// the read filter refuses file 0 again. Fail safe.
		bool settings_result_neutered = false;

		// ---- Guest stick-click limits ----
		// PS4 GPad_UpdateDigitals (0xDB8130) drops L3/R3 while max(|stick.x|, |stick.y|)
		// exceeds the constant 1.0. The PC (0x02286030) takes both limits from the
		// controller's profile instead - gpad_button_lstick_deflect_max and
		// gpad_button_rstick_deflect_max (ProfileSetting 16/17) - once its user_settings
		// file is ready. A guest without user_settings_N.cgp gets only Storage_Reset
		// here, which zeroes the DDL buffer (PS4 DDL_Buffer_ResetContext memset): both
		// limits read 0.0 and any stick deflection drops the click, so players 3/4 could
		// not sprint (L3 while pushing forward) yet L3 worked in menus. SaveChanges
		// (PS4 0x6F59B0) then writes that profile to disk as initialized, so later reads
		// succeed with the zeros. The stock default path is not safe for a guest (see
		// above), so a guest whose limits read <= 0 starts from player 1's settings
		// buffer: a private buffer copy, same DDL def and length.
		constexpr const char* stick_click_limit_members[] = {
			"gpad_button_lstick_deflect_max", "gpad_button_rstick_deflect_max",
		};

		// DDL layouts (PS4 DWARF; on the PC confirmed by decoding live profiles,
		// 2026-10-02): DDLContext {buff +0, len +8, def +0x10}; DDLDef {structList
		// +0x20, headerBitSize +0x48}; DDLStruct {memberCount +0xC, members +0x10};
		// DDLMember (0x48 bytes) {name +0, bitSize +0x18, offset +0x20}. A root
		// member's value starts at bit headerBitSize + offset, least significant first.
		struct ddl_context_view
		{
			uint8_t* buff;
			int32_t len;
			int32_t pad;
			const uint8_t* def;
		};

		// Bit position of a 32-bit member of the root struct, or -1.
		int ddl_root_member_bit(const uint8_t* def, const char* name)
		{
			if (!def || !readable(def, 0x50))
			{
				return -1;
			}
			const auto header_bits = *reinterpret_cast<const int32_t*>(def + 0x48);
			const auto* root = *reinterpret_cast<const uint8_t* const*>(def + 0x20);
			if (!root || !readable(root, 0x18))
			{
				return -1;
			}
			const auto count = *reinterpret_cast<const int32_t*>(root + 0xC);
			const auto* members = *reinterpret_cast<const uint8_t* const*>(root + 0x10);
			if (count <= 0 || count > 4096 || !members
				|| !readable(members, static_cast<size_t>(count) * 0x48))
			{
				return -1;
			}
			const auto len = std::strlen(name) + 1;
			for (int i = 0; i < count; ++i)
			{
				const auto* m = members + static_cast<size_t>(i) * 0x48;
				const auto* n = *reinterpret_cast<const char* const*>(m);
				if (!n || !readable(n, len) || std::memcmp(n, name, len) != 0)
				{
					continue;
				}
				if (*reinterpret_cast<const int32_t*>(m + 0x18) != 32)
				{
					return -1;
				}
				return header_bits + *reinterpret_cast<const int32_t*>(m + 0x20);
			}
			return -1;
		}

		// The usable buffer of a context, or null.
		const ddl_context_view* ddl_view(const void* ctx)
		{
			const auto* v = static_cast<const ddl_context_view*>(ctx);
			if (!v || !readable(v, sizeof(*v)) || !v->buff || v->len <= 0 || v->len > 0x10000
				|| !readable(v->buff, static_cast<size_t>(v->len)))
			{
				return nullptr;
			}
			return v;
		}

		// 1 = a limit reads <= 0 (or NaN), 0 = both are positive, -1 = layout not found.
		int stick_click_limits_unset(const ddl_context_view& v)
		{
			for (const auto* name : stick_click_limit_members)
			{
				const auto bit = ddl_root_member_bit(v.def, name);
				const auto byte = static_cast<size_t>(bit) / 8;
				const size_t need = bit % 8 ? 5 : 4;
				if (bit < 0 || byte + need > static_cast<size_t>(v.len))
				{
					return -1;
				}
				uint64_t raw = 0;
				std::memcpy(&raw, v.buff + byte, need);
				const auto bits = static_cast<uint32_t>(raw >> (bit % 8));
				float value;
				std::memcpy(&value, &bits, sizeof(value));
				if (!(value > 0.0f))
				{
					return 1;
				}
			}
			return 0;
		}

		void repair_guest_stick_click_limits(const int controller)
		{
			const auto* fn = reinterpret_cast<const uint8_t*>(base() + storage_get_ddl_context_rva);
			if (!readable(fn, sizeof(storage_get_ddl_context_prologue))
				|| std::memcmp(fn, storage_get_ddl_context_prologue, sizeof(storage_get_ddl_context_prologue)) != 0)
			{
				note("stick limits: Storage_GetDDLContext 0x%08X is not stock - skipped",
				     storage_get_ddl_context_rva);
				return;
			}
			const auto get = reinterpret_cast<void* (*)(int, int, int)>(base() + storage_get_ddl_context_rva);
			const auto* guest = ddl_view(get(controller, 0, 0));
			if (!guest)
			{
				note("stick limits: controller %d has no settings context", controller);
				return;
			}
			const auto unset = stick_click_limits_unset(*guest);
			if (unset != 1)
			{
				note("stick limits: controller %d %s", controller, unset ? "layout not found" : "ok");
				return;
			}
			const auto* host = ddl_view(get(0, 0, 0));
			if (!host || host->def != guest->def || host->len != guest->len
				|| stick_click_limits_unset(*host) != 0)
			{
				note("stick limits: controller %d unset, player 1's settings unusable - left", controller);
				return;
			}
			std::memcpy(guest->buff, host->buff, static_cast<size_t>(host->len));
			note("stick limits: controller %d unset - copied player 1's settings (%d bytes)",
			     controller, host->len);
		}

		char settings_read_result_stub(const int controller, const int file_type, const int slot,
		                               const int result, void* ddl_context)
		{
			if (controller >= 2)
			{
				// Success means the guest's own .cgp loaded; failure means Storage_Reset
				// builds the context.
				if (result != 0)
				{
					const auto reset = reinterpret_cast<void (*)(int, int, int)>(
						base() + storage_reset_rva);
					reset(controller, 0, 0);
				}
				repair_guest_stick_click_limits(controller);

				// Return true like the original; false would reset the ready state to 0.
				return 1;
			}
			return settings_read_result_hook.invoke<char>(controller, file_type, slot, result,
			                                              ddl_context);
		}

		// File 1 (shoutcaster_settings), same fix. The sign-in predicate needs file
		// types 0, 1, 7, 9, 0xB, 0xD, 0xF, 0x12, 0x14, 0x1B and 0x1C.
		// ShoutcasterSettingsReadResult (PS4 0x6F7180, with ShoutcasterResetSettings
		// 0x6F71F0) does nothing on success. On failure it runs Storage_Reset(ci, 1, 0),
		// execs default_shoutcaster_settings.cfg and saves; guests keep only the reset.
		// 0x40 is a redundant REX prefix on push rbx.

		utils::hook::detour shoutcaster_read_result_hook;
		bool shoutcaster_result_neutered = false;

		char shoutcaster_read_result_stub(const int controller, const int file_type, const int slot,
		                                  const int result, void* ddl_context)
		{
			if (controller >= 2)
			{
				if (result != 0)
				{
					const auto reset = reinterpret_cast<void (*)(int, int, int)>(
						base() + storage_reset_rva);
					reset(controller, 1, 0);
				}

				return 1;
			}
			return shoutcaster_read_result_hook.invoke<char>(controller, file_type, slot, result,
			                                                 ddl_context);
		}

		// The filter cannot simply be switched off. It blocked sign-in
		// (Engine.SigninLocalClient needs files 3 and 5), but switched off it killed
		// the process: a controller without a seat (GetLocalClientNum == -1) gets its
		// settings read completed on the -1 path. So the seated types are gated on
		// whether the controller has a local client, asked per call. This needs no
		// edit once a fourth seat exists.
		bool guest_has_local_client(const int controller)
		{
			const auto fn = reinterpret_cast<int (*)(int)>(base() + local_client_num_rva);
			return fn(controller) >= 0;
		}

		utils::hook::detour storage_read_hook;

		bool storage_read_stub(const int controller, const int file_type, const int index)
		{
			if (controller >= 2)
			{
				bool allowed = false;
				for (const auto t : guest_allowed_file_types)
				{
					if (static_cast<uint32_t>(file_type) == t)
					{
						allowed = true;
						break;
					}
				}
				if (!allowed)
				{
					for (const auto t : guest_seated_file_types)
					{
						if (static_cast<uint32_t>(file_type) == t)
						{
							allowed = guest_has_local_client(controller);
							// The settings files only while their guest completions are neutered.
							if (file_type == 0 && !settings_result_neutered)
							{
								allowed = false;
							}
							if (file_type == 1 && !shoutcaster_result_neutered)
							{
								allowed = false;
							}
							break;
						}
					}
				}
				// Everything not listed stays blocked. Allowing every type killed the
				// process: PS4 SettingsReadResult also writes s_settingsGlob[ci], another
				// per-controller array that is likely [2] on the PC.
				if (!allowed)
				{
					return false;
				}
			}
			return storage_read_hook.invoke<bool>(controller, file_type, index);
		}

		// clientGameStates relocation. Com_ControllerIndex_GetLocalClientNum scans two
		// slots, so it returned -1 for controller 2, and the guest's settings read then
		// ran configs with that -1 (the black screen). The array cannot grow in place
		// (live float data follows it), so it moves to a reserved address using the
		// 76-reference table. Three slots: the table's end markers are precomputed
		// for three.
		bool signin_relocated = false;

		// Field offsets from PS4 DWARF `struct ClientGameState`, confirmed on the live
		// PC array (stride 0x24 on PC, 0x1C on PS4). Only these five fields are
		// written; the layouts diverge after +0x14.
		constexpr size_t cgs_flags = 0x00;
		constexpr size_t cgs_local_client_num = 0x04;
		constexpr size_t cgs_controller_index = 0x08;
		constexpr size_t cgs_ui_context_index = 0x0C;
		constexpr size_t cgs_network_id = 0x10;

		// Number of seats in use: bit 0 of each relocated clientGameStates record,
		// the rule Com_LocalClient_IsBeingUsed applies. 1 at the menu, 3 in a full lobby.
		uint32_t seat_count()
		{
			if (!signin_relocated)
			{
				return 0;
			}
			uint32_t n = 0;
			for (uint32_t lc = 0; lc < signin_new_slots; ++lc)
			{
				uint8_t flags = 0;
				std::memcpy(&flags,
				            reinterpret_cast<const void*>(
					            base() + signin_new_base + lc * signin_stride),
				            sizeof(flags));
				if (flags & 1)
				{
					++n;
				}
			}
			return n;
		}

		// Seat count that survives the engine wiping and rebuilding the records at
		// map load; a dip there made the engine size for 2 and left the third pane
		// black. PS4 Com_LocalClient_GetUIContextIndex (0xE35C30) treats a record as
		// valid only if localClientNum == lc. A record mid-wipe fails that test and
		// the remembered bit is used; a genuine sign-out keeps localClientNum and
		// clears only the flag, so it is honoured at once.
		uint8_t remembered_seat_bits = 0;

		// Highest seat count seen with flags == 1 since the last match that had fewer
		// players (a smaller match lowers it to its own size, see set_active_stub). Drives
		// the allocation-floor commit in the stub.
		uint32_t committed_seats = 0;

		uint32_t bridged_seat_count()
		{
			if (!signin_relocated)
			{
				return 0;
			}
			uint32_t n = 0;
			for (uint32_t i = 0; i < signin_new_slots; ++i)
			{
				const auto slot = base() + signin_new_base + i * signin_stride;
				uint32_t flags = 0;
				uint32_t lcn = 0;
				std::memcpy(&flags, reinterpret_cast<const void*>(slot + cgs_flags),
				            sizeof(flags));
				std::memcpy(&lcn,
				            reinterpret_cast<const void*>(slot + cgs_local_client_num),
				            sizeof(lcn));
				const uint8_t mask = static_cast<uint8_t>(1u << i);
				if (flags & 1)
				{
					// an in-use record is constituted by definition
					remembered_seat_bits |= mask;
				}
				else if (i != 0 && lcn == i)
				{
					// flags 0 with lcn still == i is a genuine sign-out. Slot 0 is excluded:
					// its wiped state also reads lcn 0, and the host never unseats.
					remembered_seat_bits &= static_cast<uint8_t>(~mask);
				}
				if (remembered_seat_bits & mask)
				{
					++n;
				}
			}
			return n;
		}

		// Write seat i the way the game writes a fresh seat: i in all four index
		// fields, flags 0 (the in-use bit is set by the sign-in, not by us).
		// `clear` zeroes the seat first; only for the fresh buffer during relocation.
		// Repairs on the live array write only the five fields, as aligned stores.
		void write_signin_seat(const size_t new_array, const uint32_t i, const bool clear)
		{
			const auto slot = new_array + i * signin_stride;
			if (clear)
			{
				std::memset(reinterpret_cast<void*>(slot), 0, signin_stride);
			}
			const uint32_t zero = 0;
			std::memcpy(reinterpret_cast<void*>(slot + cgs_flags), &zero, sizeof(zero));
			std::memcpy(reinterpret_cast<void*>(slot + cgs_local_client_num), &i, sizeof(i));
			std::memcpy(reinterpret_cast<void*>(slot + cgs_controller_index), &i, sizeof(i));
			std::memcpy(reinterpret_cast<void*>(slot + cgs_ui_context_index), &i, sizeof(i));
			std::memcpy(reinterpret_cast<void*>(slot + cgs_network_id), &i, sizeof(i));
		}

		bool relocate_signin_field()
		{
			const auto module_base = base();
			const auto old_array = module_base + signin_old_base;
			const auto new_array = module_base + signin_new_base;

			// Verify every reference before writing anything (all or nothing). A
			// reference that already holds the new bytes is accepted.
			for (const auto& r : signin_refs)
			{
				const auto at = reinterpret_cast<const uint8_t*>(module_base + r.disp_rva);
				if (std::memcmp(at, r.new_bytes, 4) == 0)
				{
					continue;
				}
				if (std::memcmp(at, r.old_bytes, 4) != 0)
				{
					return false;
				}
			}

			// The destination must be empty.
			const auto* dst = reinterpret_cast<const uint8_t*>(new_array);
			for (size_t i = 0; i < signin_stride * signin_new_slots; ++i)
			{
				if (dst[i] != 0)
				{
					return false;
				}
			}

			std::memcpy(reinterpret_cast<void*>(new_array),
			            reinterpret_cast<const void*>(old_array),
			            signin_stride * signin_old_slots);

			for (const auto& r : signin_refs)
			{
				write_bytes(reinterpret_cast<void*>(module_base + r.disp_rva), r.new_bytes, 4);
			}

			const uint8_t slots = static_cast<uint8_t>(signin_new_slots);
			for (const auto rva : signin_bounds)
			{
				write_bytes(reinterpret_cast<void*>(module_base + rva), &slots, 1);
			}

			for (uint32_t i = signin_old_slots; i < signin_new_slots; ++i)
			{
				write_signin_seat(new_array, i, true);
			}

			// After the move the game's initializer still writes localClientNum and
			// networkID into the old array, by a path this table does not cover, which
			// left GetLocalClientNum(1) == 0. Write those two fields once here, as PS4
			// Com_InitClientGameStates (0xE353C0) does. controllerIndex and uiContextIndex
			// are left to the game, which reorders them.
			for (uint32_t i = 0; i < signin_old_slots; ++i)
			{
				const auto slot = new_array + i * signin_stride;
				std::memcpy(reinterpret_cast<void*>(slot + cgs_local_client_num), &i, sizeof(i));
				std::memcpy(reinterpret_cast<void*>(slot + cgs_network_id), &i, sizeof(i));
			}

			signin_relocated = true;
			return true;
		}

		// Re-assert the new seats' controllerIndex if something resets it. PS4
		// Com_InitClientGameStates memsets the array and never writes controllerIndex,
		// so a late initializer would put GetLocalClientNum(2) back to -1.
		void maintain_signin_seats()
		{
			if (!signin_relocated)
			{
				return;
			}

			const auto new_array = base() + signin_new_base;
			for (uint32_t i = signin_old_slots; i < signin_new_slots; ++i)
			{
				const auto slot = new_array + i * signin_stride;
				uint32_t have = 0;
				std::memcpy(&have, reinterpret_cast<const void*>(slot + cgs_controller_index),
				            sizeof(have));
				if (have != i)
				{
					write_signin_seat(new_array, i, false);
				}
			}
		}

		// CL_SplitscreenPlayerCount (PS4 0x1516BE0): the splitscreen_playerCount dvar,
		// or 1 while it is unregistered. Every consumer asks this, including
		// CL_AllocatePerLocalClientMemory (max(count, 2)) and the writer of
		// cl_maxLocalClients. The detour answers from the seat table (bit 0 of each
		// 0x24 record, what Com_LocalClient_IsBeingUsed reads), so every caller agrees
		// whenever it asks. A plain memory read: no engine call, no dvar system, no
		// thread hazard. Falls back to the original while the seat table is not
		// relocated or reads zero.

		utils::hook::detour splitscreen_player_count_hook;
		bool player_count_detoured = false;   // the game's calls reach splitscreen_player_count_stub()

		// CL_Init for local client 2. PS4 Com_Init calls CL_Init(i) for i 0..3 at boot
		// (0xE49E98); the PC boot inlines clients 0 and 1 only. CL_Frame skips a client
		// whose clientUIActives flag bit 1 ("CL_Init ran") is clear, so client 2 parked
		// at CA_CONFIRMLOADING. Calling the engine's own CL_Init(2) produces that state.
		// Its writes for lc 2 land in owned or padding memory; its two /GS range checks
		// (CL_Init's and Cbuf_Execute's) are opened for that one call only.
		// Called from the count detour, which runs on the game thread whenever the
		// count is asked (per Lua command, and by the map-load allocator), not from
		// per_controller_update_stub, which stops running after a splitscreen sign-in.
		// Resting value of the Cbuf_Execute range check: 0x02 stock, 0x04 once
		// install_cbuf_for_players34() has given local clients 2/3 their own command
		// buffers. The scoped CL_Init widens then leave it alone.
		uint8_t cbuf_range_resting = 0x02;
		// Defined with the IsActive cave further down. The cgame frame-loop widen
		// below may only run when the cave is installed.
		extern bool isactive_hooked;

		bool cl_init2_done = false;

		// Two latches. cl_init2_done: CL_Init(2) has run (in the lobby, once the third
		// seat exists). lc2_widens_done: the frame pump and netchan poll are open to
		// three, which must wait for the allocation (cl_maxLocalClients >= 3, map
		// load). With a single flag the widens were never applied and client 2 parked.
		bool lc2_widens_done = false;

		bool lc2_fully_done()
		{
			return cl_init2_done && lc2_widens_done;
		}

		// Re-entrancy guard. The count detour is itself a trigger site, so work that
		// asks for the splitscreen count re-enters it, and cl_init2_done is only set
		// after the CL_Init call.
		bool lc2_work_in_progress = false;

		struct in_progress_guard
		{
			bool& flag;
			explicit in_progress_guard(bool& f) : flag(f) { flag = true; }
			~in_progress_guard() { flag = false; }
			in_progress_guard(const in_progress_guard&) = delete;
			in_progress_guard& operator=(const in_progress_guard&) = delete;
		};

		// The five SCR_UpdateFrame bound immediates, widened as one group.

		void run_cl_init_for_local_client2()
		{
			// No cl_maxLocalClients gate on the init: SetAllUsedActive runs before the
			// allocator, so the count is still 0 when we get control. CL_Init is safe on
			// a client that owns nothing (PS4 inits all four at boot; CL_ClearState
			// null-checks the globals). Only the byte widens further down need the
			// allocation, and each checks for it.
			if (lc2_work_in_progress)
			{
				return;
			}
			const in_progress_guard guard(lc2_work_in_progress);

			const auto max_local = *reinterpret_cast<const volatile uint32_t*>(
				base() + cl_max_local_clients_rva);

			// Scoped widen: both sites are MSVC /GS range checks (cmp rbx,2 / jae
			// __report_rangecheckfailure) on a [2] array, which CL_Init(2) would trip.
			// They are opened for exactly one call and closed again, and nothing is
			// written unless both hold the expected byte.
			auto* range_a = reinterpret_cast<uint8_t*>(base() + cl_init_range_imm_rva);
			auto* range_b = reinterpret_cast<uint8_t*>(base() + cbuf_execute_range_imm_rva);
			if (*range_a != 0x02 || *range_b != cbuf_range_resting)
			{
				return;
			}

			// clientUIActives[2] flags. Slot 2 is the block voice_comm vacated.
			auto* flags = reinterpret_cast<volatile uint32_t*>(
				base() + uia_base_rva + 2 * uia_stride);
			if ((*flags & 0x2) == 0)
			{
				const uint8_t open = 0x03, shut = 0x02;
				// range_b is only opened when it rests at the stock 2; once the Cbuf widen
				// owns it (resting 4) it is already open.
				const bool touch_b = cbuf_range_resting == 0x02;
				const bool a_ok = write_bytes(range_a, &open, 1);
				const bool b_ok = !touch_b || write_bytes(range_b, &open, 1);
				if (a_ok && b_ok)
				{
					reinterpret_cast<void (*)(int)>(base() + cl_init_rva)(2);
				}
				// Always restore, even if one write failed or CL_Init threw.
				if (a_ok)
				{
					write_bytes(range_a, &shut, 1);
				}
				if (b_ok && touch_b)
				{
					write_bytes(range_b, &shut, 1);
				}
				if (!a_ok || !b_ok)
				{
					return;
				}
			}
			// Bit 1 is the engine's receipt that CL_Init ran, and what CL_Frame tests.
			// If the init did not take, the frame pump stays at two.
			if ((*flags & 0x2) == 0)
			{
				return;
			}

			// The init has taken; latch it only now, so an early call retries later.
			// This latch covers the init only. The widens below are deferred in the lobby,
			// and every trigger site keeps re-entering until lc2_widens_done.
			cl_init2_done = true;

			// The byte widens need the allocation, which happens at map load; the count
			// detour re-enters here then.
			if (max_local < 3)
			{
				return;
			}

			auto* site = reinterpret_cast<uint8_t*>(base() + cl_frame_pump_imm_rva);
			if (*site != 0x02)
			{
				return;
			}
			const uint8_t three = 0x03;
			write_bytes(site, &three, sizeof(three));

			// Netchan poll. Com_ClientPacketEvent (PS4 0xE491A0) polls each local client's
			// own netchan, 0..3 on PS4 but 0..1 on the PC, so client 2 never received his
			// connect replies and parked at CA_CONFIRMLOADING. Never widen it in the
			// frontend: clientConnection is carved for two there, and index 2 killed the
			// lobby.
			auto* poll = reinterpret_cast<uint8_t*>(base() + netchan_poll_imm_rva);
			if (*poll == 0x02)
			{
				write_bytes(poll, &three, sizeof(three));
			}

			// cgame frame loop, only with the IsActive cave.
			// PC SCR_UpdateFrame calls CG_DrawActiveFrame / CG_ProcessButDontDrawActiveFrame
			// for clients 0..1 only (PS4 0..3), so client 2 never gets a snapshot and the
			// third pane cannot draw. The five immediates are one group: MSVC split the
			// frame loop into two copies sharing the induction variable, and a second loop
			// computes r_num_viewports. Three, never four: clientUIActives slot 3 overlaps
			// the clientActive base pointer. The caved IsActive refuses
			// lc >= cl_maxLocalClients.
			// Ticking cgame for client 2 reaches per-client resources the PC only
			// allocates for two (NULL buffer pointers, [2] arrays; PS4
			// CG_ProcessSnapshots 0x2A86C0 also waits for a snapshot client 2 never
			// gets). The relocations try_apply() makes before this are what make it
			// safe; guarding them one at a time only moved the crash.
			// History: LOG.md, "BO3_CG_FRAME"
			if (isactive_hooked)
			{
				bool all_stock = true;
				for (const auto rva : cg_frame_imms)
				{
					const auto* at = reinterpret_cast<const uint8_t*>(base() + rva);
					if (!readable(at, 1) || *at != 0x02)
					{
						all_stock = false;
						break;
					}
				}
				if (all_stock)
				{
					const uint8_t bound3 = 0x03;
					uint32_t wrote = 0;
					for (const auto rva : cg_frame_imms)
					{
						auto* at = reinterpret_cast<uint8_t*>(base() + rva);
						if (write_bytes(at, &bound3, sizeof(bound3)))
						{
							++wrote;
						}
					}
					if (wrote != std::size(cg_frame_imms))
					{
						// all-or-nothing: put back whatever landed
						const uint8_t two = 0x02;
						for (const auto rva : cg_frame_imms)
						{
							write_bytes(reinterpret_cast<uint8_t*>(base() + rva),
							            &two, sizeof(two));
						}
					}
				}
			}

			// Both bounds are open with a real allocation behind them; close the second
			// latch.
			lc2_widens_done = true;
		}

		// Player 4: CL_Init(3), the same call as for lc 2, made once seat record 3 is
		// in use (guest_signin_stub). Static analysis only, untested. Its writes for
		// lc 3 hit owned memory, padding, or tables sized [4] (clientObjMap, Cbuf
		// records). The two /GS checks are read back as stock and opened to 4 for
		// this one call only.
		bool cl_init3_done = false;

		void run_cl_init_for_local_client3()
		{
			if (cl_init3_done || lc2_work_in_progress)
			{
				return;
			}
			const in_progress_guard guard(lc2_work_in_progress);
			auto* range_a = reinterpret_cast<uint8_t*>(base() + cl_init_range_imm_rva);
			auto* range_b = reinterpret_cast<uint8_t*>(base() + cbuf_execute_range_imm_rva);
			if (*range_a != 0x02 || *range_b != cbuf_range_resting)
			{
				return;
			}
			auto* flags = reinterpret_cast<volatile uint32_t*>(base() + uia_base_rva + 3 * uia_stride);
			if ((*flags & 0x2) == 0)
			{
				const uint8_t open = 0x04, shut = 0x02;
				const bool touch_b = cbuf_range_resting == 0x02;   // see run_cl_init_for_local_client2
				const bool a_ok = write_bytes(range_a, &open, 1);
				const bool b_ok = !touch_b || write_bytes(range_b, &open, 1);
				if (a_ok && b_ok)
				{
					reinterpret_cast<void (*)(int)>(base() + cl_init_rva)(3);
				}
				if (a_ok)
				{
					write_bytes(range_a, &shut, 1);
				}
				if (b_ok && touch_b)
				{
					write_bytes(range_b, &shut, 1);
				}
			}
			cl_init3_done = (*flags & 0x2) != 0;
		}

		// cl_init_watch (further down) runs on the renderer pipeline, which keeps
		// ticking through a launch, and triggers on a direct read of the seat table.

		// Seat player 3 the way the game does: Live_HandleClientSplitscreenSignin
		// (PS4 0xC16080) validates the guest and sets the seat bit and
		// userData.isActive together. Forcing the seat bit alone broke lobby hosting
		// ("Failed to host lobby"). Lobby enrolment is ensure_guest2_game_lobby()'s job.
		// Requires the LiveUser_IsUserGuest bound widen (is_user_guest_imm_rva):
		// without it controller 2 is "not a guest" and the call jumps straight to the
		// seat write, so the patch is verified in memory before calling.
		constexpr size_t userdata_is_guest = 0x29;
		constexpr uint32_t guest_join_max_attempts = 8;
		uint32_t guest_join_attempts = 0;
		bool guest_join_done = false;

		// Defined with the lobby API further down.
		bool offline_lobby_ready_for_player3();

		uint8_t seat_flags(const uint32_t lc)
		{
			uint8_t f = 0;
			std::memcpy(&f,
			            reinterpret_cast<const void*>(
				            base() + signin_new_base + lc * signin_stride),
			            sizeof(f));
			return f;
		}

		// Is controller `controller` seated? Not the same as seat_flags(controller):
		// the engine re-packs local clients at map load (host + player 3 put player 3
		// in record 1). The record's controller is at +8.
		bool controller_seated(const int32_t controller)
		{
			for (uint32_t i = 0; i < signin_new_slots; ++i)
			{
				const auto* record = reinterpret_cast<const uint8_t*>(
					base() + signin_new_base + i * signin_stride);
				int32_t owner = -1;
				std::memcpy(&owner, record + 8, sizeof(owner));
				if (owner == controller)
				{
					return (record[0] & 1) != 0;
				}
			}
			return false;
		}

		// s_gamePads must not be relocated at post_unpack: moving it before the stock
		// gamepad constructor finished crashed. This is the transaction
		// tools/prepare_gamepad_join.py proved in a live lobby.
		// gamepad_bound_rvas: the six widened loop bounds (poll, per-frame update,
		// assign, connected-unused count, GetUsedControllerCount); none needs a seat.
		constexpr size_t expected_gamepad_refs = 38;
		bool gamepads_activated = false;
		bool gamepads_activation_in_progress = false;

		// Which device feeds which slot. A gamepad record (0x70) holds GamePad.enabled
		// at +0 and the index of the device it reads at +4; 8 means no device.
		// Zero-filled new slots read device 0, the host's controller. So slots 2/3
		// start as "no device" and the game's own rescan (enumerate + assign) runs
		// once, assigning every connected controller as on a device change.
		constexpr size_t gamepad_stride = 0x70;
		constexpr size_t gamepad_device_index = 0x04;
		constexpr int32_t gamepad_no_device = 8;
		static_assert(gamepads_reloc_table.old_size == 2 * gamepad_stride);
		static_assert(gamepads_reloc_table.new_size == 4 * gamepad_stride);

		// Does controller slot `slot` have a connected device right now? Only
		// meaningful once the table is relocated (the stock array has 2 slots).
		bool gamepad_connected(const size_t slot)
		{
			if (!gamepads_activated || slot >= 4)
			{
				return false;
			}
			return *reinterpret_cast<const volatile uint8_t*>(
				base() + gamepads_reserved_rva + slot * gamepad_stride) != 0;
		}

		void fix_gamepad_type_selectors()
		{
			for (const auto& site : gamepad_type_selector_sites)
			{
				auto* p = reinterpret_cast<uint8_t*>(base() + site.rva);
				if (readable(p, sizeof(site.expected))
					&& std::memcmp(p, site.expected, sizeof(site.expected)) == 0)
				{
					write_bytes(p, gamepad_type_selector_fixed, sizeof(gamepad_type_selector_fixed));
				}
			}
		}

		int32_t gamepad_ref_value(const reloc_ref& r, const size_t destination_rva)
		{
			const auto moved = destination_rva
			                   + (r.target_rva - gamepads_reloc_table.base_rva);
			return r.rip_relative
			       ? static_cast<int32_t>(moved - (r.insn_rva + r.length))
			       : static_cast<int32_t>(moved);
		}

		int32_t gamepad_original_ref_value(const reloc_ref& r)
		{
			return r.rip_relative
			       ? static_cast<int32_t>(r.target_rva - (r.insn_rva + r.length))
			       : static_cast<int32_t>(r.target_rva);
		}

		bool gamepad_refs_match(const size_t destination_rva)
		{
			for (size_t i = 0; i < gamepads_reloc_table.count; ++i)
			{
				const auto& r = gamepads_reloc_table.refs[i];
				const auto* field = reinterpret_cast<const int32_t*>(
					base() + r.insn_rva + r.disp_offset);
				if (!readable(field, sizeof(*field))
					|| *field != gamepad_ref_value(r, destination_rva))
				{
					return false;
				}
			}
			return true;
		}

		bool gamepad_bounds_match(const uint8_t expected)
		{
			for (const auto rva : gamepad_bound_rvas)
			{
				if (*reinterpret_cast<const uint8_t*>(base() + rva) != expected)
				{
					return false;
				}
			}
			return true;
		}

		// When the table may move. The one hard precondition is that controller 2 is
		// not signed in yet: his sign-in reads his slot. No seat is needed, so player
		// 3 may join before player 2 (seats {0, 2}); at START GAME the engine packs him
		// into lc 1 before sizing per-client memory (PS4 LobbyLaunch_PreloadMap).
		// Offline lobby only, never at the live main menu.
		bool gamepads_may_activate()
		{
			if (!(seat_flags(0) & 1))
			{
				return false;
			}
			if (seat_flags(1) & 1)
			{
				return true;   // the original, proven point
			}
			return game::Com_IsRunningUILevel() && offline_lobby_ready_for_player3();
		}

		void complete_gamepads(size_t destination_abs);   // defined with the other completions

		bool activate_gamepads_in_lobby()
		{
			if (gamepads_activated)
			{
				return true;
			}
			if (gamepads_activation_in_progress || !gamepads_reserved_rva
				|| gamepads_reloc_table.count != expected_gamepad_refs)
			{
				return false;
			}

			if (!gamepads_may_activate())
			{
				return false;
			}

			const in_progress_guard guard(gamepads_activation_in_progress);
			const auto destination_rva = gamepads_reserved_rva;

			// A second entry after a successful transaction does no writes.
			if (gamepad_refs_match(destination_rva) && gamepad_bounds_match(4))
			{
				gamepads_activated = true;
				return true;
			}

			// Dry-run every reference before the first write; fail closed.
			for (size_t i = 0; i < gamepads_reloc_table.count; ++i)
			{
				const auto& r = gamepads_reloc_table.refs[i];
				const auto* field = reinterpret_cast<const int32_t*>(
					base() + r.insn_rva + r.disp_offset);
				if (!readable(field, sizeof(*field))
					|| *field != gamepad_original_ref_value(r))
				{
					return false;
				}
			}

			std::array<uint8_t, std::size(gamepad_bound_rvas)> old_bounds{};
			for (size_t i = 0; i < old_bounds.size(); ++i)
			{
				old_bounds[i] = *reinterpret_cast<const uint8_t*>(
					base() + gamepad_bound_rvas[i]);
				if (old_bounds[i] != 2 && old_bounds[i] != 4)
				{
					return false;
				}
			}

			auto* destination = reinterpret_cast<uint8_t*>(base() + destination_rva);
			std::memcpy(destination,
			            reinterpret_cast<const void*>(base() + gamepads_reloc_table.base_rva),
			            gamepads_reloc_table.old_size);
			std::memset(destination + gamepads_reloc_table.old_size, 0,
			            gamepads_reloc_table.new_size - gamepads_reloc_table.old_size);
			// New slots own no device yet (see gamepad_no_device) - a zero here
			// would make them read device 0, the host's controller.
			for (size_t slot = gamepads_reloc_table.old_size / gamepad_stride;
			     slot < gamepads_reloc_table.new_size / gamepad_stride; ++slot)
			{
				std::memcpy(destination + slot * gamepad_stride + gamepad_device_index,
				            &gamepad_no_device, sizeof(gamepad_no_device));
			}

			size_t changed_refs = 0;
			size_t changed_bounds = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < changed_bounds; ++i)
				{
					write_bytes(reinterpret_cast<void*>(base() + gamepad_bound_rvas[i]),
					            &old_bounds[i], sizeof(old_bounds[i]));
				}
				for (size_t i = 0; i < changed_refs; ++i)
				{
					const auto& r = gamepads_reloc_table.refs[i];
					const auto original = gamepad_original_ref_value(r);
					write_bytes(reinterpret_cast<void*>(base() + r.insn_rva + r.disp_offset),
					            &original, sizeof(original));
				}
				std::memset(destination, 0, gamepads_reloc_table.new_size);
			};

			for (size_t i = 0; i < gamepads_reloc_table.count; ++i)
			{
				const auto& r = gamepads_reloc_table.refs[i];
				const auto moved = gamepad_ref_value(r, destination_rva);
				if (!write_bytes(reinterpret_cast<void*>(base() + r.insn_rva + r.disp_offset),
				                 &moved, sizeof(moved)))
				{
					rollback();
					return false;
				}
				++changed_refs;
			}

			for (size_t i = 0; i < old_bounds.size(); ++i)
			{
				const uint8_t four = 4;
				if (old_bounds[i] != four
					&& !write_bytes(reinterpret_cast<void*>(base() + gamepad_bound_rvas[i]),
					                &four, sizeof(four)))
				{
					rollback();
					return false;
				}
				++changed_bounds;
			}

			if (!gamepad_refs_match(destination_rva) || !gamepad_bounds_match(4))
			{
				rollback();
				return false;
			}

			gamepads_activated = true;
			complete_gamepads(base() + destination_rva);

			// Run the game's device rescan so the new slots get the connected
			// controllers. On a byte mismatch slots 2/3 wait for the next device change.
			const auto* rescan = reinterpret_cast<const void*>(base() + gamepad_rescan_rva);
			if (readable(rescan, sizeof(gamepad_rescan_bytes))
				&& std::memcmp(rescan, gamepad_rescan_bytes, sizeof(gamepad_rescan_bytes)) == 0)
			{
				reinterpret_cast<void (*)()>(base() + gamepad_rescan_rva)();
			}
			else
			{
				note("[splitscreen] gamepads relocated rescan=bytes-mismatch\n");
			}
			return true;
		}

		// Signs in controller 2 as a guest (player 3) once the offline lobby is up.
		void try_join_guest2()
		{
			if (guest_join_done || guest_join_attempts >= guest_join_max_attempts)
			{
				return;
			}
			if (!signin_relocated || !guests_filled || !guest_array_rva)
			{
				return;
			}

			// Three or more local players are offline-only (Treyarch caps LIVE at
			// two). Seated in the LIVE party, player 3 is a local client no lobby
			// knows and the lobby loops. So wait, without spending an attempt, until
			// the lobby is not LIVE and the game lobby is up.
			// History: LOG.md, offline_lobby_ready_for_player3
			if (!offline_lobby_ready_for_player3())
			{
				return;
			}

			// Verify the is-user-guest bound widen landed before calling.
			uint8_t bound = 0;
			std::memcpy(&bound,
			            reinterpret_cast<const void*>(base() + is_user_guest_imm_rva),
			            sizeof(bound));
			if (bound != 0x03)
			{
				return;
			}

			// The host must be seated and seat 2 free. Player 2 need not be seated;
			// if he is, offline_lobby_ready_for_player3 checked he is a lobby member.
			if (!controller_seated(0) || controller_seated(2))
			{
				return;
			}

			// Controller 2 must already be marked a guest (read only, never set).
			// If it is 0 the sign-in takes the not-a-guest branch and seats him unvalidated.
			uint8_t is_guest = 0;
			std::memcpy(&is_guest,
			            reinterpret_cast<const void*>(base() + guest_array_rva
				            + 2 * client_ui_stride + userdata_is_guest),
			            sizeof(is_guest));
			if (!is_guest)
			{
				return;
			}

			// No controller on slot 2: do not spend an attempt. This runs every
			// frame, so refusals would use up the retries before one is plugged in.
			if (!gamepad_connected(2))
			{
				return;
			}

			++guest_join_attempts;

			using signin_fn = void (*)(int, bool, bool);
			reinterpret_cast<signin_fn>(base() + guest_signin_rva)(2, true, false);

			// Latch only if the seat bit is set; otherwise retry (bounded). Check by
			// controller, not record: after a round without player 2 the engine
			// keeps controller 2 in record 1.
			if (controller_seated(2))
			{
				guest_join_done = true;
			}
		}
