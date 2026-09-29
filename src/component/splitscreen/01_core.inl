// Core: patch tables (local-client count, storage), helpers (base, readable, write_bytes, allocate_near_module, note, relocate), guest fill.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// --- the per-client base that arrives mangled ---
		//
		// This table holds the correct per-client base, but with a third player the
		// value reaching the consumer is corrupted: it travels through
		// mov rax,[rsp+0x50] / ror rax,0x20 behind an Arxan integrity compare.
		// Re-reading it from the table restores it. This is a repair, not a null
		// guard: the value is neither null nor missing.
		constexpr size_t base_table_rva = 0x17ADC958;

		// imul rcx, rcx, 0x1e940   - displaced into the cave
		constexpr size_t stride_site_rva = 0x01F23651;
		constexpr uint8_t stride_site_bytes[] = {0x48, 0x69, 0xC9, 0x40, 0xE9, 0x01, 0x00};

		// Do not repair the second consumer of this value at 0x00F7E918: doing so
		// kills the process with no minidump, with or without a guard.

		// A one-byte code patch: `expect` is the original byte, `value` the new one.
		struct byte_patch
		{
			size_t rva;
			uint8_t expect;
			uint8_t value;
			const char* what;
		};

		// --- the local-client count ---
		//
		// cl_maxLocalClients (RVA 0x053A2720) holds 2 and is read by ~2630
		// `cmp <reg>, [cl_maxLocalClients]` bounds checks across the engine. PS4
		// CG_GetLocalClientGlobals (0x1D76850) returns NULL for any localClientNum
		// >= cl_maxLocalClients, which is the NULL controller 2 hit. Most other
		// per-client failures are downstream of this value.
		//
		// Raising it is safe because the containers are ready: the per-client CG
		// blocks are allocated as count * stride (PS4 CG_AllocateClientMemory
		// 0x21FD70; PC 0x00840922 `imul rdx, rdx, 0x342720`), and the fixed arrays
		// (s_storage, client_objs, client_ui) are relocated by this component.
		//
		// Ordering rule: containers first, count second. Raising the count while a
		// target array is still [2] makes per-client loops write slots 2/3 over
		// adjacent live memory.
		//
		// CL_AllocatePerLocalClientMemory (0x0135D650) computes
		// max(CL_SplitscreenPlayerCount(), 2) (PS4 0x416A32). That floor feeds
		// CG/FX/CL_AllocateClientMemory and the cl_maxLocalClients store at
		// 0x0135D489. It is not raised statically - telling the allocator four
		// while splitscreen_playerCount said one crashed a solo round at start.
		// splitscreen_player_count_stub() commits it to 3 or 4 once that many
		// seats have really seated.
		constexpr size_t alloc_floor_rva = 0x0135D68C;

		constexpr byte_patch local_client_count_patches[] = {
			// The in-game allocation pass (flags bit 2) discards that result and
			// hard-codes local = 2 with `lea r14d, [rsi-0x10]` (rsi = 18 maxClients).
			// PS4 (0x416A10) has no such override. NOP the lea; maxClients stays 18.
			{0x0135D6A4, 0x44, 0x90, "flag-4 alloc: drop the hard local=2 (1/4)"},
			{0x0135D6A5, 0x8D, 0x90, "flag-4 alloc: drop the hard local=2 (2/4)"},
			{0x0135D6A6, 0x76, 0x90, "flag-4 alloc: drop the hard local=2 (3/4)"},
			{0x0135D6A7, 0xF0, 0x90, "flag-4 alloc: drop the hard local=2 (4/4)"},

			// Boot bind loop 0x0135DDC8..0x0135DE24: the only boot code that sets a
			// slot's controllerIndex and beingUsed flag.
			{0x0135DE43, 0x02, 0x04, "boot bind loop: local clients 2 -> 4"},

			// ...and its controllerIndex clamp min(i, 1); PS4 clamps min(i, 3)
			// (0xE49DC5). Otherwise slots 2 and 3 would bind controllerIndex 1 too.
			{0x0135DE2F, 0x01, 0x03, "boot bind clamp: controllerIndex min(i,1) -> min(i,3)"},

			// Lua GetCountUsedAndSignedInLocalClients (0x01FC73E0) counts local
			// clients 0..1; PS4 (0xD2EE40) counts 0..3. The body only calls
			// predicates, so widening it cannot write anywhere.
			{0x01FBAC99, 0x02, 0x04, "GetCountUsedAndSignedInLocalClients: 2 -> 4"},

			// The lobby panel draws its rows from Engine.GetUsedControllerCount() and
			// Engine.IsControllerBeingUsed(i). GetUsedControllerCount (0x01FE4260) and
			// GetNonUsedControllerCount (0x01FE35C0) loop over two controllers; PS4
			// (0xD5B970, 0xD5BBA0) loops over four.
			// Disabled: suspected in a crash on the third sign-in (CRT fastfail in
			// WndProc 0x02334790, Dvar_GetInt on a NULL dvar). They only feed Lua
			// counters. History: LOG.md, "GetUsedControllerCount".
			// {0x01FE428E, 0x02, 0x04, "GetUsedControllerCount: 2 -> 4"},
			// {0x01FE35F9, 0x02, 0x04, "GetNonUsedControllerCount: 2 -> 4"},

			// GetMaxControllerCount (0x01FE3370) returns the constant 2.0f; PS4
			// (0xD5BCF0) returns 4. The stock datasources.lua creates per-controller
			// UI models (scriptNotify, hudItems.*, ...) for 0..GetMaxControllerCount()-1
			// at UI init, so at 2 player 3's HUD never received a script notify.
			// Never raise it past the controllers that have seats: once that let Lua
			// touch a seatless controller and killed the boot with
			// __report_rangecheckfailure (0xC0000409 subcode 8).
			// One byte: 2.0f = 0x40000000, 4.0f = 0x40800000.
			{0x01FD6BFD, 0x00, 0x80, "GetMaxControllerCount: 2.0f -> 4.0f (controllers with seats; 4 since player 4)"},

			// GetMaxLocalControllers (0x01FE3390), same shape and value. It caps
			// lobby_maxLocalPlayers (Lobby_SetMaxLocalPlayers: 4 offline, capped here),
			// which LobbyAddLocalClient checks when an unused controller presses its
			// join button. CoDMenu also subscribes the button models of controllers
			// 0..GetMaxLocalControllers()-1.
			{0x01FD6C1D, 0x00, 0x80, "GetMaxLocalControllers: 2.0f -> 4.0f (controllers with seats; 4 since player 4)"},

			// Engine.GetPlayerStats (0x01FCAD00), which the gobblegum row is built from,
			// returned nil for controller 2 because of its first gate `cmp r14d, 1 / ja`.
			// PS4 (0xD36D20) bounds the same argument at 4. The second gate, the stats
			// walk 0x01EA9A30, passes for controller 2.
			{0x01FBE6DE, 0x01, 0x03, "Engine.GetPlayerStats: controller bound 1 -> 3"},

			// LobbyHost_AddLocalClients (0x01ED7560) decides who is in the lobby. PS4
			// (0xCA5CA0) loops ci 0..3 and adds every controller that passes
			// ShouldAddController (seat in use; offline, or signed in to Demonware).
			// Controller 2 passes that; only the PC bound `cmp ebx, 2` kept it out.
			// Needs the netchan relocation first: without it, adding controller 2
			// crashed at 0x02173AD0 reading past a per-index array (0x16E69E20, stride
			// 0x128). This table is only applied when every relocation succeeded,
			// netchan included.
			{0x01ECACEB, 0x02, 0x04, "LobbyHost_AddLocalClients: controllers 2 -> 4"},

			// Activation loop, the PC twin (inlined at 0x0283AB30) of PS4
			// CL_LocalClients_SetAllUsedActive (0x1517020): at every launch,
			// SetActive(i, IsBeingUsed(i)). With a bound of two, client 2 stayed
			// used-but-inactive. Three, not four: clientUIActives slot 2 is real once
			// voice_comm has moved, slot 3 is still foreign.
			{0x027C1A45, 0x02, 0x03, "SetAllUsedActive loop: local clients 2 -> 3"},

			// Connect loop of PS4 CL_MapLoading (0x40CB40): for each active local
			// client, CL_Disconnect, SetActive, connectionState 5/6 and the connected
			// flag. The PC twin ends at 0x01359DB9 and walks clientUIActives by byte
			// offset, so its bound is a size: `cmp rsi, 0x20F0` (2 * 0x1078). Without
			// it client 2 was activated but never connected.
			// Index 3 only touches the owned head of clientUIActives[3], seat record 3
			// and [4] arrays; the body skips a client that is not in use.
			{0x01359DDC, 0xF0, 0xE0, "connect loop end: 2*0x1078 -> 4*0x1078 (low)"},
			{0x01359DDD, 0x20, 0x41, "connect loop end: 2*0x1078 -> 4*0x1078 (high)"},

			// The same loop's counter (`cmp ebx, 2` at 0x01359DDA) is a second bound
			// and the one that actually ends it. Both must move.
			{0x01359DFC, 0x02, 0x04, "connect loop counter: local clients 2 -> 4"},

			// Per-client reset loop in the same function (clears flag bit 6 and
			// keyCatchers). Must cover the same clients, or client 2 carries a stale
			// keyCatcher state into the round.
			{0x01359BF8, 0x02, 0x04, "map-load reset loop: local clients 2 -> 4"},

			// /GS range check on the per-client byte array 0x052F29C4 (index >= 2 ->
			// __report_rangecheckfailure, 0xC0000409), hit via CL_ClearKeys. Widened in
			// place: slots 2 and 3 (0x052F29C6/7) have no code references, they are
			// padding.
			{0x012F351D, 0x02, 0x04, "per-client byte array 0x052F29C4 range check: 2 -> 4"},

			// IN_Attack_Up (0x0131B260; PS4 0x3E0D00) clears gAttackEdgeDetected[lc]
			// (the byte array above) and releases two kbuttons in playersKb[lc].
			// Player 3 firing hit this /GS check. playersKb is already [4].
			{0x0131B28A, 0x02, 0x04, "IN_Attack_Up range check: local clients 2 -> 4"},
			// 0x0131C050: per-frame analog-trigger edge; on release it clears byte
			// 0x052F3360[lc] behind `cmp rbx, 2`. Slots 2/3 are unreferenced padding.
			{0x0131C0CA, 0x02, 0x04, "trigger-edge byte array 0x052F3360 range check: 2 -> 4"},

			// The CG frame function (0x00A129A9) guards a per-client byte array at
			// 0x04D1DC94 with `cmp r15, 2 / jae __report_rangecheckfailure`. That
			// fail-fast bypasses SEH: no dialog and no BOIII dump, only a WER dump.
			// Slots 2/3 have no references (padding), so it widens in place.
			{0x00A15B68, 0x02, 0x04, "CG frame per-client byte array 0x04D1DC94 range check: 2 -> 4"},

			// Netchan poll: not applied from this table. PS4 Com_ClientPacketEvent
			// (0xE491A0) polls each local client's own netchan for all four clients;
			// the PC twin (0x020F7AC7..) stops at two (`cmp ebx, 2` at 0x020F7BA2), so
			// client 2's replies were never read and it parked at CA_CONFIRMLOADING.
			// It is written at map load (netchan_poll_imm_rva): in the frontend
			// cl_maxLocalClients is 2, the clientConnection array (stride 0x25780) is
			// carved for two only, and polling index 2 there crashed the lobby.
			// {0x020F7BA4, 0x02, 0x03, "netchan poll"},   applied dynamically

			// CL_Frame pump, also not in this table. Com_Frame calls CL_Frame(lc) only
			// for lc < 2 (`cmp ebx, 2` at 0x020F95DA; PS4 0xE4D38D loops to 4), so
			// client 2's handshake is never advanced. run_cl_init_for_local_client2()
			// writes that bound only after CL_Init has run for client 2: CL_Frame runs
			// per-client code (error popup, pending-error slot) before its own gate,
			// and IsBeingUsed(2) is true as soon as player 3 signs in.

			// CL_Init's range check (0x01359468, guarding cl_waitingOnServerToLoadMap
			// at 0x053D4988) and Cbuf_Execute's are widened by
			// run_cl_init_for_local_client2() only around its own CL_Init(2) call.
			// A permanently widened /GS check leaves the stock engine running against a
			// bound that no longer matches its array.
			// {0x0135946B, 0x02, 0x03, "CL_Init range check"},   scoped instead

			// Cbuf_Execute's range check (0x020EC1AD). For client 2 that call is
			// inert: every per-client byte and dword it reads for lc 2 is unreferenced.
			// {0x020EC1B0, 0x02, 0x03, "Cbuf_Execute range check"},  scoped too

			// GetLobbyLocalClientCount (count loop 0x01EFF910, `cmp ebx, 2`). A
			// three-player lobby listed client 2 in the roster but counted two local
			// clients, so its row had no loadout data and DEACTIVATE SPLITSCREEN could
			// not release it. The body only indexes client_objs (0x0340F180), which
			// this component relocates to [4]; an empty slot returns false.
			{0x01EF31FA, 0x02, 0x04, "GetLobbyLocalClientCount loop: 2 -> 4"},

			// DEACTIVATE SPLITSCREEN: LobbyRemoveAllLocalSplitscreenClient (0x01F16D60),
			// `cmp ebx, 2` at 0x01F16E02. The body reaches the index only through
			// client_objs (relocated) and the seat lookup 0x020EF7C0, which already
			// covers index 2. Effect not confirmed. The seat table must stay
			// contiguous: the engine never produces a gap such as 0 and 2 in use.
			{0x01F0A684, 0x02, 0x04, "LobbyRemoveAllLocalSplitscreenClient loop: 2 -> 4"},

			// LiveUser_IsUserGuest (0x01EC70C0) returns false for every ci >= 2 before
			// it reads the isGuest byte. Storage_Pump's guest branch (0x02277376) lets
			// a guest inherit its loadout files from the primary, so this bound left
			// player 3 without gobblegums. The only array it indexes,
			// s_UserDataForControllerMap (0x0340F180), is relocated to four entries.
			{0x01EBA642, 0x01, 0x03, "LiveUser_IsUserGuest bound: ci<=1 -> ci<=3"},

			// Console commands disableallbutprimaryclients (0x0134C300),
			// disableallclients (0x0134C340) and a provisional variant (0x0134C390)
			// loop over two clients; PS4 CL_Command_DisableAllButPrimaryClients
			// (0x40B550) loops over four. They drop the guests on the way back to the
			// frontend; after game over client 2 stayed active and the frontend hung.
			// The bodies write only clientUIActives and the relocated seat table.
			{0x0134C352, 0x02, 0x04, "disableallbutprimaryclients loop: local clients 2 -> 4"},
			{0x0134C396, 0x02, 0x04, "disableallclients loop: local clients 2 -> 4"},
			{0x0134C3EF, 0x02, 0x04, "provisional disable-all loop: local clients 2 -> 4"},

			// IN_GamepadsMove (0x022F3EF0) polls pads for ci < 2 (`cmp edi, 2` at
			// 0x022F40F2); PS4 (0xDBA1F0) polls four. It feeds sticks, triggers and
			// buttons to usercmds and also to the menus (PS4: ->
			// CL_GamepadButtonEventForPort -> UI_CoD_KeyEvent), so without it players
			// 3/4 cannot move or press A. Per-client targets: gaGlobs, playerKeys,
			// s_gamePads (all [4]), the seat table and clientUIActives keyCatchers.
			{0x02287024, 0x02, 0x04, "IN_GamepadsMove: poll controllers 2 -> 4 (players 3/4 sticks/buttons, menus too)"},

			// Netchan thread (0x02176E80.., `cmp ebx, 2` at 0x02176EE2): transmit,
			// keepalives, acks and stale-message cleanup ran for controllers 0 and 1
			// only; PS4 Netchan_Thread (0xE7E650) does four. A stale fragment left for
			// controller 2 swallowed the host's next message, so player 3 got stuck
			// loading from the second round on. Indexed arrays: s_netchan rows and
			// clientGameStates, both four deep.
			{0x0211E444, 0x02, 0x04, "Netchan_Thread pump: controllers 2 -> 4 (transmit/acks/stale cleanup)"},

			// Client setup for a level load (0x0135DCD0), frontend branch: `cmp ebx, 2`
			// at 0x0135DD35 (the in-game branch is the boot bind loop row above).
			// PS4 Com_LocalClients_AssignUIContextsForFrontEnd (0xE35BD0) covers four.
			// Without it client 2 stayed active on the way back to the frontend. The
			// body writes userData, the seat table and clientUIActives flags only.
			{0x0135DD57, 0x02, 0x04, "frontend client setup loop: local clients 2 -> 4"},
		};

		// --- storage: give controllers 2 and 3 real buffers ---
		//
		// AllocateMemory (PS4 storage_api.cpp, called once from Storage_Init)
		// carves every controller's file buffers and scratch out of one pool sized
		// for LOCAL_CLIENT_COUNT controllers. On PC that count is 2 in two places:
		//   0x02275922  lea eax, [r9 + r10*2]   pool multiplier (SIB scale)
		//   0x02275A80  cmp r12d, 2             controller loop bound
		// Raising both lets the game's own allocator carve four controllers, as on
		// the console; nothing is hand-written. Shared files (properties->[0x20]
		// == 3) still resolve to controller 0's buffer.
		//
		// The same TU bounds the controller count in eight more places, found
		// mechanically (every `cmp <r32>, 2` whose loop indexes s_storage by
		// 0x8958). The key one is the gatekeeper 0x02276E30
		// (`cmp esi, 2 / jl ok / xor al, al / ret`) on the file-lookup paths:
		// without it the new buffers exist but nothing can find them.
		// All are `cmp r32, imm8`, so every patch is one byte.
		constexpr byte_patch storage_patches[] = {
			{0x02218DF5, 0x51, 0x91, "storage pool: SIB scale x2 -> x4"},
			{0x02218F53, 0x02, 0x04, "AllocateMemory: controllers 2 -> 4"},
			{0x02219A0E, 0x02, 0x04, "clear-all loop: controllers 2 -> 4"},
			{0x02219AF3, 0x02, 0x04, "file lookup A: controllers 2 -> 4"},
			{0x02219B95, 0x02, 0x04, "file lookup B: controllers 2 -> 4"},
			{0x02219D1A, 0x02, 0x04, "file lookup C: controllers 2 -> 4"},
			{0x02219FA4, 0x02, 0x04, "file lookup D: controllers 2 -> 4"},
			{0x0221A1C3, 0x02, 0x04, "file lookup E: controllers 2 -> 4"},
			{0x0221A30B, 0x02, 0x04, "controller gate 0x02276E30: 2 -> 4"},
			{0x0221AB16, 0x02, 0x04, "controller gate 0x02277640: 2 -> 4"},

			// The rows below must be applied here at boot: controller 1 gets storage
			// and stats readiness from the boot pass, and a controller patched later
			// misses it.
			//
			// 0x0135C8AD: the per-controller update loop that drives Storage_Pump
			// (`call 0x01E26570 / inc ebx / cmp ebx, 2`), outside the storage TU.
			// Never widen a controller bound past the seats that exist: at 4 with three
			// seats the engine reached controller 3's uninitialised command buffer and
			// crashed. Four is safe now that controller 3 has a seat and the Cbuf
			// records are [4].
			{0x0135C8CD, 0x02, 0x04, "per-controller update loop (Storage_Pump): 2 -> 4"},

			// Storage_Read refuses every controller >= 2 (`cmp edi, 2 / jge false`),
			// so no per-controller file was ever read for a guest.
			{0x0221AA7F, 0x02, 0x04, "Storage_Read controller bound: 2 -> 4"},

			// Two more of the identical shape, found with tools/bound_scan.py.
			{0x0221AC63, 0x02, 0x04, "storage fn 0x02277780 controller bound: 2 -> 4"},
			{0x0221AE3F, 0x02, 0x04, "storage fn 0x02277960 controller bound: 2 -> 4"},

			// TaskManager2_ProcessTasks per-controller loop (0x020F91D0, `cmp ebx, 2`).
			// Finished tasks were reaped only for controllers 0 and 1, so controller
			// 2's gamer-profile read stayed DONE. The 'hdd' busy query (0x02274D30)
			// checks one global task, so that unreaped task blocked storage for every
			// controller. Letting the game reap in its own frames is the fix; forcing
			// the reap from a detour took the renderer down.
			// Only safe together with the guest storage read filter: reaping a guest's
			// SETTINGS read runs autoexec and Settings_RunCallbacks with localClient
			// -1 and blacks out the client. With the filter, guests only read stats.
			{0x020ECA5B, 0x02, 0x04, "TaskManager2_ProcessTasks per-controller loop: 2 -> 4"},
		};

		// s_storageMem.pool: zero until AllocateMemory has run, so it tells whether
		// the patches can still get in before Storage_Init.
		constexpr size_t storage_pool_rva = 0x1789FD78;

		// --- launch handshake -------------------------------------------------
		constexpr size_t lobby_pool_rva = 0x155FD410;
		constexpr size_t lobby_pool_stride = 0x66828;
		constexpr size_t game_lobby_index = 1;
		constexpr size_t session_clients_offset = 0xF8;
		constexpr size_t session_client_stride = 0x30;
		constexpr size_t acks_offset = 0x2780;
		constexpr size_t launch_sequence_rva = 0x156CA510;
		constexpr size_t gate_arrays[] = {0x23D0, 0x65FA0};
		constexpr size_t copy_fields[] = {0x08, 0x0C, 0x10, 0x20};
		constexpr size_t reference_slot = 1;
		constexpr size_t injected_slots[] = {2, 3};

		size_t base()
		{
			return game::get_base();
		}

		// Reference tables come from outside the image, and an entry can point at
		// a page that is not committed yet (common in the Arxan region at
		// post_unpack). Reading it would throw and abort post_unpack, so every
		// table read checks here first.
		bool readable(const void* p, const size_t size)
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
			{
				return false;
			}
			if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
			{
				return false;
			}
			const auto start = reinterpret_cast<size_t>(mbi.BaseAddress);
			const auto end = start + mbi.RegionSize;
			return reinterpret_cast<size_t>(p) + size <= end;
		}

		bool write_bytes(void* target, const void* data, const size_t size)
		{
			DWORD old{};
			if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old))
			{
				return false;
			}

			const auto restore = utils::finally([&]
			{
				DWORD tmp{};
				VirtualProtect(target, size, old, &tmp);
			});

			std::memcpy(target, data, size);
			return true;
		}

		// Relocated arrays must land near the module: rip-relative and ABS32
		// displacements are 32-bit, and a plain VirtualAlloc lands out of reach.
		// Probe 64 KB steps just past the image, as tools/reloc_range.py does.
		//
		// Every allocation is padded on both sides. The game indexes
		// per-controller arrays with -1 ("no controller yet") and bounds checks only
		// test the upper bound. In the stock image s_storage[-1] reads mapped .data;
		// at the front of a fresh allocation it faulted (signing in the second
		// client crashed at 0x02276E71). The zero-filled padding turns an invalid
		// index into a null that the caller's own `test rax,rax` handles.
		constexpr size_t reloc_padding = 0x10000; // > any observed element size

		void* allocate_near_module(const size_t size)
		{
			const auto module_base = base();

			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
				module_base + dos->e_lfanew);
			const size_t image_size = nt->OptionalHeader.SizeOfImage;

			auto candidate = (module_base + image_size + 0xFFFF) & ~static_cast<size_t>(0xFFFF);

			for (size_t i = 0; i < 0x4000; ++i, candidate += 0x10000)
			{
				if (candidate - module_base > 0x60000000)
				{
					break; // beyond the reach of a 32-bit displacement
				}

				auto* p = VirtualAlloc(reinterpret_cast<void*>(candidate),
				                       size + 2 * reloc_padding,
				                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
				if (p)
				{
					// Hand back the middle. VirtualAlloc zero-fills, so the slack
					// on both sides reads as null for out-of-range indices.
					return reinterpret_cast<void*>(
						reinterpret_cast<size_t>(p) + reloc_padding);
				}
			}

			return nullptr;
		}

		// Never call printf from here: it takes post_unpack down. note() formats
		// into a buffer and goes only to the component's trace file, so a patch
		// that stands down on a new build still says so. Diagnostic build only.
		void trace_text(const char* text);   // defined after trace_write

		template <typename... Args>
		void note(const char* fmt, Args... args)
		{
#ifndef SS_DIAG
			(void)fmt;
			((void)args, ...);
#else
			char buf[512]{};
			std::snprintf(buf, sizeof(buf), fmt, args...);
			for (auto* p = buf; *p; ++p)
			{
				if (*p == '\n' || *p == '\r')
				{
					*p = ' ';
				}
			}
			trace_text(buf);
#endif
		}

		// Where each table landed, filled by relocate(). The client-object table
		// holds absolute pointers into the client-UI array, so once both have
		// moved, every pointer (entries 0 and 1 too) is recomputed from the new
		// bases.
		size_t new_base_rva[8] = {};

		constexpr size_t gamepads_reserved_size = 0x1C0;
		size_t gamepads_reserved_rva = 0;

		bool reserve_gamepads_region()
		{
			if (gamepads_reserved_rva)
			{
				return true;
			}
			auto* reserved = allocate_near_module(gamepads_reserved_size);
			if (!reserved)
			{
				return false;
			}
			gamepads_reserved_rva = reinterpret_cast<size_t>(reserved) - base();
			return true;
		}

		bool relocate(const reloc_table& t, const size_t slot)
		{
			auto* fresh = allocate_near_module(t.new_size);

			if (!fresh)
			{
				note("[splitscreen] %s: no memory within reach of a 32-bit offset\n", t.name);
				return false;
			}

			const auto new_rva = reinterpret_cast<size_t>(fresh) - base();

			std::memcpy(fresh, reinterpret_cast<void*>(base() + t.base_rva), t.old_size);
			std::memset(static_cast<uint8_t*>(fresh) + t.old_size, 0, t.new_size - t.old_size);

			// Verify every reference against the image before writing any of them, so
			// a stale table cannot corrupt code.
			size_t deviating = 0;
			for (size_t i = 0; i < t.count; ++i)
			{
				const auto& r = t.refs[i];
				const auto* field = reinterpret_cast<const int32_t*>(
					base() + r.insn_rva + r.disp_offset);
				const auto expected = r.rip_relative
					                      ? static_cast<int32_t>(r.target_rva - (r.insn_rva + r.length))
					                      : static_cast<int32_t>(r.target_rva);
				if (!readable(field, sizeof(int32_t)) || *field != expected)
				{
					++deviating;
				}
			}

			// End-bound refs are verified in the same pass: all or nothing.
			for (size_t i = 0; i < t.end_count; ++i)
			{
				const auto& r = t.end_refs[i];
				const auto* field = reinterpret_cast<const int32_t*>(
					base() + r.insn_rva + r.disp_offset);
				const auto expected = r.rip_relative
					                      ? static_cast<int32_t>(r.target_rva - (r.insn_rva + r.length))
					                      : static_cast<int32_t>(r.target_rva);
				if (!readable(field, sizeof(int32_t)) || *field != expected)
				{
					++deviating;
				}
			}

			if (deviating)
			{
				note("[splitscreen] %s: %zu of %zu references do not match the image - "
				       "NOTHING written\n", t.name, deviating, t.count);
				return false;
			}

			size_t done = 0;
			for (size_t i = 0; i < t.count; ++i)
			{
				const auto& r = t.refs[i];
				const auto moved = new_rva + (r.target_rva - t.base_rva);
				const auto value = r.rip_relative
					                   ? static_cast<int32_t>(moved - (r.insn_rva + r.length))
					                   : static_cast<int32_t>(moved);

				if (write_bytes(reinterpret_cast<void*>(base() + r.insn_rva + r.disp_offset),
				                &value, sizeof(value)))
				{
					++done;
				}
			}

			// End-bound references: loops like `for (p = &a[0]; p < &a[N]; p++)` bake
			// in the end address. They get new base + NEW size; shifting them like
			// ordinary refs would still walk the old element count.
			size_t end_done = 0;
			for (size_t i = 0; i < t.end_count; ++i)
			{
				const auto& r = t.end_refs[i];
				auto* field = reinterpret_cast<void*>(base() + r.insn_rva + r.disp_offset);
				if (!readable(field, sizeof(int32_t)))
				{
					continue;
				}
				const auto moved = new_rva + t.new_size;
				const auto value = r.rip_relative
					                   ? static_cast<int32_t>(moved - (r.insn_rva + r.length))
					                   : static_cast<int32_t>(moved);
				if (write_bytes(field, &value, sizeof(value)))
				{
					++end_done;
				}
			}

			if (done == t.count && end_done == t.end_count)
			{
				if (slot < std::size(new_base_rva))
				{
					new_base_rva[slot] = new_rva;
				}
				return true;
			}
			note("[splitscreen] %s: only %zu/%zu references and %zu/%zu end bounds moved to RVA 0x%zX\n",
			       t.name, done, t.count, end_done, t.end_count, new_rva);
			return false;
		}

		// PC element size of the client-UI array (static ctor: `mov edx,0x1170 /
		// mov r8d,2`). PS4 clientUIActive_t is 0x1078; strides differ by platform.
		constexpr size_t client_ui_stride = 0x1170;


		// Guest identities for controllers 2 and 3 in the relocated userData
		// array. The client-object table has no writer (it is statically
		// initialised), so its entries are produced, not moved.
		//
		// Guests get a private copy of element 1's identity head plus their own
		// XUID (element 1's + i) and name digit ('0' + i). Element 1, not 0: it is
		// a signed-in secondary profile (+0x29 = 01), and existing that way at
		// boot is what earns controller 1 its storage and stats readiness.
		// Element 0 is the primary; copying it gave an empty name and a duplicate
		// identity.
		constexpr size_t userdata_xuid = 0x00;
		constexpr size_t userdata_name_digit = 0x11;
		constexpr size_t userdata_signedin = 0x30;


		// The copy cannot happen at post_unpack, where element 1 is still zeroed.
		// The table pointers are written at post_unpack; the guest records are
		// filled once element 1 is signed in.
		size_t guest_array_rva = 0;
		bool guests_filled = false;

		// Defined further down with the count patches; the session-mode fix in
		// fill_guests_when_ready() is gated on it.
		extern bool raise_local_client_count;

		void fill_guests_when_ready()
		{
			if (guests_filled || !guest_array_rva)
			{
				return;
			}

			const auto array = base() + guest_array_rva;
			const auto donor = array + client_ui_stride; // element 1

			uint32_t donor_signedin = 0;
			std::memcpy(&donor_signedin,
			            reinterpret_cast<const void*>(donor + userdata_signedin),
			            sizeof(donor_signedin));

			// signInState (+0x30; PS4 userData_t +0x70): 0 not signed in, 1 locally,
			// 2 to Live. LiveUser_IsSignedIn is `signInState > 0` on both platforms,
			// so fill as soon as the donor reaches 1: the boot pump runs only a few
			// times, and mirror_signin_state keeps the guests level afterwards.
			if (donor_signedin == 0)
			{
				return; // not populated yet - try again next tick
			}

			// Do not delay the fill past boot: controller 2 then gets no storage.

			// Copy only the identity head, never the whole record. Storage_Pump needs
			// only xuid and signInState (both in the first 0x40 bytes, which hold no
			// pointers). The whole 0x1170 record duplicates heap pointers the profile
			// owns; copying it crashed.
			constexpr size_t identity_head = 0x40;
			static_assert(identity_head > userdata_signedin, "signInState must be copied");

			for (size_t i = 2; i < 4; ++i)
			{
				const auto dst = array + i * client_ui_stride;
				std::memcpy(reinterpret_cast<void*>(dst),
				            reinterpret_cast<const void*>(donor), identity_head);

				// Guest xuid = donor xuid + i. Do not use the console guest-id formula
				// (dwGetGuestUserID): the PC does not implement it, and the game's own
				// working guest (element 1) already uses an id space unrelated to the
				// host's.
				uint64_t xuid = 0;
				std::memcpy(&xuid, reinterpret_cast<const void*>(donor + userdata_xuid),
				            sizeof(xuid));
				xuid += i;
				std::memcpy(reinterpret_cast<void*>(dst + userdata_xuid), &xuid, sizeof(xuid));

				*reinterpret_cast<char*>(dst + userdata_name_digit) =
					static_cast<char>('0' + static_cast<int>(i));
			}

			// Put the session in local-splitscreen mode, as the console does when
			// splitscreen is activated (Lua SessionModeSetOffline). Otherwise a lobby
			// entered from the main menu stays a bot-game session ("2 Players
			// (18 Max)") and the third seat joins through the network path. Calls the
			// engine's own setter (SetNetworkMode, reached from the
			// SessionModeSetOffline binding 0x01FD0170) instead of poking the bitfield.
			// Only ever forces offline; this mod is offline-only.
			// Known gap: this runs once at boot, while the mode is still 0, so a later
			// menu choice is not corrected (PRIVATE GAME works because that menu sets
			// offline itself). A real fix must run when the lobby is created.
			if (raise_local_client_count)
			{
				constexpr uint32_t session_state_rva = 0x1686E874;
				constexpr uint32_t set_network_mode_rva = 0x020EAE30;
				constexpr uint32_t network_mode_mask = 0x3C0;
				constexpr uint32_t network_mode_shift = 6;

				const auto* state = reinterpret_cast<const uint32_t*>(
					base() + session_state_rva);
				if (readable(state, sizeof(uint32_t)))
				{
					const auto net = (*state & network_mode_mask) >> network_mode_shift;
					if (net != 0)
					{
						const auto set_net = reinterpret_cast<void (*)(int)>(
							base() + set_network_mode_rva);
						set_net(0);
					}
				}
			}

			guests_filled = true;
		}

		// PC Storage_Pump(ControllerIndex_t) (PS4 0xF7F120). The game runs it only
		// a few times during boot and not again once the menu is up, so a guest
		// that appears late gets no storage. Calling the game's own function once
		// more is the same call the boot sequence makes.
		constexpr uint32_t storage_pump_rva = 0x0221A680;
		constexpr uint8_t storage_pump_prologue[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40};

