// Game build: BlackOps3.exe, PE checksum 0x06531394 (Steam build 24784313,
// the exe ezz BOIII 3.0 runs). Every address in CODE targets that build.
// Some addresses in comments and trace strings still name older builds
// (0x06517980, 0x0888C368); translate them with tools/port_map.py
// (BO3_PORT_PAIR=2026 for 0x06517980).
#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"
#include "game/utils.hpp"
#include "scheduler.hpp"
#include "splitscreen_reloc.hpp"
#include "splitscreen_signin.hpp"

#include <utils/hook.hpp>
#include <utils/finally.hpp>
#include "splitscreen_ezz.hpp"

#include <d3d11.h>   // sun shadow sidecar views (COM calls only, no import library)

// Local splitscreen for up to four players; the stock PC build stops at two.
//
// PS4 and PC share the engine, but the PC port fixed the local-client count
// at two: per-client arrays that are [4] on PS4 are [2] here, and loops and
// range checks stop at 2. This component, applied from post_unpack, does:
//   1. relocate those arrays so slots 2 and 3 exist instead of overwriting
//      whatever the linker put next (tables in splitscreen_reloc.hpp)
//   2. widen the one-byte loop bounds and range checks that stop at two
//      (local_client_count_patches, storage_patches and the later sections)
//   3. give guests an identity, storage and a seat the way the game does
//   4. repair one per-client base pointer that arrives corrupted, and keep
//      the launch handshake from stalling on the added local clients
//
// It is a component rather than an external script because player data for
// a third local client is rejected at startup, before any tool can attach.
//
// s_gamePads is relocated late, at the first real two-player lobby: doing it
// at startup crashed gamepad init (RVA 0x022E9550). Four-player-only pieces
// remain in tools/chain4.ps1.
//
// Conventions: every patch checks the original bytes first and stands down
// on a mismatch. note() writes to the trace file; set_status() writes the
// status block that is read from outside the process. BO3_SS_SKIP=<group>
// leaves one group of patches out (list next to the component class).
//
// The PS4 debug build (cod_Debug.elf) is the reference for names and logic;
// struct sizes and strides differ between platforms and are measured on PC.
// The investigation history behind each patch is in LOG.md and git history.

namespace splitscreen
{
	namespace
	{
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
		// The floor patch's RVA, so BO3_SS_SKIP=floor can leave out exactly this
		// entry of the table below.
		constexpr size_t alloc_floor_rva = 0x0135D68C;

		constexpr byte_patch local_client_count_patches[] = {
			// CL_AllocatePerLocalClientMemory (0x0135D650) computes
			// max(CL_SplitscreenPlayerCount(), 2). This is a floor, not a cap; the
			// result feeds CG/FX/CL_AllocateClientMemory and the cl_maxLocalClients
			// store at 0x0135D489, so no further patch is needed there.
			{0x0135D68C, 0x02, 0x04, "local-client count floor: max(count,2) -> max(count,4)"},

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

		// s_storageMem.pool: zero until AllocateMemory has run, so it shows whether
		// the patches got in before Storage_Init.
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

		// Allocator diagnostics, read from outside via the status block.
		uint32_t alloc_regions_seen = 0;
		uint32_t alloc_free_seen = 0;
		uint32_t alloc_last_error = 0;

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
				++alloc_regions_seen;
				if (candidate - module_base > 0x60000000)
				{
					break; // beyond the reach of a 32-bit displacement
				}

				auto* p = VirtualAlloc(reinterpret_cast<void*>(candidate),
				                       size + 2 * reloc_padding,
				                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
				if (p)
				{
					++alloc_free_seen;
					// Hand back the middle. VirtualAlloc zero-fills, so the slack
					// on both sides reads as null for out-of-range indices.
					return reinterpret_cast<void*>(
						reinterpret_cast<size_t>(p) + reloc_padding);
				}
				alloc_last_error = GetLastError();
			}

			return nullptr;
		}

		// Never call printf from here: it takes post_unpack down. note() formats
		// into a buffer and goes only to the component's trace file (refused in the
		// player build), so a patch that stands down on a new build still says so.
		void trace_text(const char* text);   // defined after trace_write

		template <typename... Args>
		void note(const char* fmt, Args... args)
		{
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
		}

		// Defined next to the status block; declared here for relocate().
		void set_status(size_t index, uint32_t value);

		// Where each table landed, filled by relocate(). The client-object table
		// holds absolute pointers into the client-UI array, so once both have
		// moved, every pointer (entries 0 and 1 too) is recomputed from the new
		// bases.
		size_t new_base_rva[8] = {};

		// Per-table breadcrumb in status slot 11 + slot, the last stage reached:
		// 1 entered, 2 allocated, 3 copied, 4 verified, 5 written, 6 reported.
		void mark(const size_t slot, const uint32_t stage)
		{
			set_status(11 + slot, stage);
		}

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
			mark(slot, 1);

			auto* fresh = allocate_near_module(t.new_size);

			// Publish the allocator counters now: they diagnose exactly the case where
			// a later step fails.
			set_status(5, alloc_regions_seen);
			set_status(6, alloc_free_seen);
			set_status(7, alloc_last_error);

			if (!fresh)
			{
				note("[splitscreen] %s: no memory within reach of a 32-bit offset\n", t.name);
				return false;
			}
			mark(slot, 2);

			const auto new_rva = reinterpret_cast<size_t>(fresh) - base();

			std::memcpy(fresh, reinterpret_cast<void*>(base() + t.base_rva), t.old_size);
			std::memset(static_cast<uint8_t*>(fresh) + t.old_size, 0, t.new_size - t.old_size);
			mark(slot, 3);

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
			mark(slot, 4);

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

			mark(slot, 5);
			note("[splitscreen] %s: %zu/%zu references moved to RVA 0x%zX (0x%X -> 0x%X bytes)\n",
			       t.name, done, t.count, new_rva, t.old_size, t.new_size);
			mark(slot, 6);
			if (done == t.count && end_done == t.end_count)
			{
				if (slot < std::size(new_base_rva))
				{
					new_base_rva[slot] = new_rva;
				}
				return true;
			}
			return false;
		}

		// PC element size of the client-UI array (static ctor: `mov edx,0x1170 /
		// mov r8d,2`). PS4 clientUIActive_t is 0x1078; strides differ by platform.
		constexpr size_t client_ui_stride = 0x1170;

		// PC element size of Storage (PS4 0x8658): same layout, but files are
		// 0x220 here against 0x218; inShutdown is at +0x8850.
		constexpr size_t storage_stride = 0x8958;

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
						note("[splitscreen] session network mode %u -> 0 (local)\n", net);
					}
				}
			}

			guests_filled = true;
			set_status(15, donor_signedin);
		}

		// PC Storage_Pump(ControllerIndex_t) (PS4 0xF7F120). The game runs it only
		// a few times during boot and not again once the menu is up, so a guest
		// that appears late gets no storage. Calling the game's own function once
		// more is the same call the boot sequence makes.
		constexpr uint32_t storage_pump_rva = 0x0221A680;
		constexpr uint8_t storage_pump_prologue[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40};

		size_t storage_base_rva = 0;
		bool guests_pumped = false;

		// Tick counters per scheduler pipeline, published to show which pipelines
		// actually run (main once ticked only once in sixty seconds).
		uint32_t ticks_main = 0;
		uint32_t ticks_renderer = 0;
		uint32_t ticks_async = 0;

		// --- s_targets: the storage pending-vector table ---
		//
		// Needed before controller 2 or 3 can be pumped at all. PS4
		// GetPendingVector (0xF81A40): &s_targets + type*ROW + 0x30
		// + controller*0x410 + operation*0x208, with ROW 0x850 on PC (two
		// controllers) and 0x1070 on PS4 (four). Four target types, so the table
		// grows 0x2140 -> 0x41C0. Without it, storage calls for controller 2
		// overwrite the next target's activeQuery pointer. The row stride changes,
		// so each row is copied on its own.
		//
		// Do not touch the SIB scale in `lea rbx,[rbx + r13*2]`: that 2 is
		// operations per controller (0x410 == 2 * 0x208), not the controller count.
		constexpr size_t targets_rva = 0x033BCDF0;
		constexpr size_t targets_types = 4;
		constexpr size_t targets_old_row = 0x850;
		constexpr size_t targets_new_row = 0x1070;

		struct targets_lea
		{
			uint32_t insn_rva;  // 7-byte rip-relative lea, disp32 at +3
			uint32_t offset;    // offset into the table it points at
		};

		constexpr targets_lea targets_leas[] = {
			{0x0221B146, 0x00}, {0x0221B28F, 0x00}, {0x0221B2E2, 0x00}, {0x0221B50A, 0x00},
			{0x0221B1A4, 0x08},
			{0x0221B223, 0x20}, {0x0221B203, 0x28},
			{0x0221B2BE, 0x30}, {0x0221B5D1, 0x30},
		};

		// Every imm32 in the storage TU that carries the old row stride 0x850 (six,
		// found with tools/pe_find.py). One is not an imul but `add rdi, 0x850`
		// (0x02277CF4) in StorageTarget_GetType (PS4 0xF81890); missing it walks
		// the new table with the old geometry. The old total 0x2140 appears nowhere.
		constexpr uint32_t targets_strides[] = {
			0x0221B152, 0x0221B1C4, 0x0221B20D, 0x0221B22D, 0x0221B28B, 0x0221B5E2,
		};

		// Enabled. It matches the console layout, and off is not safe either: once
		// s_storage[2] holds an xuid the game does storage work for controller 2
		// by itself, and every such call reads past a two-controller row.
		// History: LOG.md, "targets_widen".
		constexpr bool enable_targets_widen = true;

		bool widen_storage_targets()
		{
			if (!enable_targets_widen)
			{
				return false;
			}

			const auto module_base = base();
			const auto old_table = module_base + targets_rva;

			// Verify every site before writing anything: all or nothing.
			for (const auto& l : targets_leas)
			{
				const auto insn = module_base + l.insn_rva;
				const auto* b = reinterpret_cast<const uint8_t*>(insn);
				if (b[1] != 0x8D || (b[0] != 0x48 && b[0] != 0x4A && b[0] != 0x4C && b[0] != 0x4E))
				{
					return false;
				}
				int32_t disp = 0;
				std::memcpy(&disp, reinterpret_cast<const void*>(insn + 3), sizeof(disp));
				if (insn + 7 + disp != old_table + l.offset)
				{
					return false;
				}
			}
			for (const auto rva : targets_strides)
			{
				uint32_t imm = 0;
				std::memcpy(&imm, reinterpret_cast<const void*>(module_base + rva), sizeof(imm));
				if (imm != targets_old_row)
				{
					return false;
				}
			}

			auto* fresh = allocate_near_module(targets_types * targets_new_row);
			if (!fresh)
			{
				return false;
			}
			const auto new_table = reinterpret_cast<size_t>(fresh);

			std::memset(fresh, 0, targets_types * targets_new_row);
			for (size_t t = 0; t < targets_types; ++t)
			{
				std::memcpy(reinterpret_cast<void*>(new_table + t * targets_new_row),
				            reinterpret_cast<const void*>(old_table + t * targets_old_row),
				            targets_old_row);
			}

			for (const auto& l : targets_leas)
			{
				const auto insn = module_base + l.insn_rva;
				const auto disp = static_cast<int32_t>(
					static_cast<int64_t>(new_table + l.offset) - static_cast<int64_t>(insn + 7));
				write_bytes(reinterpret_cast<void*>(insn + 3), &disp, sizeof(disp));
			}
			const uint32_t row = static_cast<uint32_t>(targets_new_row);
			for (const auto rva : targets_strides)
			{
				write_bytes(reinterpret_cast<void*>(module_base + rva), &row, sizeof(row));
			}

			set_status(27, static_cast<uint32_t>(new_table - module_base));
			return true;
		}

		// --- s_localFileOpData ---
		//
		// PS4 StartOp (0xF7C740) indexes s_localFileOpData[ci] (stride 0x1820), a
		// four-element global. On PC it is [2] at 0x17908CF0 and element 2 lands
		// on the A/B experiments table, so controller 2's local-file work
		// corrupted it (crash at 0x02275034). Only the count changes, so it is a
		// flat copy with one address-taking reference (0x02274BFE).
		constexpr size_t localfileop_rva = 0x17889DF0;
		constexpr size_t localfileop_elem = 0x1820;
		constexpr uint32_t localfileop_lea = 0x022180CE; // 7-byte lea, disp32 at +3

		// Where it ended up. A gamer-profile TaskRecord holds its LocalFileOpData
		// pointer at +0x48, so (opData - base) / 0x1820 is the controller index.
		size_t localfileop_new_rva = 0;

		bool widen_local_file_ops()
		{
			const auto module_base = base();
			const auto old_array = module_base + localfileop_rva;
			const auto insn = module_base + localfileop_lea;

			const auto* b = reinterpret_cast<const uint8_t*>(insn);
			if (b[0] != 0x48 || b[1] != 0x8D)
			{
				return false;
			}
			int32_t disp = 0;
			std::memcpy(&disp, reinterpret_cast<const void*>(insn + 3), sizeof(disp));
			if (insn + 7 + disp != old_array)
			{
				return false;
			}

			auto* fresh = allocate_near_module(4 * localfileop_elem);
			if (!fresh)
			{
				return false;
			}
			const auto new_array = reinterpret_cast<size_t>(fresh);
			std::memset(fresh, 0, 4 * localfileop_elem);
			std::memcpy(fresh, reinterpret_cast<const void*>(old_array),
			            2 * localfileop_elem);

			const auto new_disp = static_cast<int32_t>(
				static_cast<int64_t>(new_array) - static_cast<int64_t>(insn + 7));
			if (!write_bytes(reinterpret_cast<void*>(insn + 3), &new_disp, sizeof(new_disp)))
			{
				return false;
			}

			localfileop_new_rva = new_array - module_base;
			set_status(29, static_cast<uint32_t>(localfileop_new_rva));
			return true;
		}

		// Why guest pumping retries: PS4 Storage_Pump assigns a new xuid in two
		// stages. The first call clears storage and sets inShutdown; a later call
		// assigns the xuid, but only once no storage target is busy, and targets
		// stop being busy only when ProcessQueue at the end of the same function
		// drains them. A controller pumped once and abandoned stays in shutdown.
		constexpr uint32_t max_pump_attempts = 120;
		uint32_t pump_attempts = 0;

		// Guest pumps go through the detour: once installed, Storage_Pump starts
		// with a jmp into our stub, and invoke() runs the original.
		utils::hook::detour storage_pump_hook;
		bool storage_pump_hooked = false;
		bool inside_guest_pump = false;

		void pump_guest_storage()
		{
			if (guests_pumped || !guests_filled || !storage_base_rva)
			{
				return;
			}

			// Bounded: if this many spaced attempts have not drained the queues,
			// something else is wrong.
			if (pump_attempts >= max_pump_attempts)
			{
				return;
			}
			set_status(26, ++pump_attempts);

			// The detour is installed only after its prologue was verified; without
			// it, do not call blind.
			if (!storage_pump_hooked)
			{
				guests_pumped = true;
				set_status(20, 2);
				return;
			}

			inside_guest_pump = true;
			storage_pump_hook.invoke<void>(2);
			storage_pump_hook.invoke<void>(3);
			inside_guest_pump = false;

			// Stop once both guests hold an xuid; a non-zero reading is conclusive.
			const auto storage = base() + storage_base_rva;
			uint64_t xuid2 = 0;
			uint64_t xuid3 = 0;
			std::memcpy(&xuid2, reinterpret_cast<const void*>(storage + 2 * storage_stride),
			            sizeof(xuid2));
			std::memcpy(&xuid3, reinterpret_cast<const void*>(storage + 3 * storage_stride),
			            sizeof(xuid3));
			set_status(23, static_cast<uint32_t>(xuid3));
			if (xuid2 && xuid3)
			{
				guests_pumped = true;
				set_status(20, 1);
			}
		}

		void pump_on_main()
		{
			set_status(21, ++ticks_main);
		}

		void pump_on_renderer()
		{
			set_status(24, ++ticks_renderer);
		}

		void count_async_ticks()
		{
			set_status(25, ++ticks_async);
		}

		// Quiescence gate: pump the guests from the async pipeline only after the
		// game's own Storage_Pump count has stayed unchanged for several checks.
		// Pumping while the game was still pumping made s_storage[0] and [1] lose
		// their xuids.
		uint32_t last_seen_pumps = 0;
		uint32_t quiet_ticks = 0;
		constexpr uint32_t quiet_required = 6; // x500ms = 3 s of no game storage work

		void reap_guest_tasks(); // defined with the detour, below

		void pump_guests_when_quiet()
		{
			if (guests_pumped || !guests_filled || !storage_base_rva)
			{
				return;
			}

			// Also wait until the game has pumped at least once: before that storage
			// is not initialised, and calling in crashed (null read at 0x022E9B94).
			if (ticks_main == 0)
			{
				return;
			}

			if (ticks_main != last_seen_pumps)
			{
				last_seen_pumps = ticks_main;
				quiet_ticks = 0;
				return;
			}
			if (++quiet_ticks < quiet_required)
			{
				return;
			}
			quiet_ticks = 0;

			// Reap first: Storage_Pump refuses to assign while any target is
			// busy, and the busy queries are global task lookups.
			reap_guest_tasks();
			pump_guest_storage();
		}

		// Com_ControllerIndex_GetLocalClientNum (PC 0x020EF7C0). On PS4 the lobby's
		// gobblegum row reaches BG_UnlockablesGetLocalCACRoot (0xE4130), which
		// asserts that CG_GetLocalClientGlobals(GetLocalClientNum(ci)) is non-null.
		// Retail PC has no asserts, so a -1 silently draws an empty row. Published
		// per controller (one byte, -1 = 0xFF) so the value is measured.
		constexpr uint32_t local_client_num_rva = 0x020E3040;

		// cl_maxLocalClients (old RVA 0x053A2720), stored by the allocator at
		// 0x0135D489. Published so the count raise can be confirmed from outside.
		constexpr uint32_t cl_max_local_clients_rva = 0x05323720;
		constexpr uint32_t seed_max_local_clients = 4;
		uint32_t max_local_seeds = 0;
		// Enables the count patches and the cl_maxLocalClients hold. The caller
		// applies them only after the container relocations succeeded.
		bool raise_local_client_count = true;

		// Off: forcing cl_maxLocalClients fights the allocator, which writes that
		// global itself from the count it really used.
		bool hold_max_local_clients = false;

		// --- why SigninLocalClient(2) refuses ---
		//
		// Native SigninLocalClient (0x01F17AD0) returns 2.0f to Lua when the
		// predicate 0x01E0B520(ci) is false. The predicate and its sub-checks only
		// read state, so they are called directly and the results published as a
		// bit mask (bit clear = that check failed). No detour or code cave needed.
		//
		// Sub-checks, in the order the predicate evaluates them:
		//   0x01EA9A30(ci)        ten-record stats-source walk   (0x0340D660)
		//   0x01EAF490(ci, 1)     six-record loadout-reset walk  (0x0340D880)
		//   0x015E2C90(ci, 1)     -> 0x02276E30(ci,3,0) && (ci,5,0)
		//   0x02276E30(ci, t, 0)  for each required file type below
		constexpr uint32_t signin_predicate_rva = 0x01DFEA90;
		constexpr uint32_t ten_record_walk_rva = 0x01E9CFA0;
		constexpr uint32_t six_record_walk_rva = 0x01EA2A00;
		constexpr uint32_t storage_pair_rva = 0x015E2CB0;
		constexpr uint32_t storage_has_file_rva = 0x0221A300;

		// File types the predicate requires, read off the disassembly of 0x01E0B520
		// (eleven calls). Take this list from the disassembly, not from memory.
		constexpr int signin_required_files[] = {0, 1, 7, 9, 0xB, 0xD, 0xF, 0x12, 0x14, 0x1B, 0x1C};

		void probe_signin_predicate(const int ci)
		{
			const auto pred = reinterpret_cast<bool (*)(int)>(base() + signin_predicate_rva);
			const auto ten = reinterpret_cast<bool (*)(int, int)>(base() + ten_record_walk_rva);
			const auto six = reinterpret_cast<bool (*)(int, int)>(base() + six_record_walk_rva);
			const auto pair = reinterpret_cast<bool (*)(int, int)>(base() + storage_pair_rva);
			const auto has = reinterpret_cast<bool (*)(int, int, int)>(base() + storage_has_file_rva);

			uint32_t bits = 0;
			// ezz BOIII detours this predicate with a side effect (Storage_Pump);
			// called from this async thread it crashes in the game's va(). A
			// diagnostic must not run game code on the wrong thread: skip it if hooked.
			const bool pred_hooked = *reinterpret_cast<const uint8_t*>(base() + signin_predicate_rva) == 0xE9;
			if (!pred_hooked && pred(ci)) { bits |= 1u << 0; }   // the whole predicate
			if (ten(ci, 1)) { bits |= 1u << 1; }
			if (six(ci, 1)) { bits |= 1u << 2; }
			if (pair(ci, 1)) { bits |= 1u << 3; }
			// storage_pair's own two, broken out so a failure is attributable
			if (has(ci, 3, 0)) { bits |= 1u << 4; }
			if (has(ci, 5, 0)) { bits |= 1u << 5; }

			uint32_t bit = 6;
			for (const auto t : signin_required_files)
			{
				if (has(ci, t, 0)) { bits |= 1u << bit; }
				++bit;
			}
			set_status(63, bits);
		}

		// splitscreen_playerCount. This slot holds the dvar pointer; the current int
		// is at +0x28. CL_SplitscreenPlayerCount (PS4 0x1516BE0) returns it, and
		// CL_AllocatePerLocalClientMemory allocates max(count, 2) of the whole
		// per-local-client family (clients, clientConnections, snapshots,
		// parseEntities, the 0x1E940 block).
		// Raise the dvar, not the allocator's floor of 2: patching only the floor made
		// the allocator disagree with every other reader and was the round-start crash.
		constexpr uint32_t splitscreen_player_count_dvar_rva = 0x05355A00;
		constexpr uint32_t dvar_current_offset = 0x28;

		uint8_t* active_count_slots = nullptr;

		// CL_LocalClients_SetAllUsedActive(), no arguments.
		// The count must already be right in the lobby: PS4 CL_ConnectFromLobby
		// allocates (0x4155A0) before it activates the clients (0x41566C).
		constexpr uint32_t set_all_used_active_rva = 0x027C19C0;

		// Dvar_SetInt. Calling SetAllUsedActive is not enough: its early-out skips
		// unless a client's active state changes, and local client 2 is outside its
		// loop bound of 2. Do not call Dvar_SetInt from this DLL, see below.
		constexpr uint32_t dvar_set_int_rva = 0x0226B3A0;

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
		uint32_t active_refreshes = 0;

		// Com_LocalClient_IsBeingUsed, the local client's controller index and
		// LiveUser_IsSignedIn: the conditions PS4 GetCountUsedAndSignedInLocalClients
		// (0xD2EE40) counts over lc 0..3. The PC bounds that loop at 2.
		constexpr uint32_t is_being_used_rva = 0x020E3210;
		constexpr uint32_t lc_controller_index_rva = 0x020E31B0;
		constexpr uint32_t live_user_is_signed_in_rva = 0x01EBA5A0;

		// Status 72, one byte per local client: bit 0 IsBeingUsed(lc), bit 1 signed in,
		// bits 4-7 the controller index (0xF = -1). Pure predicates, safe to call.
		void probe_local_client_slots()
		{
			const auto used = reinterpret_cast<bool (*)(int)>(base() + is_being_used_rva);
			const auto ctrl = reinterpret_cast<int (*)(int)>(base() + lc_controller_index_rva);
			const auto signed_in = reinterpret_cast<bool (*)(int)>(base() + live_user_is_signed_in_rva);

			uint32_t packed = 0;
			for (int lc = 0; lc < 4; ++lc)
			{
				uint32_t b = 0;
				const bool u = used(lc);
				if (u) { b |= 1u; }
				const int ci = ctrl(lc);
				if (u && signed_in(ci)) { b |= 2u; }
				b |= static_cast<uint32_t>(ci & 0xF) << 4;
				packed |= b << (lc * 8);
			}
			set_status(72, packed);
		}

		// Hold splitscreen_playerCount at the real number of local clients.
		// PS4 CL_LocalClient_SetActive (0x15167D0) sets it from
		// CL_LocalClient_GetActiveCount (0x1516A20, i < 4). The PC unrolls that count
		// to two elements, so the engine alone never writes more than 2, and it writes
		// its value back on every activation toggle, so a one-shot set does not hold.
		// Only ever raised; lowering it would fight the engine when a player leaves.
		uint32_t player_count_writes = 0;

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
				set_status(82, 0); // not registered yet - CL_SplitscreenPlayerCount returns 1
				return;
			}

			auto* current = reinterpret_cast<uint32_t*>(dvar + dvar_current_offset);
			if (!readable(current, sizeof(uint32_t)))
			{
				set_status(82, 0xFFFFFFFF);
				return;
			}

			// Widen the dvar's domain too. It is registered as 1..4 but reads {1, 2} at
			// runtime, so the engine rejects its own Dvar_SetInt(3) ("not a valid value").
			// The domain is {min, max} at +0x88. Only max is raised (2 -> 4), so a stock
			// 2-player session is unaffected.
			constexpr size_t dvar_domain_offset = 0x88;
			auto* domain_max = reinterpret_cast<uint32_t*>(
				dvar + dvar_domain_offset + sizeof(uint32_t));
			// No status slot is free; the live dvar's +0x8C reads 4 once this has run.
			if (readable(domain_max, sizeof(uint32_t)) && *domain_max == 2)
			{
				const uint32_t four = 4;
				write_bytes(domain_max, &four, sizeof(four));
			}

			// Take the larger of the predicate count and the seat count. In a round the
			// guests read as not signed in, so the predicate count alone drops to 1 and
			// the map-load reallocation shrank cl_maxLocalClients back to 2.
			const auto want = std::max(true_local_client_count(), seat_count());
			set_status(82, *current);
			set_status(83, want);

			if (want > 1 && *current < want)
			{
				if (write_bytes(current, &want, sizeof(want)))
				{
					set_status(84, ++player_count_writes);
				}
			}
		}

		void probe_local_client_nums()
		{
			const auto fn = reinterpret_cast<int (*)(int)>(base() + local_client_num_rva);
			uint32_t packed = 0;
			for (int ci = 0; ci < 4; ++ci)
			{
				packed |= static_cast<uint32_t>(fn(ci) & 0xFF) << (ci * 8);
			}
			set_status(51, packed);

			uint32_t max_local = 0;
			std::memcpy(&max_local,
			            reinterpret_cast<const void*>(base() + cl_max_local_clients_rva),
			            sizeof(max_local));
			set_status(61, max_local);

			// Holding cl_maxLocalClients at 4 is off (hold_max_local_clients). The
			// allocator writes this global from the count it really used (PS4 0x41744B),
			// so forcing 4 makes loops run to four over memory carved for fewer, which
			// crashed. Status 62 counts re-applications.
			if (hold_max_local_clients && raise_local_client_count && max_local == 2)
			{
				auto* v = reinterpret_cast<uint32_t*>(base() + cl_max_local_clients_rva);
				if (write_bytes(v, &seed_max_local_clients, sizeof(seed_max_local_clients)))
				{
					set_status(62, ++max_local_seeds);
				}
			}

			// Only worth asking once controller 2 has a seat.
			if (fn(2) >= 0)
			{
				probe_signin_predicate(2);
			}

			probe_local_client_slots();

			// Set splitscreen_playerCount here, in the lobby: allocation happens before
			// activation, and per_controller_update_stub stops running after a
			// splitscreen sign-in. max_local >= 2 is the "game is up" test; touching the
			// dvar system during early startup black-screened the client.
			if (active_count_slots != nullptr && max_local >= 2)
			{
				const auto live = true_local_client_count();
				set_status(91, live);
				set_status(92, last_active_refresh);
				if (live > 1 && live != last_active_refresh
				    && set_splitscreen_player_count(live))
				{
					last_active_refresh = live;
					set_status(88, ++active_refreshes);
					set_status(89, live);
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
		uint32_t active_mirrored = 0;

		void mirror_signin_state()
		{
			// Unconditional: the probe is wanted during boot too.
			probe_local_client_nums();

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
					set_status(55, ++active_mirrored);
				}
			}
		}

		// TaskManager2_ProcessTasks(ControllerIndex_t), PS4 0x0101CEC0. The game only
		// calls it for the controller it is working on, so controller 2's finished
		// 'hdd' gamer-profile task is never reaped. That keeps StorageTarget_IsBusy
		// true for every controller and stalls Storage_Pump.
		// Storage work must run on the game's own thread, never from the async
		// pipeline while the game might touch storage.
		constexpr uint32_t process_tasks_rva = 0x02253C50;
		constexpr uint8_t process_tasks_prologue[] = {0x83, 0xF9, 0xFF};
		bool process_tasks_ok = false;

		// The reap runs after the per-controller update (the only caller of
		// Storage_Pump) has returned. Reaping inside the Storage_Pump detour re-entered
		// storage through the completion handlers and halved startup survival.
		// reaper_enabled toggles the reap for bisecting. With it on the game stayed
		// alive but rendered black: check that the game draws, not only that it runs.
		bool reaper_enabled = false;

		// Wait until the frontend is up before reaping. Reaping during boot runs the
		// storage read callbacks out of order and leaves the renderer black. PS4 also
		// re-reads at sign-in time (Storage_UserSignedIn), not at boot.
		constexpr uint32_t reap_after_updates = 2000;
		uint32_t update_calls = 0;

		// ClearStorage (PS4 0xF7E7E0, clear_storage_rva further down) makes a
		// controller re-read: it clears each storage target, resets
		// readOnLoginProcessed[t] and zeroes the xuid so Storage_Pump re-assigns it.
		// It only marks state dirty; the game redoes the login read on its own frames.
		// If the re-assign never happens the controller loses its storage (status 35
		// records the xuid afterwards).

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
		constexpr uint32_t settings_read_result_rva = 0x0164DC50;
		constexpr uint8_t settings_read_result_prologue[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x20};
		constexpr uint32_t storage_reset_rva = 0x0221AB10;

		utils::hook::detour settings_read_result_hook;
		uint32_t guest_settings_completions = 0;
		uint32_t guest_settings_resets = 0;

		// File 0 is allowed only while this is true. If the hook is not installed,
		// the read filter refuses file 0 again. Fail safe.
		bool settings_result_neutered = false;

		char settings_read_result_stub(const int controller, const int file_type, const int slot,
		                               const int result, void* ddl_context)
		{
			if (controller >= 2)
			{
				// Status 64: 0x8000 | controller << 8 | StorageResult. Success means the
				// guest's own .cgp loaded; failure means Storage_Reset builds the context.
				set_status(64, 0x8000u | (static_cast<uint32_t>(controller) << 8)
				                       | (static_cast<uint32_t>(result) & 0xFFu));
				set_status(65, ++guest_settings_completions);

				if (result != 0)
				{
					const auto reset = reinterpret_cast<void (*)(int, int, int)>(
						base() + storage_reset_rva);
					reset(controller, 0, 0);
					set_status(66, ++guest_settings_resets);
				}

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
		constexpr uint32_t shoutcaster_read_result_rva = 0x01650640;
		// 0x40 is a redundant REX prefix on push rbx.
		constexpr uint8_t shoutcaster_read_result_prologue[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20};

		utils::hook::detour shoutcaster_read_result_hook;
		uint32_t guest_shoutcaster_completions = 0;
		uint32_t guest_shoutcaster_resets = 0;
		bool shoutcaster_result_neutered = false;

		char shoutcaster_read_result_stub(const int controller, const int file_type, const int slot,
		                                  const int result, void* ddl_context)
		{
			if (controller >= 2)
			{
				set_status(68, 0x8000u | (static_cast<uint32_t>(controller) << 8)
				                       | (static_cast<uint32_t>(result) & 0xFFu));
				set_status(69, ++guest_shoutcaster_completions);

				if (result != 0)
				{
					const auto reset = reinterpret_cast<void (*)(int, int, int)>(
						base() + storage_reset_rva);
					reset(controller, 1, 0);
					set_status(70, ++guest_shoutcaster_resets);
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
		uint32_t guest_reads_allowed = 0;
		uint32_t guest_reads_blocked = 0;
		uint64_t guest_read_mask = 0;

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
				// Status 49/50: 64-bit mask of the file types guests request.
				if (file_type >= 0 && file_type < 64)
				{
					const auto bit = 1ull << file_type;
					guest_read_mask |= bit;
					set_status(49, static_cast<uint32_t>(guest_read_mask));
					set_status(50, static_cast<uint32_t>(guest_read_mask >> 32));
				}
				// Everything not listed stays blocked. Allowing every type killed the
				// process: PS4 SettingsReadResult also writes s_settingsGlob[ci], another
				// per-controller array that is likely [2] on the PC.
				if (!allowed)
				{
					set_status(47, ++guest_reads_blocked);
					return false;
				}
				set_status(46, ++guest_reads_allowed);
			}
			return storage_read_hook.invoke<bool>(controller, file_type, index);
		}

		constexpr uint32_t storage_read_rva = 0x0221AA70;
		constexpr uint8_t storage_read_prologue[] = {0x48, 0x89, 0x5C, 0x24, 0x08};

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

		// Highest seat count seen this session with flags == 1. Drives the one-way
		// allocation-floor commit in the stub.
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

			// Verify every reference before writing anything (all or nothing).
			uint32_t already = 0;
			for (const auto& r : signin_refs)
			{
				const auto at = reinterpret_cast<const uint8_t*>(module_base + r.disp_rva);
				if (std::memcmp(at, r.new_bytes, 4) == 0)
				{
					++already;
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

			uint32_t done = 0;
			for (const auto& r : signin_refs)
			{
				if (write_bytes(reinterpret_cast<void*>(module_base + r.disp_rva),
				                r.new_bytes, 4))
				{
					++done;
				}
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

			set_status(44, done);
			set_status(45, already);
			signin_relocated = true;
			return true;
		}

		// Re-assert the new seats' controllerIndex if something resets it, and count
		// it (status 52). PS4 Com_InitClientGameStates memsets the array and never
		// writes controllerIndex, so a late initializer would put
		// GetLocalClientNum(2) back to -1. A count of 0 means nothing clobbers it.
		uint32_t seat_reasserts = 0;

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
					set_status(52, ++seat_reasserts);
				}
			}

			// Status 53: slot 2's raw controllerIndex.
			uint32_t raw = 0;
			std::memcpy(&raw,
			            reinterpret_cast<const void*>(new_array + 2 * signin_stride
				            + cgs_controller_index),
			            sizeof(raw));
			set_status(53, raw);

			// Status 54: slot 1's localClientNum, reported but not repaired. If it drifts
			// back to 0 the initializer ran again.
			uint32_t lcn1 = 0;
			std::memcpy(&lcn1,
			            reinterpret_cast<const void*>(new_array + 1 * signin_stride
				            + cgs_local_client_num),
			            sizeof(lcn1));
			set_status(54, lcn1);
		}

		// StartOp (PS4 0xF7C740) begins a gamer-profile file operation. Counted from
		// inside the component (status 37..40 per controller, 41 = index outside 0..3)
		// because the boot storage read is over before an external tool can attach.
		constexpr uint32_t start_op_rva = 0x02218090;
		constexpr uint8_t start_op_prologue[] = {0x48, 0x89, 0x5C, 0x24, 0x10};
		// CL_SplitscreenPlayerCount (PS4 0x1516BE0): the splitscreen_playerCount dvar,
		// or 1 while it is unregistered. Every consumer asks this, including
		// CL_AllocatePerLocalClientMemory (max(count, 2)) and the writer of
		// cl_maxLocalClients. The detour answers from the seat table (bit 0 of each
		// 0x24 record, what Com_LocalClient_IsBeingUsed reads), so every caller agrees
		// whenever it asks. A plain memory read: no engine call, no dvar system, no
		// thread hazard. Falls back to the original while the seat table is not
		// relocated or reads zero.
		constexpr uint32_t splitscreen_player_count_rva = 0x027C1AB0;
		constexpr uint8_t splitscreen_player_count_prologue[] = {
			0x48, 0x8B, 0x0D, 0x49, 0x3F, 0xB9, 0x02, // mov rcx, [rip -> splitscreen_playerCount]
			0x48, 0x85, 0xC9,                         // test rcx, rcx
		};

		utils::hook::detour splitscreen_player_count_hook;
		uint32_t player_count_queries = 0;
		uint32_t player_count_last = 0;

		// CL_Init for local client 2. PS4 Com_Init calls CL_Init(i) for i 0..3 at boot
		// (0xE49E98); the PC boot inlines clients 0 and 1 only. CL_Frame skips a client
		// whose clientUIActives flag bit 1 ("CL_Init ran") is clear, so client 2 parked
		// at CA_CONFIRMLOADING. Calling the engine's own CL_Init(2) produces that state.
		// Its writes for lc 2 land in owned or padding memory; its two /GS range checks
		// (CL_Init's and Cbuf_Execute's) are opened for that one call only.
		// Called from the count detour, which runs on the game thread whenever the
		// count is asked (per Lua command, and by the map-load allocator), not from
		// per_controller_update_stub, which stops running after a splitscreen sign-in.
		constexpr uint32_t cl_init_rva = 0x01359410;
		constexpr uint32_t cl_init_range_imm_rva = 0x0135948B;
		constexpr uint32_t cbuf_execute_range_imm_rva = 0x020DFA30;
		// Resting value of the Cbuf_Execute range check: 0x02 stock, 0x04 once
		// install_cbuf_for_players34() has given local clients 2/3 their own command
		// buffers. The scoped CL_Init widens then leave it alone.
		uint8_t cbuf_range_resting = 0x02;
		// Defined with the IsActive cave further down. The cgame frame-loop widen
		// below may only run when the cave is installed.
		extern bool isactive_caved;
		constexpr uint32_t cl_frame_pump_imm_rva = 0x020ECE5E;
		constexpr uint32_t netchan_poll_imm_rva = 0x020EB424;

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

		// SetActive hook entries, reported with a result code in status 87 as
		// (calls << 8) | code.
		uint32_t set_active_calls = 0;

		void report87(const uint32_t code)
		{
			set_status(87, (set_active_calls << 8) | (code & 0xFF));
		}

		// The five SCR_UpdateFrame bound immediates (the BO3_CG_FRAME group), shared
		// with patch_probe_once.
		constexpr uint32_t cg_frame_imms[] = {
			0x013E10D4,   // cmp r13d,2 - the r_num_viewports counting loop
			0x013E11D2,   // cmp ebx,2  - cgame frame loop, copy A exit 1
			0x013E11DF,   // cmp ebx,2  - cgame frame loop, copy A exit 2
			0x013E1264,   // cmp ebx,2  - cgame frame loop, copy B tail
			0x013E12A6,   // cmp ebx,2  - the loading-screen scan
		};

		// Diagnostic, opt-in: BO3_PATCH_PROBE=1 writes the five immediates in a game
		// without a third player, to tell whether the write itself trips Arxan (the
		// 3-player deaths ~15 s into a round). Safe with two players: the widened
		// frame loop is gated by the caved IsActive, and the viewport count only
		// counts CA_ACTIVE slots.
		void patch_probe_once()
		{
			static int state = -1;   // -1 unread, 1 armed, 0 off or done
			if (state == 0)
			{
				return;
			}
			if (state < 0)
			{
				char buf[8] = {};
				GetEnvironmentVariableA("BO3_PATCH_PROBE", buf, sizeof(buf));
				state = std::strcmp(buf, "1") == 0 ? 1 : 0;
				if (state == 0)
				{
					return;
				}
			}
			if (!isactive_caved)
			{
				return;   // the gate the widened loop relies on is not in yet
			}
			state = 0;
			for (const auto rva : cg_frame_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(base() + rva);
				if (!readable(at, 1) || *at != 0x02)
				{
					note("[splitscreen] patch probe: 0x%X not stock, nothing written\n", rva);
					return;
				}
			}
			const uint8_t bound3 = 0x03;
			for (const auto rva : cg_frame_imms)
			{
				write_bytes(reinterpret_cast<uint8_t*>(base() + rva), &bound3, sizeof(bound3));
			}
			note("[splitscreen] patch probe: five CG_FRAME immediates written (2 -> 3)\n");
		}

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

			// Status 87 codes: 1 init ran and frame pump opened, 2 init did not set bit 1,
			// 3 range checks not in their resting state, 4 a range write failed, 5 frame
			// pump byte unexpected, 6 widens deferred until the allocation; 10, 11 and
			// 20 + n (seats seen) come from the watch.
			//
			// Scoped widen: both sites are MSVC /GS range checks (cmp rbx,2 / jae
			// __report_rangecheckfailure) on a [2] array, which CL_Init(2) would trip.
			// They are opened for exactly one call and closed again, and nothing is
			// written unless both hold the expected byte.
			auto* range_a = reinterpret_cast<uint8_t*>(base() + cl_init_range_imm_rva);
			auto* range_b = reinterpret_cast<uint8_t*>(base() + cbuf_execute_range_imm_rva);
			if (*range_a != 0x02 || *range_b != cbuf_range_resting)
			{
				report87(3);
				return;
			}

			// clientUIActives[2] flags. Slot 2 is the block voice_comm vacated.
			auto* flags = reinterpret_cast<volatile uint32_t*>(
				base() + 0x05359BC0 + 2 * 0x1078);
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
					report87(4);
					return;
				}
			}
			// Bit 1 is the engine's receipt that CL_Init ran, and what CL_Frame tests.
			// If the init did not take, the frame pump stays at two.
			if ((*flags & 0x2) == 0)
			{
				report87(2);
				return;
			}

			// The init has taken; latch it only now, so an early call retries later.
			// This latch covers the init only. The widens below are deferred in the lobby
			// (code 6), and every trigger site keeps re-entering until lc2_widens_done.
			cl_init2_done = true;

			// The byte widens need the allocation, which happens at map load; the count
			// detour re-enters here then.
			if (max_local < 3)
			{
				report87(6);
				return;
			}

			auto* site = reinterpret_cast<uint8_t*>(base() + cl_frame_pump_imm_rva);
			if (*site != 0x02)
			{
				report87(5);
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
				// Status 86: cl_maxLocalClients when the poll was widened.
				write_bytes(poll, &three, sizeof(three));
				set_status(86, max_local);
			}

			// cgame frame loop, opt-in with BO3_CG_FRAME=on and only with the IsActive cave.
			// PC SCR_UpdateFrame calls CG_DrawActiveFrame / CG_ProcessButDontDrawActiveFrame
			// for clients 0..1 only (PS4 0..3), so client 2 never gets a snapshot and the
			// third pane cannot draw. The five immediates are one group: MSVC split the
			// frame loop into two copies sharing the induction variable, and a second loop
			// computes r_num_viewports. Three, never four: clientUIActives slot 3 overlaps
			// the clientActive base pointer. The caved IsActive refuses
			// lc >= cl_maxLocalClients.
			// Off by default: ticking cgame for client 2 reaches per-client resources the
			// PC only allocates for two (NULL buffer pointers, [2] arrays; PS4
			// CG_ProcessSnapshots 0x2A86C0 also waits for a snapshot client 2 never gets).
			// Guarding them one at a time only moves the crash. The default build runs
			// three players with two panes.
			// History: LOG.md, "BO3_CG_FRAME"
			char cg_frame_env[16] = {};
			GetEnvironmentVariableA("BO3_CG_FRAME", cg_frame_env, sizeof(cg_frame_env));
			if (isactive_caved && std::strcmp(cg_frame_env, "on") == 0)
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

			report87(1);
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
			auto* flags = reinterpret_cast<volatile uint32_t*>(base() + 0x05359BC0 + 3 * 0x1078);
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
		constexpr uint32_t guest_signin_rva = 0x01DFFED0;
		constexpr uint32_t is_user_guest_imm_rva = 0x01EBA642;
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
		constexpr uint32_t gamepad_bound_rvas[] = {
			0x02284ADB,
			0x02284BCB,
			0x02285981,
			0x02285D89,
			0x01FD6E79,
			0x01FD7B0E,
		};
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
		constexpr uint32_t gamepad_rescan_rva = 0x02286010;
		constexpr uint8_t gamepad_rescan_bytes[] = {
			0x48, 0x83, 0xEC, 0x28,                      // sub rsp, 28h
			0xE8, 0xE7, 0xEF, 0xFF, 0xFF,                // call enumerate
			0xE8, 0xD2, 0xE9, 0xFF, 0xFF,                // call assign
			0x48, 0x83, 0xC4, 0x28,                      // add rsp, 28h
			0xE9, 0x49, 0xD1, 0xE5, 0xFF,                // jmp seat-model refresh
		};

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

		int32_t gamepad_device_of(const size_t slot)
		{
			return *reinterpret_cast<const volatile int32_t*>(
				base() + gamepads_reserved_rva + slot * gamepad_stride + gamepad_device_index);
		}

		// Defined after the trace helpers: one line with every slot's device.
		void log_gamepad_slots(const char* what);

		// Device-type selector. Both gamepad loops (poll, per-frame update) reuse the
		// loop-bound register as the constant 2 of the device-type selector, so the
		// widened bound 4 made devices 4..7 (the non-XInput API) type 4, which nothing
		// handles. The same 11 bytes, rewritten without the register:
		//     lea eax,[rdx-4]; cmp eax,4; sbb ecx,ecx; and ecx,2
		// give 2 for devices 4..7, else 0, whatever the bound. The length must not
		// change (both ends are jump targets). Valid with the stock bound too, so it
		// is applied at startup.
		struct type_selector_site
		{
			uint32_t rva;
			uint8_t expected[11];
		};
		constexpr type_selector_site gamepad_type_selector_sites[] = {
			{0x022859A6, {0x33, 0xC9, 0x8D, 0x42, 0xFC, 0x83, 0xF8, 0x03, 0x0F, 0x46, 0xCD}},
			{0x02285DAA, {0x33, 0xC9, 0x8D, 0x42, 0xFC, 0x83, 0xF8, 0x03, 0x0F, 0x46, 0xCE}},
		};
		constexpr uint8_t gamepad_type_selector_fixed[11] = {
			0x8D, 0x42, 0xFC,                            // lea eax, [rdx-4]
			0x83, 0xF8, 0x04,                            // cmp eax, 4
			0x1B, 0xC9,                                  // sbb ecx, ecx
			0x83, 0xE1, 0x02,                            // and ecx, 2
		};

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
				set_status(58, static_cast<uint32_t>(expected_gamepad_refs));
				set_status(59, static_cast<uint32_t>(std::size(gamepad_bound_rvas)));
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
			set_status(58, static_cast<uint32_t>(expected_gamepad_refs));
			set_status(59, static_cast<uint32_t>(std::size(gamepad_bound_rvas)));
			complete_gamepads(base() + destination_rva);

			// Run the game's device rescan so the new slots get the connected
			// controllers. On a byte mismatch slots 2/3 wait for the next device change.
			const auto* rescan = reinterpret_cast<const void*>(base() + gamepad_rescan_rva);
			if (readable(rescan, sizeof(gamepad_rescan_bytes))
				&& std::memcmp(rescan, gamepad_rescan_bytes, sizeof(gamepad_rescan_bytes)) == 0)
			{
				reinterpret_cast<void (*)()>(base() + gamepad_rescan_rva)();
				log_gamepad_slots("gamepads relocated rescan=1");
			}
			else
			{
				log_gamepad_slots("gamepads relocated rescan=bytes-mismatch");
			}
			return true;
		}

		// Signs in controller 2 as a guest (player 3) once the offline lobby is up.
		// Progress codes go through report87, not new status slots: a free literal
		// slot cannot be assumed (11+i, 16+i and 37+i are computed indices).
		void try_join_guest2()
		{
			if (guest_join_done || guest_join_attempts >= guest_join_max_attempts)
			{
				return;
			}
			// Diagnostic, opt-in: BO3_GUESTS=1 leaves seat 2 empty, for a
			// two-player A/B round on the same build.
			{
				static int guests_env = -1;
				if (guests_env < 0)
				{
					char buf[8] = {};
					GetEnvironmentVariableA("BO3_GUESTS", buf, sizeof(buf));
					guests_env = std::strcmp(buf, "1") == 0 ? 1 : 2;
				}
				if (guests_env == 1)
				{
					return;
				}
			}
			if (!signin_relocated || !guests_filled || !guest_array_rva)
			{
				report87(40);
				return;
			}

			// Three or more local players are offline-only (Treyarch caps LIVE at
			// two). Seated in the LIVE party, player 3 is a local client no lobby
			// knows and the lobby loops. So wait, without spending an attempt, until
			// the lobby is not LIVE and the game lobby is up.
			// History: LOG.md, offline_lobby_ready_for_player3
			if (!offline_lobby_ready_for_player3())
			{
				report87(54);
				return;
			}

			// Verify the is-user-guest bound widen landed before calling.
			uint8_t bound = 0;
			std::memcpy(&bound,
			            reinterpret_cast<const void*>(base() + is_user_guest_imm_rva),
			            sizeof(bound));
			if (bound != 0x03)
			{
				report87(41);
				return;
			}

			// The host must be seated and seat 2 free. Player 2 need not be seated;
			// if he is, offline_lobby_ready_for_player3 checked he is a lobby member.
			if (!controller_seated(0) || controller_seated(2))
			{
				report87(42);
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
				report87(43);
				return;
			}

			// No controller on slot 2: do not spend an attempt. This runs every
			// frame, so refusals would use up the retries before one is plugged in.
			if (!gamepad_connected(2))
			{
				report87(56);
				return;
			}

			++guest_join_attempts;
			report87(44);

			using signin_fn = void (*)(int, bool, bool);
			reinterpret_cast<signin_fn>(base() + guest_signin_rva)(2, true, false);

			// Latch only if the seat bit is set; otherwise retry (bounded). Check by
			// controller, not record: after a round without player 2 the engine
			// keeps controller 2 in record 1.
			if (controller_seated(2))
			{
				guest_join_done = true;
				report87(45);
			}
			else
			{
				report87(46);
			}
		}

		// ============ Pane fix: 3 and 4 rendered panes (PLAN_3-4_SCREENS.md) ====
		// The PC pane dispatcher (PS4: CL_SetupScreenPlacements 0x3EC820) caps
		// the pane count in four places: C1 GetActiveCount is inlined for two;
		// C2 the pane loop bound is `cmp ebx,2`; C3 clientUIActives is [2] with a
		// foreign slot 2; C4 the geometry table has no 3- and 4-pane rows.

		// ---- Phase 1: clientUIActives [2] -> [4] ----
		// PS4: clientUIActive_t[4] x 0x1078, same stride on PC. Loop-end sentinels
		// at slot 2 are owned by widen_client_ui_walker_bounds(), not this table.
		// Warning: applying this table black-screened the frontend;
		// install_isactive_cave() avoids the move. History: LOG.md, clientUIActives
		constexpr uint32_t uia_base_rva = 0x05359BC0;
		constexpr uint32_t uia_stride = 0x1078;
		constexpr uint32_t uia_old_size = 2 * uia_stride;   // 0x20F0
		constexpr uint32_t uia_new_size = 4 * uia_stride;   // 0x41E0

		struct uia_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint32_t delta;   // byte offset within the OLD array
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[12];
		};

		constexpr uia_ref uia_refs[] = {
			{0x000AB105, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0xB4, 0xEA, 0x2A, 0x05}},
			{0x00425961, 7, 3, 0x4, true, {0x48, 0x8D, 0x15, 0x5C, 0x42, 0xF3, 0x04}},
			{0x004F5B16, 8, 3, 0x8, false, {0x83, 0xBC, 0x30, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x0056C1DD, 7, 3, 0x0, false, {0x8B, 0x84, 0x08, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0058F1BA, 8, 4, 0x10, false, {0x46, 0x39, 0x84, 0x19, 0xD0, 0x9B, 0x35, 0x05}},
			{0x005DB390, 8, 3, 0x10, false, {0x83, 0xBC, 0x10, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x005DCC30, 7, 3, 0x10, true, {0x48, 0x8D, 0x0D, 0x99, 0xCF, 0xD7, 0x04}},
			{0x005DFF8D, 7, 3, 0x10, true, {0x48, 0x8D, 0x0D, 0x3C, 0x9C, 0xD7, 0x04}},
			{0x006BD277, 8, 4, 0x10, false, {0x44, 0x39, 0xBC, 0x10, 0xD0, 0x9B, 0x35, 0x05}},
			{0x006BEC8A, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x006C267A, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x006C405A, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x006CCDB0, 8, 3, 0x10, false, {0x83, 0xBC, 0x19, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x00904898, 7, 3, 0x18, true, {0x48, 0x8D, 0x05, 0x39, 0x53, 0xA5, 0x04}},
			{0x0090E3E0, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0xD9, 0xB7, 0xA4, 0x04}},
			{0x0091F048, 7, 3, 0x18, true, {0x48, 0x8D, 0x05, 0x89, 0xAB, 0xA3, 0x04}},
			{0x009541FF, 8, 3, 0x8, false, {0x83, 0xBC, 0x10, 0xC8, 0x9B, 0x35, 0x05, 0x0A}},
			{0x00989244, 8, 4, 0x10, false, {0x44, 0x39, 0xB4, 0x08, 0xD0, 0x9B, 0x35, 0x05}},
			{0x00A8446B, 7, 3, 0x10, true, {0x48, 0x8D, 0x0D, 0x5E, 0x57, 0x8D, 0x04}},
			{0x00FC722C, 9, 4, 0x10, false, {0x42, 0x83, 0xBC, 0x2B, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x00FC7249, 9, 4, 0x10, false, {0x42, 0x83, 0xBC, 0x2B, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x00FC7296, 8, 4, 0x10, false, {0x42, 0x8B, 0x8C, 0x2B, 0xD0, 0x9B, 0x35, 0x05}},
			{0x00FC72A5, 12, 4, 0x10, false, {0x42, 0xC7, 0x84, 0x2B, 0xD0, 0x9B, 0x35, 0x05, 0x00, 0x00, 0x00, 0x00}},
			{0x00FEF2F1, 7, 3, 0x10, true, {0x48, 0x8D, 0x0D, 0xD8, 0xA8, 0x36, 0x04}},
			{0x0101F218, 7, 3, 0x0, false, {0x8B, 0x84, 0x38, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0102EDF4, 7, 3, 0x0, false, {0x8B, 0x84, 0x18, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0104C2C0, 9, 4, 0x4, false, {0x42, 0xF6, 0x84, 0x38, 0xC4, 0x9B, 0x35, 0x05, 0x10}},
			{0x010D3827, 8, 4, 0x10, false, {0x42, 0x39, 0xB4, 0x30, 0xD0, 0x9B, 0x35, 0x05}},
			{0x01134436, 8, 3, 0x8, false, {0x83, 0xBC, 0x30, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x0113F960, 8, 4, 0x0, false, {0x42, 0x8B, 0x84, 0x38, 0xC0, 0x9B, 0x35, 0x05}},
			{0x011951D4, 8, 3, 0x8, false, {0x83, 0xBC, 0x10, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x011D8E8D, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x30, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x011F20F7, 8, 3, 0x8, false, {0x83, 0xBC, 0x10, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x0122D0E0, 8, 3, 0x8, false, {0x83, 0xBC, 0x10, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x012F3A92, 9, 4, 0x8, false, {0x41, 0x83, 0xBC, 0x2E, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x012F6F43, 7, 3, 0x8, true, {0x48, 0x8D, 0x15, 0x7E, 0x2C, 0x06, 0x04}},
			{0x012F6F7F, 7, 3, 0x0, true, {0x4C, 0x8D, 0x3D, 0x3A, 0x2C, 0x06, 0x04}},
			{0x012F70C6, 8, 4, 0x0, false, {0x42, 0x8B, 0x84, 0x30, 0xC0, 0x9B, 0x35, 0x05}},
			{0x012F766E, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x30, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x012F76A6, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x30, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x012FF3F9, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xC8, 0xA7, 0x05, 0x04}},
			{0x012FF8FF, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xBA, 0xA2, 0x05, 0x04}},
			{0x012FFFDA, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0xDF, 0x9B, 0x05, 0x04}},
			{0x01300204, 8, 4, 0x0, false, {0x41, 0x8B, 0x84, 0x3A, 0xC0, 0x9B, 0x35, 0x05}},
			{0x01301A17, 9, 4, 0x4, false, {0x41, 0xF6, 0x84, 0x3A, 0xC4, 0x9B, 0x35, 0x05, 0x18}},
			{0x01301F39, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x88, 0x7C, 0x05, 0x04}},
			{0x01308B42, 8, 3, 0x8, false, {0x83, 0xBC, 0x11, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131B3B7, 7, 3, 0x8, true, {0x48, 0x8D, 0x15, 0x0A, 0xE8, 0x03, 0x04}},
			{0x0131B729, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x00, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131B8A9, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x00, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131B9D2, 8, 3, 0x8, false, {0x83, 0xBC, 0x11, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131BC07, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x38, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131BE2F, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x38, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131C096, 8, 3, 0x8, false, {0x83, 0xBC, 0x30, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131C246, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x7B, 0xD9, 0x03, 0x04}},
			{0x0131C362, 8, 3, 0x8, false, {0x83, 0xBC, 0x11, 0xC8, 0x9B, 0x35, 0x05, 0x07}},
			{0x0131DCA9, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x18, 0xBF, 0x03, 0x04}},
			{0x01320EEC, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0xD5, 0x8C, 0x03, 0x04}},
			{0x01326EA3, 12, 4, 0x8, false, {0x41, 0xC7, 0x84, 0x3F, 0xC8, 0x9B, 0x35, 0x05, 0x0A, 0x00, 0x00, 0x00}},
			{0x01327001, 8, 4, 0x8, false, {0x41, 0x8B, 0x9C, 0x0F, 0xC8, 0x9B, 0x35, 0x05}},
			{0x013270B9, 9, 4, 0x8, false, {0x41, 0x83, 0xBC, 0x1F, 0xC8, 0x9B, 0x35, 0x05, 0x0A}},
			{0x013270F3, 8, 4, 0x10, false, {0x41, 0x39, 0xBC, 0x1F, 0xD0, 0x9B, 0x35, 0x05}},
			{0x0132A63F, 9, 4, 0x8, false, {0x41, 0x83, 0xBC, 0x07, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x0132E31F, 7, 3, 0x1078, true, {0x48, 0x8D, 0x1D, 0x12, 0xC9, 0x02, 0x04}},
			{0x01333123, 7, 3, 0x8, true, {0x48, 0x8D, 0x15, 0x9E, 0x6A, 0x02, 0x04}},
			{0x013392AE, 7, 3, 0x0, false, {0x8B, 0x84, 0x38, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0133DF11, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xA8, 0xBC, 0x01, 0x04}},
			{0x0133F0E7, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xD2, 0xAA, 0x01, 0x04}},
			{0x0133F28C, 8, 4, 0x8, false, {0x43, 0x8B, 0x84, 0x06, 0xC8, 0x9B, 0x35, 0x05}},
			{0x0133F317, 8, 4, 0x4, false, {0x41, 0x8B, 0x94, 0x06, 0xC4, 0x9B, 0x35, 0x05}},
			{0x0133F33E, 9, 4, 0x4, false, {0x41, 0xF6, 0x84, 0x06, 0xC4, 0x9B, 0x35, 0x05, 0x08}},
			{0x0133F3BE, 8, 4, 0x4, false, {0x45, 0x8B, 0x8C, 0x06, 0xC4, 0x9B, 0x35, 0x05}},
			{0x0133F683, 8, 4, 0x8, false, {0x43, 0x8B, 0x84, 0x06, 0xC8, 0x9B, 0x35, 0x05}},
			{0x0133F712, 8, 4, 0x4, false, {0x41, 0x8B, 0x94, 0x06, 0xC4, 0x9B, 0x35, 0x05}},
			{0x0133F738, 9, 4, 0x4, false, {0x41, 0xF6, 0x84, 0x06, 0xC4, 0x9B, 0x35, 0x05, 0x08}},
			{0x0133F7B9, 8, 4, 0x4, false, {0x45, 0x8B, 0x8C, 0x06, 0xC4, 0x9B, 0x35, 0x05}},
			{0x0133FA73, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0x46, 0xA1, 0x01, 0x04}},
			{0x0133FB41, 8, 4, 0x8, false, {0x42, 0x8B, 0x84, 0x2F, 0xC8, 0x9B, 0x35, 0x05}},
			{0x0133FEE3, 8, 4, 0x8, false, {0x4C, 0x63, 0xB4, 0x03, 0xC8, 0x9B, 0x35, 0x05}},
			{0x0133FF4E, 7, 3, 0x4, false, {0x8B, 0x94, 0x3B, 0xC4, 0x9B, 0x35, 0x05}},
			{0x0133FF68, 8, 3, 0x4, false, {0xF6, 0x84, 0x3B, 0xC4, 0x9B, 0x35, 0x05, 0x08}},
			{0x0133FFD3, 7, 3, 0x4, false, {0x8B, 0x8C, 0x03, 0xC4, 0x9B, 0x35, 0x05}},
			{0x01340889, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x38, 0x93, 0x01, 0x04}},
			{0x01340DF3, 7, 3, 0x0, true, {0x48, 0x8D, 0x2D, 0xC6, 0x8D, 0x01, 0x04}},
			{0x01342308, 8, 3, 0x4, false, {0xF6, 0x84, 0x10, 0xC4, 0x9B, 0x35, 0x05, 0x01}},
			{0x01342380, 7, 3, 0x4, false, {0x8B, 0x84, 0x18, 0xC4, 0x9B, 0x35, 0x05}},
			{0x01343E2D, 8, 4, 0x4, false, {0x42, 0x8B, 0x8C, 0x0B, 0xC4, 0x9B, 0x35, 0x05}},
			{0x01343ECA, 8, 4, 0x8, false, {0x42, 0x8B, 0x84, 0x0B, 0xC8, 0x9B, 0x35, 0x05}},
			{0x01343F81, 8, 4, 0x4, false, {0x44, 0x8B, 0x8C, 0x03, 0xC4, 0x9B, 0x35, 0x05}},
			{0x01344043, 8, 4, 0x4, false, {0x44, 0x8B, 0x8C, 0x01, 0xC4, 0x9B, 0x35, 0x05}},
			{0x013441CA, 8, 3, 0x4, false, {0xF6, 0x84, 0x08, 0xC4, 0x9B, 0x35, 0x05, 0x08}},
			{0x013442AD, 8, 3, 0x4, false, {0xF6, 0x84, 0x19, 0xC4, 0x9B, 0x35, 0x05, 0x01}},
			{0x0134527E, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x3B, 0x49, 0x01, 0x04}},
			{0x01345831, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x88, 0x43, 0x01, 0x04}},
			{0x01345C88, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x31, 0x3F, 0x01, 0x04}},
			{0x0134710B, 7, 3, 0x4, true, {0x48, 0x8D, 0x0D, 0xB2, 0x2A, 0x01, 0x04}},
			{0x01347598, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x21, 0x26, 0x01, 0x04}},
			{0x013479B8, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x01, 0x22, 0x01, 0x04}},
			{0x013481C7, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xF2, 0x19, 0x01, 0x04}},
			{0x0134B86A, 7, 3, 0x8, true, {0x44, 0x89, 0x25, 0x57, 0xE3, 0x00, 0x04}},
			{0x0134B871, 7, 3, 0x1080, true, {0x44, 0x89, 0x25, 0xC8, 0xF3, 0x00, 0x04}},
			{0x0134B900, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xB9, 0xE2, 0x00, 0x04}},
			{0x0134B9EB, 8, 4, 0x8, false, {0x41, 0x8B, 0x9C, 0x05, 0xC8, 0x9B, 0x35, 0x05}},
			{0x0134BE8E, 8, 4, 0x0, false, {0x43, 0x8B, 0x84, 0x35, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0134BEF8, 9, 4, 0x10, false, {0x43, 0x83, 0xBC, 0x35, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x0134C062, 7, 3, 0x0, true, {0x4C, 0x8D, 0x3D, 0x57, 0xDB, 0x00, 0x04}},
			{0x0134C529, 7, 3, 0x8, true, {0x48, 0x8D, 0x15, 0x98, 0xD6, 0x00, 0x04}},
			{0x0134C6BB, 7, 3, 0x0, true, {0x48, 0x8D, 0x1D, 0xFE, 0xD4, 0x00, 0x04}},
			{0x0134C707, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xB2, 0xD4, 0x00, 0x04}},
			{0x0134CB56, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x6B, 0xD0, 0x00, 0x04}},
			{0x0134CBC7, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xFA, 0xCF, 0x00, 0x04}},
			{0x0134CCEF, 7, 3, 0x8, true, {0x48, 0x8D, 0x35, 0xD2, 0xCE, 0x00, 0x04}},
			{0x0134CE6E, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x4B, 0xCD, 0x00, 0x04}},
			{0x0134D05C, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x5D, 0xCB, 0x00, 0x04}},
			{0x0134D2C8, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0xF1, 0xC8, 0x00, 0x04}},
			{0x0134D320, 7, 3, 0x0, true, {0x4C, 0x8D, 0x25, 0x99, 0xC8, 0x00, 0x04}},
			{0x0134D38A, 7, 3, 0x0, true, {0x4C, 0x8D, 0x25, 0x2F, 0xC8, 0x00, 0x04}},
			{0x0134D43C, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x7D, 0xC7, 0x00, 0x04}},
			{0x0134D4F6, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xC3, 0xC6, 0x00, 0x04}},
			{0x0134D564, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x55, 0xC6, 0x00, 0x04}},
			{0x0134D94C, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x75, 0xC2, 0x00, 0x04}},
			{0x013512D4, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xED, 0x88, 0x00, 0x04}},
			{0x01351399, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x28, 0x88, 0x00, 0x04}},
			{0x0135142C, 7, 3, 0x0, true, {0x4C, 0x8D, 0x35, 0x8D, 0x87, 0x00, 0x04}},
			{0x0135945D, 12, 4, 0x8, false, {0x42, 0xC7, 0x84, 0x37, 0xC8, 0x9B, 0x35, 0x05, 0x00, 0x00, 0x00, 0x00}},
			{0x013594B5, 9, 4, 0x0, false, {0x42, 0x83, 0x8C, 0x37, 0xC0, 0x9B, 0x35, 0x05, 0x02}},
			{0x013594FF, 7, 3, 0x0, true, {0x48, 0x8D, 0x2D, 0xBA, 0x06, 0x00, 0x04}},
			{0x013598AD, 7, 3, 0x8, true, {0x48, 0x8D, 0x3D, 0x14, 0x03, 0x00, 0x04}},
			{0x01359905, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xBC, 0x02, 0x00, 0x04}},
			{0x01359947, 7, 3, 0x4, true, {0x48, 0x8D, 0x15, 0x76, 0x02, 0x00, 0x04}},
			{0x01359B88, 7, 3, 0x0, true, {0x4C, 0x8D, 0x2D, 0x31, 0x00, 0x00, 0x04}},
			{0x01359C21, 7, 3, 0x8, true, {0x48, 0x8D, 0x35, 0xA0, 0xFF, 0xFF, 0x03}},
			{0x01359EB8, 7, 3, 0x8, true, {0x4C, 0x8D, 0x05, 0x09, 0xFD, 0xFF, 0x03}},
			{0x01359FFF, 7, 3, 0x8, true, {0x48, 0x8D, 0x35, 0xC2, 0xFB, 0xFF, 0x03}},
			{0x0135A274, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x45, 0xF9, 0xFF, 0x03}},
			{0x0135CA5D, 7, 3, 0x10, true, {0x48, 0x8D, 0x0D, 0x6C, 0xD1, 0xFF, 0x03}},
			{0x0135CE1C, 8, 3, 0x0, false, {0x83, 0x8C, 0x03, 0xC0, 0x9B, 0x35, 0x05, 0x08}},
			{0x0135CE26, 8, 3, 0x0, false, {0x83, 0xA4, 0x03, 0xC0, 0x9B, 0x35, 0x05, 0xF7}},
			{0x0135D1B4, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x05, 0xCA, 0xFF, 0x03}},
			{0x0135D8A1, 7, 3, 0x0, true, {0x4C, 0x8D, 0x2D, 0x18, 0xC3, 0xFF, 0x03}},
			{0x0135E264, 8, 4, 0x0, false, {0x42, 0x8B, 0x84, 0x30, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0135FB90, 7, 3, 0x0, false, {0x48, 0x8D, 0xBA, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0135FBA1, 7, 3, 0x8, false, {0x8B, 0xB4, 0x10, 0xC8, 0x9B, 0x35, 0x05}},
			{0x01361701, 7, 3, 0x0, true, {0x48, 0x8D, 0x35, 0xB8, 0x84, 0xFF, 0x03}},
			{0x01361822, 7, 3, 0x10, true, {0x48, 0x8D, 0x05, 0xA7, 0x83, 0xFF, 0x03}},
			{0x013619ED, 7, 3, 0x10, true, {0x48, 0x8D, 0x0D, 0xDC, 0x81, 0xFF, 0x03}},
			{0x01361A92, 7, 3, 0x0, true, {0x48, 0x8D, 0x35, 0x27, 0x81, 0xFF, 0x03}},
			{0x01361C07, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0xB2, 0x7F, 0xFF, 0x03}},
			{0x01361F9D, 7, 3, 0x18, true, {0x48, 0x8D, 0x05, 0x34, 0x7C, 0xFF, 0x03}},
			{0x0136217F, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x42, 0x7A, 0xFF, 0x03}},
			{0x01364FB1, 7, 3, 0x0, true, {0x48, 0x8D, 0x1D, 0x08, 0x4C, 0xFF, 0x03}},
			{0x01365077, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x42, 0x4B, 0xFF, 0x03}},
			{0x01366938, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x89, 0x32, 0xFF, 0x03}},
			{0x013CAFFF, 9, 4, 0x8, false, {0x41, 0x83, 0xBC, 0x1F, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x013CC98F, 9, 4, 0x4, false, {0x41, 0xF6, 0x84, 0x07, 0xC4, 0x9B, 0x35, 0x05, 0x88}},
			{0x013CFC42, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x7F, 0x9F, 0xF8, 0x03}},
			{0x013D205A, 8, 3, 0x8, false, {0x83, 0xBC, 0x10, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x013D206C, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x013D3A6D, 8, 3, 0x10, false, {0x83, 0xBC, 0x10, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x013D8C0D, 6, 2, 0x0, true, {0x8B, 0x05, 0xAD, 0x0F, 0xF8, 0x03}},
			{0x013DA84B, 6, 2, 0x8, true, {0x39, 0x1D, 0x77, 0xF3, 0xF7, 0x03}},
			{0x013DA869, 7, 3, 0x8, false, {0x8B, 0xB4, 0x38, 0xC8, 0x9B, 0x35, 0x05}},
			{0x013DA87F, 7, 3, 0x8, false, {0x39, 0xB4, 0x38, 0xC8, 0x9B, 0x35, 0x05}},
			{0x013DA8A0, 7, 3, 0x8, false, {0x8B, 0xB4, 0x38, 0xC8, 0x9B, 0x35, 0x05}},
			{0x013DC5CE, 6, 2, 0x0, true, {0x8B, 0x0D, 0xEC, 0xD5, 0xF7, 0x03}},
			{0x013DDE95, 6, 2, 0x0, true, {0x8B, 0x0D, 0x25, 0xBD, 0xF7, 0x03}},
			{0x013DF81D, 7, 3, 0x8, true, {0x48, 0x8D, 0x3D, 0xA4, 0xA3, 0xF7, 0x03}},
			{0x013E151D, 7, 3, 0x8, true, {0x48, 0x8D, 0x3D, 0xA4, 0x86, 0xF7, 0x03}},
			{0x013E1599, 6, 2, 0x8, true, {0x8B, 0x05, 0x29, 0x86, 0xF7, 0x03}},
			{0x013E2F22, 6, 2, 0x8, true, {0x8B, 0x05, 0xA0, 0x6C, 0xF7, 0x03}},
			{0x013E2F39, 6, 2, 0x0, true, {0x8B, 0x05, 0x81, 0x6C, 0xF7, 0x03}},
			{0x013E3261, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x60, 0x69, 0xF7, 0x03}},
			{0x013E32F5, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xCC, 0x68, 0xF7, 0x03}},
			{0x013E35A9, 7, 3, 0x8, true, {0x4C, 0x8D, 0x35, 0x18, 0x66, 0xF7, 0x03}},
			{0x013E378A, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x37, 0x64, 0xF7, 0x03}},
			{0x0164E034, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x85, 0xBB, 0xD0, 0x03}},
			{0x01E00C11, 7, 3, 0x0, true, {0x48, 0x8D, 0x35, 0xA8, 0x8F, 0x55, 0x03}},
			{0x01E00CDF, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0xDA, 0x8E, 0x55, 0x03}},
			{0x01E19439, 7, 3, 0x8, true, {0x48, 0x8D, 0x15, 0x88, 0x07, 0x54, 0x03}},
			{0x01E194FE, 8, 4, 0x8, false, {0x46, 0x8B, 0xB4, 0x38, 0xC8, 0x9B, 0x35, 0x05}},
			{0x01E1B785, 8, 3, 0x8, false, {0x83, 0xBC, 0x39, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x01E4B195, 8, 3, 0x8, false, {0x83, 0xBC, 0x11, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x01E88FF7, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xCA, 0x0B, 0x4D, 0x03}},
			{0x01EB794D, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x74, 0x22, 0x4A, 0x03}},
			{0x01EBE2BE, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x03, 0xB9, 0x49, 0x03}},
			{0x01EC1529, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x90, 0x86, 0x49, 0x03}},
			{0x01EC1DBD, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x04, 0x7E, 0x49, 0x03}},
			{0x01EC2404, 7, 3, 0x10, true, {0x48, 0x8D, 0x15, 0xC5, 0x77, 0x49, 0x03}},
			{0x01EC2468, 7, 3, 0x10, true, {0x48, 0x8D, 0x05, 0x61, 0x77, 0x49, 0x03}},
			{0x01EC4B58, 7, 3, 0x10, true, {0x48, 0x8D, 0x3D, 0x71, 0x50, 0x49, 0x03}},
			{0x01EE8D8C, 9, 4, 0x8, false, {0x42, 0x83, 0xBC, 0x38, 0xC8, 0x9B, 0x35, 0x05, 0x0A}},
			{0x01EEE9B9, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x08, 0xB2, 0x46, 0x03}},
			{0x01F097A6, 7, 3, 0x8, true, {0x48, 0x8D, 0x3D, 0x1B, 0x04, 0x45, 0x03}},
			{0x01F1C591, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x28, 0xD6, 0x43, 0x03}},
			{0x01F1D0B2, 8, 3, 0x4, false, {0xF6, 0x84, 0x08, 0xC4, 0x9B, 0x35, 0x05, 0x10}},
			{0x01F1D84E, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0x73, 0xC3, 0x43, 0x03}},
			{0x01F23FD8, 8, 3, 0x10, false, {0x83, 0xBC, 0x18, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x01F2F9C7, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x01F3A8D4, 7, 3, 0x0, false, {0x8B, 0x84, 0x18, 0xC0, 0x9B, 0x35, 0x05}},
			{0x01F726A2, 7, 3, 0x10, true, {0x48, 0x8D, 0x05, 0x27, 0x75, 0x3E, 0x03}},
			{0x01F93EA7, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x01FA41A0, 8, 3, 0x8, false, {0x83, 0xBC, 0x11, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x01FBAEE0, 8, 3, 0x8, false, {0x83, 0xBC, 0x10, 0xC8, 0x9B, 0x35, 0x05, 0x0B}},
			{0x01FBFB22, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x9F, 0xA0, 0x39, 0x03}},
			{0x02066941, 8, 3, 0x10, false, {0x83, 0xBC, 0x10, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x02074EC9, 9, 4, 0x10, false, {0x42, 0x83, 0xBC, 0x00, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x02084482, 8, 3, 0x10, false, {0x83, 0xBC, 0x10, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x020876B9, 9, 4, 0x10, false, {0x42, 0x83, 0xBC, 0x12, 0xD0, 0x9B, 0x35, 0x05, 0x00}},
			{0x020E3A3C, 7, 3, 0x0, false, {0x49, 0x8D, 0xBE, 0xC0, 0x9B, 0x35, 0x05}},
			{0x020E3B21, 7, 3, 0x0, false, {0x49, 0x8D, 0x9E, 0xC0, 0x9B, 0x35, 0x05}},
			{0x020ECA08, 7, 3, 0x8, true, {0x48, 0x8D, 0x0D, 0xB9, 0xD1, 0x26, 0x03}},
			{0x020F077B, 7, 3, 0x0, true, {0x48, 0x8D, 0x1D, 0x3E, 0x94, 0x26, 0x03}},
			{0x020F2091, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0x28, 0x7B, 0x26, 0x03}},
			{0x020F20AB, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0x0E, 0x7B, 0x26, 0x03}},
			{0x02216570, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x49, 0x36, 0x14, 0x03}},
			{0x02216D0A, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0xAF, 0x2E, 0x14, 0x03}},
			{0x02387A80, 7, 3, 0x8, true, {0x48, 0x8D, 0x1D, 0x41, 0x21, 0xFD, 0x02}},
			{0x0252BECC, 7, 3, 0x8, true, {0x48, 0x8D, 0x1D, 0xF5, 0xDC, 0xE2, 0x02}},
			{0x02582C5B, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x02584707, 8, 4, 0x0, false, {0x42, 0x8B, 0x84, 0x02, 0xC0, 0x9B, 0x35, 0x05}},
			{0x02586192, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x2F, 0x3A, 0xDD, 0x02}},
			{0x02586992, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x2F, 0x32, 0xDD, 0x02}},
			{0x025870DE, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0xE3, 0x2A, 0xDD, 0x02}},
			{0x0258F23A, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x7F, 0xA9, 0xDC, 0x02}},
			{0x02591C18, 8, 4, 0x0, false, {0x42, 0x8B, 0x8C, 0x22, 0xC0, 0x9B, 0x35, 0x05}},
			{0x0259A746, 7, 3, 0x0, false, {0x8B, 0x84, 0x10, 0xC0, 0x9B, 0x35, 0x05}},
			{0x025A0A8C, 7, 3, 0x0, true, {0x4C, 0x8D, 0x35, 0x2D, 0x91, 0xDB, 0x02}},
			{0x025A4188, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x31, 0x5A, 0xDB, 0x02}},
			{0x02789616, 7, 3, 0x0, false, {0x8B, 0x84, 0x30, 0xC0, 0x9B, 0x35, 0x05}},
			{0x02797C8E, 8, 3, 0x8, false, {0x83, 0xBC, 0x08, 0xC8, 0x9B, 0x35, 0x05, 0x00}},
			{0x027C1610, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0xB1, 0x85, 0xB9, 0x02}},
			{0x027C1649, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x78, 0x85, 0xB9, 0x02}},
			{0x027C1689, 7, 3, 0x8, true, {0x48, 0x8D, 0x05, 0x38, 0x85, 0xB9, 0x02}},
			{0x027C16BE, 7, 3, 0x0, true, {0x48, 0x8D, 0x3D, 0xFB, 0x84, 0xB9, 0x02}},
			{0x027C1715, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0xA4, 0x84, 0xB9, 0x02}},
			{0x027C1782, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x37, 0x84, 0xB9, 0x02}},
			{0x027C17B9, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0x00, 0x84, 0xB9, 0x02}},
			{0x027C17ED, 7, 3, 0x1078, true, {0x48, 0x8D, 0x0D, 0x44, 0x94, 0xB9, 0x02}},
			{0x027C1824, 6, 2, 0x1078, true, {0x84, 0x0D, 0x0E, 0x94, 0xB9, 0x02}},
			{0x027C1863, 6, 2, 0x1078, true, {0x84, 0x15, 0xCF, 0x93, 0xB9, 0x02}},
			{0x027C1886, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x33, 0x83, 0xB9, 0x02}},
			{0x027C18D1, 6, 2, 0x1078, true, {0x84, 0x0D, 0x61, 0x93, 0xB9, 0x02}},
			{0x027C18E3, 7, 3, 0x0, true, {0x48, 0x8D, 0x0D, 0xD6, 0x82, 0xB9, 0x02}},
			{0x027C1923, 6, 2, 0x1078, true, {0x84, 0x15, 0x0F, 0x93, 0xB9, 0x02}},
			{0x027C194D, 7, 3, 0x0, true, {0x48, 0x8D, 0x3D, 0x6C, 0x82, 0xB9, 0x02}},
			{0x027C1992, 6, 2, 0x1078, true, {0x84, 0x15, 0xA0, 0x92, 0xB9, 0x02}},
			{0x027C19D1, 7, 3, 0x0, true, {0x48, 0x8D, 0x1D, 0xE8, 0x81, 0xB9, 0x02}},
			{0x027C1A0D, 7, 3, 0x0, true, {0x40, 0x84, 0x35, 0xAC, 0x81, 0xB9, 0x02}},
			{0x027C1A1C, 7, 3, 0x1078, true, {0x40, 0x84, 0x35, 0x15, 0x92, 0xB9, 0x02}},
		};

		bool client_ui_actives_relocated = false;
		size_t uia_new_base_rva = 0;

		bool relocate_client_ui_actives()
		{
			if (client_ui_actives_relocated)
			{
				return true;
			}
			const auto b = base();

			// Row bisect: BO3_PANES_MAX=N applies only the first N rows (sorted by
			// RVA), to find bad rows by binary search over launches. Unset/0 = all.
			size_t uia_apply_count = std::size(uia_refs);
			{
				char max_buf[16] = {};
				GetEnvironmentVariableA("BO3_PANES_MAX", max_buf, sizeof(max_buf));
				const auto v = static_cast<size_t>(std::strtoul(max_buf, nullptr, 10));
				if (v > 0 && v < uia_apply_count)
				{
					uia_apply_count = v;
				}
			}
			// BO3_PANES_SKIP=i,j,k: row indices to skip, to mask a bad row without rebuilding.
			bool uia_skip[std::size(uia_refs)] = {};
			{
				char skip_buf[256] = {};
				GetEnvironmentVariableA("BO3_PANES_SKIP", skip_buf, sizeof(skip_buf));
				const char* p = skip_buf;
				while (*p)
				{
					char* end = nullptr;
					const auto idx = static_cast<size_t>(std::strtoul(p, &end, 10));
					if (end == p)
					{
						break;
					}
					if (idx < std::size(uia_refs))
					{
						uia_skip[idx] = true;
					}
					p = (*end == ',') ? end + 1 : end;
				}
			}

			for (const auto& r : uia_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}

			auto* destination = allocate_near_module(uia_new_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			// Copy the two live elements. At post_unpack no pointer into the old buffer exists yet.
			if (!readable(reinterpret_cast<const void*>(b + uia_base_rva), uia_old_size))
			{
				return false;
			}
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + uia_base_rva), uia_old_size);

			int32_t old_values[std::size(uia_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					if (uia_skip[i])
					{
						continue; // never written, old_values[i] never captured
					}
					write_bytes(reinterpret_cast<void*>(
						            b + uia_refs[i].rva + uia_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};
			const auto field_value = [&](const uia_ref& r)
			{
				const size_t tgt = new_base + r.delta;
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			for (size_t i = 0; i < uia_apply_count; ++i)
			{
				if (uia_skip[i])
				{
					++done; // keep rollback indexing aligned; nothing written
					continue;
				}
				const auto& r = uia_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}
			for (size_t i = 0; i < uia_apply_count; ++i)
			{
				if (uia_skip[i])
				{
					continue;
				}
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + uia_refs[i].rva + uia_refs[i].disp_off), sizeof(seen));
				if (seen != field_value(uia_refs[i]))
				{
					rollback();
					return false;
				}
			}

			uia_new_base_rva = new_base;
			client_ui_actives_relocated = true;
			return true;
		}

		// ---- Phase 2: the pane geometry table (GetLocalClientViewParams) ----
		// PC: [2 wide][2 total][2 pane] x 0x10. PS4: [2][4][4] x 0x10 = 0x200,
		// indexed pane*0x10 + (total-1)*0x40 + wide*0x100. The patch doubles the
		// index scales and points both base leas at ps4_view_params, the PS4 ELF
		// .data at VA 0x03060D50 (data/ps4_clientviewparams.bin).
		// All six sites change together or none do: a partial patch indexed past
		// the table into FOV constants and blacked out the frontend.
		constexpr uint32_t viewparams_rva = 0x032666F0;

		struct vp_patch
		{
			uint32_t rva;
			uint8_t len;
			uint8_t off;        // byte to change (or disp offset for a base lea)
			bool is_base;       // true = rip-disp to the table, false = single byte
			uint8_t to;         // new byte value when !is_base
			uint8_t expect[7];
		};

		constexpr vp_patch viewparams_patches[] = {
			// wide scale on the total==0 path: 0x40 -> 0x100
			{0x010CB56F, 4, 3, false, 0x08, {0x48, 0xC1, 0xE0, 0x06}},
			// shared index lea: scale 2 -> 4
			{0x010CB5F0, 4, 3, false, 0x91, {0x48, 0x8D, 0x14, 0x51}},
			// horizontal path index lea: scale 2 -> 4
			{0x010CB5F6, 4, 3, false, 0x96, {0x48, 0x8D, 0x0C, 0x56}},
			// non-horizontal path index lea: scale 2 -> 4
			{0x010CB652, 4, 3, false, 0x96, {0x48, 0x8D, 0x04, 0x56}},
			// the two table base leas
			{0x010CB5FA, 7, 3, true, 0x00, {0x48, 0x8D, 0x15, 0xEF, 0xB0, 0x19, 0x02}},
			{0x010CB664, 7, 3, true, 0x00, {0x48, 0x8D, 0x15, 0x85, 0xB0, 0x19, 0x02}},
		};

		constexpr uint8_t ps4_view_params[0x200] = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
			0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		};

		bool view_params_relocated = false;
		size_t viewparams_new_rva = 0;

		bool relocate_view_params()
		{
			if (view_params_relocated)
			{
				return true;
			}
			const auto b = base();

			for (const auto& p : viewparams_patches)
			{
				const auto* site = reinterpret_cast<const void*>(b + p.rva);
				if (!readable(site, p.len) || std::memcmp(site, p.expect, p.len) != 0)
				{
					return false;
				}
			}

			auto* destination = allocate_near_module(sizeof(ps4_view_params));
			if (!destination)
			{
				return false;
			}
			std::memcpy(destination, ps4_view_params, sizeof(ps4_view_params));
			const auto new_rva = reinterpret_cast<size_t>(destination) - b;

			// One transaction over all six sites, each remembered for rollback.
			uint8_t old_bytes[std::size(viewparams_patches)][4] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					const auto& p = viewparams_patches[i];
					auto* f = reinterpret_cast<void*>(b + p.rva + p.off);
					write_bytes(f, old_bytes[i], p.is_base ? 4u : 1u);
				}
			};

			for (size_t i = 0; i < std::size(viewparams_patches); ++i)
			{
				const auto& p = viewparams_patches[i];
				auto* field = reinterpret_cast<void*>(b + p.rva + p.off);
				const size_t n = p.is_base ? 4u : 1u;
				std::memcpy(old_bytes[i], field, n);
				bool wrote;
				if (p.is_base)
				{
					const int32_t disp = static_cast<int32_t>(
						new_rva - (p.rva + p.len));
					wrote = write_bytes(field, &disp, sizeof(disp));
				}
				else
				{
					wrote = write_bytes(field, &p.to, sizeof(p.to));
				}
				if (!wrote)
				{
					rollback();
					return false;
				}
				++done;
			}

			// Read every site back before declaring success.
			for (size_t i = 0; i < std::size(viewparams_patches); ++i)
			{
				const auto& p = viewparams_patches[i];
				const auto* field = reinterpret_cast<const uint8_t*>(b + p.rva + p.off);
				if (p.is_base)
				{
					int32_t seen = 0;
					std::memcpy(&seen, field, sizeof(seen));
					if (seen != static_cast<int32_t>(new_rva - (p.rva + p.len)))
					{
						rollback();
						return false;
					}
				}
				else if (*field != p.to)
				{
					rollback();
					return false;
				}
			}

			viewparams_new_rva = new_rva;
			view_params_relocated = true;
			return true;
		}

		// ---- Two [2] arrays the third pane's code path indexes at 2 ----
		// scrPlaceView (stride 0x7C; PS4: ScreenPlacement scrPlaceView[4], index
		// assert compiled out on PC): pane 2's viewport setup overwrote a pointer
		// global past the array. And an unnamed 0x54-byte per-client screen-effect
		// state whose slot 2 is a live pointer global. Both are relocated, never
		// widened; the engine fills slots 2/3 when context 2 activates.

		struct flat24_site
		{
			uint32_t rva;       // lea instruction start (7 bytes, disp at +3)
			uint8_t expect[7];
		};

		constexpr flat24_site scrplace_sites[] = {
			{0x013E55C9, {0x48, 0x8D, 0x0D, 0x30, 0x62, 0x39, 0x04}}, // GetView
			{0x013E55E3, {0x48, 0x8D, 0x0D, 0x16, 0x62, 0x39, 0x04}}, // GetViewUIContext
			{0x013E5609, {0x48, 0x8D, 0x0D, 0xF0, 0x61, 0x39, 0x04}}, // GetViewWritable
		};
		constexpr flat24_site perclient54_sites[] = {
			{0x010E5056, {0x48, 0x8D, 0x05, 0x63, 0xE2, 0xBC, 0x03}}, // reset
			{0x0111715D, {0x48, 0x8D, 0x05, 0x5C, 0xC1, 0xB9, 0x03}}, // expiry check
		};

		// aaGlobArray: AimAssist per-client globals, stride 0x4E30 (PS4:
		// AimAssist_GetClientGlobals 0x48010). Written for lc 2 via CG_SetView;
		// slot 2 is foreign. Superseded by aaglob_v2_sites; kept as the record.
		constexpr flat24_site aaglob_sites[] = {
			{0x0002D7B6, {0x48, 0x8D, 0x05, 0x13, 0x8A, 0x5C, 0x03}},
			{0x0002DAD3, {0x48, 0x8D, 0x05, 0xF6, 0x86, 0x5C, 0x03}},
			{0x0002F70F, {0x48, 0x8D, 0x05, 0xBA, 0x6A, 0x5C, 0x03}},
			{0x0002FC45, {0x48, 0x8D, 0x05, 0x84, 0x65, 0x5C, 0x03}},
			{0x0002FFD1, {0x48, 0x8D, 0x05, 0xF8, 0x61, 0x5C, 0x03}},
			{0x00034D16, {0x48, 0x8D, 0x05, 0xB3, 0x14, 0x5C, 0x03}},
			{0x000369C8, {0x48, 0x8D, 0x05, 0x01, 0xF8, 0x5B, 0x03}},
			{0x00043793, {0x48, 0x8D, 0x0D, 0x36, 0x2A, 0x5B, 0x03}},
			{0x00043E7D, {0x48, 0x8D, 0x05, 0x4C, 0x23, 0x5B, 0x03}},
			{0x0004EDEE, {0x48, 0x8D, 0x05, 0xDB, 0x73, 0x5A, 0x03}},
			{0x0005219A, {0x48, 0x8D, 0x05, 0x2F, 0x40, 0x5A, 0x03}},
			{0x00056D33, {0x48, 0x8D, 0x0D, 0x96, 0xF4, 0x59, 0x03}},
			{0x0005B969, {0x48, 0x8D, 0x05, 0x60, 0xA8, 0x59, 0x03}},
			{0x0005BA60, {0x48, 0x8D, 0x05, 0x69, 0xA7, 0x59, 0x03}},
			{0x0005D383, {0x48, 0x8D, 0x05, 0x46, 0x8E, 0x59, 0x03}},
			{0x000604DE, {0x48, 0x8D, 0x05, 0xEB, 0x5C, 0x59, 0x03}},
			{0x000639B3, {0x48, 0x8D, 0x05, 0x16, 0x28, 0x59, 0x03}},
			{0x00065353, {0x48, 0x8D, 0x05, 0x76, 0x0E, 0x59, 0x03}},
			{0x00066CC7, {0x48, 0x8D, 0x05, 0x02, 0xF5, 0x58, 0x03}},
			{0x00066D50, {0x48, 0x8D, 0x05, 0x79, 0xF4, 0x58, 0x03}},
			{0x00071B48, {0x48, 0x8D, 0x05, 0x81, 0x46, 0x58, 0x03}},
			{0x00073644, {0x48, 0x8D, 0x05, 0x85, 0x2B, 0x58, 0x03}},
			{0x000753E0, {0x48, 0x8D, 0x05, 0xE9, 0x0D, 0x58, 0x03}},
		};

		// Per-local-client u16 LUI element handles, written by the UI registrar
		// loop; a missing handle for context 2 crashed the LUI renderer. Slots
		// 2/3 are foreign. One reference.
		constexpr flat24_site uielem_sites[] = {
			{0x01F269D6, {0x4C, 0x8D, 0x35, 0xFB, 0x64, 0xA3, 0x15}},
		};

		// The table above moves only the writer. The one reader, an ABS32 load in
		// UI_CoD_HUD_UpdateVisibilityBits that a lea-only scan misses, must follow,
		// or visibility bits go to the old handles and panes 1 and 2 have no HUD.
		bool uielem_reader_retargeted = false;
		size_t uielem_new_rva = 0;
		bool uielem_relocated = false;

		void retarget_uielem_reader()
		{
			if (uielem_reader_retargeted || !uielem_relocated || !uielem_new_rva)
			{
				return;
			}
			auto* insn = reinterpret_cast<uint8_t*>(base() + 0x026F89D2);
			constexpr uint8_t expect[] = {0x43, 0x0F, 0xB7, 0x8C, 0x67, 0xD8, 0xCE, 0x95, 0x17};
			if (!readable(insn, sizeof(expect)) || std::memcmp(insn, expect, sizeof(expect)) != 0)
			{
				note("[splitscreen] uielem reader: bytes differ - skipped\n");
				return;
			}
			const auto disp = static_cast<int32_t>(uielem_new_rva);
			if (write_bytes(insn + 5, &disp, sizeof(disp)))
			{
				uielem_reader_retargeted = true;
			}
		}

		bool scrplace_relocated = false;
		bool perclient54_relocated = false;
		bool aaglob_relocated = false;
		size_t scrplace_new_rva = 0;
		size_t perclient54_new_rva = 0;
		size_t aaglob_new_rva = 0;

		// Copies the two live elements into a zeroed 4-slot block and repoints
		// the 7-byte leas; verified before and after, rolled back on failure.
		bool relocate_flat24(const char* name, const uint32_t old_base,
		                     const uint32_t stride, const flat24_site* sites,
		                     const size_t count, bool& flag, size_t& new_rva_out)
		{
			if (flag)
			{
				return true;
			}
			const auto b = base();

			for (size_t i = 0; i < count; ++i)
			{
				const auto* site = reinterpret_cast<const void*>(b + sites[i].rva);
				if (!readable(site, sizeof(sites[i].expect))
					|| std::memcmp(site, sites[i].expect,
					               sizeof(sites[i].expect)) != 0)
				{
					note("[splitscreen] %s: unexpected bytes at 0x%X - skipped\n",
					     name, sites[i].rva);
					return false;
				}
			}

			auto* destination = static_cast<uint8_t*>(allocate_near_module(4u * stride));
			if (!destination)
			{
				return false;
			}
			std::memset(destination, 0, 4u * stride);
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + old_base), 2u * stride);
			const auto new_rva = reinterpret_cast<size_t>(destination) - b;

			// 32 slots (aaGlobArray has 23 sites); the check keeps rollback in bounds.
			int32_t old_disp[32] = {};
			if (count > std::size(old_disp))
			{
				note("[splitscreen] %s: too many sites (%zu)\n", name, count);
				return false;
			}
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					auto* f = reinterpret_cast<void*>(b + sites[i].rva + 3);
					write_bytes(f, &old_disp[i], sizeof(old_disp[i]));
				}
			};

			for (size_t i = 0; i < count; ++i)
			{
				auto* field = reinterpret_cast<void*>(b + sites[i].rva + 3);
				std::memcpy(&old_disp[i], field, sizeof(old_disp[i]));
				const int32_t disp = static_cast<int32_t>(new_rva - (sites[i].rva + 7));
				if (!write_bytes(field, &disp, sizeof(disp)))
				{
					rollback();
					return false;
				}
				++done;
			}

			for (size_t i = 0; i < count; ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen,
				            reinterpret_cast<const void*>(b + sites[i].rva + 3),
				            sizeof(seen));
				if (seen != static_cast<int32_t>(new_rva - (sites[i].rva + 7)))
				{
					rollback();
					return false;
				}
			}

			new_rva_out = new_rva;
			flag = true;
			return true;
		}

		// ---- The third screen without moving clientUIActives ----
		// CG_SetView does not touch clientUIActives, so the pane pipeline's only
		// use of slot 2 is the IsActive(lc) gate in the dispatcher loop. The cave
		// returns 0 for lc >= cl_maxLocalClients and otherwise does the stock read
		// clientUIActives[lc].flags & 1.
		constexpr uint32_t isactive_rva = 0x027C18E0;
		constexpr uint8_t isactive_expected[] = {
			0x48, 0x63, 0xC1,                   // movsxd rax, ecx
			0x48, 0x8D, 0x0D,                   // lea rcx, [clientUIActives]
		};

		bool isactive_caved = false;

		bool install_isactive_cave()
		{
			if (isactive_caved)
			{
				return true;
			}
			if (!signin_relocated)
			{
				return false; // needs the sign-in relocation first
			}
			const auto b = base();
			auto* fn = reinterpret_cast<uint8_t*>(b + isactive_rva);
			if (!readable(fn, sizeof(isactive_expected))
				|| std::memcmp(fn, isactive_expected, sizeof(isactive_expected)) != 0)
			{
				return false;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x80));
			if (!cave)
			{
				return false;
			}
			const auto cave_addr = reinterpret_cast<size_t>(cave);

			std::vector<uint8_t> c;
			const auto rip32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};
			const auto imm32 = [&](const uint32_t v)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};

			c.insert(c.end(), {0x83, 0xF9, 0x02});          // cmp ecx, 2
			c.insert(c.end(), {0x7C, 0x00});                // jl  stock  (patched)
			const auto jl_at = c.size() - 1;

			// Mirrors CG_GetLocalClientGlobals, which returns NULL for lc >=
			// cl_maxLocalClients to callers that do not null-check. That dvar only
			// becomes 3 at map load; until then the pane loop skips client 2.
			c.insert(c.end(), {0x3B, 0x0D});                // cmp ecx, [cl_maxLocalClients]
			rip32(b + cl_max_local_clients_rva);
			c.insert(c.end(), {0x7C, 0x03});                // jl  +3 (storage exists)
			c.insert(c.end(), {0x33, 0xC0});                // xor eax, eax
			c.insert(c.end(), {0xC3});                      // ret  -> not active yet

			// Otherwise the stock read (slot 2 is real since voice_comm moved off it).
			// Do not answer from the seat table: back at the menu the seat stays in
			// use while lc 2's active bit is cleared -> black loading screen.

			// stock path: clientUIActives[lc].flags & 1
			const auto stock_off = c.size();
			c[jl_at] = static_cast<uint8_t>(stock_off - (jl_at + 1));
			c.insert(c.end(), {0x48, 0x63, 0xC1});          // movsxd rax, ecx
			c.insert(c.end(), {0x48, 0x69, 0xC0});          // imul rax, rax, 0x1078
			imm32(uia_stride);
			c.insert(c.end(), {0x48, 0x8D, 0x0D});          // lea rcx, [clientUIActives]
			rip32(b + uia_base_rva);
			c.insert(c.end(), {0x8B, 0x04, 0x08});          // mov eax, [rax+rcx]
			c.insert(c.end(), {0x83, 0xE0, 0x01});          // and eax, 1
			c.insert(c.end(), {0xC3});                      // ret

			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}

			uint8_t patch[5] = {0xE9};
			const auto rel = static_cast<int32_t>(cave_addr - (b + isactive_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(fn, patch, sizeof(patch)))
			{
				return false;
			}
			isactive_caved = true;
			return true;
		}

		// ---- Phase 3: the pane count and the dispatcher bound ----
		// The dispatcher has its own inline GetActiveCount, separate from the one
		// install_active_count_fix() caves; both are needed. Widen the bounds only
		// once storage and geometry are both live.
		constexpr uint32_t get_active_count_rva = 0x027C18C0;
		constexpr uint8_t get_active_count_expected[] = {
			0x33, 0xC0,                                 // xor eax, eax
			0xF6, 0x05, 0xF7, 0x82, 0xB9, 0x02, 0x01,   // test byte [rip+..], 1
			0xB9, 0x01, 0x00, 0x00, 0x00,               // mov ecx, 1
			0x0F, 0x45, 0xC1,                           // cmovne eax, ecx
		};

		struct pane_bound
		{
			uint32_t rva;
			uint8_t offset;
			uint8_t from;
			uint8_t to;
			uint8_t expect[5];
			uint8_t expect_len;
		};

		// Only the CG_SetView dispatcher bound is widened. Not its siblings:
		// `mov edi,1` seeds a downward walk over clientUIActives (raising it writes
		// to element[-1]), and the two `cmp edi,2` 2D/HUD loops are unclassified.
		constexpr pane_bound pane_bounds[] = {
			{0x0132E2E4, 2, 0x02, 0x04, {0x83, 0xFB, 0x02}, 3},
		};

		bool pane_counts_installed = false;

		// widen_ui_registrar_bound() may only run after the element-handle array
		// is relocated (later in try_apply than relocate_lui_roots).
		// Defined with the LUI roots relocation, further down.
		extern bool lui_roots_relocated;
		extern size_t uiroot_new_base_rva;

		// Caps the LUI render loop at three UI contexts:
		// `cmp r15d,[cl_maxLocalClients]` -> `cmp r15d,3` + NOPs. Context 2 (pane
		// 3's HUD) is safe only with the element handles relocated and their
		// reader retargeted; without them it crashed on a NULL element.
		// History: LOG.md, 0x0270D553
		constexpr uint32_t lui_ctx_bound_rva = 0x02683285;
		constexpr uint8_t lui_ctx_expected[] = {0x44, 0x3B, 0x3D, 0x94, 0x04, 0xCA, 0x02};
		constexpr uint8_t lui_ctx_patched[]  = {0x41, 0x83, 0xFF, 0x03, 0x90, 0x90, 0x90};
		bool lui_ctx_held = false;

		bool hold_lui_context_count()
		{
			if (lui_ctx_held)
			{
				return true;
			}
			auto* at = reinterpret_cast<uint8_t*>(base() + lui_ctx_bound_rva);
			if (!readable(at, sizeof(lui_ctx_expected))
				|| std::memcmp(at, lui_ctx_expected, sizeof(lui_ctx_expected)) != 0)
			{
				note("[splitscreen] lui ctx bound: unexpected bytes - skipped\n");
				return false;
			}
			if (!write_bytes(at, lui_ctx_patched, sizeof(lui_ctx_patched)))
			{
				return false;
			}
			uint8_t back[sizeof(lui_ctx_patched)] = {};
			std::memcpy(back, at, sizeof(back));
			if (std::memcmp(back, lui_ctx_patched, sizeof(back)) != 0)
			{
				write_bytes(at, lui_ctx_expected, sizeof(lui_ctx_expected));
				return false;
			}
			lui_ctx_held = true;
			return true;
		}

		bool ui_registrar_widened = false;

		bool widen_ui_registrar_bound()
		{
			if (ui_registrar_widened)
			{
				return true;
			}
			if (!uielem_relocated || !lui_roots_relocated)
			{
				return false;
			}
			constexpr uint8_t want[] = {0x83, 0xFF, 0x02};   // cmp edi, 2
			auto* at = reinterpret_cast<uint8_t*>(base() + 0x01F26A52);
			if (!readable(at, sizeof(want))
				|| std::memcmp(at, want, sizeof(want)) != 0)
			{
				return false;
			}
			const uint8_t four = 0x04;
			if (!write_bytes(at + 2, &four, sizeof(four)))
			{
				return false;
			}
			uint8_t back = 0;
			std::memcpy(&back, at + 2, sizeof(back));
			if (back != four)
			{
				const uint8_t two = 0x02;
				write_bytes(at + 2, &two, sizeof(two));
				return false;
			}
			ui_registrar_widened = true;
			return true;
		}

		// Snapshot guard in the HUD-refresh reader, which reads cg->nextSnap
		// (+0x30). For client 2 it is NULL (CG_ProcessSnapshots stays bounded at
		// 2; widening it gave zero panes). The cave sends NULL to the function's
		// own "snapshot not usable -> return 0" exit, so nothing downstream runs.
		constexpr uint32_t snapguard_rva = 0x01F3A82B;
		constexpr uint32_t snapguard_ret0 = 0x01F3A858;   // xor eax,eax; ...ret
		constexpr uint32_t snapguard_resume = 0x01F3A832; // the jne after the test
		constexpr uint8_t snapguard_expected[] = {
			0x48, 0x8B, 0x57, 0x30,   // mov rdx, [rdi+0x30]
			0xF6, 0x02, 0x10,         // test byte [rdx], 0x10
		};
		bool snapguard_installed = false;

		bool install_snapguard_cave()
		{
			if (snapguard_installed)
			{
				return true;
			}
			const auto b = base();
			auto* site = reinterpret_cast<uint8_t*>(b + snapguard_rva);
			if (!readable(site, sizeof(snapguard_expected))
				|| std::memcmp(site, snapguard_expected,
				               sizeof(snapguard_expected)) != 0)
			{
				return false;
			}
			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
			if (!cave)
			{
				return false;
			}
			const auto cave_addr = reinterpret_cast<size_t>(cave);
			std::vector<uint8_t> c;
			const auto rel32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};
			c.insert(c.end(), {0x48, 0x8B, 0x57, 0x30});   // mov rdx, [rdi+0x30]
			c.insert(c.end(), {0x48, 0x85, 0xD2});         // test rdx, rdx
			c.insert(c.end(), {0x0F, 0x84});               // jz -> return-0 exit
			rel32(b + snapguard_ret0);
			c.insert(c.end(), {0xF6, 0x02, 0x10});         // test byte [rdx], 0x10
			c.insert(c.end(), {0xE9});                     // jmp resume (the jne)
			rel32(b + snapguard_resume);
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}
			uint8_t patch[7] = {0xE9, 0, 0, 0, 0, 0x90, 0x90};
			const auto rel = static_cast<int32_t>(cave_addr - (b + snapguard_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return false;
			}
			snapguard_installed = true;
			return true;
		}

		// Guard for the per-client scene-buffer clear (see R_InitSceneBuffers
		// below): it memsets A[lc] and C[lc], which were NULL for lc 2.
		// Do not raise the allocator's client count (2): A uses the count as its
		// row stride behind 49 precomputed-index readers, and C's slot 2 is
		// another array's base.
		constexpr uint32_t scene_a_rva = 0x0AE13DC8;
		constexpr uint32_t scene_b_rva = 0x0AE13DD8;
		constexpr uint32_t scene_c_rva = 0x10596AE0;
		constexpr uint32_t scene_size_rva = 0x0F43794C;
		constexpr uint32_t scene_b_store_rva = 0x01C87448;
		constexpr uint8_t scene_b_store_expect[] = {
			0x48, 0x89, 0x84, 0x33, 0xD8, 0x3D, 0xE1, 0x0A};
		constexpr size_t scene_slots = 4;
		bool scene_b_relocated = false;
		bool scene_buffers_filled = false;
		uint32_t scene_b_new_rva = 0;
		// C's relocated 4-slot block (0 = not relocated). Never write old
		// C[2]/C[3]: three leas use C[2] as another array's base.
		size_t scene_c_new = 0;
		constexpr uint32_t pcbuf_clear_rva = 0x01C87480;
		constexpr uint32_t pcbuf_count_rva = 0x0F437948;
		constexpr uint8_t pcbuf_expected[] = {0x48, 0x89, 0x5C, 0x24, 0x08};
		bool pcbuf_guarded = false;

		bool install_perclient_buffer_guard()
		{
			if (pcbuf_guarded)
			{
				return true;
			}
			const auto b = base();
			auto* fn = reinterpret_cast<uint8_t*>(b + pcbuf_clear_rva);
			if (!readable(fn, sizeof(pcbuf_expected))
				|| std::memcmp(fn, pcbuf_expected, sizeof(pcbuf_expected)) != 0)
			{
				return false;
			}
			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
			if (!cave)
			{
				return false;
			}
			const auto cave_addr = reinterpret_cast<size_t>(cave);
			std::vector<uint8_t> c;
			const auto rel32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};
			// Return only when C[lc] is NULL; a count gate would leave client 2's
			// buffers uncleared once fill_scene_buffers() allocates them.
			// rax/r10 are volatile and free at entry.
			c.insert(c.end(), {0x48, 0x63, 0xC1});
			c.insert(c.end(), {0x49, 0xBA});
			{
				// the moved C if its relocation ran (it runs before this guard)
				const auto c_base = static_cast<uint64_t>(
					scene_c_new ? scene_c_new : b + scene_c_rva);
				const auto* cp = reinterpret_cast<const uint8_t*>(&c_base);
				c.insert(c.end(), cp, cp + 8);
			}
			c.insert(c.end(), {0x49, 0x8B, 0x04, 0xC2});
			c.insert(c.end(), {0x48, 0x85, 0xC0});
			c.insert(c.end(), {0x75, 0x01});
			c.insert(c.end(), {0xC3});
			// work: displaced prologue, then jump back past it
			c.insert(c.end(), pcbuf_expected,
			         pcbuf_expected + sizeof(pcbuf_expected));
			c.insert(c.end(), {0xE9});
			rel32(b + pcbuf_clear_rva + sizeof(pcbuf_expected));
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}
			uint8_t patch[5] = {0xE9};
			const auto rel = static_cast<int32_t>(
				cave_addr - (b + pcbuf_clear_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(fn, patch, sizeof(patch)))
			{
				return false;
			}
			pcbuf_guarded = true;
			return true;
		}

		// ============ R_InitSceneBuffers: per-client renderer scene buffers ====
		// The allocator fills A[lc], B[lc] (N bytes) and C[lc] (N*8) for lc < 2
		// (PS4 R_InitSceneBuffers 0x0093B060 loops to 4). Do not raise its bound:
		// it also bounds a release loop over another array, and A[2] is B[0].
		// Instead B is relocated to free A[2]/A[3], and fill_scene_buffers()
		// allocates the skipped slots at the engine's sizes.

		// Move B (one store, disp32 at +4, [rbx + rsi + disp] with rsi = module
		// base, so the new array must be within disp32 of the module base).
		bool relocate_scene_buffer_b()
		{
			if (scene_b_relocated)
			{
				return true;
			}
			const auto b = base();
			auto* insn = reinterpret_cast<uint8_t*>(b + scene_b_store_rva);
			if (!readable(insn, sizeof(scene_b_store_expect))
				|| std::memcmp(insn, scene_b_store_expect,
				               sizeof(scene_b_store_expect)) != 0)
			{
				note("[splitscreen] scene B store mismatch, not relocating\n");
				return false;
			}

			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(scene_slots * sizeof(void*)));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, scene_slots * sizeof(void*));

			const auto delta = reinterpret_cast<size_t>(fresh) - b;
			if (delta > 0x7FFFFFFF)
			{
				note("[splitscreen] scene B cave out of disp32 range\n");
				return false;
			}
			const auto new_rva = static_cast<uint32_t>(delta);

			// carry the two live pointers across before anything reads them
			std::memcpy(fresh, reinterpret_cast<const void*>(b + scene_b_rva),
			            2 * sizeof(void*));

			if (!write_bytes(insn + 4, &new_rva, sizeof(new_rva)))
			{
				return false;
			}
			scene_b_new_rva = new_rva;
			scene_b_relocated = true;
			note("[splitscreen] scene buffer B [2]->[4] at RVA 0x%08X\n", new_rva);
			return true;
		}

		// Allocate the slots the count-2 loop skips. Runs on the renderer
		// pipeline and does nothing once the slots are filled.
		void fill_scene_buffers()
		{
			if (scene_buffers_filled || !scene_b_relocated
				|| !raise_local_client_count)
			{
				return;
			}
			const auto b = base();
			uint32_t elems = 0;
			std::memcpy(&elems, reinterpret_cast<const void*>(b + scene_size_rva),
			            sizeof(elems));
			if (elems == 0 || elems > 0x100000)
			{
				return; // config not copied yet
			}

			auto** a = reinterpret_cast<void**>(b + scene_a_rva);
			// Only the relocated C has real slots 2/3 (old C[2] is a foreign array).
			if (!scene_c_new)
			{
				return;
			}
			auto** c = reinterpret_cast<void**>(scene_c_new);
			auto** bb = reinterpret_cast<void**>(b + scene_b_new_rva);
			if (!readable(a, scene_slots * sizeof(void*))
				|| !readable(c, scene_slots * sizeof(void*)))
			{
				return;
			}
			if (a[0] == nullptr || c[0] == nullptr)
			{
				return; // R_InitSceneBuffers has not run yet
			}
			if (a[2] != nullptr && c[2] != nullptr)
			{
				scene_buffers_filled = true;
				return;
			}

			const auto grab = [](const size_t bytes) -> void*
			{
				// zero-filled by the OS, which is the state the engine's own
				// allocator hands back here
				return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
				                    PAGE_READWRITE);
			};

			for (size_t lc = 2; lc < scene_slots; ++lc)
			{
				if (a[lc] == nullptr)
				{
					auto* p = grab(elems);
					if (!p) { return; }
					a[lc] = p;
				}
				if (bb[lc] == nullptr)
				{
					auto* p = grab(elems);
					if (!p) { return; }
					bb[lc] = p;
				}
				if (c[lc] == nullptr)
				{
					auto* p = grab(static_cast<size_t>(elems) * 8);
					if (!p) { return; }
					c[lc] = p;
				}
			}
			scene_buffers_filled = true;
			note("[splitscreen] scene buffers allocated for clients 2/3"
			     " (%u elements)\n", elems);
		}

		// ============ cgEntCollWorld / cgEntCollNodes: entity collision ========
		// Both are [2] with foreign slots 2/3, cleared for lc 2 by the inlined
		// CG_ClearEntityCollWorld, so both are relocated. That function (PS4
		// 0x189890, called from CG_SetInitialSnapshot) builds the free-list, so
		// the engine initializes slots 2/3 itself.
		// The site tables come from tools/gen_entcoll_sites.py; never edit them by
		// hand (a hand-built table missed 8 field accessors and hung the game).
		// target_off: a site may point at a field of element 0.
		// History: LOG.md, ae57c92
		struct entcoll_site
		{
			uint32_t rva;         // instruction start
			uint8_t disp_off;     // byte offset of the disp32 inside it
			uint8_t insn_len;     // total instruction length
			bool rip;             // true: disp is rip-relative; false: absolute RVA
			uint32_t target_off;  // target's offset inside element 0
		};

		constexpr uint32_t entcoll_world_base = 0x04764BA0;
		constexpr uint32_t entcoll_world_stride = 0x401C;
		constexpr uint32_t entcoll_nodes_base = 0x032608B0;
		constexpr uint32_t entcoll_node_bytes = 0xC400;
		constexpr size_t entcoll_slots = 4;

		constexpr entcoll_site entcoll_world_sites[] = {
			{0x0058B173, 3, 7, true,  0x0028}, // lea rcx,[rip+..]
			{0x0058B1F6, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0058B2DE, 3, 7, false, 0x0000}, // lea rdi,[rsi+0x047E3BA0]
			{0x0058B445, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0058B60C, 3, 7, false, 0x0000}, // lea rdi,[rsi+0x047E3BA0]
			{0x0058B710, 3, 7, true,  0x0028}, // lea rax,[rip+..]
			{0x0058B886, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0129DE34, 3, 7, true,  0x001C}, // lea rcx,[rip+..]
			{0x0129DF28, 3, 7, true,  0x001C}, // lea rcx,[rip+..]
			{0x0129E784, 3, 7, true,  0x001C}, // lea r10,[rip+..]
			{0x0129EA61, 3, 7, true,  0x001C}, // lea r10,[rip+..]
			{0x012AEC27, 3, 7, true,  0x001C}, // lea rdi,[rip+..]
			{0x012AEE47, 3, 7, true,  0x001C}, // lea rdi,[rip+..]
		};

		constexpr entcoll_site entcoll_node_sites[] = {
			{0x0058B17A, 3, 7, true,  0x0000}, // lea r10,[rip+..]
			{0x0058B2FD, 4, 8, false, 0x0000}, // mov rcx,[rsi+rbx*8+0x032DF8B0]
			{0x0058B44F, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
			{0x0058B660, 4, 8, false, 0x0000}, // mov rsi,[rsi+rdx*8+..]
			{0x0058B721, 4, 8, false, 0x0000}, // mov r9,[rsi+r9*8+..]
			{0x0058B853, 4, 8, false, 0x0000}, // lea rdx,[r12+..]
			{0x0070DE25, 4, 8, false, 0x0000}, // mov rax,[rcx+rax*8+..]
			{0x0070FB49, 4, 8, false, 0x0000}, // mov rax,[rdx+rax*8+..]
			{0x0072E424, 4, 8, false, 0x0000}, // mov rax,[r8+r14*8+..]
			{0x0072E607, 4, 8, false, 0x0000}, // mov rax,[rcx+r14*8+..]
			{0x00731AA3, 4, 8, false, 0x0000}, // mov rax,[r12+r14*8+..]
			{0x00FEEED2, 3, 7, true,  0x0000}, // lea rsi,[rip+..]
			{0x0129DE45, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129DECC, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129DF21, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129E78B, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129E820, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x0129EA5A, 3, 7, true,  0x0000}, // lea r8,[rip+..]
			{0x012AEC63, 3, 7, true,  0x0000}, // lea rax,[rip+..]
		};

		bool entcoll_relocated = false;
		size_t entcoll_world_new = 0;
		size_t entcoll_nodes_new = 0;

		// Point a site table at new_abs, all or nothing. Every site must first
		// resolve to old_rva + target_off, or nothing is written; the original
		// disps go to `saved` for rollback.
		bool rewrite_entcoll(const entcoll_site* sites, const size_t count,
		                     const uint32_t old_rva, const size_t new_abs, int32_t* saved)
		{
			const auto b = base();

			for (size_t i = 0; i < count; ++i)
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + sites[i].rva);
				if (!readable(insn, sites[i].insn_len))
				{
					note("[splitscreen] entcoll site 0x%08X unreadable\n", sites[i].rva);
					return false;
				}
				std::memcpy(&saved[i], insn + sites[i].disp_off, sizeof(int32_t));
				const auto want = static_cast<int64_t>(b) + old_rva + sites[i].target_off;
				const auto have = sites[i].rip
					? static_cast<int64_t>(b + sites[i].rva + sites[i].insn_len) + saved[i]
					: static_cast<int64_t>(b) + saved[i];
				if (have != want)
				{
					note("[splitscreen] site 0x%08X does not reference 0x%08X+0x%X - "
					     "nothing written\n", sites[i].rva, old_rva, sites[i].target_off);
					return false;
				}
			}

			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t j = 0; j < done; ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + sites[j].rva);
					write_bytes(insn + sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
			};

			for (size_t i = 0; i < count; ++i)
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + sites[i].rva);
				const auto target = new_abs + sites[i].target_off;
				int32_t disp = 0;

				if (sites[i].rip)
				{
					const auto end = static_cast<int64_t>(b + sites[i].rva
					                                      + sites[i].insn_len);
					const auto delta = static_cast<int64_t>(target) - end;
					if (delta > INT32_MAX || delta < INT32_MIN)
					{
						note("[splitscreen] entcoll 0x%08X out of rip range\n",
						     sites[i].rva);
						rollback();
						return false;
					}
					disp = static_cast<int32_t>(delta);
				}
				else
				{
					const auto rva = static_cast<int64_t>(target)
					                 - static_cast<int64_t>(b);
					if (rva < 0 || rva > INT32_MAX)
					{
						note("[splitscreen] entcoll 0x%08X out of abs range\n",
						     sites[i].rva);
						rollback();
						return false;
					}
					disp = static_cast<int32_t>(rva);
				}

				if (!write_bytes(insn + sites[i].disp_off, &disp, sizeof(disp)))
				{
					note("[splitscreen] entcoll 0x%08X write failed\n", sites[i].rva);
					rollback();
					return false;
				}
				++done;
			}
			return true;
		}

		// ============ Completion of earlier relocations ============
		// Sites that tools/audit_reloc_tables.py found still pointing at the old
		// array of playersKb (key-state bytes) and s_gamePads while the engine
		// used the moved one (data/reloc_sites/completion_2026-09-28.txt).
		// Not completed: numdestructibles' hits are the previous array's end
		// markers (rewriting them crashed at boot); read the loop before
		// rewriting a "still targets the old base" hit.
		constexpr entcoll_site players_kb_completion_sites[] = {
			{0x012F3574, 4, 9, false, 0x01A8}, // cmp byte ptr [rbx + r14 + 0x52f2b98], 0
			{0x012F357F, 4, 9, false, 0x01A9}, // cmp byte ptr [rbx + r14 + 0x52f2b99], 0
			{0x012F3591, 4, 9, false, 0x01A9}, // mov byte ptr [rbx + r14 + 0x52f2b99], 0
			{0x012F359A, 4, 9, false, 0x01C0}, // cmp byte ptr [rbx + r14 + 0x52f2bb0], 0
			{0x012F35A5, 4, 9, false, 0x01C1}, // cmp byte ptr [rbx + r14 + 0x52f2bb1], 0
			{0x012F35B7, 4, 9, false, 0x01C1}, // mov byte ptr [rbx + r14 + 0x52f2bb1], 0
			{0x012F35C0, 4, 9, false, 0x0220}, // cmp byte ptr [rbx + r14 + 0x52f2c10], 0
			{0x012F35CB, 4, 9, false, 0x0221}, // cmp byte ptr [rbx + r14 + 0x52f2c11], 0
			{0x012F35DD, 4, 9, false, 0x0221}, // mov byte ptr [rbx + r14 + 0x52f2c11], 0
			{0x012F35E6, 4, 9, false, 0x0238}, // cmp byte ptr [rbx + r14 + 0x52f2c28], 0
			{0x012F35F1, 4, 9, false, 0x0239}, // cmp byte ptr [rbx + r14 + 0x52f2c29], 0
			{0x012F3603, 4, 9, false, 0x0239}, // mov byte ptr [rbx + r14 + 0x52f2c29], 0
			{0x012F360C, 4, 9, false, 0x0250}, // cmp byte ptr [rbx + r14 + 0x52f2c40], 0
			{0x012F3617, 4, 9, false, 0x0251}, // cmp byte ptr [rbx + r14 + 0x52f2c41], 0
			{0x012F3629, 4, 9, false, 0x0251}, // mov byte ptr [rbx + r14 + 0x52f2c41], 0
			{0x012F3632, 4, 9, false, 0x0268}, // cmp byte ptr [rbx + r14 + 0x52f2c58], 0
			{0x012F363D, 4, 9, false, 0x0269}, // cmp byte ptr [rbx + r14 + 0x52f2c59], 0
			{0x012F364F, 4, 9, false, 0x0269}, // mov byte ptr [rbx + r14 + 0x52f2c59], 0
			{0x012F3658, 4, 9, false, 0x0280}, // cmp byte ptr [rbx + r14 + 0x52f2c70], 0
			{0x012F3663, 4, 9, false, 0x0281}, // cmp byte ptr [rbx + r14 + 0x52f2c71], 0
			{0x012F3675, 4, 9, false, 0x0281}, // mov byte ptr [rbx + r14 + 0x52f2c71], 0
			{0x012F367E, 4, 9, false, 0x0298}, // cmp byte ptr [rbx + r14 + 0x52f2c88], 0
			{0x012F3689, 4, 9, false, 0x0299}, // cmp byte ptr [rbx + r14 + 0x52f2c89], 0
			{0x012F369B, 4, 9, false, 0x0299}, // mov byte ptr [rbx + r14 + 0x52f2c89], 0
			{0x012F36A4, 4, 9, false, 0x02B0}, // cmp byte ptr [rbx + r14 + 0x52f2ca0], 0
			{0x012F36AF, 4, 9, false, 0x02B1}, // cmp byte ptr [rbx + r14 + 0x52f2ca1], 0
			{0x012F36C1, 4, 9, false, 0x02B1}, // mov byte ptr [rbx + r14 + 0x52f2ca1], 0
			{0x012F36CA, 4, 9, false, 0x02C8}, // cmp byte ptr [rbx + r14 + 0x52f2cb8], 0
			{0x012F36D5, 4, 9, false, 0x02C9}, // cmp byte ptr [rbx + r14 + 0x52f2cb9], 0
			{0x012F36E7, 4, 9, false, 0x02C9}, // mov byte ptr [rbx + r14 + 0x52f2cb9], 0
			{0x012F36F0, 4, 9, false, 0x0100}, // cmp byte ptr [rbx + r14 + 0x52f2af0], 0
			{0x012F36FB, 4, 9, false, 0x0101}, // cmp byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F370D, 4, 9, false, 0x0101}, // mov byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F3716, 4, 9, false, 0x02E0}, // cmp byte ptr [rbx + r14 + 0x52f2cd0], 0
			{0x012F3721, 4, 9, false, 0x02E1}, // cmp byte ptr [rbx + r14 + 0x52f2cd1], 0
			{0x012F3733, 4, 9, false, 0x02E1}, // mov byte ptr [rbx + r14 + 0x52f2cd1], 0
			{0x012F373C, 4, 9, false, 0x0328}, // cmp byte ptr [rbx + r14 + 0x52f2d18], 0
			{0x012F3747, 4, 9, false, 0x0329}, // cmp byte ptr [rbx + r14 + 0x52f2d19], 0
			{0x012F3789, 4, 9, false, 0x0329}, // mov byte ptr [rbx + r14 + 0x52f2d19], 0
			{0x012F3792, 4, 9, false, 0x0340}, // cmp byte ptr [rbx + r14 + 0x52f2d30], 0
			{0x012F379D, 4, 9, false, 0x0341}, // cmp byte ptr [rbx + r14 + 0x52f2d31], 0
			{0x012F37AF, 4, 9, false, 0x0341}, // mov byte ptr [rbx + r14 + 0x52f2d31], 0
			{0x012F37B8, 4, 9, false, 0x0358}, // cmp byte ptr [rbx + r14 + 0x52f2d48], 0
			{0x012F37C3, 4, 9, false, 0x0359}, // cmp byte ptr [rbx + r14 + 0x52f2d49], 0
			{0x012F37D5, 4, 9, false, 0x0359}, // mov byte ptr [rbx + r14 + 0x52f2d49], 0
			{0x012F37DE, 4, 9, false, 0x0370}, // cmp byte ptr [rbx + r14 + 0x52f2d60], 0
			{0x012F37E9, 4, 9, false, 0x0371}, // cmp byte ptr [rbx + r14 + 0x52f2d61], 0
			{0x012F37F8, 4, 9, false, 0x0371}, // mov byte ptr [rbx + r14 + 0x52f2d61], 0
			{0x012F3801, 4, 9, false, 0x0388}, // cmp byte ptr [rbx + r14 + 0x52f2d78], 0
			{0x012F380C, 4, 9, false, 0x0389}, // cmp byte ptr [rbx + r14 + 0x52f2d79], 0
			{0x012F381E, 4, 9, false, 0x0389}, // mov byte ptr [rbx + r14 + 0x52f2d79], 0
			{0x012F3827, 4, 9, false, 0x03A0}, // cmp byte ptr [rbx + r14 + 0x52f2d90], 0
			{0x012F3832, 4, 9, false, 0x03A1}, // cmp byte ptr [rbx + r14 + 0x52f2d91], 0
			{0x012F3844, 4, 9, false, 0x03A1}, // mov byte ptr [rbx + r14 + 0x52f2d91], 0
			{0x012F384D, 4, 9, false, 0x03B8}, // cmp byte ptr [rbx + r14 + 0x52f2da8], 0
			{0x012F3858, 4, 9, false, 0x03B9}, // cmp byte ptr [rbx + r14 + 0x52f2da9], 0
			{0x012F386A, 4, 9, false, 0x03B9}, // mov byte ptr [rbx + r14 + 0x52f2da9], 0
			{0x012F3873, 4, 9, false, 0x03D0}, // cmp byte ptr [rbx + r14 + 0x52f2dc0], 0
			{0x012F387E, 4, 9, false, 0x03D1}, // cmp byte ptr [rbx + r14 + 0x52f2dc1], 0
			{0x012F3890, 4, 9, false, 0x03D1}, // mov byte ptr [rbx + r14 + 0x52f2dc1], 0
			{0x012F3899, 4, 9, false, 0x03E8}, // cmp byte ptr [rbx + r14 + 0x52f2dd8], 0
			{0x012F38A4, 4, 9, false, 0x03E9}, // cmp byte ptr [rbx + r14 + 0x52f2dd9], 0
			{0x012F38B3, 4, 9, false, 0x03E9}, // mov byte ptr [rbx + r14 + 0x52f2dd9], 0
			{0x012F38BC, 4, 9, false, 0x0460}, // cmp byte ptr [rbx + r14 + 0x52f2e50], 0
			{0x012F38C7, 4, 9, false, 0x0461}, // cmp byte ptr [rbx + r14 + 0x52f2e51], 0
			{0x012F38D9, 4, 9, false, 0x0461}, // mov byte ptr [rbx + r14 + 0x52f2e51], 0
			{0x012F38E2, 4, 9, false, 0x02F8}, // cmp byte ptr [rbx + r14 + 0x52f2ce8], 0
			{0x012F38ED, 4, 9, false, 0x02F9}, // cmp byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F38FC, 4, 9, false, 0x02F9}, // mov byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F3905, 4, 9, false, 0x0400}, // cmp byte ptr [rbx + r14 + 0x52f2df0], 0
			{0x012F3910, 4, 9, false, 0x0401}, // cmp byte ptr [rbx + r14 + 0x52f2df1], 0
			{0x012F3922, 4, 9, false, 0x0401}, // mov byte ptr [rbx + r14 + 0x52f2df1], 0
			{0x012F392B, 4, 9, false, 0x0418}, // cmp byte ptr [rbx + r14 + 0x52f2e08], 0
			{0x012F3936, 4, 9, false, 0x0419}, // cmp byte ptr [rbx + r14 + 0x52f2e09], 0
			{0x012F3948, 4, 9, false, 0x0419}, // mov byte ptr [rbx + r14 + 0x52f2e09], 0
			{0x012F3951, 4, 9, false, 0x0430}, // cmp byte ptr [rbx + r14 + 0x52f2e20], 0
			{0x012F395C, 4, 9, false, 0x0431}, // cmp byte ptr [rbx + r14 + 0x52f2e21], 0
			{0x012F396E, 4, 9, false, 0x0431}, // mov byte ptr [rbx + r14 + 0x52f2e21], 0
			{0x012F3977, 4, 9, false, 0x0448}, // cmp byte ptr [rbx + r14 + 0x52f2e38], 0
			{0x012F3982, 4, 9, false, 0x0449}, // cmp byte ptr [rbx + r14 + 0x52f2e39], 0
			{0x012F3996, 4, 9, false, 0x0449}, // mov byte ptr [rbx + r14 + 0x52f2e39], 0
			{0x012F39A4, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F39AF, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F39C1, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F39CA, 4, 9, false, 0x02F8}, // cmp byte ptr [rbx + r14 + 0x52f2ce8], 0
			{0x012F39D5, 4, 9, false, 0x02F9}, // cmp byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F39E7, 4, 9, false, 0x02F9}, // mov byte ptr [rbx + r14 + 0x52f2ce9], 0
			{0x012F39F0, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F39FB, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3A0D, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3A16, 4, 9, false, 0x0310}, // cmp byte ptr [rbx + r14 + 0x52f2d00], 0
			{0x012F3A21, 4, 9, false, 0x0311}, // cmp byte ptr [rbx + r14 + 0x52f2d01], 0
			{0x012F3A33, 4, 9, false, 0x0311}, // mov byte ptr [rbx + r14 + 0x52f2d01], 0
			{0x012F3A3C, 4, 9, false, 0x0100}, // cmp byte ptr [rbx + r14 + 0x52f2af0], 0
			{0x012F3A47, 4, 9, false, 0x0101}, // cmp byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F3A5E, 4, 9, false, 0x0101}, // mov byte ptr [rbx + r14 + 0x52f2af1], 0
			{0x012F3AB2, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F3ABD, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3ACF, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3AE1, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
			{0x012F3AEC, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012F3AFE, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
			{0x012FEEE7, 3, 8, false, 0x0160}, // cmp byte ptr [rdi + rax + 0x52f2b50], 0
			{0x012FEEF1, 3, 8, false, 0x0161}, // cmp byte ptr [rdi + rax + 0x52f2b51], 0
			{0x012FEF41, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FEF4C, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FEF65, 4, 9, false, 0x0161}, // mov byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FEF83, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FEF8E, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF000, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FF00B, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF0E3, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
			{0x012FF0EE, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF101, 4, 9, false, 0x0161}, // mov byte ptr [rdi + r14 + 0x52f2b51], 0
			{0x012FF114, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
			{0x012FF11F, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF130, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
			{0x012FF13B, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF178, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
			{0x012FF183, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF196, 4, 9, false, 0x0191}, // mov byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF19F, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
			{0x012FF1AA, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF1BD, 4, 9, false, 0x0179}, // mov byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF1EA, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
			{0x012FF1F5, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF208, 4, 9, false, 0x0191}, // mov byte ptr [rdi + r14 + 0x52f2b81], 0
			{0x012FF211, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
			{0x012FF21C, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x012FF22F, 4, 9, false, 0x0179}, // mov byte ptr [rdi + r14 + 0x52f2b69], 0
			{0x01306C23, 3, 8, false, 0x0208}, // cmp byte ptr [rbx + rsi + 0x52f2bf8], 0
			{0x01306C2D, 3, 8, false, 0x0209}, // cmp byte ptr [rbx + rsi + 0x52f2bf9], 0
			{0x01306C45, 3, 8, false, 0x0209}, // mov byte ptr [rbx + rsi + 0x52f2bf9], 0
			{0x01306C50, 3, 8, false, 0x01D8}, // cmp byte ptr [rbx + rsi + 0x52f2bc8], 0
			{0x01306C5A, 3, 8, false, 0x01D9}, // cmp byte ptr [rbx + rsi + 0x52f2bc9], 0
			{0x01306C71, 3, 8, false, 0x01F0}, // cmp byte ptr [rbx + rsi + 0x52f2be0], 0
			{0x01306C7B, 3, 8, false, 0x01F1}, // cmp byte ptr [rbx + rsi + 0x52f2be1], 0
			{0x01306CBF, 3, 8, false, 0x020A}, // cmp byte ptr [rbx + rsi + 0x52f2bfa], 0
			{0x01306CD0, 3, 8, false, 0x020A}, // mov byte ptr [rbx + rsi + 0x52f2bfa], 1
			{0x01306CF0, 4, 10, false, 0x01D9}, // mov word ptr [rbx + rsi + 0x52f2bc9], 0x100
			{0x01306CFA, 4, 10, false, 0x01F1}, // mov word ptr [rbx + rsi + 0x52f2be1], 0x100
			{0x01306D11, 3, 8, false, 0x020A}, // mov byte ptr [rbx + rsi + 0x52f2bfa], 0
			{0x01306D19, 3, 8, false, 0x01DA}, // cmp byte ptr [rbx + rsi + 0x52f2bca], 0
			{0x01306D28, 3, 8, false, 0x01D8}, // cmp byte ptr [rbx + rsi + 0x52f2bc8], 0
			{0x01306D32, 3, 8, false, 0x01D9}, // cmp byte ptr [rbx + rsi + 0x52f2bc9], 0
			{0x01306D44, 3, 8, false, 0x01D9}, // mov byte ptr [rbx + rsi + 0x52f2bc9], 0
			{0x01306D59, 3, 8, false, 0x01F2}, // cmp byte ptr [rbx + rsi + 0x52f2be2], 0
			{0x01306D68, 3, 8, false, 0x01F0}, // cmp byte ptr [rbx + rsi + 0x52f2be0], 0
			{0x01306D72, 3, 8, false, 0x01F1}, // cmp byte ptr [rbx + rsi + 0x52f2be1], 0
			{0x01306D84, 3, 8, false, 0x01F1}, // mov byte ptr [rbx + rsi + 0x52f2be1], 0
			{0x01308DD0, 4, 9, false, 0x01A8}, // mov byte ptr [rax + r14 + 0x52f2b98], 1
			{0x01308DD9, 4, 9, false, 0x01A9}, // mov byte ptr [rax + r14 + 0x52f2b99], 1
			{0x0131B73B, 4, 9, false, 0x02B0}, // cmp byte ptr [rdx + r8 + 0x52f2ca0], 0
			{0x0131B746, 4, 9, false, 0x0118}, // cmp byte ptr [rdx + r8 + 0x52f2b08], 0
			{0x0131B8BB, 4, 9, false, 0x02B0}, // cmp byte ptr [rdx + r8 + 0x52f2ca0], 0
			{0x0131B8C6, 4, 9, false, 0x0118}, // cmp byte ptr [rdx + r8 + 0x52f2b08], 0
			{0x0131BC5E, 4, 9, false, 0x02F8}, // cmp byte ptr [rdi + r15 + 0x52f2ce8], 0
			{0x0131BC71, 5, 11, false, 0x02F8}, // mov word ptr [rdi + r15 + 0x52f2ce8], 0x101
			{0x0131BCDA, 4, 9, false, 0x0118}, // cmp byte ptr [rdi + r15 + 0x52f2b08], 0
			{0x0131BCF1, 5, 11, false, 0x0118}, // mov word ptr [rdi + r15 + 0x52f2b08], 0x101
			{0x0131BD01, 4, 9, false, 0x02B0}, // cmp byte ptr [rdi + r15 + 0x52f2ca0], 0
			{0x0131BD10, 4, 9, false, 0x0118}, // cmp byte ptr [rdi + r15 + 0x52f2b08], 0
		};
		constexpr entcoll_site gamepads_completion_sites[] = {
			{0x02284AF2, 2, 7, true , 0x0074}, // cmp dword ptr [rip + 0x15b7c7bb], 8
			{0x022861A1, 4, 9, false, 0x0000}, // cmp byte ptr [rcx + r9 + 0x17e6e310], 0
		};

		// The array's current base: a moved site's target minus its field offset.
		// 0 if the site still points at the old array (then complete nothing).
		size_t moved_base_from_site(const entcoll_site& s, const uint32_t old_base)
		{
			const auto b = base();
			const auto* insn = reinterpret_cast<const uint8_t*>(b + s.rva);
			if (!readable(insn, s.insn_len))
			{
				return 0;
			}
			int32_t disp = 0;
			std::memcpy(&disp, insn + s.disp_off, sizeof(disp));
			const auto target = s.rip ? static_cast<int64_t>(b + s.rva + s.insn_len) + disp
			                          : static_cast<int64_t>(b) + static_cast<uint32_t>(disp);
			const auto moved = static_cast<size_t>(target) - s.target_off;
			return moved == b + old_base ? 0 : moved;
		}

		void trace_text(const char* text);   // defined after trace_write

		void complete_relocation(const char* name, const entcoll_site& moved_site,
		                         const uint32_t old_base, const entcoll_site* sites,
		                         const size_t count, int32_t* saved, bool& done)
		{
			if (done)
			{
				return;
			}
			const auto fresh = moved_base_from_site(moved_site, old_base);
			char line[160]{};
			if (!fresh)
			{
				std::snprintf(line, sizeof(line), "%s completion: base not moved - skipped", name);
			}
			else if (!rewrite_entcoll(sites, count, old_base, fresh, saved))
			{
				std::snprintf(line, sizeof(line), "%s completion: a site did not match - NOTHING written", name);
			}
			else
			{
				done = true;
				std::snprintf(line, sizeof(line), "%s completion: %zu remaining references moved to the new array",
				              name, count);
			}
			trace_text(line);
		}

		bool players_kb_completed = false;
		bool gamepads_completed = false;

		// Probe site the playersKb relocation always rewrites (IN_Attack_Up's
		// kbutton read). s_gamePads uses its own verified destination instead.
		constexpr entcoll_site players_kb_probe = {0x0131B2B1, 4, 8, false, 0x0198};   // reloc.hpp players_kb row

		void complete_players_kb()
		{
			static int32_t saved[std::size(players_kb_completion_sites)]{};
			complete_relocation("playersKb", players_kb_probe, 0x052739F0,
			                    players_kb_completion_sites, std::size(players_kb_completion_sites),
			                    saved, players_kb_completed);
		}

		void complete_gamepads(const size_t destination_abs)
		{
			if (gamepads_completed)
			{
				return;
			}
			static int32_t saved[std::size(gamepads_completion_sites)]{};
			if (rewrite_entcoll(gamepads_completion_sites, std::size(gamepads_completion_sites),
			                    0x17DEF3E0, destination_abs, saved))
			{
				gamepads_completed = true;
				trace_text("s_gamePads completion: 2 remaining references moved");
			}
			else
			{
				trace_text("s_gamePads completion: a site did not match - NOTHING written");
			}
		}


		// Retarget one RIP-relative loop end marker (`lea reg,[rip+d32]`), which site
		// tables cannot hold because it points past the array (tools/endmarker_scan.py).
		// Writes nothing and returns false unless it currently targets old_target_rva.
		bool retarget_end_marker(const uint32_t insn_rva, const uint8_t disp_off, const uint8_t insn_len,
		                         const uint32_t old_target_rva, const size_t new_target_abs)
		{
			const auto b = base();
			auto* disp_at = reinterpret_cast<uint8_t*>(b + insn_rva + disp_off);
			int32_t cur = 0;
			if (!readable(disp_at, sizeof(cur)))
			{
				return false;
			}
			std::memcpy(&cur, disp_at, sizeof(cur));
			const auto end = static_cast<int64_t>(b + insn_rva + insn_len);
			if (end + cur != static_cast<int64_t>(b + old_target_rva))
			{
				note("[splitscreen] end marker 0x%08X does not point at 0x%08X - untouched\n",
				     insn_rva, old_target_rva);
				return false;
			}
			const auto delta = static_cast<int64_t>(new_target_abs) - end;
			if (delta > INT32_MAX || delta < INT32_MIN)
			{
				return false;
			}
			const auto d32 = static_cast<int32_t>(delta);
			return write_bytes(disp_at, &d32, sizeof(d32));
		}

		bool relocate_entity_collision()
		{
			if (entcoll_relocated)
			{
				return true;
			}
			const auto b = base();

			auto* world = static_cast<uint8_t*>(
				allocate_near_module(entcoll_slots * entcoll_world_stride));
			auto* nodes = static_cast<uint8_t*>(
				allocate_near_module(entcoll_slots * sizeof(void*)));
			if (!world || !nodes)
			{
				return false;
			}
			std::memset(world, 0, entcoll_slots * entcoll_world_stride);
			std::memset(nodes, 0, entcoll_slots * sizeof(void*));

			// carry the two live elements across before any code reads them
			std::memcpy(world, reinterpret_cast<const void*>(b + entcoll_world_base),
			            2 * entcoll_world_stride);
			std::memcpy(nodes, reinterpret_cast<const void*>(b + entcoll_nodes_base),
			            2 * sizeof(void*));

			// Node storage for slots 2/3, which the engine never allocates.
			// CG_ClearEntityCollWorld clears it; it only has to exist.
			auto** node_ptrs = reinterpret_cast<void**>(nodes);
			for (size_t lc = 2; lc < entcoll_slots; ++lc)
			{
				auto* buf = VirtualAlloc(nullptr, entcoll_node_bytes,
				                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				if (!buf)
				{
					return false;
				}
				node_ptrs[lc] = buf;
			}

			static int32_t saved_world[std::size(entcoll_world_sites)]{};
			static int32_t saved_nodes[std::size(entcoll_node_sites)]{};

			if (!rewrite_entcoll(entcoll_world_sites, std::size(entcoll_world_sites),
			                     entcoll_world_base, reinterpret_cast<size_t>(world), saved_world))
			{
				return false;
			}
			if (!rewrite_entcoll(entcoll_node_sites, std::size(entcoll_node_sites),
			                     entcoll_nodes_base, reinterpret_cast<size_t>(nodes), saved_nodes))
			{
				// undo the world group too - either both move or neither does
				for (size_t j = 0; j < std::size(entcoll_world_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(
						b + entcoll_world_sites[j].rva);
					write_bytes(insn + entcoll_world_sites[j].disp_off,
					            &saved_world[j], sizeof(int32_t));
				}
				return false;
			}

			entcoll_world_new = reinterpret_cast<size_t>(world);
			entcoll_nodes_new = reinterpret_cast<size_t>(nodes);
			entcoll_relocated = true;
			note("[splitscreen] entity collision [2]->[4]: world RVA 0x%08X,"
			     " nodes RVA 0x%08X (%zu + %zu sites)\n",
			     static_cast<uint32_t>(entcoll_world_new - b),
			     static_cast<uint32_t>(entcoll_nodes_new - b),
			     std::size(entcoll_world_sites), std::size(entcoll_node_sites));
			return true;
		}

		// Clientfield pending-callback buffer, [2] -> [4] clients. Client 2 faulted
		// at 0x00132F94. Layout: entries 2 x 2048 x 32 bytes, then counts [2] x 4
		// at +0x20000 (total 0x20008). Growing it alone would put lc 2's entries on
		// the counts, so the count offset moves to 0x40000 (6 sites) and the size
		// to 0x40010 (3 sites). Consumers read the buffer through cf_pointer_rva.
		// All or nothing with rollback; gated on BO3_CG_FRAME.
		struct cf_imm
		{
			uint32_t rva;
			uint8_t off;
			uint32_t was;
			uint32_t want;
		};
		constexpr uint32_t cf_buffer_rva = 0x049925A0;
		constexpr uint32_t cf_pointer_rva = 0x04992590;
		constexpr size_t cf_new_bytes = 0x40010;   // 4 * 0x10000 + 4 * 4
		constexpr uint32_t cf_lea_sites[] = {0x008F26E3, 0x008F2DB0}; // 7 B, disp @3
		constexpr cf_imm cf_imms[] = {
			{0x00132F59, 3, 0x20000, 0x40000}, // mov r8d,[r9+0x20000]
			{0x00132F7F, 3, 0x20000, 0x40000}, // mov [r9+0x20000],eax
			{0x0013304D, 3, 0x20000, 0x40000}, // mov r8d,[r10+0x20000]
			{0x00133079, 3, 0x20000, 0x40000}, // mov [r10+0x20000],eax
			{0x001337F6, 4, 0x20000, 0x40000}, // lea r15,[r8*4+0x20000]
			{0x00136BD4, 4, 0x20000, 0x40000}, // lea r14,[rbx*4+0x20000]
			{0x0013444B, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (clear)
			{0x00137160, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (clear)
			{0x008F2DB9, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (init memset)
		};
		bool cf_relocated = false;
		size_t cf_new_buffer = 0;

		bool relocate_clientfield_callbacks()
		{
			if (cf_relocated)
			{
				return true;
			}
			const auto b = base();

			// every site must still be stock before anything is written
			for (const auto& s : cf_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				uint32_t cur = 0;
				if (!readable(at, sizeof(cur)))
				{
					return false;
				}
				std::memcpy(&cur, at, sizeof(cur));
				if (cur != s.was)
				{
					note("[splitscreen] clientfield imm 0x%08X reads 0x%X, want 0x%X"
					     " - not patching\n", s.rva, cur, s.was);
					return false;
				}
			}
			int32_t saved_lea[std::size(cf_lea_sites)]{};
			for (size_t i = 0; i < std::size(cf_lea_sites); ++i)
			{
				const auto* insn = reinterpret_cast<const uint8_t*>(b + cf_lea_sites[i]);
				if (!readable(insn, 7) || insn[1] != 0x8D)
				{
					return false;
				}
				std::memcpy(&saved_lea[i], insn + 3, sizeof(int32_t));
				const auto tgt = static_cast<size_t>(cf_lea_sites[i] + 7 + saved_lea[i]);
				if (tgt != cf_buffer_rva)
				{
					note("[splitscreen] clientfield lea 0x%08X -> 0x%zX, not the buffer\n",
					     cf_lea_sites[i], tgt);
					return false;
				}
			}

			auto* fresh = static_cast<uint8_t*>(allocate_near_module(cf_new_bytes));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, cf_new_bytes);
			// Carry the two live client regions and their counts across, in
			// case the engine's init already ran.
			std::memcpy(fresh, reinterpret_cast<const void*>(b + cf_buffer_rva), 0x20000);
			std::memcpy(fresh + 0x40000,
			            reinterpret_cast<const void*>(b + cf_buffer_rva + 0x20000), 8);

			size_t done_lea = 0, done_imm = 0;
			const auto rollback = [&]
			{
				for (size_t j = 0; j < done_lea; ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + cf_lea_sites[j]);
					write_bytes(insn + 3, &saved_lea[j], sizeof(int32_t));
				}
				for (size_t j = 0; j < done_imm; ++j)
				{
					auto* at = reinterpret_cast<uint8_t*>(b + cf_imms[j].rva + cf_imms[j].off);
					write_bytes(at, &cf_imms[j].was, sizeof(uint32_t));
				}
			};

			for (size_t i = 0; i < std::size(cf_lea_sites); ++i)
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + cf_lea_sites[i]);
				const auto end = static_cast<int64_t>(b + cf_lea_sites[i] + 7);
				const auto delta = static_cast<int64_t>(reinterpret_cast<size_t>(fresh)) - end;
				if (delta > INT32_MAX || delta < INT32_MIN)
				{
					rollback();
					return false;
				}
				const auto d32 = static_cast<int32_t>(delta);
				if (!write_bytes(insn + 3, &d32, sizeof(d32)))
				{
					rollback();
					return false;
				}
				++done_lea;
			}
			for (const auto& s : cf_imms)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva + s.off);
				if (!write_bytes(at, &s.want, sizeof(s.want)))
				{
					rollback();
					return false;
				}
				++done_imm;
			}

			// Also update the pointer the consumers read, in case the engine's
			// init already stored the old buffer there.
			auto* ptr = reinterpret_cast<void**>(b + cf_pointer_rva);
			if (readable(ptr, sizeof(void*)))
			{
				void* v = fresh;
				write_bytes(ptr, &v, sizeof(v));
			}

			cf_new_buffer = reinterpret_cast<size_t>(fresh);
			cf_relocated = true;
			note("[splitscreen] clientfield callbacks [2]->[4]: buffer RVA 0x%08X,"
			     " 2 leas + %zu immediates\n",
			     static_cast<uint32_t>(cf_new_buffer - b), std::size(cf_imms));
			return true;
		}

		// clientObjMap: per-client DObj handle table, PC [2][0x702] -> PS4 layout
		// [4][0x704] (PS4 0x101D3470; viewmodel handle = 0x700 + lc). Client 2 hit
		// MSVC's range check (fail-fast 0xC0000409, so no crash dialog), and with
		// 0x702-word rows lc 2's viewmodel aliased lc 3's entity 0. The table cannot
		// grow in place, so it moves, and every row-dependent constant changes with
		// it: stride, row count, byte size 0x1C08 -> 0x3820 (memset and /GS bound)
		// and the client count 2 -> 4 of the PC-only free-all / rebuild-all pair.
		constexpr uint32_t entword_base = 0x16D545D0;
		constexpr uint32_t entword_old_row = 0x702;             // PC handles per client
		constexpr uint32_t entword_row = 0x704;                 // PS4 handles per client
		constexpr uint32_t entword_client_bytes = entword_row * 2;   // 0xE08
		constexpr size_t entword_slots = 4;
		constexpr entcoll_site entword_sites[] = {
			{0x020F3CF4, 3, 7, true,  0}, // lea rcx,[rip+..]
			{0x020F55FD, 4, 8, false, 0}, // mov word [rax+rdx*2+0x16DD3540],di
			{0x020F5689, 3, 7, true,  0}, // lea rcx,[rip+..]
			{0x020F5858, 3, 7, true,  0}, // lea rcx,[rip+..]   (the clear)
			{0x020F5967, 5, 9, false, 0}, // movsx rcx,word [rax+r10+..]  (crash site)
			{0x020F5999, 5, 9, false, 0}, // mov word [rax+r10+..],dx
			{0x020F5CB3, 5, 9, false, 0}, // movzx ecx,word cs:[rcx+rax*2+..]
			{0x020F8F4C, 3, 7, true,  0}, // lea rsi,[rip+..]
		};
		// size = immediate width in bytes (4 = imm32, 1 = imm8)
		struct entword_imm { uint32_t rva; uint8_t off; uint8_t size; uint32_t was; uint32_t want; };
		constexpr entword_imm entword_imms[] = {
			{0x020F5861, 2, 4, 0x1C08, 0x3820}, // mov r8d,0x1c08  - memset size
			{0x020F598F, 2, 4, 0x1C08, 0x3820}, // cmp rax,0x1c08  - the /GS bound
			{0x020F3CDF, 2, 4, 0x702, 0x704},   // imul ecx,ecx,0x702   ClearAllSkel(lc)
			{0x020F3CE5, 1, 4, 0x702, 0x704},   // mov edi,0x702        ClearAllSkel count
			{0x020F55EC, 3, 4, 0x702, 0x704},   // imul rdx,rdx,0x702   Com_ClientDObjCreate
			{0x020F5680, 2, 4, 0x702, 0x704},   // imul edx,edx,0x702   Com_GetClientDObj
			{0x020F58DE, 2, 4, 0x702, 0x704},   // imul edi,edi,0x702   Com_SafeClientDObjFree
			{0x020F5CA9, 3, 4, 0x702, 0x704},   // imul r13,r13,0x702   rebuild-all row
			{0x020F8D60, 2, 4, 0x702, 0x704},   // cmp esi,0x702        rebuild-all count
			{0x020F8F9F, 2, 4, 0x702, 0x704},   // cmp ebx,0x702        free-all count
			{0x020F8D7D, 3, 1, 0x02, 0x04},     // cmp r12d,2           rebuild-all clients
			{0x020F8FA9, 2, 1, 0x02, 0x04},     // cmp ebp,2            free-all clients
		};
		bool entword_relocated = false;
		size_t entword_new = 0;
		const char* entword_result = "clientObjMap: not attempted";

		bool entword_imm_reads(const entword_imm& s, uint32_t want)
		{
			const auto* at = reinterpret_cast<const uint8_t*>(base() + s.rva + s.off);
			uint32_t cur = 0;
			if (!readable(at, s.size))
			{
				return false;
			}
			std::memcpy(&cur, at, s.size);
			return cur == want;
		}

		bool relocate_entword_table()
		{
			if (entword_relocated)
			{
				return true;
			}
			const auto b = base();
			for (const auto& s : entword_imms)
			{
				if (!entword_imm_reads(s, s.was))
				{
					note("[splitscreen] entword imm 0x%08X is not 0x%X - not patching\n", s.rva, s.was);
					entword_result = "clientObjMap: NOT moved - an immediate did not match";
					return false;
				}
			}

			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(entword_slots * entword_client_bytes));
			if (!fresh)
			{
				entword_result = "clientObjMap: NOT moved - allocation failed";
				return false;
			}
			// Re-stride the two stock rows: old row r (0x702 words) -> new row r
			// (0x704 words). Handles 0x702/0x703 of a row start empty.
			std::memset(fresh, 0, entword_slots * entword_client_bytes);
			for (uint32_t r = 0; r < 2; ++r)
			{
				std::memcpy(fresh + r * entword_client_bytes,
				            reinterpret_cast<const void*>(b + entword_base + r * entword_old_row * 2),
				            entword_old_row * 2);
			}

			static int32_t saved[std::size(entword_sites)]{};
			if (!rewrite_entcoll(entword_sites, std::size(entword_sites),
			                     entword_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			size_t done = 0;
			for (const auto& s : entword_imms)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva + s.off);
				if (!write_bytes(at, &s.want, s.size))
				{
					for (size_t j = 0; j < done; ++j)
					{
						auto* back = reinterpret_cast<uint8_t*>(
							b + entword_imms[j].rva + entword_imms[j].off);
						write_bytes(back, &entword_imms[j].was, entword_imms[j].size);
					}
					for (size_t j = 0; j < std::size(entword_sites); ++j)
					{
						auto* insn = reinterpret_cast<uint8_t*>(b + entword_sites[j].rva);
						write_bytes(insn + entword_sites[j].disp_off, &saved[j], sizeof(int32_t));
					}
					entword_result = "clientObjMap: NOT moved - an immediate write failed (rolled back)";
					return false;
				}
				++done;
			}
			entword_new = reinterpret_cast<size_t>(fresh);
			entword_relocated = true;
			entword_result = "clientObjMap [2][0x702] -> [4][0x704] (8 sites, 12 immediates)";
			note("[splitscreen] clientObjMap [2][0x702] -> [4][0x704] at RVA 0x%08X (8 sites, %zu imms)\n",
			     static_cast<uint32_t>(entword_new - b), std::size(entword_imms));
			return true;
		}

		// s_exposureAdaptions [3] -> [5]: one auto-exposure buffer per local client
		// plus the extra cam, as on PS4 (0xAE89550; RB_FxBloomLDRColorGrade picks
		// `isExtraCam ? 4 : localClientNum`). The PC picked `extraCam ? 2 : lc`, so
		// player 3 shared the extra cam's buffer and player 4 read
		// exposureOutputBuffer (pane 4 overexposed). The slots after [3] are
		// foreign, so the array moves and the selector's 30 bytes are rewritten to
		// PS4's rule. Runs at post_unpack, before R_InitLightingData.
		constexpr uint32_t exposure_base = 0x0F64EBB0;
		constexpr uint32_t exposure_stride = 0x110;
		constexpr uint32_t exposure_old_count = 3;
		constexpr uint32_t exposure_new_count = 5;
		constexpr uint32_t exposure_texture_off = 0x108;
		constexpr entcoll_site exposure_base_sites[] = {
			{0x01CBF7C0, 3, 7, true, 0},   // lea rbx,[base]  free loop
			{0x01CC028F, 3, 7, true, 0},   // lea rcx,[base]  table fill
			{0x01CC06C4, 3, 7, true, 0},   // lea rbx,[base]  create loop
		};
		constexpr entcoll_site exposure_fill_end_site[] = {
			{0x01CBFF0C, 3, 7, true, exposure_old_count * exposure_stride},   // lea r13,[end]
		};
		constexpr entcoll_site exposure_all_end_sites[] = {
			{0x01CBF7C7, 3, 7, true, exposure_old_count * exposure_stride},   // lea rdi,[end] free
			{0x01CC06D2, 3, 7, true, exposure_old_count * exposure_stride},   // lea rdi,[end] create
		};
		constexpr uint32_t exposure_select_rva = 0x01C5FC0C;
		constexpr uint8_t exposure_select_stock[] = {
			0xB8, 0x02, 0x00, 0x00, 0x00,                   // mov eax,2
			0x75, 0x06,                                     // jne +6
			0x8B, 0x82, 0x98, 0x03, 0x00, 0x00,             // mov eax,[rdx+0x398]
			0x8B, 0xC8,                                     // mov ecx,eax
			0x48, 0x8B, 0x82, 0xB0, 0x03, 0x00, 0x00,       // mov rax,[rdx+0x3B0]
			0x48, 0x8B, 0xB4, 0xC8, 0xF0, 0x10, 0x00, 0x00, // mov rsi,[rax+rcx*8+0x10F0]
		};
		constexpr uint32_t exposure_select_lea_off = 19;   // lea rsi,[rip+d] inside the patch
		const char* exposure_result = "exposure adaptions: not attempted";
		size_t exposure_new = 0;

		bool relocate_exposure_adaptions()
		{
			if (exposure_new)
			{
				return true;
			}
			const auto b = base();
			auto* select = reinterpret_cast<uint8_t*>(b + exposure_select_rva);
			if (!readable(select, sizeof(exposure_select_stock))
				|| std::memcmp(select, exposure_select_stock, sizeof(exposure_select_stock)) != 0)
			{
				exposure_result = "exposure adaptions: NOT moved - selector bytes differ at 0x01C6BFDC";
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(exposure_new_count * exposure_stride));
			if (!fresh)
			{
				exposure_result = "exposure adaptions: NOT moved - allocation failed";
				return false;
			}
			std::memset(fresh, 0, exposure_new_count * exposure_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + exposure_base),
			            exposure_old_count * exposure_stride);
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			// New selector bytes; the lea is rip-relative to its own end.
			uint8_t patch[sizeof(exposure_select_stock)] = {
				0xB8, 0x04, 0x00, 0x00, 0x00,                   // mov eax,4 (extra cam = MAX_LOCAL_CLIENTS)
				0x75, 0x06,                                     // jne +6
				0x8B, 0x82, 0x98, 0x03, 0x00, 0x00,             // mov eax,[rdx+0x398] (localClientNum)
				0x69, 0xC0, 0x10, 0x01, 0x00, 0x00,             // imul eax,eax,0x110
				0x48, 0x8D, 0x35, 0x00, 0x00, 0x00, 0x00,       // lea rsi,[rip+d] -> new+0x108
				0x48, 0x01, 0xC6,                               // add rsi,rax
				0x90,                                           // nop
			};
			const auto lea_end = static_cast<int64_t>(b + exposure_select_rva + exposure_select_lea_off + 7);
			const auto lea_disp = static_cast<int64_t>(fresh_abs + exposure_texture_off) - lea_end;
			if (lea_disp < INT32_MIN || lea_disp > INT32_MAX)
			{
				exposure_result = "exposure adaptions: NOT moved - new block out of rip range";
				return false;
			}
			const auto d32 = static_cast<int32_t>(lea_disp);
			std::memcpy(patch + exposure_select_lea_off + 3, &d32, sizeof(d32));

			static int32_t saved_base[std::size(exposure_base_sites)]{};
			static int32_t saved_fill[std::size(exposure_fill_end_site)]{};
			static int32_t saved_all[std::size(exposure_all_end_sites)]{};
			if (!rewrite_entcoll(exposure_base_sites, std::size(exposure_base_sites),
			                     exposure_base, fresh_abs, saved_base))
			{
				exposure_result = "exposure adaptions: NOT moved - a base lea did not match";
				return false;
			}
			const auto undo_base = [&]
			{
				for (size_t j = 0; j < std::size(exposure_base_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exposure_base_sites[j].rva);
					write_bytes(insn + exposure_base_sites[j].disp_off, &saved_base[j], sizeof(int32_t));
				}
			};
			if (!rewrite_entcoll(exposure_fill_end_site, std::size(exposure_fill_end_site),
			                     exposure_base, fresh_abs, saved_fill))
			{
				undo_base();
				exposure_result = "exposure adaptions: NOT moved - the fill end lea did not match";
				return false;
			}
			const auto undo_fill = [&]
			{
				auto* insn = reinterpret_cast<uint8_t*>(b + exposure_fill_end_site[0].rva);
				write_bytes(insn + exposure_fill_end_site[0].disp_off, &saved_fill[0], sizeof(int32_t));
			};
			// target = new_abs + target_off(3*0x110): passing new + 2*0x110 lands on new + 5*0x110.
			if (!rewrite_entcoll(exposure_all_end_sites, std::size(exposure_all_end_sites), exposure_base,
			                     fresh_abs + (exposure_new_count - exposure_old_count) * exposure_stride,
			                     saved_all))
			{
				undo_fill();
				undo_base();
				exposure_result = "exposure adaptions: NOT moved - a create/free end lea did not match";
				return false;
			}
			if (!write_bytes(select, patch, sizeof(patch)))
			{
				for (size_t j = 0; j < std::size(exposure_all_end_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exposure_all_end_sites[j].rva);
					write_bytes(insn + exposure_all_end_sites[j].disp_off, &saved_all[j], sizeof(int32_t));
				}
				undo_fill();
				undo_base();
				exposure_result = "exposure adaptions: NOT moved - selector write failed (rolled back)";
				return false;
			}
			exposure_new = fresh_abs;
			exposure_result = "exposure adaptions [3] -> [5] (PS4 MAX_LOCAL_CLIENTS+1), selector extraCam ? 4 : lc";
			return true;
		}

		// UI model node pool 0x9000 -> 0xFFFF nodes (the node index is a u16).
		// MP ran out of LUI memory at 3-/4-player match start: each player's
		// CustomClassList takes ~11.1k nodes. History: LOG.md, "ROOT CAUSE of the MP LUI".
		// PS4: UI_Model_Init 0xD68140, UI_Model_ResetNode 0xD682E0. PC node is 0x28
		// bytes: +0x1C own index, +0x1E next sibling / next free.
		// The sites include leaf getters without .pdata (missing them made values
		// nil). GetModel's 0x9000 is an end sentinel and stays; the reset loop bound
		// becomes 0xFFFF. Runs at post_unpack, before the hidden UI_Model_Init, which
		// still fills the old array; the new one is pre-built in the reset state,
		// its free list skipping node 0x9000.
		constexpr uint32_t model_pool_base = 0x16293160;
		constexpr uint32_t model_pool_stride = 0x28;
		constexpr uint32_t model_pool_old_count = 0x9000;
		constexpr uint32_t model_pool_new_count = 0xFFFF;
		constexpr uint32_t model_pool_sentinel = 0x9000;
		constexpr uint32_t model_pool_self_off = 0x1C;
		constexpr uint32_t model_pool_next_off = 0x1E;
		constexpr entcoll_site model_pool_sites[] = {
			{0x0200C70E, 3, 7, true , 0x00},   // lea rdx,[base]            AllocateNode
			{0x0200C75F, 3, 7, true , 0x00},   // lea rdx,[base]            AllocateNode
			{0x0200CA81, 3, 7, true , 0x00},   // lea rbx,[base]            FreeModel
			{0x0200CA9E, 3, 7, true , 0x00},   // lea rsi,[base]            FreeModel
			{0x0200CAD3, 3, 7, true , 0x00},   // lea rbx,[base]            FreeModel
			{0x0200CC9F, 3, 7, true , 0x00},   // lea rax,[base]            GetBool   (leaf, no .pdata)
			{0x0200CCCF, 3, 7, true , 0x08},   // lea rax,[base+8]          GetDataType (leaf)
			{0x0200CCEF, 3, 7, true , 0x00},   // lea rax,[base]            GetFunction (leaf)
			{0x0200CD2F, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
			{0x0200CE4D, 3, 7, true , 0x00},   // lea r11,[base]            GetModel
			{0x0200CFA0, 3, 7, true , 0x00},   // lea rax,[base]            GetReal   (leaf)
			{0x0200CFD4, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
			{0x0200CFFF, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
			{0x0200D20F, 5, 9, false, 0x20},   // movzx ebx,[r13+rax*8+base+0x20]  notify
			{0x0200D413, 3, 7, true , 0x20},   // lea rax,[base+0x20]       Reset (subscription heads)
			{0x0200D438, 3, 7, true , 0x22},   // lea rdi,[base+0x22]       Reset (persistent)
			{0x0200D498, 3, 7, true , 0x00},   // lea r9,[base]             typed get/set
			{0x0200D4FC, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D555, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D5AC, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D5F7, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D661, 3, 7, true , 0x00},   // lea r15,[base]            SetString
			{0x0200D74C, 3, 7, true , 0x00},   // lea rax,[base]
			{0x0200D7F1, 4, 8, false, 0x20},   // lea rdx,[rcx*8+base+0x20] Subscribe
			{0x0200D850, 3, 7, false, 0x20},   // lea rdx,[r10+base+0x20]   (leaf)
			{0x0200D97F, 4, 8, false, 0x20},   // movzx ecx,[rax+rbp+base+0x20]  unsubscribe
			{0x0200D9C7, 4, 8, false, 0x1A},   // movzx ecx,[rax+rbp+base+0x1A]
			{0x0200D9E7, 4, 8, false, 0x1E},   // movzx ebx,[rdi+rdx*8+base+0x1E]
		};
		// Command buffers for local clients 2/3. MP players 3/4 never spawned: their
		// class choice (a client command, Cbuf_AddText(lc)) was dropped because cbuf
		// records 2/3 were empty, and Com_Frame executed only lc < 2. As PS4
		// Cbuf_Init 0xE2FB20 does, give records 2/3 a 64 KB buffer each (the records
		// are already [4], reloc_tables "cbuf"), then widen Cbuf_Execute's range
		// check (the bytes it guards for 2/3 are padding) and Com_Frame's loop,
		// both 2 -> 4. Runs at post_unpack, before Cbuf_Init; all or nothing.
		constexpr uint32_t cbuf_old_records_rva = 0x1681EFB8;
		constexpr uint32_t cbuf_exec_lea_rva = 0x020DFBC8;   // lea rax,[records] in Cbuf_ExecuteInternal
		constexpr uint8_t cbuf_exec_lea_head[] = {0x48, 0x8D, 0x05};
		constexpr uint32_t cbuf_range_check_rva = 0x020DFA2D;
		constexpr uint8_t cbuf_range_check_stock[] = {0x48, 0x83, 0xFB, 0x02, 0x73, 0x13};
		constexpr uint32_t cbuf_frame_bound_rva = 0x020ECDB3;
		constexpr uint8_t cbuf_frame_bound_stock[] = {0x83, 0xFE, 0x02, 0x7C, 0xE9};
		constexpr uint32_t cbuf_text_size = 0x10000;
		constexpr size_t cbuf_record_stride = 0x10;
		const char* cbuf34_result = "command buffers 2/3: not attempted";

		bool install_cbuf_for_players34()
		{
			const auto b = base();
			const auto* lea = reinterpret_cast<const uint8_t*>(b + cbuf_exec_lea_rva);
			if (!readable(lea, 7) || std::memcmp(lea, cbuf_exec_lea_head, sizeof(cbuf_exec_lea_head)) != 0)
			{
				cbuf34_result = "command buffers 2/3: NOT installed - Cbuf_ExecuteInternal lea differs";
				return false;
			}
			int32_t disp = 0;
			std::memcpy(&disp, lea + 3, sizeof(disp));
			auto* records = reinterpret_cast<uint8_t*>(b + cbuf_exec_lea_rva + 7 + static_cast<int64_t>(disp));
			if (records == reinterpret_cast<uint8_t*>(b + cbuf_old_records_rva))
			{
				cbuf34_result = "command buffers 2/3: NOT installed - the cbuf records were not relocated";
				return false;
			}
			if (!readable(records, 4 * cbuf_record_stride))
			{
				cbuf34_result = "command buffers 2/3: NOT installed - records unreadable";
				return false;
			}
			for (size_t i = 2 * cbuf_record_stride; i < 4 * cbuf_record_stride; ++i)
			{
				if (records[i] != 0)
				{
					cbuf34_result = "command buffers 2/3: NOT installed - records 2/3 are not empty";
					return false;
				}
			}
			auto* range = reinterpret_cast<uint8_t*>(b + cbuf_range_check_rva);
			auto* bound = reinterpret_cast<uint8_t*>(b + cbuf_frame_bound_rva);
			if (!readable(range, sizeof(cbuf_range_check_stock))
				|| std::memcmp(range, cbuf_range_check_stock, sizeof(cbuf_range_check_stock)) != 0
				|| !readable(bound, sizeof(cbuf_frame_bound_stock))
				|| std::memcmp(bound, cbuf_frame_bound_stock, sizeof(cbuf_frame_bound_stock)) != 0)
			{
				cbuf34_result = "command buffers 2/3: NOT installed - range check or Com_Frame bound bytes differ";
				return false;
			}
			auto* text = static_cast<uint8_t*>(
				VirtualAlloc(nullptr, 2 * cbuf_text_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
			if (!text)
			{
				cbuf34_result = "command buffers 2/3: NOT installed - allocation failed";
				return false;
			}
			for (size_t lc = 2; lc < 4; ++lc)
			{
				auto* rec = records + lc * cbuf_record_stride;
				auto* data = text + (lc - 2) * cbuf_text_size;
				const int32_t maxsize = static_cast<int32_t>(cbuf_text_size);
				const int32_t cursize = 0;
				std::memcpy(rec + 0x0, &data, sizeof(data));
				std::memcpy(rec + 0x8, &maxsize, sizeof(maxsize));
				std::memcpy(rec + 0xC, &cursize, sizeof(cursize));
			}
			const uint8_t four = 0x04, two = 0x02;
			if (!write_bytes(range + 3, &four, 1))
			{
				std::memset(records + 2 * cbuf_record_stride, 0, 2 * cbuf_record_stride);
				cbuf34_result = "command buffers 2/3: NOT installed - range check write failed";
				return false;
			}
			if (!write_bytes(bound + 2, &four, 1))
			{
				write_bytes(range + 3, &two, 1);
				std::memset(records + 2 * cbuf_record_stride, 0, 2 * cbuf_record_stride);
				cbuf34_result = "command buffers 2/3: NOT installed - Com_Frame bound write failed (rolled back)";
				return false;
			}
			cbuf_range_resting = 0x04;
			cbuf34_result = "command buffers 2/3: 64 KB each, Cbuf_Execute range 2 -> 4, Com_Frame Cbuf loop 2 -> 4";
			return true;
		}

		// Lobby join clients [2] -> [4] and LobbyMsgTransport_Update 2 -> 4. With
		// 3-4 players seated, "Failed to host lobby": the party join waits for every
		// member to agree, and controllers 2/3 were never polled (PS4 0xCD2100 loops
		// c < 4). The agreement request handler indexes s_joinClient (PC [2] x
		// 0xB0, PS4 [4] x 0xB8), whose slot 2 is foreign, so it moves first (9 sites
		// + 2 end markers). Slots 2/3 start as copies of slot 1 with state (+0) 0 and
		// controller index (+0xAC) 2/3.
		constexpr uint32_t joinclient_base = 0x156CB4B0;
		constexpr uint32_t joinclient_stride = 0xB0;
		constexpr uint32_t joinclient_old_count = 2;
		constexpr uint32_t joinclient_new_count = 4;
		constexpr uint32_t joinclient_ci_off = 0xAC;
		constexpr entcoll_site joinclient_sites[] = {
			{0x01ED81CF, 3, 7, true , 0x0},     // lea rbx,[base]          reset loop
			{0x01ED8223, 3, 7, true , 0x0},     // lea rcx,[base]          getter (leaf)
			{0x01ED825C, 2, 6, true , 0x0},     // mov [base],eax          init (leaf)
			{0x01ED8262, 3, 7, true , 0xAC},    // mov qword [base+0xAC]   init: slot0 ci, slot1 state
			{0x01ED8252, 2, 10, true, 0x15C},   // mov dword [base+0x15C],1  init: slot1 ci
			{0x01ED8298, 3, 7, true , 0x0},     // lea rax,[base]          agreement request handler
			{0x01ED8438, 3, 7, true , 0x0},     // lea rax,[base]
			{0x01ED86D5, 3, 7, true , 0xA8},    // lea rbx,[base+0xA8]     update loop start
			{0x02E904FF, 3, 7, true , 0x6E},    // lea rbx,[base+0x6E]     static ctor (ran already)
		};
		constexpr entcoll_site joinclient_end_sites[] = {
			{0x01ED81D8, 3, 7, true , 0x160},   // lea rsi,[base+2*0xB0]        reset loop end
			{0x01ED86DE, 3, 7, true , 0x208},   // lea r14,[base+0xA8+2*0xB0]   update loop end
		};
		constexpr uint32_t lobbymsg_bound_rva = 0x01EEC68E;
		constexpr uint8_t lobbymsg_bound_stock[] = {0x83, 0xFB, 0x02, 0x7C, 0xC5};   // cmp ebx,2 / jl
		constexpr uint32_t netchan_get_lea_rva = 0x0211BF51;                          // lea rax,[s_netchan]
		constexpr uint32_t netchan_old_base = 0x16DEAEB0;
		const char* joinclient_result = "lobby join clients: not attempted";
		size_t joinclient_new = 0;

		bool relocate_join_clients()
		{
			if (joinclient_new)
			{
				return true;
			}
			const auto b = base();
			auto* bound = reinterpret_cast<uint8_t*>(b + lobbymsg_bound_rva);
			if (!readable(bound, sizeof(lobbymsg_bound_stock))
				|| std::memcmp(bound, lobbymsg_bound_stock, sizeof(lobbymsg_bound_stock)) != 0)
			{
				joinclient_result = "lobby join clients: NOT moved - LobbyMsgTransport_Update bytes differ";
				return false;
			}
			// The widened loop reads controllers 2/3 through the netchan table, so
			// it must already be the relocated [4] one (reloc_tables "netchan").
			const auto* nlea = reinterpret_cast<const uint8_t*>(b + netchan_get_lea_rva);
			int32_t nd = 0;
			if (!readable(nlea, 7))
			{
				joinclient_result = "lobby join clients: NOT moved - netchan lea unreadable";
				return false;
			}
			std::memcpy(&nd, nlea + 3, sizeof(nd));
			if (netchan_get_lea_rva + 7 + static_cast<int64_t>(nd) == netchan_old_base)
			{
				joinclient_result = "lobby join clients: NOT moved - the netchan table is still [2]";
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(joinclient_new_count * joinclient_stride));
			if (!fresh)
			{
				joinclient_result = "lobby join clients: NOT moved - allocation failed";
				return false;
			}
			const auto* old = reinterpret_cast<const uint8_t*>(b + joinclient_base);
			std::memcpy(fresh, old, joinclient_old_count * joinclient_stride);
			for (uint32_t s = joinclient_old_count; s < joinclient_new_count; ++s)
			{
				auto* slot = fresh + s * joinclient_stride;
				std::memcpy(slot, old + joinclient_stride, joinclient_stride);   // like slot 1
				const int32_t idle = 0, ci = static_cast<int32_t>(s);
				std::memcpy(slot + 0, &idle, sizeof(idle));
				std::memcpy(slot + joinclient_ci_off, &ci, sizeof(ci));
			}
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);
			static int32_t saved[std::size(joinclient_sites)]{};
			static int32_t saved_end[std::size(joinclient_end_sites)]{};
			if (!rewrite_entcoll(joinclient_sites, std::size(joinclient_sites), joinclient_base, fresh_abs, saved))
			{
				joinclient_result = "lobby join clients: NOT moved - a reference did not match";
				return false;
			}
			// end markers: target_off is 2*stride past their start; the new end is 4*stride
			if (!rewrite_entcoll(joinclient_end_sites, std::size(joinclient_end_sites), joinclient_base,
			                     fresh_abs + (joinclient_new_count - joinclient_old_count) * joinclient_stride, saved_end))
			{
				for (size_t j = 0; j < std::size(joinclient_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + joinclient_sites[j].rva);
					write_bytes(insn + joinclient_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				joinclient_result = "lobby join clients: NOT moved - an end marker did not match (rolled back)";
				return false;
			}
			const uint8_t four = 0x04;
			if (!write_bytes(bound + 2, &four, 1))
			{
				joinclient_result = "lobby join clients [2] -> [4] moved, but the message loop widen FAILED";
				joinclient_new = fresh_abs;
				return false;
			}
			joinclient_new = fresh_abs;
			joinclient_result = "lobby join clients [2] -> [4] (9 sites + 2 end markers), LobbyMsgTransport_Update 2 -> 4";
			note("[splitscreen] lobby join clients [2] -> [4] at RVA 0x%08X, lobby message loop 2 -> 4\n",
			     static_cast<uint32_t>(fresh_abs - b));
			return true;
		}

		// Lua Engine.GetClientNum / GetPredictedClientNum returned -1 for controllers
		// 2/3 (no MP HUD in panes 3/4): both start with `cmp ecx,1 / ja -> -1`, while
		// PS4 0xD18000 accepts < 4. Behind the check the PC only indexes cg globals
		// while lc < cl_maxLocalClients, so widening to 3 is safe. Eight other Lua
		// bindings with this check are frontend/online and stay.
		struct ctrl_check_patch
		{
			uint32_t rva;           // the cmp
			uint8_t stock[9];       // cmp ecx,1 ; ja rel32
			const char* what;
		};
		constexpr ctrl_check_patch lua_ctrl_checks[] = {
			{0x01F427FE, {0x83, 0xF9, 0x01, 0x0F, 0x87, 0x12, 0x18, 0x00, 0x00}, "Engine.GetClientNum"},
			{0x01F4FC2E, {0x83, 0xF9, 0x01, 0x0F, 0x87, 0x17, 0x18, 0x00, 0x00}, "Engine.GetPredictedClientNum"},
		};
		const char* lua_ctrl_result = "lua controller checks: not attempted";

		bool widen_lua_controller_checks()
		{
			const auto b = base();
			for (const auto& p : lua_ctrl_checks)
			{
				const auto* site = reinterpret_cast<const uint8_t*>(b + p.rva);
				if (!readable(site, sizeof(p.stock)) || std::memcmp(site, p.stock, sizeof(p.stock)) != 0)
				{
					lua_ctrl_result = "lua controller checks: NOT widened - bytes differ";
					return false;
				}
			}
			uint32_t done = 0;
			for (const auto& p : lua_ctrl_checks)
			{
				const uint8_t three = 0x03;   // ja when controller > 3
				if (write_bytes(reinterpret_cast<void*>(b + p.rva + 2), &three, 1))
				{
					++done;
				}
			}
			lua_ctrl_result = done == std::size(lua_ctrl_checks)
				                  ? "lua controller checks 1 -> 3: Engine.GetClientNum, GetPredictedClientNum"
				                  : "lua controller checks: a write FAILED";
			return done == std::size(lua_ctrl_checks);
		}

		// UI model string hunk "UIModelAllocator" 0xC0000 -> 4 MB (PS4 0x80000). With
		// four class lists Hunk_UserAlloc returned NULL and UI_Model_SetString
		// crashed. The hidden UI_Model_Init creates the hunk through the visible
		// Hunk_UserCreateFromBuffer (PS4 0x10D1C90); this detour swaps in a bigger
		// buffer for exactly that call. Nothing else references the static buffer.
		constexpr uint32_t hunk_create_rva = 0x02276DA0;
		constexpr uint8_t hunk_create_prologue[] = {
			0x49, 0x63, 0xC0,                          // movsxd rax,r8d
			0x4C, 0x8D, 0x1D, 0x46, 0xBB, 0x14, 0x01,  // lea r11,[rip+0x0114B486] (scheme table)
		};
		constexpr size_t model_string_stock_size = 0xC0000;
		constexpr size_t model_string_new_size = 0x400000;
		utils::hook::detour hunk_create_hook;
		void* model_string_buffer = nullptr;
		const char* model_string_result = "ui model string hunk: not installed";

		// "ClientCache_ClientPool" hunk 0x3880 -> 0x7100. Each player centity takes
		// two blocks per local client from it; with four players it ran full and the
		// ET_PLAYER handler did memset(NULL). The PS4 pool is sized for 4 local
		// clients, the PC one for two, so it doubles.
		constexpr size_t client_cache_stock_size = 0x3880;
		constexpr size_t client_cache_new_size = 0x7100;
		void* client_cache_buffer = nullptr;
		const char* client_cache_result = "client cache pool: not seen";

		void* hunk_create_stub(void* buffer, size_t size, int scheme, int flags, void* arg5,
		                       const char* name, int arg7)
		{
			if (name && size == client_cache_stock_size && std::strcmp(name, "ClientCache_ClientPool") == 0)
			{
				// One buffer per process: a re-created pool reuses it, like the
				// stock static buffer.
				if (!client_cache_buffer)
				{
					client_cache_buffer = VirtualAlloc(nullptr, client_cache_new_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				}
				if (client_cache_buffer)
				{
					buffer = client_cache_buffer;
					size = client_cache_new_size;
					client_cache_result = "client cache pool 0x3880 -> 0x7100 (ClientCache_ClientPool)";
					note("[splitscreen] client cache pool 0x3880 -> 0x7100 (ClientCache_ClientPool)\n");
				}
				else
				{
					client_cache_result = "client cache pool: allocation failed - stock 0x3880 kept";
				}
			}
			if (!model_string_buffer && name && size == model_string_stock_size
				&& std::strcmp(name, "UIModelAllocator") == 0)
			{
				auto* bigger = VirtualAlloc(nullptr, model_string_new_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				if (bigger)
				{
					model_string_buffer = bigger;
					buffer = bigger;
					size = model_string_new_size;
					model_string_result = "ui model string hunk 0xC0000 -> 0x400000 (UIModelAllocator)";
				}
				else
				{
					model_string_result = "ui model string hunk: allocation failed - stock 0xC0000 kept";
				}
			}
			return hunk_create_hook.invoke<void*>(buffer, size, scheme, flags, arg5, name, arg7);
		}

		bool install_model_string_hunk()
		{
			const auto* p = reinterpret_cast<const uint8_t*>(base() + hunk_create_rva);
			if (!readable(p, sizeof(hunk_create_prologue))
				|| std::memcmp(p, hunk_create_prologue, sizeof(hunk_create_prologue)) != 0)
			{
				model_string_result = "ui model string hunk: NOT hooked - Hunk_UserCreateFromBuffer bytes differ";
				return false;
			}
			hunk_create_hook.create(reinterpret_cast<void*>(base() + hunk_create_rva), hunk_create_stub);
			model_string_result = "ui model string hunk: hooked, waiting for UI_Model_Init";
			return true;
		}

		constexpr uint32_t model_pool_bound_rva = 0x0200D40E;
		constexpr uint8_t model_pool_bound_stock[] = {0xBE, 0x00, 0x90, 0x00, 0x00};   // mov esi,0x9000
		constexpr uint8_t model_pool_bound_new[] = {0xBE, 0xFF, 0xFF, 0x00, 0x00};     // mov esi,0xFFFF
		const char* model_pool_result = "ui model pool: not attempted";
		size_t model_pool_new = 0;

		bool relocate_ui_model_pool()
		{
			if (model_pool_new)
			{
				return true;
			}
			const auto b = base();
			auto* bound = reinterpret_cast<uint8_t*>(b + model_pool_bound_rva);
			if (!readable(bound, sizeof(model_pool_bound_stock))
				|| std::memcmp(bound, model_pool_bound_stock, sizeof(model_pool_bound_stock)) != 0)
			{
				model_pool_result = "ui model pool: NOT moved - reset bound bytes differ at 0x0200DACE";
				return false;
			}
			// The old array must still be all zero: if UI_Model_Init had already
			// run, live nodes would be left behind.
			const auto* old_nodes = reinterpret_cast<const uint8_t*>(b + model_pool_base);
			if (!readable(old_nodes, 16 * model_pool_stride))
			{
				model_pool_result = "ui model pool: NOT moved - old array unreadable";
				return false;
			}
			for (size_t i = 0; i < 16 * model_pool_stride; ++i)
			{
				if (old_nodes[i] != 0)
				{
					model_pool_result = "ui model pool: NOT moved - old array already initialised";
					return false;
				}
			}
			auto* fresh = static_cast<uint8_t*>(
				allocate_near_module(static_cast<size_t>(model_pool_new_count) * model_pool_stride));
			if (!fresh)
			{
				model_pool_result = "ui model pool: NOT moved - allocation failed";
				return false;
			}
			std::memset(fresh, 0, static_cast<size_t>(model_pool_new_count) * model_pool_stride);
			for (uint32_t i = 0; i < model_pool_new_count; ++i)
			{
				auto* node = fresh + static_cast<size_t>(i) * model_pool_stride;
				uint32_t next = i + 1;
				if (next == model_pool_sentinel)
				{
					next = model_pool_sentinel + 1;
				}
				if (next >= model_pool_new_count || i == 0 || i == model_pool_sentinel)
				{
					next = 0;   // end of list; node 0 is the null handle, 0x9000 the end sentinel
				}
				const auto self = static_cast<uint16_t>(i);
				const auto link = static_cast<uint16_t>(next);
				std::memcpy(node + model_pool_self_off, &self, sizeof(self));
				std::memcpy(node + model_pool_next_off, &link, sizeof(link));
			}
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			static int32_t saved[std::size(model_pool_sites)]{};
			if (!rewrite_entcoll(model_pool_sites, std::size(model_pool_sites), model_pool_base, fresh_abs, saved))
			{
				model_pool_result = "ui model pool: NOT moved - a reference did not match";
				return false;
			}
			if (!write_bytes(bound, model_pool_bound_new, sizeof(model_pool_bound_new)))
			{
				for (size_t j = 0; j < std::size(model_pool_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + model_pool_sites[j].rva);
					write_bytes(insn + model_pool_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				model_pool_result = "ui model pool: NOT moved - reset bound write failed (rolled back)";
				return false;
			}
			model_pool_new = fresh_abs;
			model_pool_result = "ui model pool 0x9000 -> 0xFFFF nodes (28 sites, reset bound, 0x9000 kept as end sentinel)";
			install_model_string_hunk();
			note("[splitscreen] ui model pool 0x9000 -> 0xFFFF nodes at RVA 0x%08X\n",
			     static_cast<uint32_t>(fresh_abs - b));
			return true;
		}

		// Per-view sun-shadow (SST) ring 4 -> 8 entries. Each view takes a record
		// (0x21F0 bytes of GPU buffers) from a 4-entry ring; with four views a record
		// is reused every frame while the GPU may still draw from it (pane 4 shadow
		// flicker). 8 = 4 views x 2 frames, the stock margin. Patched: base leas,
		// mask 3 -> 7, free count 4 -> 8, static constructor count 3 -> 7. The
		// constructors only write zeros, so a zeroed block is constructed. Runs at
		// post_unpack, before the renderer creates the buffers.
		constexpr uint32_t sst_base = 0x10B21260;
		constexpr uint32_t sst_stride = 0x21F0;
		constexpr uint32_t sst_old_count = 4;
		constexpr uint32_t sst_new_count = 8;
		constexpr entcoll_site sst_sites[] = {
			{0x01D0E059, 3, 7, true, 0},            // lea rdx,[base]        alloc
			{0x01D0D598, 3, 7, true, sst_stride},   // lea rbp,[base+0x21F0] buffer creation
			{0x01D0DF0A, 3, 7, true, sst_stride},   // lea rbp,[base+0x21F0] free loop
			{0x02E8A83A, 3, 7, true, 0},            // lea rbx,[base]        static constructor
		};
		constexpr entcoll_site sst_end_site[] = {
			{0x01D0D5A6, 3, 7, true, sst_old_count * sst_stride},   // lea r14,[end] creation loop end
		};
		struct sst_imm { uint32_t rva; uint8_t off; uint8_t size; uint32_t was; uint32_t want; };
		constexpr sst_imm sst_imms[] = {
			{0x01D0E056, 2, 1, 3, sst_new_count - 1},   // and eax,3 -> 7
			{0x01D0DF11, 2, 4, 4, sst_new_count},       // mov r14d,4 -> 8
			{0x02E8A841, 1, 4, 3, sst_new_count - 1},   // mov edi,3 (dec/jns) -> 7
		};
		const char* sst_result = "sun-shadow ring: not attempted";
		size_t sst_new = 0;

		bool relocate_sst_ring()
		{
			if (sst_new)
			{
				return true;
			}
			const auto b = base();
			for (const auto& s : sst_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				uint32_t cur = 0;
				if (!readable(at, s.size))
				{
					sst_result = "sun-shadow ring: NOT moved - immediate unreadable";
					return false;
				}
				std::memcpy(&cur, at, s.size);
				if (cur != s.was)
				{
					sst_result = "sun-shadow ring: NOT moved - an immediate differs";
					return false;
				}
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(sst_new_count * sst_stride));
			if (!fresh)
			{
				sst_result = "sun-shadow ring: NOT moved - allocation failed";
				return false;
			}
			std::memset(fresh, 0, sst_new_count * sst_stride);
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);

			static int32_t saved[std::size(sst_sites)]{};
			static int32_t saved_end[std::size(sst_end_site)]{};
			if (!rewrite_entcoll(sst_sites, std::size(sst_sites), sst_base, fresh_abs, saved))
			{
				sst_result = "sun-shadow ring: NOT moved - a base lea did not match";
				return false;
			}
			const auto undo_sites = [&]
			{
				for (size_t j = 0; j < std::size(sst_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + sst_sites[j].rva);
					write_bytes(insn + sst_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
			};
			// target = new_abs + target_off(4*stride): passing new + 4*stride lands on new + 8*stride.
			if (!rewrite_entcoll(sst_end_site, std::size(sst_end_site), sst_base,
			                     fresh_abs + (sst_new_count - sst_old_count) * sst_stride, saved_end))
			{
				undo_sites();
				sst_result = "sun-shadow ring: NOT moved - the end lea did not match";
				return false;
			}
			size_t done = 0;
			for (const auto& s : sst_imms)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva + s.off);
				if (!write_bytes(at, &s.want, s.size))
				{
					for (size_t j = 0; j < done; ++j)
					{
						write_bytes(reinterpret_cast<uint8_t*>(b + sst_imms[j].rva + sst_imms[j].off),
						            &sst_imms[j].was, sst_imms[j].size);
					}
					auto* insn = reinterpret_cast<uint8_t*>(b + sst_end_site[0].rva);
					write_bytes(insn + sst_end_site[0].disp_off, &saved_end[0], sizeof(int32_t));
					undo_sites();
					sst_result = "sun-shadow ring: NOT moved - an immediate write failed (rolled back)";
					return false;
				}
				++done;
			}
			sst_new = fresh_abs;
			sst_result = "sun-shadow ring [4] -> [8] (per-view records, 5 leas + 3 immediates)";
			return true;
		}

		// cl_voiceCommunication (no code here). The engine writes client 2's
		// clientUIActives record ([2] x 0x1078) past the array, into
		// cl_voiceCommunication, which voice code also writes. Moving clientUIActives
		// is a closed dead end (History: LOG.md, clientUIActives), so reloc_tables'
		// "voice_comm" entry moves the neighbour to a [4] block at startup (12 refs;
		// PS4 CL_GetLocalClientVoiceCommunication 0x1DA6C40). The other 12 refs in
		// that span are clientUIActives' loop end markers, owned by
		// widen_client_ui_walker_bounds(). Slot 3 is not handled here.

		// DWARF-map batch 1: PS4 LOCAL_CLIENT_COUNT globals on the cgame path, found
		// on the PC by stride, sites from tools/gen_reloc_sites.py (data/reloc_sites/).
		// Slots 2..3 are foreign, so each is relocated. Gated on BO3_CG_FRAME.
		// cgDC - the per-client display context (CG_Init memsets cgDC[lc])
		constexpr uint32_t cgdc_base = 0x049B2CD0;
		constexpr uint32_t cgdc_stride = 0x1838;
		constexpr entcoll_site cgdc_sites[] = {
			{0x008F0ABC, 3, 7, false, 0x0000}, // lea rbx, [rbx + 0x4a31cd0]
			{0x010AAC43, 5, 9, false, 0x002C}, // movss xmm0, dword ptr [rax + rcx + 0x4a31cfc]
		};
		bool cgdc_relocated = false;

		bool relocate_cgdc()
		{
			if (cgdc_relocated) { return true; }
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * cgdc_stride));
			if (!fresh) { return false; }
			std::memset(fresh, 0, 4 * cgdc_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + cgdc_base), 2 * cgdc_stride);
			static int32_t saved[std::size(cgdc_sites)]{};
			if (!rewrite_entcoll(cgdc_sites, std::size(cgdc_sites), cgdc_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			cgdc_relocated = true;
			note("[splitscreen] cgdc [2]->[4] at RVA 0x%08X (%zu sites)\n",
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), std::size(cgdc_sites));
			return true;
		}

		// Not relocated: a stride-0x1660 array of 18 per-client elements and a
		// stride-0x188 pool of handle-indexed entries. A stride match is not enough,
		// the index must be lc; gen_reloc_sites.py v2 refuses arrays a vector
		// constructor sizes other than [2].

		// playerKeys: per-client key/binding state (PS4 PlayerKeyState[4], 0x1810;
		// PC [2] x 0x1940). Client 2's state was foreign memory, so the key-event
		// walker called stricmp on a dangling binding pointer. 82 sites (38 RIP,
		// 44 ABS32) from tools/gen_reloc_sites.py v2.1, incl. leaf accessors and
		// ABS32 stores with an immediate (missing those split the state in two).
		// Until CL_ClearKeys(lc) runs, client 2's binding pointers are NULL, which
		// stricmp's null guards accept.
		constexpr uint32_t playerkeys_base = 0x0531D850;
		constexpr uint32_t playerkeys_stride = 0x1940;
		constexpr entcoll_site playerkeys_sites[] = {
			{0x012F2B9B, 3, 7, false, 0x1938}, // mov esi, dword ptr [rax + rsi + 0x539d988]
			{0x0133A477, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x4061bf2]
			{0x0133DE94, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405e1d5]
			{0x0133F27A, 3, 7, false, 0x0138}, // lea r15, [r8 + 0x539c188]
			{0x0133F2C6, 4, 8, false, 0x0130}, // inc dword ptr [r8 + rbp + 0x539c180]
			{0x0133F2D5, 4, 8, false, 0x0130}, // dec dword ptr [r8 + rbp + 0x539c180]
			{0x0133F2DD, 4, 8, false, 0x0130}, // mov eax, dword ptr [r8 + rbp + 0x539c180]
			{0x0133F2E8, 4, 8, false, 0x0130}, // mov dword ptr [r8 + rbp + 0x539c180], eax
			{0x0133F668, 3, 7, false, 0x0138}, // lea r12, [r8 + 0x539c188]
			{0x0133F6C1, 4, 8, false, 0x0130}, // inc dword ptr [rbx + r8 + 0x539c180]
			{0x0133F6D0, 4, 8, false, 0x0130}, // dec dword ptr [rbx + r8 + 0x539c180]
			{0x0133F6D8, 4, 8, false, 0x0130}, // mov eax, dword ptr [rbx + r8 + 0x539c180]
			{0x0133F6E3, 4, 8, false, 0x0130}, // mov dword ptr [rbx + r8 + 0x539c180], eax
			{0x0133FEC2, 4, 8, false, 0x0138}, // lea r12, [r12 + 0x539c188]
			{0x0133FF22, 3, 7, false, 0x0130}, // inc dword ptr [rdi + rax + 0x539c180]
			{0x013405A4, 3, 7, true , 0x1938}, // lea rax, [rip + 0x405d3fd]
			{0x01340E1F, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405b24a]
			{0x01341D93, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x405a40e]
			{0x01341E42, 3, 7, true , 0x0000}, // lea r12, [rip + 0x405a227]
			{0x01341ED9, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a190]
			{0x01341F93, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a0d6]
			{0x01342029, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a040]
			{0x01342261, 3, 7, false, 0x0138}, // lea r13, [rdx + 0x539c188]
			{0x013422C6, 3, 7, false, 0x0130}, // inc dword ptr [rsi + rdx + 0x539c180]
			{0x013422D4, 3, 7, false, 0x0130}, // dec dword ptr [rsi + rdx + 0x539c180]
			{0x013422DB, 3, 7, false, 0x0130}, // mov eax, dword ptr [rsi + rdx + 0x539c180]
			{0x013422E6, 3, 7, false, 0x0130}, // mov dword ptr [rsi + rdx + 0x539c180], eax
			{0x01343E68, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], eax
			{0x01343E7E, 4, 12, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], 3
			{0x01343E9D, 4, 9, false, 0x1938}, // cmp dword ptr [rsi + r9 + 0x539d988], 2
			{0x01343EB8, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], r8d
			{0x01343EC2, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], r12d
			{0x0134499C, 4, 8, false, 0x1038}, // mov ebp, dword ptr [rax + r15 + 0x539d088]
			{0x013449A4, 4, 8, false, 0x1020}, // mov edi, dword ptr [rax + r15 + 0x539d070]
			{0x013449AC, 4, 8, false, 0x1008}, // mov r14d, dword ptr [rax + r15 + 0x539d058]
			{0x01345375, 3, 7, true , 0x012C}, // lea rax, [rip + 0x4056e20]
			{0x01345650, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056a19]
			{0x013456F0, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056979]
			{0x013457EF, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405687a]
			{0x013459E0, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056689]
			{0x01345DC2, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x40562a7]
			{0x01345F97, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x40560d2]
			{0x013462E1, 4, 8, false, 0x0140}, // cmp r14d, dword ptr [rax + r11 + 0x539c190]
			{0x0134638D, 4, 8, false, 0x0144}, // cmp r11d, dword ptr [rax + r14 + 0x539c194]
			{0x013463D8, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r15d
			{0x013463E0, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x0134640C, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r15d
			{0x01346414, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x01346468, 3, 7, false, 0x0148}, // lea rcx, [rax + 0x539c198]
			{0x0134648B, 4, 8, false, 0x0140}, // mov dword ptr [rsi + rax + 0x539c190], r14d
			{0x0134649A, 4, 12, false, 0x193C}, // mov dword ptr [rdi + r14 + 0x539d98c], 1
			{0x013464F5, 4, 8, false, 0x0140}, // cmp r10d, dword ptr [rcx + r14 + 0x539c190]
			{0x01346537, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r11d
			{0x0134653F, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x01346566, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r11d
			{0x0134656E, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
			{0x0134674A, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x4055a57]
			{0x01346917, 3, 7, false, 0x0148}, // lea rcx, [rsi + 0x539c198]
			{0x01346928, 4, 8, false, 0x0140}, // mov qword ptr [rbx + rsi + 0x539c190], rax
			{0x01346930, 3, 11, false, 0x193C}, // mov dword ptr [rdi + rsi + 0x539d98c], 1
			{0x01346B24, 3, 7, true , 0x0144}, // lea rax, [rip + 0x4055689]
			{0x01346BD2, 3, 7, true , 0x0138}, // lea rax, [rip + 0x40555cf]
			{0x01346C62, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4055407]
			{0x01346D0B, 3, 7, true , 0x0140}, // lea rcx, [rip + 0x405549e]
			{0x01346E37, 3, 7, true , 0x0138}, // lea rax, [rip + 0x405536a]
			{0x01346EE2, 3, 7, true , 0x0140}, // lea r13, [rip + 0x40552c7]
			{0x0134700D, 3, 7, true , 0x0140}, // lea rax, [rip + 0x405519c]
			{0x013470A0, 3, 7, true , 0x0138}, // lea rax, [rip + 0x4055101]
			{0x0134724D, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x4054f54]
			{0x013475E6, 3, 7, true , 0x193C}, // lea rdi, [rip + 0x40563bf]
			{0x013478B4, 3, 7, true , 0x0000}, // lea r15, [rip + 0x40547b5]
			{0x01347993, 3, 7, true , 0x193C}, // lea rcx, [rip + 0x4056012]
			{0x013479FA, 3, 7, true , 0x0148}, // lea rsi, [rip + 0x40547b7]
			{0x01347C68, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4054401]
			{0x01347D9C, 3, 7, true , 0x0000}, // lea r10, [rip + 0x40542cd]
			{0x01347E2F, 3, 7, true , 0x0000}, // lea r12, [rip + 0x405423a]
			{0x01347E3B, 3, 7, true , 0x0148}, // lea rax, [rip + 0x4054376]
			{0x01347E77, 3, 7, true , 0x0148}, // lea rax, [rip + 0x405433a]
			{0x01347F7F, 3, 7, true , 0x0138}, // lea rax, [rip + 0x4054222]
			{0x013481CE, 3, 7, true , 0x0000}, // lea r8, [rip + 0x4053e9b]
			{0x01DDE2EA, 3, 7, true , 0x0000}, // lea rax, [rip + 0x35b12cf]
			// Not a site: 0x0219DA4D is unreachable Arxan filler whose bytes happen
			// to equal playerKeys+0x2994. Never write unproven bytes.
		};
		bool playerkeys_relocated = false;

		bool relocate_playerkeys()
		{
			if (playerkeys_relocated) { return true; }
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * playerkeys_stride));
			if (!fresh) { return false; }
			std::memset(fresh, 0, 4 * playerkeys_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + playerkeys_base), 2 * playerkeys_stride);
			static int32_t saved[std::size(playerkeys_sites)]{};
			if (!rewrite_entcoll(playerkeys_sites, std::size(playerkeys_sites), playerkeys_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			// The binding-clear loop ends on a pointer, &playerKeys[2]+0x148; without
			// this it clears player 1's bindings only. New end: &new[4]+0x148.
			if (!retarget_end_marker(0x01347A03, 3, 7, playerkeys_base + 2 * playerkeys_stride + 0x148,
			                         reinterpret_cast<size_t>(fresh) + 4 * playerkeys_stride + 0x148))
			{
				for (size_t j = 0; j < std::size(playerkeys_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + playerkeys_sites[j].rva);
					write_bytes(insn + playerkeys_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			playerkeys_relocated = true;
			note("[splitscreen] playerKeys [2]->[4] at RVA 0x%08X (%zu sites)\n",
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), std::size(playerkeys_sites));
			return true;
		}

		// g_notetrackLerps: PS4 [LOCAL_CLIENT_COUNT][16] x 0x34 (CG_InitNotetrackLerps
		// 0x13C6A0), [2] on PC. For lc 2, CG_UpdateNotetrackLerps read an entity number
		// from foreign memory and wrote through a wild pointer. 34 sites (5 RIP,
		// 29 ABS32), data/reloc_sites/sites_notetracklerps.txt.
		// CG_InitNotetrackLerps(2) initializes the new row.
		constexpr uint32_t notetracklerps_base = 0x0474B130;
		constexpr uint32_t notetracklerps_stride = 0x340;
		constexpr entcoll_site notetracklerps_sites[] = {
			{0x00248E3F, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x45812ea]
			{0x00249355, 3, 7, true , 0x0034}, // lea rcx, [rip + 0x4580e08]
			{0x0024E039, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x457c0f0]
			{0x0026EE8A, 4, 9, false, 0x0000}, // cmp dword ptr [rdi + r10 + 0x47ca130], 3
			{0x0026EE99, 4, 8, false, 0x002C}, // movsxd r8, dword ptr [rdi + r10 + 0x47ca15c]
			{0x002735C3, 4, 8, false, 0x0000}, // mov eax, dword ptr [rdi + r10 + 0x47ca130]
			{0x002735CF, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
			{0x002735D7, 3, 7, true , 0x0014}, // lea rax, [rip + 0x4556b66]
			{0x00273605, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027361B, 4, 8, false, 0x0014}, // mov eax, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027366C, 4, 12, false, 0x0000}, // mov dword ptr [rdi + r10 + 0x47ca130], 3
			{0x00275949, 4, 9, false, 0x0000}, // cmp dword ptr [rdi + r10 + 0x47ca130], 3
			{0x0027595F, 4, 8, false, 0x002C}, // movsxd r8, dword ptr [rdi + r10 + 0x47ca15c]
			{0x0027A063, 4, 8, false, 0x0024}, // mov edx, dword ptr [rdi + r10 + 0x47ca154]
			{0x0027A06B, 4, 8, false, 0x0028}, // mov r8d, dword ptr [rdi + r10 + 0x47ca158]
			{0x0027A07F, 4, 8, false, 0x0000}, // mov eax, dword ptr [rdi + r10 + 0x47ca130]
			{0x0027A0AA, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
			{0x0027A0B4, 6, 10, false, 0x0008}, // movss xmm2, dword ptr [rdi + r10 + 0x47ca138]
			{0x0027A0BE, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A0C8, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
			{0x0027A0E7, 6, 10, false, 0x0018}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca148]
			{0x0027A0FA, 6, 10, false, 0x000C}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca13c]
			{0x0027A10C, 6, 10, false, 0x001C}, // movss xmm2, dword ptr [rdi + r10 + 0x47ca14c]
			{0x0027A11F, 6, 10, false, 0x0010}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca140]
			{0x0027A131, 6, 10, false, 0x0020}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca150]
			{0x0027A167, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
			{0x0027A171, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A1A8, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
			{0x0027A1B2, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A1D9, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
			{0x0027A1F5, 3, 7, true , 0x0014}, // lea rax, [rip + 0x454ff48]
			{0x0027A216, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A22C, 4, 8, false, 0x0014}, // mov eax, dword ptr [rdi + r10 + 0x47ca144]
			{0x0027A273, 4, 12, false, 0x0000}, // mov dword ptr [rdi + r10 + 0x47ca130], 3
		};
		bool notetracklerps_relocated = false;

		bool relocate_notetracklerps()
		{
			if (notetracklerps_relocated) { return true; }
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * notetracklerps_stride));
			if (!fresh) { return false; }
			std::memset(fresh, 0, 4 * notetracklerps_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + notetracklerps_base), 2 * notetracklerps_stride);
			static int32_t saved[std::size(notetracklerps_sites)]{};
			if (!rewrite_entcoll(notetracklerps_sites, std::size(notetracklerps_sites),
			                     notetracklerps_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			notetracklerps_relocated = true;
			note("[splitscreen] g_notetrackLerps [2]->[4] at RVA 0x%08X (%zu sites)\n",
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), std::size(notetracklerps_sites));
			return true;
		}

		// DWARF-map batch 1b: PS4 [4] globals the cgame frame touches, matched to the
		// PC by stride and confirmed by the index register (lc, or the cg_t index).
		// Sites from tools/gen_reloc_sites.py v2.2 (data/reloc_sites/sites_<name>.txt),
		// each verified against the old address before anything is written.
		struct perclient_array
		{
			const char* name;
			uint32_t base;              // old RVA of slot 0
			uint32_t stride;            // bytes per local client
			const entcoll_site* sites;
			size_t count;
			uint32_t ctor_rva;          // 0: zero-fill IS the initial state
			uint8_t ctor_sig[8];        // the constructor's first bytes, verified
		};

		// cg_pmove - pmove_t[LOCAL_CLIENT_COUNT] (PS4 0x0451EFE0, 0x1660), used every
		// frame by CG_PredictPlayerState_Internal. The element constructor stores a
		// vtable at +0x2C0 (a zeroed slot would call through NULL), so it runs on 2/3.
		constexpr entcoll_site cg_pmove_sites[] = {
			{0x00926DC4, 2, 6, true , 0x0340}, // mov dword ptr [rip + 0x43f1cb6], esi
			{0x00926DCA, 3, 7, true , 0x0344}, // mov byte ptr [rip + 0x43f1cb3], sil
			{0x00926DD1, 2, 10, true , 0x02D0}, // mov dword ptr [rip + 0x43f1c35], 0x7e967699
			{0x00926DDB, 2, 10, true , 0x02D4}, // mov dword ptr [rip + 0x43f1c2f], 0x7e967699
			{0x00926DE5, 2, 10, true , 0x02D8}, // mov dword ptr [rip + 0x43f1c29], 0x7e967699
			{0x00926DEF, 2, 10, true , 0x02E0}, // mov dword ptr [rip + 0x43f1c27], 0xfe967699
			{0x00926DF9, 2, 10, true , 0x02E4}, // mov dword ptr [rip + 0x43f1c21], 0xfe967699
			{0x00926E03, 2, 10, true , 0x02E8}, // mov dword ptr [rip + 0x43f1c1b], 0xfe967699
			{0x00926E0D, 2, 6, true , 0x19A0}, // mov dword ptr [rip + 0x43f32cd], esi
			{0x00926E14, 2, 6, true , 0x19A4}, // mov byte ptr [rip + 0x43f32ca], dh
			{0x00926E1A, 2, 10, true , 0x1930}, // mov dword ptr [rip + 0x43f324c], 0x7e967699
			{0x00926E24, 2, 10, true , 0x1934}, // mov dword ptr [rip + 0x43f3246], 0x7e967699
			{0x00926E2E, 2, 10, true , 0x1938}, // mov dword ptr [rip + 0x43f3240], 0x7e967699
			{0x00926E38, 2, 10, true , 0x1940}, // mov dword ptr [rip + 0x43f323e], 0xfe967699
			{0x00926E42, 2, 10, true , 0x1944}, // mov dword ptr [rip + 0x43f3238], 0xfe967699
			{0x00926E4C, 2, 10, true , 0x1948}, // mov dword ptr [rip + 0x43f3232], 0xfe967699
			{0x009D18A0, 4, 8, false, 0x02B0}, // mov dword ptr [rsi + r15 + 0x4d189f0], ebx
			{0x009D18A8, 3, 7, false, 0x00A8}, // lea rbx, [r15 + 0x4d187e8]
			{0x009D18AF, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi + 0x4d18740], r13
			{0x009D18BA, 4, 9, false, 0x02AC}, // mov byte ptr [rsi + r15 + 0x4d189ec], 0
			{0x009D1915, 4, 12, false, 0x0294}, // mov dword ptr [rsi + r15 + 0x4d189d4], 0
			{0x009D1927, 4, 8, false, 0x0290}, // mov dword ptr [rsi + r15 + 0x4d189d0], eax
			{0x009D1C16, 3, 7, false, 0x0008}, // lea rbx, [r15 + 0x4d18748]
			{0x009D1C6D, 3, 7, false, 0x0000}, // lea rcx, [r15 + 0x4d18740]
			{0x009D1CD0, 3, 7, false, 0x0008}, // lea r13, [rax + 0x4d18748]
			{0x009D1CF4, 3, 7, false, 0x0008}, // mov eax, dword ptr [rsi + rcx + 0x4d18748]
			{0x009D1D10, 3, 7, false, 0x0058}, // lea r8, [rcx + 0x4d18798]
			{0x009D1E56, 4, 8, false, 0x0000}, // mov rax, qword ptr [rsi + rcx + 0x4d18740]
			{0x009D1E5E, 3, 7, false, 0x0000}, // lea rcx, [rcx + 0x4d18740]
			{0x009D1E8D, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rax + 0x4d18740]
			{0x009D1F36, 4, 8, false, 0x0000}, // mov rax, qword ptr [rsi + rax + 0x4d18740]
			{0x009D1F7A, 3, 7, false, 0x0000}, // lea rcx, [r14 + 0x4d18740]
			{0x009D1FE1, 6, 10, false, 0x0294}, // movss xmm8, dword ptr [rsi + r14 + 0x4d189d4]
			{0x009D2000, 4, 8, false, 0x0290}, // cmp dword ptr [rsi + r14 + 0x4d189d0], eax
			{0x009D2086, 4, 8, false, 0x0290}, // mov eax, dword ptr [rsi + r14 + 0x4d189d0]
			{0x009D2106, 4, 8, false, 0x0290}, // mov eax, dword ptr [rsi + r14 + 0x4d189d0]
			{0x010BBA0C, 4, 9, false, 0x1618}, // cmp dword ptr [r14 + rax + 0x4d19d58], 0
			{0x010BBA17, 3, 7, false, 0x161C}, // lea rbx, [rax + 0x4d19d5c]
			{0x010BBA53, 4, 8, false, 0x1618}, // cmp edi, dword ptr [r14 + r15 + 0x4d19d58]
			{0x010C153D, 4, 9, false, 0x1618}, // cmp dword ptr [r12 + rax + 0x4d19d58], 0
			{0x010C1548, 3, 7, false, 0x161C}, // lea rdi, [rax + 0x4d19d5c]
			{0x010C1583, 4, 8, false, 0x1618}, // cmp r14d, dword ptr [r12 + r13 + 0x4d19d58]
			{0x023A0ACE, 3, 7, true , 0x0000}, // mov rdx, qword ptr [rip + 0x28feafb]
			{0x023AAE5A, 4, 9, false, 0x0000}, // cmp qword ptr [rax + r15 + 0x4d18740], 0
			{0x02621DD8, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x267d7f1]
			{0x02CCE142, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x1fd0ff7]
			{0x02EF9657, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x1da0082]
		};

		// s_cameraShakeSet - CameraShakeSet[4] (PS4 0x03F61EF0, 0x104). CG_ClearCameraShakes
		// (0x005830A0) memsets [lc]; CG_ShakeCamera reads it every frame.
		constexpr entcoll_site camerashake_sites[] = {
			{0x005830A3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42608e6]
			{0x0058491C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425f06d]
			{0x0058651E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425d46b]
		};

		// moverInfos - mover_info_t[4] (PS4 0x03F60F50, 0x390), camera-tween mover records.
		// Its constructor is a no-op, so zero is the initial state.
		constexpr entcoll_site moverinfos_sites[] = {
			{0x004CB9D4, 3, 7, true , 0x0000}, // lea rax, [rip + 0x43177d5]
			{0x004F0FDB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42f21ce]
			{0x02CCD75A, 3, 7, true , 0x0000}, // lea rbx, [rip + 0x1a9c44f]
		};

		// moveInfoEntNum - int[4] (PS4 0x03F61D90), read next to moverInfos at 0x004F0FCA.
		// Its slot 2 is used by 3 foreign leas.
		constexpr entcoll_site moveinfoentnum_sites[] = {
			{0x004F0FCA, 3, 7, false, 0x0000}, // lea rsi, [r11 + 0x47e3140]
		};

		// rumbleGlobArray - RumbleGlobals[4] (PS4 0x045269F0, 0x410). GetRumbleGlobals is
		// inlined 8 times; every one indexes with the lc argument.
		constexpr entcoll_site rumble_sites[] = {
			{0x009E6E50, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336619]
			{0x009E6E99, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x43365d0]
			{0x009E6F03, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4336566]
			{0x009E6F98, 3, 7, true , 0x0000}, // lea rax, [rip + 0x43364d1]
			{0x009E7033, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336436]
			{0x009E70AB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x43363be]
			{0x009E7112, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336357]
			{0x009EF6A1, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x432ddc8]
		};

		// atGlobArray - AimTargetGlob[4] (PS4 0x03159380, 0x1604). AimTarget_GetGlobArray
		// (0x000771E0) and the clear (0x0007E100) take lc; used per frame by aim assist.
		constexpr entcoll_site atglob_sites[] = {
			{0x000771E3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36085d6]
			{0x0007AA64, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3604d55]
			{0x0007E103, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36016b6]
			{0x00086724, 4, 8, false, 0x1600}, // mov dword ptr [rax + rdx + 0x3680dc0], r14d
		};

		// g_aimtarget_cmd - AimTarget_Cmd[4] (PS4 0x031592E0, 0x10), indexed lc*16 next to
		// atGlobArray at 0x0008672C. Slot 2 lands on a foreign global.
		constexpr entcoll_site aimtargetcmd_sites[] = {
			{0x0007A991, 3, 7, true , 0x0000}, // lea r12, [rip + 0x3604de8]
			{0x0008672C, 4, 8, false, 0x0004}, // mov dword ptr [rdx + r11*8 + 0x367f784], r14d
			{0x0008978A, 4, 8, false, 0x0004}, // movsxd rdx, dword ptr [rax + r11*8 + 0x367f784]
			{0x000897BA, 4, 8, false, 0x0004}, // mov dword ptr [rdx + r11*8 + 0x367f784], eax
		};

		// gArcData - ARC_DATA[4] (PS4 0x03FF7A10, 0xEEC), grenade arc prediction
		// (CG_ArcPrediction_Update/Render). Indexed by lc next to cg_t (0x342720).
		constexpr entcoll_site arcdata_sites[] = {
			{0x005F11E7, 3, 7, true , 0x0000}, // lea r14, [rip + 0x42270c2]
			{0x005F66A0, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221c09]
			{0x005F6852, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221a57]
			{0x005F6981, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221928]
			{0x005F8562, 3, 7, true , 0x0000}, // lea r9, [rip + 0x421fd47]
			{0x005F86BE, 3, 7, true , 0x0000}, // lea r9, [rip + 0x421fbeb]
			{0x005FBB81, 4, 8, false, 0x0E60}, // mov dword ptr [rsi + r12 + 0x4819110], r15d
			{0x005FBBBD, 4, 8, false, 0x0E60}, // mov dword ptr [rsi + r12 + 0x4819110], r15d
			{0x005FBBC5, 5, 11, false, 0x0EE8}, // mov word ptr [rsi + r12 + 0x4819198], 0x100
			{0x005FF962, 3, 8, false, 0x0EE8}, // mov byte ptr [rsi + rax + 0x4819198], 1
			{0x005FF97B, 3, 8, false, 0x0EE9}, // mov byte ptr [rsi + rax + 0x4819199], 1
			{0x005FF98C, 3, 8, false, 0x0E60}, // cmp dword ptr [rsi + rax + 0x4819110], 1
			{0x005FF996, 3, 8, false, 0x0EE9}, // mov byte ptr [rsi + rax + 0x4819199], 0
		};

		// cg_zbarriers - cgZBarrier_t[4][128] (PS4 0x03F2FE90, row 0xC400 = 128 x 0x188),
		// handed out per lc by CG_InitZBarrier. PC row 2 was foreign memory. The
		// map-start clear is widened in widen_zbarrier_clear().
		constexpr entcoll_site zbarriers_sites[] = {
			{0x00461048, 3, 7, true , 0x0000}, // lea r10, [rip + 0x43698a1]
			{0x004616C4, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4369225]
		};

		constexpr perclient_array batch1b[] = {
			{"cg_pmove", 0x04C99740, 0x1660, cg_pmove_sites, std::size(cg_pmove_sites), 0x009B4720, {0x33, 0xD2, 0x0F, 0x57, 0xC0, 0x48, 0x8D, 0x05}},
			{"camerashake", 0x04764990, 0x104, camerashake_sites, std::size(camerashake_sites), 0x00000000, {}},
			{"moverinfos", 0x047641B0, 0x390, moverinfos_sites, std::size(moverinfos_sites), 0x00000000, {}},
			{"moveinfoentnum", 0x04764140, 0x4, moveinfoentnum_sites, std::size(moveinfoentnum_sites), 0x00000000, {}},
			{"rumble", 0x04C9E470, 0x410, rumble_sites, std::size(rumble_sites), 0x00000000, {}},
			{"atglob", 0x036007C0, 0x1604, atglob_sites, std::size(atglob_sites), 0x00000000, {}},
			{"aimtargetcmd", 0x03600780, 0x10, aimtargetcmd_sites, std::size(aimtargetcmd_sites), 0x00000000, {}},
			{"arcdata", 0x047992B0, 0xEEC, arcdata_sites, std::size(arcdata_sites), 0x00000000, {}},
			{"zbarriers", 0x0474B8F0, 0xC400, zbarriers_sites, std::size(zbarriers_sites), 0x00000000, {}},
		};
		size_t batch1b_new[std::size(batch1b)] = {};

		// Per-local-client [2][18] x 0x132 array: the game session's 18 member slots
		// for each local client. lc 2 ran past it into a static cmd_function_t node.
		// Moved to [4]; its clear (memset 0x2B08) widens to four rows.
		constexpr entcoll_site session_member_sites[] = {
			{0x020E4180, 3, 7, true , 0x0000}, // lea rcx, [arr]           clear
			{0x020E6224, 3, 7, false, 0x0000}, // lea rdx, [rax + arr]     rax = image base
			{0x020E6260, 3, 7, true , 0x0021}, // lea rax, [arr + 0x21]
			{0x020E6271, 3, 7, false, 0x0001}, // lea rsi, [rsi + arr + 1] image-base relative
		};
		constexpr uint32_t session_member_base = 0x1684FAA0;
		constexpr uint32_t session_member_stride = 18 * 0x132;   // 0x1584
		constexpr uint32_t session_member_clear_rva = 0x020E4189;
		size_t session_member_new = 0;

		bool relocate_session_members()
		{
			if (session_member_new)
			{
				return true;
			}
			const auto b = base();
			auto* len = reinterpret_cast<uint8_t*>(b + session_member_clear_rva);
			constexpr uint8_t len_old[] = {0x41, 0xB8, 0x08, 0x2B, 0x00, 0x00}; // mov r8d, 0x2B08
			constexpr uint8_t len_new[] = {0x41, 0xB8, 0x10, 0x56, 0x00, 0x00}; // mov r8d, 0x5610
			if (!readable(len, sizeof(len_old)) || std::memcmp(len, len_old, sizeof(len_old)) != 0)
			{
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * session_member_stride));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, 4 * session_member_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + session_member_base),
			            2 * session_member_stride);
			int32_t saved[std::size(session_member_sites)] = {};
			if (!rewrite_entcoll(session_member_sites, std::size(session_member_sites),
			                     session_member_base, reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			if (!write_bytes(len, len_new, sizeof(len_new)))
			{
				for (size_t j = 0; j < std::size(session_member_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + session_member_sites[j].rva);
					write_bytes(insn + session_member_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			session_member_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		// One [2]->[4] move: new block near the module, live slots 0..1 copied,
		// 2..3 zeroed (and constructed if the array has an element constructor),
		// then every site rewritten, verified first, all or nothing. Returns the
		// new block or 0.
		size_t relocate_perclient(const perclient_array& a)
		{
			const auto b = base();
			if (a.ctor_rva)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + a.ctor_rva);
				if (!readable(at, sizeof(a.ctor_sig)) || std::memcmp(at, a.ctor_sig, sizeof(a.ctor_sig)) != 0)
				{
					note("[splitscreen] %s: constructor bytes differ - nothing moved\n", a.name);
					return 0;
				}
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * a.stride));
			if (!fresh)
			{
				return 0;
			}
			std::memset(fresh, 0, 4 * a.stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + a.base), 2 * a.stride);
			if (a.ctor_rva)
			{
				using ctor_t = void* (*)(void*);
				const auto ctor = reinterpret_cast<ctor_t>(b + a.ctor_rva);
				for (size_t lc = 2; lc < 4; ++lc)
				{
					ctor(fresh + lc * a.stride);
				}
			}
			std::vector<int32_t> saved(a.count);
			if (!rewrite_entcoll(a.sites, a.count, a.base, reinterpret_cast<size_t>(fresh), saved.data()))
			{
				return 0;
			}
			note("[splitscreen] %s [2]->[4] at RVA 0x%08X (%zu sites)\n", a.name,
			     static_cast<uint32_t>(reinterpret_cast<size_t>(fresh) - b), a.count);
			return reinterpret_cast<size_t>(fresh);
		}

		// aaGlobArray ([2] x 0x4E30), complete table: 50 sites (28 rip, 22 abs,
		// data/reloc_sites/sites_aimglob.txt). Moving only the `lea reg,[base]`
		// sites left field accessors on the old array (player 4 crashed aiming).
		// 0x00039BB9 is a site: rcx holds the module base there. Zero-fill is the
		// initial state (AimTarget_Init memsets each slot).
		constexpr entcoll_site aaglob_v2_sites[] = {
			{0x0002D7B6, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3647a13]
			{0x0002DAD3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36476f6]
			{0x0002F70F, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3645aba]
			{0x0002FC45, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3645584]
			{0x0002FFD1, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36451f8]
			{0x00034D16, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36404b3]
			{0x000369C8, 3, 7, true , 0x0000}, // lea rax, [rip + 0x363e801]
			{0x00039BB9, 3, 7, false, 0x0000}, // lea rsi, [rcx + 0x36751d0]
			{0x0003FF57, 3, 7, false, 0x4E18}, // mov ebx, dword ptr [rax + rbx + 0x3679fe8]
			{0x00043773, 3, 7, true , 0x4E20}, // lea rdx, [rip + 0x3636876]
			{0x00043793, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3631a36]
			{0x00043E7D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x363134c]
			{0x000457A3, 3, 7, true , 0x4E20}, // lea rcx, [rip + 0x3634846]
			{0x0004EDEE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36263db]
			{0x000520D3, 3, 7, true , 0x2814}, // lea rcx, [rip + 0x362590a]
			{0x00052104, 3, 7, true , 0x0214}, // lea rcx, [rip + 0x36232d9]
			{0x0005219A, 3, 7, true , 0x0000}, // lea rax, [rip + 0x362302f]
			{0x00056D33, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x361e496]
			{0x0005B969, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3619860]
			{0x0005BA60, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3619769]
			{0x0005D383, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3617e46]
			{0x000604DE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3614ceb]
			{0x000639B3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3611816]
			{0x00065353, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360fe76]
			{0x00066CC7, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360e502]
			{0x00066D50, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360e479]
			{0x0006DC2A, 3, 7, false, 0x00C8}, // lea rsi, [r13 + 0x3675298]
			{0x0006DC47, 6, 10, false, 0x01C0}, // movss dword ptr [rdi + r13 + 0x3675390], xmm6
			{0x0006DC6E, 3, 7, false, 0x00CC}, // lea r12, [r13 + 0x367529c]
			{0x0006DC8E, 6, 10, false, 0x01C4}, // movss dword ptr [rdi + r13 + 0x3675394], xmm6
			{0x0006DC98, 4, 9, false, 0x018D}, // cmp byte ptr [rdi + r13 + 0x367535d], 0
			{0x0006DCA3, 6, 10, false, 0x0194}, // movss xmm6, dword ptr [rdi + r13 + 0x3675364]
			{0x0006DCE4, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DCEE, 6, 10, false, 0x01C8}, // movss dword ptr [rdi + r13 + 0x3675398], xmm6
			{0x0006DD24, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DD2E, 6, 10, false, 0x01CC}, // movss dword ptr [rdi + r13 + 0x367539c], xmm6
			{0x0006DD64, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DD6E, 6, 10, false, 0x01D0}, // movss dword ptr [rdi + r13 + 0x36753a0], xmm6
			{0x0006DDA7, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DDB1, 6, 10, false, 0x01D4}, // movss dword ptr [rdi + r13 + 0x36753a4], xmm6
			{0x0006DDE7, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DDF1, 6, 10, false, 0x01D8}, // movss dword ptr [rdi + r13 + 0x36753a8], xmm6
			{0x0006DE64, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
			{0x0006DE6E, 6, 10, false, 0x01DC}, // movss dword ptr [rdi + r13 + 0x36753ac], xmm6
			{0x0006F5EB, 5, 9, false, 0x01E0}, // movss dword ptr [rax + rdx + 0x36753b0], xmm6
			{0x00070E6D, 5, 9, false, 0x01E4}, // movss dword ptr [rax + rcx + 0x36753b4], xmm6
			{0x00071B48, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3603681]
			{0x00073644, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3601b85]
			{0x000753E0, 3, 7, true , 0x0000}, // lea rax, [rip + 0x35ffde9]
			{0x02C6A7A5, 3, 7, true , 0x01B4}, // lea rax, [rip + 0x9915d8]
		};
		constexpr perclient_array aaglob_array = {
			"aaGlobArray", 0x035F61D0, 0x4E30, aaglob_v2_sites, std::size(aaglob_v2_sites), 0, {}};

		// CG_InitZBarriers (PC 0x004616C0, PS4 0x1697B0): PS4 clears four rows and
		// four counts. Widen the memset 0x18800 -> 0x31000, and turn the 11-byte
		// qword store to numcgZBarriers into xorps/movups/nop, a 16-byte store
		// (its slots 2..3 are padding; xmm0 is volatile). Otherwise player 3's
		// count never resets and passes 128 on the next map.
		bool widen_zbarrier_clear(const size_t zb_new)
		{
			const auto b = base();
			auto* imm = reinterpret_cast<uint8_t*>(b + 0x004616CD);
			auto* clr = reinterpret_cast<uint8_t*>(b + 0x004616D8);
			constexpr uint8_t imm_old[] = {0x41, 0xB8, 0x00, 0x88, 0x01, 0x00};
			constexpr uint8_t clr_old[] = {0x48, 0xC7, 0x05, 0x1D, 0x2A, 0x30, 0x04, 0x00, 0x00, 0x00, 0x00};
			// the memset's rcx must already point at the moved rows
			const auto* lea = reinterpret_cast<const uint8_t*>(b + 0x004616C4);
			int32_t lea_disp = 0;
			std::memcpy(&lea_disp, lea + 3, sizeof(lea_disp));
			if (!zb_new || b + 0x004616CB + lea_disp != zb_new
			    || !readable(imm, sizeof(imm_old)) || std::memcmp(imm, imm_old, sizeof(imm_old)) != 0
			    || !readable(clr, sizeof(clr_old)) || std::memcmp(clr, clr_old, sizeof(clr_old)) != 0)
			{
				note("[splitscreen] zbarrier clear: bytes differ - not widened\n");
				return false;
			}
			const auto disp = static_cast<int32_t>(0x04764100 - (0x004616DB + 7));
			uint8_t clr_new[11] = {0x0F, 0x57, 0xC0, 0x0F, 0x11, 0x05, 0, 0, 0, 0, 0x90};
			std::memcpy(clr_new + 6, &disp, sizeof(disp));
			constexpr uint8_t imm_new[] = {0x41, 0xB8, 0x00, 0x10, 0x03, 0x00};
			if (!write_bytes(clr, clr_new, sizeof(clr_new)))
			{
				return false;
			}
			if (!write_bytes(imm, imm_new, sizeof(imm_new)))
			{
				write_bytes(clr, clr_old, sizeof(clr_old));
				return false;
			}
			return true;
		}

		void relocate_batch1b()
		{
			for (size_t i = 0; i < std::size(batch1b); ++i)
			{
				if (!batch1b_new[i])
				{
					batch1b_new[i] = relocate_perclient(batch1b[i]);
				}
				if (batch1b_new[i] && std::strcmp(batch1b[i].name, "zbarriers") == 0)
				{
					widen_zbarrier_clear(batch1b_new[i]);
				}
			}
		}

		// DWARF-map batch 2.
		// totalCoverageArea_s - totalCoverageArea_t[4][18] (PS4 0x04E73820, row 0x360),
		// CG_TotalCoverage_Frame(lc). Slot 2 overlaps foreign globals.
		constexpr entcoll_site totalcoverage_sites[] = {
			{0x0125F881, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3ae2bb8]
			{0x01262A2B, 3, 7, true , 0x0008}, // lea rax, [rip + 0x3adfa16]
			{0x01262B66, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x3adf8d3]
			{0x01262B71, 3, 7, true , 0x0018}, // lea rax, [rip + 0x3adf8e0]
			{0x012633A3, 3, 7, true , 0x0018}, // lea rcx, [rip + ..]  (missed by both scans)
			{0x01264CA6, 3, 7, true , 0x0004}, // lea r10, [rip + 0x3add797]
		};
		// gaGlobs - GpadAxesGlob[4] x 0x48 (PS4 0x05A4F090): per-client gamepad axis
		// bindings, read by CL_GamepadAxisValue(lc, axis). Slot 2 is foreign.
		// CL_InitGamepadAxisBindings loops up to an end marker, &gaGlobs[2] + 0x1C;
		// it moves to &new[4] + 0x1C, else the loop stops after client 0.
		constexpr entcoll_site gaglobs_sites[] = {
			{0x0133F00B, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405c6de]
			{0x0133F085, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405c664]
			{0x0133F5AA, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405c13f]
			{0x0133FB06, 3, 7, false, 0x0000}, // lea r14, [r13 + 0x539b6d0]
			{0x0133FD4E, 3, 7, false, 0x0000}, // lea r9, [r10 + 0x539b6d0]
			{0x01340140, 3, 7, true , 0x001C}, // lea r8, [rip + 0x405b5c5]
			{0x0134067B, 3, 7, false, 0x0000}, // lea rdi, [r15 + 0x539b6d0]
		};
		// s_rightStickModels (word[5] per controller, stride 0xA) overlaps
		// s_gamepadButtons[0] with its slot 3, so it moves. s_gamepadButtons (PC
		// stride 0x2E, PS4 0x2A) moves too: slots 2..3 have no code refs but hold
		// static list nodes reached through links (a widen without the move crashed).
		constexpr entcoll_site gamepadbuttons_sites[] = {
			{0x013402B6, 4, 8, false, 0x0002}, // lea rdi, [r12 + 0x539b782]           init
			{0x013402C7, 5, 9, false, 0x0000}, // mov word [r14 + r12 + 0x539b780], r13w
			{0x01340323, 5, 9, false, 0x002C}, // mov word [r14 + r12 + 0x539b7ac], ax  KeyPressBits
			{0x0134036D, 3, 7, true , 0x0000}, // lea rcx, [rip -> 0x0539B780]        CL_ModelForButton
			{0x01340383, 3, 7, true , 0x002C}, // lea rcx, [rip -> 0x0539B7AC]        KeyPressBits getter
			{0x013403D2, 3, 7, true , 0x0002}, // lea rax, [rip -> 0x0539B782]        per-controller reset
		};
		constexpr entcoll_site rightstick_sites[] = {
			{0x01340235, 5, 9, false, 0x0000}, // mov word ptr [r12 + rbx*2 + 0x539b760], ax
			{0x01340243, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
			{0x01340253, 5, 9, false, 0x0002}, // mov word ptr [r12 + rbx*2 + 0x539b762], ax
			{0x01340261, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
			{0x01340271, 5, 9, false, 0x0004}, // mov word ptr [r12 + rbx*2 + 0x539b764], ax
			{0x0134027F, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
			{0x0134028F, 5, 9, false, 0x0006}, // mov word ptr [r12 + rbx*2 + 0x539b766], ax
			{0x013402A8, 5, 9, false, 0x0008}, // mov word ptr [r12 + rbx*2 + 0x539b768], ax
			{0x013403FB, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x405b37e]
			{0x013404F8, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x405b281]
		};
		size_t gaglobs_new = 0;

		bool relocate_gaglobs()
		{
			if (gaglobs_new)
			{
				return true;
			}
			constexpr uint32_t ga_base = 0x0531C6D0;
			constexpr uint32_t ga_stride = 0x48;
			constexpr uint32_t end_rva = 0x0134014A; // lea r10, [rip + d32], 7 bytes
			const auto b = base();
			auto* end_disp = reinterpret_cast<uint8_t*>(b + end_rva + 3);
			int32_t end_old = 0;
			if (!readable(end_disp, sizeof(end_old)))
			{
				return false;
			}
			std::memcpy(&end_old, end_disp, sizeof(end_old));
			const auto* end_insn = reinterpret_cast<const uint8_t*>(b + end_rva);
			if (end_insn[0] != 0x4C || end_insn[1] != 0x8D || end_insn[2] != 0x15
			    || static_cast<int64_t>(end_rva) + 7 + end_old != static_cast<int64_t>(ga_base) + 2 * ga_stride + 0x1C)
			{
				note("[splitscreen] gaGlobs: end marker bytes differ - nothing moved\n");
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * ga_stride));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, 4 * ga_stride);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + ga_base), 2 * ga_stride);
			std::vector<int32_t> saved(std::size(gaglobs_sites));
			if (!rewrite_entcoll(gaglobs_sites, std::size(gaglobs_sites), ga_base,
			                     reinterpret_cast<size_t>(fresh), saved.data()))
			{
				return false;
			}
			const auto end_new = static_cast<int64_t>(reinterpret_cast<size_t>(fresh) + 4 * ga_stride + 0x1C)
			                     - static_cast<int64_t>(b + end_rva + 7);
			const auto end_new32 = static_cast<int32_t>(end_new);
			if (end_new != end_new32 || !write_bytes(end_disp, &end_new32, sizeof(end_new32)))
			{
				for (size_t j = 0; j < std::size(gaglobs_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + gaglobs_sites[j].rva);
					write_bytes(insn + gaglobs_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			gaglobs_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		constexpr perclient_array batch2[] = {
			{"totalcoverage", 0x04CC3420, 0x360, totalcoverage_sites, std::size(totalcoverage_sites), 0, {}},
			{"rightstick", 0x0531C760, 0xA, rightstick_sites, std::size(rightstick_sites), 0, {}},
			{"gamepadbuttons", 0x0531C780, 0x2E, gamepadbuttons_sites, std::size(gamepadbuttons_sites), 0, {}},
		};
		size_t batch2_new[std::size(batch2)] = {};

		// cgExploderTriggers (1000 x 0x30 per client, row 0xBB80) and cgExploderTriggerCount
		// (int per client): [4] on PS4 (0x03E9C550), [2] on PC. Trigger row 2 is foreign data
		// and count slot 3 is written by a static initializer. CG_ExplodersInit clears the
		// counts with one 7-byte qword store and memsets the triggers (0x17700), so both move
		// into one block, the counts right behind the four rows, and the memset length becomes
		// 0x2EE10: one clear covers everything, as on PS4.
		// Sites: exploder_triggers_reloc.validated.txt, not the generator's output.
		constexpr entcoll_site exploder_trig_sites[] = {
			{0x001FD4B8, 3, 7, true , 0x0010}, // lea rax,[rip+..]  +0x10   CG_ExploderUpdate walk, imul lc,0xBB80
			{0x001FD776, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          CG_ExplodersInit memset (length widened below)
			{0x002008EA, 3, 7, true , 0x0018}, // lea rdi,[rip+..]  +0x18   CG_FindTrigger(lc, ...)
			{0x00205679, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
			{0x002070EA, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
			{0x00208B38, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
		};
		constexpr entcoll_site exploder_count_sites[] = {
			{0x001FD466, 3, 7, false, 0x0000}, // lea r12, [r10 + 0x43806c8]
			{0x001FD78F, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x4182f32], rax
			{0x0020090A, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x417fdb7]
			{0x00205612, 3, 7, false, 0x0000}, // lea rax, [rax + 0x43806c8]
			{0x0020706C, 4, 8, false, 0x0000}, // lea rcx, [rax*4 + 0x43806c8]
			{0x00208AAC, 4, 8, false, 0x0000}, // lea rcx, [rax*4 + 0x43806c8]
		};
		constexpr uint32_t exploder_trig_base = 0x046946E0;
		constexpr uint32_t exploder_trig_stride = 0xBB80;
		constexpr uint32_t exploder_count_base = 0x043016C8;
		size_t exploder_trig_new = 0;

		bool relocate_exploder_triggers()
		{
			if (exploder_trig_new)
			{
				return true;
			}
			const auto b = base();
			auto* len = reinterpret_cast<uint8_t*>(b + 0x001FD77F);
			constexpr uint8_t len_old[] = {0x41, 0xB8, 0x00, 0x77, 0x01, 0x00}; // mov r8d, 0x17700
			constexpr uint8_t len_new[] = {0x41, 0xB8, 0x10, 0xEE, 0x02, 0x00}; // mov r8d, 0x2EE10
			if (!readable(len, sizeof(len_old)) || std::memcmp(len, len_old, sizeof(len_old)) != 0)
			{
				note("[splitscreen] exploder triggers: memset length bytes differ - nothing moved\n");
				return false;
			}
			constexpr size_t rows = 4 * exploder_trig_stride; // 0x2EE00
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(rows + 0x10));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, rows + 0x10);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + exploder_trig_base), 2 * exploder_trig_stride);
			std::memcpy(fresh + rows, reinterpret_cast<const void*>(b + exploder_count_base), 2 * sizeof(int32_t));
			std::vector<int32_t> saved_trig(std::size(exploder_trig_sites));
			std::vector<int32_t> saved_count(std::size(exploder_count_sites));
			if (!rewrite_entcoll(exploder_trig_sites, std::size(exploder_trig_sites), exploder_trig_base,
			                     reinterpret_cast<size_t>(fresh), saved_trig.data()))
			{
				return false;
			}
			const auto undo_trig = [&]
			{
				for (size_t j = 0; j < std::size(exploder_trig_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exploder_trig_sites[j].rva);
					write_bytes(insn + exploder_trig_sites[j].disp_off, &saved_trig[j], sizeof(int32_t));
				}
			};
			if (!rewrite_entcoll(exploder_count_sites, std::size(exploder_count_sites), exploder_count_base,
			                     reinterpret_cast<size_t>(fresh + rows), saved_count.data()))
			{
				undo_trig();
				return false;
			}
			if (!write_bytes(len, len_new, sizeof(len_new)))
			{
				undo_trig();
				for (size_t j = 0; j < std::size(exploder_count_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + exploder_count_sites[j].rva);
					write_bytes(insn + exploder_count_sites[j].disp_off, &saved_count[j], sizeof(int32_t));
				}
				return false;
			}
			exploder_trig_new = reinterpret_cast<size_t>(fresh);
			note("[splitscreen] cgExploderTriggers + counts [2]->[4] at RVA 0x%08X\n",
			     static_cast<uint32_t>(exploder_trig_new - b));
			return true;
		}

		void relocate_batch2()
		{
			for (size_t i = 0; i < std::size(batch2); ++i)
			{
				if (!batch2_new[i])
				{
					batch2_new[i] = relocate_perclient(batch2[i]);
				}
			}
			relocate_exploder_triggers();
			relocate_gaglobs();
		}

		// ---- Batch 3: screen effects and compass ----
		// s_screenBlur (x 0x1C), s_screenElectrified and s_screenBurn (x 0xC) are [2] arrays
		// packed back to back (PS4 0x04001E50 / EC0 / EF0), so each slot 2 is the next base.
		// CG_CompassUpdateActors(lc) runs every frame, so player 3 wrote 0x2C00 bytes past
		// s_compassActors across the other compass tables. CG_ClearCompassPingData clears each
		// table with a two-row length (batch3_clear_len); it becomes four rows only for a table
		// that actually moved.
		constexpr entcoll_site screenblur_sites[] = {
			{0x0060136E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x421c09b]
			{0x0060C333, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42110d6]
			{0x00641853, 3, 7, true , 0x0018}, // lea rcx, [rip + 0x41dbbce]
			{0x006467B2, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41d6c57]
			{0x00652BA0, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41ca869]
		};
		constexpr entcoll_site screenelec_sites[] = {
			{0x00602C3B, 4, 8, false, 0x0000}, // lea rdx, [rcx*4 + 0x481d448]
			{0x0060C363, 3, 7, true , 0x0000}, // lea r8, [rip + 0x42110de]
			{0x0061136D, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d44c], r12d
			{0x0061137F, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d44c], eax
			{0x00611394, 4, 8, false, 0x0008}, // mov dword ptr [r13 + rdi*4 + 0x481d450], eax
			{0x0061139C, 4, 8, false, 0x0000}, // mov qword ptr [r13 + rdi*4 + 0x481d448], r12
		};
		constexpr entcoll_site screenburn_sites[] = {
			{0x0060C393, 3, 7, true , 0x0000}, // lea r8, [rip + 0x42110c6]
			{0x006113A9, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d464], r12d
			{0x006113BB, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d464], eax
			{0x006113D0, 4, 8, false, 0x0008}, // mov dword ptr [r13 + rdi*4 + 0x481d468], eax
			{0x006113D8, 4, 8, false, 0x0000}, // mov qword ptr [r13 + rdi*4 + 0x481d460], r12
			{0x0063FECB, 4, 8, false, 0x0000}, // lea rdx, [rcx*4 + 0x481d460]
		};
		constexpr entcoll_site compass_actors_sites[] = {
			{0x00598884, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426bcf5]   CG_ClearCompassPingData
			{0x005A1EC3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42626b6]
			{0x005A1FA3, 3, 7, true , 0x0000}, // lea rcx,[array] / imul rax,rax,0x2C00
			{0x005A3B0D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4260a6c]
			{0x005A5343, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425f236]
			{0x005A6CFF, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425d87a]
			{0x005B3452, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4251127]
			{0x005B4DB4, 3, 7, true , 0x0000}, // lea r11, [rip + 0x424f7c5]
			{0x005B7FDB, 3, 7, true , 0x0000}, // lea r11, [rip + 0x424c59e]
			{0x005CC464, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4238115]
			{0x005D2B8D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42319ec]
			{0x005D2BBE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42319bb]
			{0x005D445E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x423011b]
		};
		constexpr entcoll_site compass_vehicles_sites[] = {
			{0x005988AC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42735cd]
			{0x005A20A3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4269dd6]
			{0x005AB961, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4260518]
			{0x005D95D5, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42328a4]
		};
		constexpr entcoll_site compass_artillery_sites[] = {
			{0x00593D7F, 3, 7, true , 0x0010}, // lea rax, [rip + 0x427950a]
			{0x00593DD2, 4, 8, false, 0x0010}, // mov dword ptr [rsi + rbx*4 + 0x480d290], r9d
			{0x00593DDE, 3, 7, false, 0x0000}, // mov dword ptr [rsi + rbx*4 + 0x480d280], eax
			{0x00593DE8, 3, 7, false, 0x0004}, // mov dword ptr [rsi + rbx*4 + 0x480d284], eax
			{0x00593DF1, 3, 7, false, 0x0008}, // mov dword ptr [rsi + rbx*4 + 0x480d288], eax
			{0x00593DFB, 3, 7, false, 0x000C}, // mov dword ptr [rsi + rbx*4 + 0x480d28c], eax
			{0x00593E18, 5, 9, false, 0x0000}, // addss xmm0, dword ptr [rsi + rbx*4 + 0x480d280]
			{0x00593E21, 5, 9, false, 0x0000}, // movss dword ptr [rsi + rbx*4 + 0x480d280], xmm0
			{0x00593E36, 5, 9, false, 0x0004}, // addss xmm0, dword ptr [rsi + rbx*4 + 0x480d284]
			{0x00593E3F, 5, 9, false, 0x0004}, // movss dword ptr [rsi + rbx*4 + 0x480d284], xmm0
			{0x005988E8, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274991]
			{0x005A1FC3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b2b6]
		};
		constexpr entcoll_site compass_heli_sites[] = {
			{0x005988FC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274a6d]
			{0x005A2043, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b326]
			{0x005D9285, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42340e4]
		};
		constexpr entcoll_site compass_0240_sites[] = {
			{0x00598910, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274c19]
			{0x005A2023, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b506]
			{0x005D9145, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42343e4]
		};
		constexpr entcoll_site compass_0120_sites[] = {
			{0x00598924, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4275085]
			{0x005A2063, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b946]
			{0x005D8DC8, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4234be1]
		};
		constexpr entcoll_site compass_0500_sites[] = {
			{0x00598938, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42762b1]
			{0x005A1FE3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426cc06]
			{0x005CC5B0, 3, 7, true , 0x0004}, // lea rax, [rip + 0x424263d]
		};
		constexpr entcoll_site compass_0400_sites[] = {
			{0x0059894C, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4276c9d]
			{0x005A2083, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426d566]
			{0x005D94A5, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x4236144]
		};
		constexpr perclient_array batch3[] = {
			{"screenblur", 0x0479E410, 0x1C, screenblur_sites, std::size(screenblur_sites), 0, {}},
			{"screenelec", 0x0479E448, 0xC, screenelec_sites, std::size(screenelec_sites), 0, {}},
			{"screenburn", 0x0479E460, 0xC, screenburn_sites, std::size(screenburn_sites), 0, {}},
			{"compass_actors", 0x04785580, 0x2C00, compass_actors_sites, std::size(compass_actors_sites), 0, {}},
			{"compass_vehicles", 0x0478CE80, 0x900, compass_vehicles_sites, std::size(compass_vehicles_sites), 0, {}},
			{"compass_artillery", 0x0478E280, 0x78, compass_artillery_sites, std::size(compass_artillery_sites), 0, {}},
			{"compass_heli", 0x0478E370, 0xE0, compass_heli_sites, std::size(compass_heli_sites), 0, {}},
			{"compass_0240", 0x0478E530, 0x240, compass_0240_sites, std::size(compass_0240_sites), 0, {}},
			{"compass_0120", 0x0478E9B0, 0x120, compass_0120_sites, std::size(compass_0120_sites), 0, {}},
			{"compass_0500", 0x0478FBF0, 0x500, compass_0500_sites, std::size(compass_0500_sites), 0, {}},
			{"compass_0400", 0x047905F0, 0x400, compass_0400_sites, std::size(compass_0400_sites), 0, {}},
		};
		constexpr uint32_t batch3_clear_len[] = {
			0x00000000, // screenblur (no clear site)
			0x00000000, // screenelec (no clear site)
			0x00000000, // screenburn (no clear site)
			0x0059888D, // compass_actors
			0x005988B5, // compass_vehicles
			0x005988F1, // compass_artillery
			0x00598905, // compass_heli
			0x00598919, // compass_0240
			0x0059892D, // compass_0120
			0x00598941, // compass_0500
			0x00598955, // compass_0400
		};
		size_t batch3_new[std::size(batch3)] = {};

		void relocate_batch3()
		{
			const auto b = base();
			for (size_t i = 0; i < std::size(batch3); ++i)
			{
				if (batch3_new[i])
				{
					continue;
				}
				const auto& a = batch3[i];
				auto* len = batch3_clear_len[i] ? reinterpret_cast<uint8_t*>(b + batch3_clear_len[i]) : nullptr;
				const uint32_t len_old = 2 * a.stride;
				const uint32_t len_new = 4 * a.stride;
				if (len)
				{
					uint32_t cur = 0;
					if (!readable(len, 6) || len[0] != 0x41 || len[1] != 0xB8)
					{
						continue;
					}
					std::memcpy(&cur, len + 2, sizeof(cur));
					if (cur != len_old)
					{
						note("[splitscreen] %s: clear length differs - not moved\n", a.name);
						continue;
					}
				}
				batch3_new[i] = relocate_perclient(a);
				if (batch3_new[i] && len)
				{
					write_bytes(len + 2, &len_new, sizeof(len_new));
				}
			}
		}

		// ---- Batch 4: CG_AllocateClientMemory's pointer tables and the destructibles ----
		// CG_AllocateClientMemory (PS4 0x21FD70) allocates the cg_weaponsArray, cg_destructibles
		// and cg_ikBuf buffers of every local client, client 2 included, but the pointer tables
		// are [2], so slot 2 was stored over foreign globals (weapons[2] replaced a weapon-info
		// pointer used by everyone). Moving the tables is the whole fix. cg_numDestructibles ->
		// cg_updateTime and s_destructible_gamestates ([2][32] x 0x84, row 0x1080) ->
		// s_num_destructible_gamestates are packed the same way: each slot 2 is the next global.
		constexpr entcoll_site cg_weaponsarray_sites[] = {
			{0x0044D1AA, 4, 8, false, 0x0000}, // add r13, qword ptr [rax + rcx*8 + 0x49d9410]
			{0x00843ACF, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x49d9410], rax
			{0x00853DE9, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x49d9410]
			{0x00856EE2, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x4182527], rax
			{0x00856EE9, 3, 7, true , 0x0008}, // mov qword ptr [rip + 0x4182528], rax
			{0x008F258B, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rcx + r14*8 + 0x49d9410]
			{0x0119A273, 4, 8, false, 0x0000}, // add r15, qword ptr [rdx + r14*8 + 0x49d9410]
			{0x011CA1FD, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x380f22c]
			{0x0122C649, 4, 8, false, 0x0000}, // add rdi, qword ptr [rbp + r14*8 + 0x49d9410]
			{0x0126671A, 4, 8, false, 0x0000}, // add rdi, qword ptr [r10 + rsi*8 + 0x49d9410]
			{0x026D13F4, 4, 8, false, 0x0000}, // add r13, qword ptr [rcx + rax*8 + 0x49d9410]
			{0x0271F6BE, 4, 8, false, 0x0000}, // add rbx, qword ptr [r14 + rax*8 + 0x49d9410]
		};
		constexpr entcoll_site cg_ikbuf_sites[] = {
			{0x00843B03, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x4a315c0], rax
			{0x00853DC0, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x4a315c0]
		};
		// ikStates is not in this batch: see relocate_ikstates and relocate_batch4.
		constexpr entcoll_site cg_destructibles_sites[] = {
			{0x00843AF1, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x17f00ff0], rax
			{0x00853DD9, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x17f00ff0]
			{0x00856F0C, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x176aa0dd], rax
			{0x00856F13, 3, 7, true , 0x0008}, // mov qword ptr [rip + 0x176aa0de], rax
			{0x022F28C0, 4, 8, false, 0x0000}, // mov rax, qword ptr [r13 + rdi*8 + 0x17f00ff0]
			{0x022F5C96, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b921e3]
			{0x022F5CB3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b921c6]
			{0x022F5CF3, 4, 8, false, 0x0000}, // mov r10, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5D7D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5D8D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5D9D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5DAA, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5DB7, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5DDB, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
			{0x022F5EF1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rsi*8 + 0x17f00ff0]
			{0x022F5F10, 4, 8, false, 0x0000}, // mov rdi, qword ptr [r14 + rsi*8 + 0x17f00ff0]
			{0x022F6067, 4, 8, false, 0x0000}, // add r8, qword ptr [rdi + r12*8 + 0x17f00ff0]
			{0x022F60B4, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r12*8 + 0x17f00ff0]
			{0x022F60C8, 4, 8, false, 0x0000}, // mov r8, qword ptr [rdx + r12*8 + 0x17f00ff0]
			{0x022F60EA, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r12*8 + 0x17f00ff0]
			{0x022F6110, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + r12*8 + 0x17f00ff0]
			{0x022F6154, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdi + r12*8 + 0x17f00ff0]
			{0x022F6161, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r12*8 + 0x17f00ff0]
			{0x022FAB99, 4, 8, false, 0x0000}, // add rdx, qword ptr [r12 + r15*8 + 0x17f00ff0]
			{0x022FAC3A, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b8d23f]
			{0x022FC8DF, 4, 8, false, 0x0000}, // lea rsi, [rdi*8 + 0x17f00ff0]
			{0x022FD47C, 4, 8, false, 0x0000}, // add rdi, qword ptr [rsi + r15*8 + 0x17f00ff0]
			{0x0230064F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b8782a]
		};
		constexpr entcoll_site numdestructibles_sites[] = {
			// Three leas that point at this base are deliberately not listed: they are end markers
			// of loops over s_destructibles (0x80 x 0x108), which ends exactly where this array
			// begins. Moving them made the destructible walk run off the end (tools/sentinel_check.py).
			{0x022F5A58, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15bd1511]
			{0x022F5D62, 4, 8, false, 0x0000}, // inc dword ptr [rdi + r11*4 + 0x17f400e0]
			{0x022F5F55, 4, 12, false, 0x0000}, // mov dword ptr [r14 + rsi*4 + 0x17f400e0], 0
			{0x022F601F, 4, 8, false, 0x0000}, // mov dword ptr [rdi + r14*4 + 0x17f400e0], eax
			{0x022F91CB, 4, 8, false, 0x0000}, // cmp r13d, dword ptr [rdi + r12*4 + 0x17f400e0]
			{0x022FAB84, 4, 8, false, 0x0000}, // cmp dword ptr [r12 + r15*4 + 0x17f400e0], ebx
			{0x022FABC2, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r12 + r15*4 + 0x17f400e0]
			{0x022FC94D, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + r15 + 0x17f400e0], ebx
			{0x022FC98F, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r14 + r15 + 0x17f400e0]
		};
		constexpr entcoll_site cg_updatetime_sites[] = {
			{0x022F5F61, 4, 12, false, 0x0000}, // mov dword ptr [r14 + rsi*4 + 0x17f400e8], 0
			{0x022FC920, 4, 8, false, 0x0000}, // mov eax, dword ptr [r14 + r15 + 0x17f400e8]
			{0x022FC92A, 4, 8, false, 0x0000}, // mov dword ptr [r14 + r15 + 0x17f400e8], eax
			{0x022FC945, 4, 8, false, 0x0000}, // mov dword ptr [r14 + r15 + 0x17f400e8], eax
		};
		constexpr entcoll_site destr_gamestates_sites[] = {
			{0x0230200C, 3, 7, true , 0x0002}, // lea rax, [rip + 0x15b85e7f]
			{0x02302043, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b85e46]
			{0x02302B07, 3, 7, true , 0x0000}, // lea r15, [rip + 0x15b85382]
			{0x02302BB1, 3, 7, true , 0x0002}, // lea rax, [rip + 0x15b852da]
			{0x02302BE3, 3, 7, false, 0x0000}, // lea rdx, [r11 + 0x17f01000]
			{0x02302BEA, 3, 7, false, 0x0000}, // lea r9, [r11 + 0x17f01000]
		};
		constexpr entcoll_site destr_numgamestates_sites[] = {
			{0x02301FF3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b87f96]
			{0x02302ACD, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b874bc]
			{0x02302BA1, 4, 8, false, 0x0000}, // mov r9d, dword ptr [r11 + r10*4 + 0x17f03100]
			{0x02302BF1, 4, 8, false, 0x0000}, // mov dword ptr [r11 + r10*4 + 0x17f03100], eax
		};
		constexpr perclient_array batch4[] = {
			{"cg_weaponsarray", 0x0495A410, 0x8, cg_weaponsarray_sites, std::size(cg_weaponsarray_sites), 0, {}},
			{"cg_ikbuf", 0x049B25C0, 0x8, cg_ikbuf_sites, std::size(cg_ikbuf_sites), 0, {}},
			{"cg_destructibles", 0x17E820C0, 0x8, cg_destructibles_sites, std::size(cg_destructibles_sites), 0, {}},
			{"numdestructibles", 0x17EC11B0, 0x4, numdestructibles_sites, std::size(numdestructibles_sites), 0, {}},
			{"cg_updatetime", 0x17EC11B8, 0x4, cg_updatetime_sites, std::size(cg_updatetime_sites), 0, {}},
			{"destr_gamestates", 0x17E820D0, 0x1080, destr_gamestates_sites, std::size(destr_gamestates_sites), 0, {}},
			{"destr_numgamestates", 0x17E841D0, 0x4, destr_numgamestates_sites, std::size(destr_numgamestates_sites), 0, {}},
		};
		size_t batch4_new[std::size(batch4)] = {};

		bool ik_reset_widened = false;

		// ikStates: PS4 `IKState* ikStates[5]` (0x120DB5D0), the server's state plus one per
		// local client. The PC table has three slots ([0] server, [1 + lc]), walked by the IK
		// reset loop up to its end marker. Client 2's slot is that unreferenced end address,
		// but client 3's is a foreign byte flag, so the table moves to [5] and the reset loop's
		// end marker (lea r14) moves with it.
		constexpr entcoll_site ikstates_sites[] = {
			{0x023F7B43, 3, 7, true , 0x0008}, // lea rdx, [ikStates+8]   IK_AllocateLocalClientMemory
			{0x023F7D0D, 3, 7, true , 0x0000}, // lea rdx, [ikStates]
			{0x023F7D3F, 3, 7, true , 0x0000}, // lea rdx, [ikStates]
			{0x023F7E15, 3, 7, true , 0x0008}, // lea rax, [ikStates+8]
			{0x023F8200, 3, 7, true , 0x0000}, // lea rsi, [ikStates]
			{0x023F84AC, 3, 7, true , 0x0000}, // lea rsi, [ikStates]      reset loop start
			{0x023F8594, 3, 7, true , 0x0000}, // lea rsi, [ikStates]
			{0x023F9260, 3, 7, true , 0x0008}, // lea rcx, [ikStates+8]
			{0x0245A539, 4, 9, false, 0x0008}, // cmp qword [rbx+rcx*8+ikStates+8], 0
		};
		constexpr uint32_t ikstates_base = 0x17F297C0;
		constexpr uint32_t ikstates_old_slots = 3;
		constexpr uint32_t ikstates_new_slots = 5;
		size_t ikstates_new = 0;

		bool relocate_ikstates()
		{
			if (ikstates_new)
			{
				return true;
			}
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(ikstates_new_slots * 8));
			if (!fresh)
			{
				return false;
			}
			std::memset(fresh, 0, ikstates_new_slots * 8);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + ikstates_base), ikstates_old_slots * 8);
			int32_t saved[std::size(ikstates_sites)] = {};
			if (!rewrite_entcoll(ikstates_sites, std::size(ikstates_sites), ikstates_base,
			                     reinterpret_cast<size_t>(fresh), saved))
			{
				return false;
			}
			if (!retarget_end_marker(0x023F84B3, 3, 7, ikstates_base + ikstates_old_slots * 8,
			                         reinterpret_cast<size_t>(fresh) + ikstates_new_slots * 8))
			{
				for (size_t j = 0; j < std::size(ikstates_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + ikstates_sites[j].rva);
					write_bytes(insn + ikstates_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				return false;
			}
			ikstates_new = reinterpret_cast<size_t>(fresh);
			return true;
		}

		void relocate_batch4()
		{
			for (size_t i = 0; i < std::size(batch4); ++i)
			{
				if (!batch4_new[i])
				{
					batch4_new[i] = relocate_perclient(batch4[i]);
				}
			}
			// Move ikStates to [5]. If a reference fails to verify, fall back to moving the reset
			// loop's end marker one slot, which covers client 2 (three players); with two players
			// that slot is NULL and the loop skips it.
			if (!ik_reset_widened)
			{
				ik_reset_widened = relocate_ikstates()
					|| retarget_end_marker(0x023F84B3, 3, 7, 0x17F297D8, base() + 0x17F297E0);
			}
		}

		// ---- Batch 5: two unnamed per-client cgame arrays (no PS4 [4] global has either shape) ----
		// cg_clientents30: 30 entries of 0x11E0 per client (stride 0x21840). Row 2 holds foreign
		// pointer globals, so player 3's entity interpolation read a NULL pointer and crashed.
		// The static initializer only zeroes a field, so zero-fill is the initial state.
		constexpr entcoll_site cg_clientents30_sites[] = {
			{0x0019891D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2bdc]
			{0x001989B0, 3, 7, true , 0x0080}, // lea rcx, [rip + 0x40c2bc9]
			{0x001989EB, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2b0e]
			{0x00198A89, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2a70]
			{0x0019A3BC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c113d]
			{0x0019A462, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c1097]
			{0x0019A63D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0ebc]
			{0x0019A754, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0da5]
			{0x0019A8A4, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0c55]
			{0x0019A9D6, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0b23]
			{0x0019B831, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x40bfcc8]
			{0x02CA73F8, 3, 7, true , 0x00F0}, // lea rax, [rip + 0x153abf1]
		};
		// cg_perclient_3c0: 8 entries of 0x78 per client, right before s_screenBlur; row 2 ran
		// over the screen-effect arrays and foreign globals.
		constexpr entcoll_site cg_perclient_3c0_sites[] = {
			{0x0060F6B5, 3, 7, true , 0x0000}, // lea rax, [rip + 0x420d5c4]
			{0x0060F844, 3, 7, true , 0x0000}, // lea rax, [rip + 0x420d435]
			{0x00641878, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41db401]
			{0x00643596, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41d96e3]
			{0x0065785F, 3, 7, false, 0x0000}, // lea rcx, [rcx + 0x481cc80]
			{0x00662085, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x41babf4]
		};
		constexpr perclient_array batch5[] = {
			{"cg_clientents30", 0x041DC500, 0x21840, cg_clientents30_sites, std::size(cg_clientents30_sites), 0, {}},
			{"cg_perclient_3c0", 0x0479DC80, 0x3C0, cg_perclient_3c0_sites, std::size(cg_perclient_3c0_sites), 0, {}},
		};
		size_t batch5_new[std::size(batch5)] = {};

		void relocate_batch5()
		{
			for (size_t i = 0; i < std::size(batch5); ++i)
			{
				if (!batch5_new[i])
				{
					batch5_new[i] = relocate_perclient(batch5[i]);
				}
			}
		}

		// ---- Batch 6: cgame threaded-notify queues ----
		// PS4 CG_ThreadedNotifyList_* (Init 0x2956D0): per local client 100 items plus
		// s_processQueueHead/Tail and s_firstFree, all [4]. On PC (items 0x50, stride 0x1F40) all
		// four are [2] and packed back to back, so CG_Init(2) at map load linked 100 items over
		// player 1's queue pointers and ~8 KB of live globals: a wild writer consistent with the
		// Arxan faults, the lost default_aitype and "Data is corrupt" in 3-player rounds.
		// Runs with CG_FRAME on and off, so it is not gated on BO3_CG_FRAME.
		constexpr entcoll_site tnotify_list_sites[] = {
			{0x00A18777, 3, 7, true , 0x0044}, // lea rcx, [rip + 0x430c0e6]
			{0x00A2179C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x430307d]
			{0x00A217DC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x430303d]
			{0x00A2181C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302ffd]
			{0x00A21865, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302fb4]
			{0x00A218BC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302f5d]
			{0x00A21A0B, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302e0e]
			{0x00A21B09, 3, 7, true , 0x0000}, // lea r11, [rip + 0x4302d10]
			{0x00A21B10, 3, 7, true , 0x0048}, // lea rdx, [rip + 0x4302d51]
			{0x00A21B69, 3, 7, true , 0x0044}, // lea rsi, [rip + 0x4302cf4]
			{0x02D2C855, 3, 7, true , 0x0040}, // lea rax, [rip + 0x1f7ea04]
		};
		constexpr entcoll_site tnotify_head_sites[] = {
			{0x00A219F1, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286c0], rbx
			{0x00A21B22, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286c0], r10
			{0x00A21CC4, 4, 8, false, 0x0000}, // mov r15, qword ptr [r9 + rbx*8 + 0x4d286c0]
			{0x00A21CDB, 4, 12, false, 0x0000}, // mov qword ptr [r9 + rbx*8 + 0x4d286c0], 0
			{0x00A21FA2, 4, 8, false, 0x0000}, // mov rax, qword ptr [r12 + rdi*8 + 0x4d286c0]
			{0x00A21FBB, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286c0], rbx
		};
		constexpr entcoll_site tnotify_tail_sites[] = {
			{0x00A219DE, 4, 8, false, 0x0000}, // mov rax, qword ptr [r12 + rdi*8 + 0x4d286d0]
			{0x00A219F9, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286d0], rbx
			{0x00A21B31, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286d0], r10
			{0x00A21CE7, 4, 12, false, 0x0000}, // mov qword ptr [r9 + rbx*8 + 0x4d286d0], 0
			{0x00A21FB2, 4, 9, false, 0x0000}, // cmp qword ptr [r12 + rdi*8 + 0x4d286d0], 0
			{0x00A21FC5, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286d0], r13
		};
		constexpr entcoll_site tnotify_free_sites[] = {
			{0x00A21915, 4, 9, false, 0x0000}, // cmp qword ptr [r12 + rdi*8 + 0x4d286e0], 0
			{0x00A219A9, 4, 8, false, 0x0000}, // mov rbx, qword ptr [r12 + rdi*8 + 0x4d286e0]
			{0x00A219D2, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286e0], rax
			{0x00A21B5E, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286e0], rax
			{0x00A21EEE, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + rax*8 + 0x4d286e0]
			{0x00A21F02, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286e0], rsi
		};
		constexpr perclient_array batch6[] = {
			{"tnotify_list", 0x04CA5820, 0x1F40, tnotify_list_sites, std::size(tnotify_list_sites), 0, {}},
			{"tnotify_head", 0x04CA96C0, 0x8, tnotify_head_sites, std::size(tnotify_head_sites), 0, {}},
			{"tnotify_tail", 0x04CA96D0, 0x8, tnotify_tail_sites, std::size(tnotify_tail_sites), 0, {}},
			{"tnotify_free", 0x04CA96E0, 0x8, tnotify_free_sites, std::size(tnotify_free_sites), 0, {}},
		};
		size_t batch6_new[std::size(batch6)] = {};

		// The engine's static initializer sets every item of both clients to zero except
		// +0x04 = 0x3FF. Its lea is in the site table, so it fills slots 0/1 of the new block;
		// relocate_batch6 gives slots 2/3 the same values. If its bytes differ, nothing moves.
		constexpr uint8_t tnotify_static_init[] = {
			0xB9, 0xC7, 0x00, 0x00, 0x00,                         // mov ecx, 0xC7
			0x48, 0x8D, 0x05,                                     // lea rax, [rip+..]
		};
		constexpr uint8_t tnotify_static_init_body[] = {
			0x89, 0x50, 0xC0,                                     // mov [rax-0x40], edx
			0xC7, 0x40, 0xC4, 0xFF, 0x03, 0x00, 0x00,             // mov dword [rax-0x3C], 0x3FF
		};

		void relocate_batch6()
		{
			const auto b = base();
			const auto bytes_at = [b](const uint32_t rva, const uint8_t* expect, const size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, expect, n) == 0;
			};
			if (!batch6_new[0])
			{
				if (!bytes_at(0x02D2C850, tnotify_static_init, sizeof(tnotify_static_init))
					|| !bytes_at(0x02D2C862, tnotify_static_init_body, sizeof(tnotify_static_init_body)))
				{
					note("[splitscreen] tnotify: static initializer differs - queues not moved\n");
					return;
				}
				batch6_new[0] = relocate_perclient(batch6[0]);
				if (!batch6_new[0])
				{
					return;   // the pointer arrays only make sense with the list moved
				}
				auto* items = reinterpret_cast<uint8_t*>(batch6_new[0]);
				for (size_t lc = 2; lc < 4; ++lc)
				{
					for (size_t i = 0; i < 100; ++i)
					{
						*reinterpret_cast<uint32_t*>(items + lc * 0x1F40 + i * 0x50 + 0x04) = 0x3FF;
					}
				}
			}
			for (size_t i = 1; i < std::size(batch6); ++i)
			{
				if (!batch6_new[i])
				{
					batch6_new[i] = relocate_perclient(batch6[i]);
				}
			}
		}

		// ---- Batch 7: renderer [2] x 0x240 array that slid the image by 8 bytes ----
		// An element holds 16 {int id, int age} entries, a qword count at +0x80, an id bitmask
		// at +0xC0 and a flag at +0x238. A routine drops stale entries by shifting the list down
		// 8 bytes up to &e[count-1]. With index 2 the element was foreign and the count garbage,
		// so ~190 MB of the image through .idata slid down by 8: Arxan faults, "Cannot find AI
		// Type" and "Data is corrupt". The initializer writes only zeros. History: LOG.md, "190 MB".
		constexpr entcoll_site fxgpu_client_sites[] = {
			{0x01CBD588, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd841ea1]
			{0x02E89DB4, 3, 7, true , 0x00C0}, // lea rbx, [rip + 0xc608505]
		};
		constexpr perclient_array batch7[] = {
			{"view_idsets_240", 0x0F48C880, 0x240, fxgpu_client_sites, std::size(fxgpu_client_sites), 0, {}},
		};
		size_t batch7_new[std::size(batch7)] = {};

		void relocate_batch7()
		{
			for (size_t i = 0; i < std::size(batch7); ++i)
			{
				if (!batch7_new[i])
				{
					batch7_new[i] = relocate_perclient(batch7[i]);
				}
			}
		}

		// ---- Batch 8: renderer scene buffers ----
		// scene_pc480: 4 entries of 0x120 per client, sized for two. Client 2's slot lies over
		// the dpvs globals (PS4 GfxSceneDpvs: entVisData[4] etc.), so renderer workers crashed
		// using a float as entVisData[lc].
		// scene_c: the per-client pointer table R_InitSceneBuffers fills (PS4 dpvsGlob); slot 2
		// is the base of another array. The allocator's store is among its sites, so C[0]/C[1]
		// land in the new block too.
		constexpr entcoll_site scene_pc480_sites[] = {
			{0x01C84F2F, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9201136]
			{0x01C85077, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200fee]
			{0x01C850AE, 3, 7, true , 0x001C}, // lea r8, [rip + 0x9200fb7]
			{0x01C854D8, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200b8d]
			{0x01C855E7, 3, 7, true , 0x0138}, // lea rdi, [rip + 0x9200b9a]
			{0x01C856A2, 3, 7, true , 0x001C}, // lea rax, [rip + 0x92009c3]
			{0x01C858A7, 3, 7, true , 0x001C}, // lea rax, [rip + 0x92007be]
			{0x01C8593E, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200727]
			{0x01C87AF0, 4, 8, false, 0x001C}, // mov ecx, dword ptr [r8 + rbx + 0xae9243c]
			{0x01C87B9A, 4, 8, false, 0x001C}, // mov ecx, dword ptr [r8 + rbx + 0xae9243c]
			{0x01C87C2E, 3, 7, true , 0x0020}, // lea rax, [rip + 0x91fe43b]
			{0x01C87C6A, 3, 7, true , 0x0034}, // lea rax, [rip + 0x91fe413]
			{0x01C87CAA, 3, 7, true , 0x0048}, // lea rax, [rip + 0x91fe3e7]
			{0x01C87CEA, 3, 7, true , 0x005C}, // lea rax, [rip + 0x91fe3bb]
			{0x01C87D80, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fe2e5]
			{0x01C8A9D8, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fb68d]
			{0x01C8AF48, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fb11d]
			{0x01C8B094, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fafd1]
			{0x01CE1DA1, 3, 7, true , 0x037C}, // lea rax, [rip + 0x91a4624]
			{0x01D0DB6F, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91784f6]
		};
		constexpr entcoll_site scene_c_sites[] = {
			{0x01C85A0C, 4, 8, false, 0x0000}, // mov rax, qword ptr [r9 + r10 + 0x10615a60]
			{0x01C85A4E, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + rcx*8 + 0x10615a60]
			{0x01C8745D, 4, 8, false, 0x0000}, // mov qword ptr [rbx + rsi + 0x10615a60], rax
			{0x01C8752F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + rdi + 0x10615a60]
		};
		constexpr perclient_array batch8[] = {
			{"scene_pc480", 0x0AE134A0, 0x480, scene_pc480_sites, std::size(scene_pc480_sites), 0, {}},
			{"scene_c", 0x10596AE0, 0x8, scene_c_sites, std::size(scene_c_sites), 0, {}},
		};
		size_t batch8_new[std::size(batch8)] = {};

		void relocate_batch8()
		{
			for (size_t i = 0; i < std::size(batch8); ++i)
			{
				if (!batch8_new[i])
				{
					batch8_new[i] = relocate_perclient(batch8[i]);
				}
			}
			scene_c_new = batch8_new[1];
		}

		// ---- Batch 9: renderer per-client array (x 0xA24) ----
		// Client 2's slot covers the frame-limiter target and the globals around it (the
		// limiter's actual writer is batch 10). The static initializer only writes zero
		// fields, so zero-filled slots 2/3 are the constructed state.
		constexpr entcoll_site rview_a24_sites[] = {
			{0x01C9C189, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd83b9ec]
			{0x01C9C3C6, 3, 7, true , 0x0A20}, // lea rdi, [rip + 0xd83c1cf]
			{0x01C9C3CD, 3, 7, true , 0x0000}, // lea rbp, [rip + 0xd83b7a8]
			{0x01C9C440, 4, 8, true , 0x01C0}, // movss xmm0, dword ptr [rip + 0xd83b8f4]
			{0x01C9C457, 4, 8, true , 0x01C4}, // divss xmm0, dword ptr [rip + 0xd83b8e1]
			{0x01C9C464, 4, 8, true , 0x01C8}, // movss xmm0, dword ptr [rip + 0xd83b8d8]
			{0x01C9C471, 4, 8, true , 0x01CC}, // divss xmm1, dword ptr [rip + 0xd83b8cf]
			{0x01C9C47E, 4, 8, true , 0x01B0}, // movss xmm0, dword ptr [rip + 0xd83b8a6]
			{0x01C9C48B, 4, 8, true , 0x01B4}, // movss xmm1, dword ptr [rip + 0xd83b89d]
			{0x01C9C498, 4, 8, true , 0x01B8}, // movss xmm0, dword ptr [rip + 0xd83b894]
			{0x01C9C4A5, 4, 8, true , 0x01BC}, // movss xmm1, dword ptr [rip + 0xd83b88b]
			{0x01C9C4B2, 4, 8, true , 0x01D8}, // movss xmm0, dword ptr [rip + 0xd83b89a]
			{0x01C9C4BF, 4, 8, true , 0x01DC}, // movss xmm1, dword ptr [rip + 0xd83b891]
			{0x01C9C4CC, 4, 8, true , 0x01E0}, // movss xmm0, dword ptr [rip + 0xd83b888]
			{0x01C9C4D9, 4, 8, true , 0x01E8}, // movss xmm1, dword ptr [rip + 0xd83b883]
			{0x01C9C4E6, 4, 8, true , 0x01EC}, // movss xmm0, dword ptr [rip + 0xd83b87a]
			{0x01C9C4F3, 4, 8, true , 0x01F0}, // movss xmm1, dword ptr [rip + 0xd83b871]
			{0x01C9C500, 4, 8, true , 0x01F4}, // movss xmm0, dword ptr [rip + 0xd83b868]
			{0x01C9C50D, 4, 8, true , 0x01F8}, // movss xmm1, dword ptr [rip + 0xd83b85f]
			{0x01C9C51A, 4, 8, true , 0x01E4}, // movss xmm0, dword ptr [rip + 0xd83b83e]
			{0x01C9C85F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd83b316]
			{0x01C9CA22, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd83b153]
			{0x01C9CB0F, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd83b066]
			{0x02E89C1A, 3, 7, true , 0x0000}, // lea rbx, [rip + 0xc5e0d2b]
		};
		constexpr perclient_array batch9[] = {
			{"rview_a24", 0x0F464FCC, 0xA24, rview_a24_sites, std::size(rview_a24_sites), 0, {}},
		};
		size_t batch9_new[std::size(batch9)] = {};

		void relocate_batch9()
		{
			for (size_t i = 0; i < std::size(batch9); ++i)
			{
				if (!batch9_new[i])
				{
					batch9_new[i] = relocate_perclient(batch9[i]);
				}
			}
		}

		// ---- Batch 10: frame-limiter stall (renderer per-view array, x 0x30) ----
		// With three views the main thread sat in the frame limiter (`while (Sys_Milliseconds()
		// < target) Sys_Sleep(1)`) because the target held a float: no frames, no LUI tick,
		// frozen players. The writer is this array, indexed by the view's local client (`lea
		// r,[i+i*2]; shl r,4`, so perclient_sweep missed it); element 2 covers a pointer global
		// and the limiter target. On PS4 it is a member of a larger renderer struct. No static
		// initializer references it. History: LOG.md, "frame limiter".
		constexpr entcoll_site rview_org30_sites[] = {
			{0x01C73DFD, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd8651dc]
			{0x01C747DB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd8647fe]
			{0x01C74EDC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd8640fd]
			{0x01C75012, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd863fc7]
			{0x01C7AB76, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85e463]
			{0x01C7AFB0, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85e029]
			{0x01C7C8CC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85c70d]
			{0x01C7D057, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85bf82]
			{0x01C7E2E4, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85acf5]
			{0x01C7E4E5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85aaf4]
			{0x01C7E8BA, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85a71f]
			{0x01CB2BC5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd826414]
			{0x01CDD3EC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd7fbbed]
		};
		constexpr perclient_array batch10[] = {
			{"rview_org30", 0x0F466430, 0x30, rview_org30_sites, std::size(rview_org30_sites), 0, {}},
		};
		size_t batch10_new[std::size(batch10)] = {};

		void relocate_batch10()
		{
			for (size_t i = 0; i < std::size(batch10); ++i)
			{
				if (!batch10_new[i])
				{
					batch10_new[i] = relocate_perclient(batch10[i]);
				}
			}
		}

		// ---- Batch 11: aim-target actor lists ----
		// PS4 aim_target_actors [4]: 64 centity pointers per client (base + lc*0x200), built by
		// the aim_target submitter and read by a worker job. [2] on PC; slot 2 was
		// AimTarget_Cmd's old home (moved in batch 1b), so player 3's pass read stale bytes as
		// entity pointers and crashed. Zero-filled slots mean "no actors".
		constexpr entcoll_site aimactors_sites[] = {
			{0x0007A998, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36049e1]
			{0x000897A8, 4, 8, false, 0x0000}, // mov qword ptr [rax + rcx*8 + 0x367f380], r8
		};
		constexpr perclient_array batch11[] = {
			{"aimactors", 0x03600380, 0x200, aimactors_sites, std::size(aimactors_sites), 0, {}},
		};
		size_t batch11_new[std::size(batch11)] = {};

		void relocate_batch11()
		{
			for (size_t i = 0; i < std::size(batch11); ++i)
			{
				if (!batch11_new[i])
				{
					batch11_new[i] = relocate_perclient(batch11[i]);
				}
			}
		}


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

		// ---- Guests start from player 1's classes and stats ----
		// As on console: PS4 Live_HandleClientSplitscreenSignin (0xC16080) calls
		// LiveStats_CopyFromSponsor (0xC6A6B0) for a guest without stats, so the guest plays
		// with the sponsor's level, unlocks and classes and keeps nothing. On PC one function
		// reads every controller's "<name>_<controller>.cgp" (one call site, 0x58-byte file
		// descriptors). The hook runs just before it and, for controllers 1..3, copies player 1's
		// <name>_0.cgp over <name>_N.cgp for the loadout and stats files; the game then reads a
		// real file through its own path. BO3_GUEST_COPY=off disables it.
		constexpr uint32_t save_read_callsite = 0x0221806E;
		constexpr uint32_t save_read_rva = 0x01C144B0;
		constexpr size_t save_desc_stride = 0x58;
		constexpr size_t save_desc_name_max = 0x40;
		constexpr size_t save_desc_other_dir = 0x4C;
		// Branch on the descriptor's "other directory" byte. BOIII (patch_players_folder_name)
		// makes it a jmp (0xEB), so every file goes to boiii_players whatever the byte says.
		constexpr uint32_t save_dir_branch_rva = 0x01C1451E;
		constexpr uint32_t save_base_dvar_rva = 0x179E63E0;
		constexpr uint32_t dvar_get_string_rva = 0x02262A70;
		constexpr const char* sponsor_copy_names[] = {
			"loadouts_zm_offline", "loadouts_mp_offline", "loadouts_cp_offline",
			"stats_zm_offline", "stats_mp_offline", "stats_cp_offline",
			"stats_cp_nightmare_offline", "stats_fr_offline",
		};
		bool guest_copy_installed = false;
		constexpr uint32_t save_write_callsite = 0x02218049;   // same task layout as the read
		constexpr uint32_t save_write_rva = 0x01C145C0;

		// Trace only: which controller's files the game writes, and when.
		void save_write_stub(const int controller, uint8_t* files, const int count)
		{
			reinterpret_cast<void (*)(int, uint8_t*, int)>(base() + save_write_rva)(controller, files, count);
			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" save write: controller ");
			l.dec(static_cast<uint64_t>(controller < 0 ? 0 : controller));
			l.str(",");
			for (int i = 0; files && i < count && i < 16; ++i)
			{
				const auto* desc = files + static_cast<size_t>(i) * save_desc_stride;
				if (strnlen(reinterpret_cast<const char*>(desc), save_desc_name_max) < save_desc_name_max)
				{
					l.str(" ");
					l.str(reinterpret_cast<const char*>(desc));
				}
			}
			trace_write(l);
		}

		bool is_sponsor_copy_name(const char* name)
		{
			for (const auto* n : sponsor_copy_names)
			{
				if (std::strcmp(name, n) == 0)
				{
					return true;
				}
			}
			return false;
		}

		void save_read_stub(const int controller, uint8_t* files, const int count)
		{
			size_t copied = 0;
			size_t missing = 0;
			size_t failed = 0;
			trace_line names;
			if (controller >= 1 && controller < 4 && files && count > 0)
			{
				const auto b = base();
				const bool dir_forced = *reinterpret_cast<const uint8_t*>(b + save_dir_branch_rva) == 0xEB;
				const auto* dvar = *reinterpret_cast<void* const*>(b + save_base_dvar_rva);
				const auto get_string = reinterpret_cast<const char* (*)(const void*)>(b + dvar_get_string_rva);
				const char* root = dvar ? get_string(dvar) : nullptr;
				for (int i = 0; root && *root && i < count; ++i)
				{
					const auto* desc = files + static_cast<size_t>(i) * save_desc_stride;
					if (strnlen(reinterpret_cast<const char*>(desc), save_desc_name_max) >= save_desc_name_max)
					{
						names.str(" ?");
						continue;
					}
					const auto* name = reinterpret_cast<const char*>(desc);
					names.str(" ");
					names.str(name);
					if (!dir_forced && desc[save_desc_other_dir] != 0)
					{
						names.str("(other dir)");
						continue;
					}
					if (!is_sponsor_copy_name(name))
					{
						continue;
					}
					names.str("*");
					char src[MAX_PATH]{};
					char dst[MAX_PATH]{};
					const auto ns = std::snprintf(src, sizeof(src), "%s\\boiii_players\\%s_0.cgp", root, name);
					const auto nd = std::snprintf(dst, sizeof(dst), "%s\\boiii_players\\%s_%d.cgp", root, name, controller);
					if (ns <= 0 || nd <= 0 || ns >= MAX_PATH || nd >= MAX_PATH)
					{
						++failed;
						continue;
					}
					if (GetFileAttributesA(src) == INVALID_FILE_ATTRIBUTES)
					{
						++missing;   // player 1 has no such file yet - leave the guest's alone
						continue;
					}
					if (CopyFileA(src, dst, FALSE))
					{
						++copied;
					}
					else
					{
						++failed;
					}
				}
			}

			reinterpret_cast<void (*)(int, uint8_t*, int)>(base() + save_read_rva)(controller, files, count);

			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" save read: controller ");
			l.dec(static_cast<uint64_t>(controller < 0 ? 0 : controller));
			l.str(controller < 0 ? " (negative)" : "");
			l.str(", ");
			l.dec(static_cast<uint64_t>(count < 0 ? 0 : count));
			l.str(" files; from player 1: copied ");
			l.dec(copied);
			l.str(", player 1 missing ");
			l.dec(missing);
			l.str(", failed ");
			l.dec(failed);
			if (names.n)
			{
				l.str(" |");
				names.b[names.n] = 0;
				l.str(names.b);
			}
			trace_write(l);
		}

		void install_guest_copy()
		{
			trace_line l;
			char env[16] = {};
			GetEnvironmentVariableA("BO3_GUEST_COPY", env, sizeof(env));
			if (std::strcmp(env, "off") == 0)
			{
				l.str("guest copy: OFF (BO3_GUEST_COPY=off)");
				trace_write(l);
				return;
			}
			if (guest_copy_installed)
			{
				return;
			}
			if (!call_site_targets(save_read_callsite, save_read_rva))
			{
				l.str("guest copy: NOT installed - 0x02274B9E is not call 0x01C20880");
				trace_write(l);
				return;
			}
			try
			{
				utils::hook::call(base() + save_read_callsite, save_read_stub);
				if (call_site_targets(save_write_callsite, save_write_rva))
				{
					utils::hook::call(base() + save_write_callsite, save_write_stub);
				}
			}
			catch (...)
			{
				l.str("guest copy: NOT installed - hook failed");
				trace_write(l);
				return;
			}
			guest_copy_installed = true;
			l.str("guest copy: installed - controllers 1..3 read player 1's loadouts + stats");
			trace_write(l);
		}

		// The game reads the saves only once, at boot, so each join re-reads the eight files for
		// a fresh copy (PS4 CanPerformFileOp 0xF80F80 refuses nothing for a second read).
		// Storage_Read (ecx controller, edx type, r8d slot -> bool) is entered at its own
		// address so storage_read_stub's guest filter still applies. Types (PC property table):
		//   11 loadouts_cp_offline  15 loadouts_mp_offline  20 loadouts_zm_offline
		//    7 stats_cp_offline      9 stats_cp_nightmare_offline
		//   13 stats_mp_offline     18 stats_zm_offline     22 stats_fr_offline
		constexpr int sponsor_copy_types[] = {11, 15, 20, 7, 9, 13, 18, 22};

		void reread_guest_saves(const int controller)
		{
			if (!guest_copy_installed || controller < 1 || controller > 3)
			{
				return;
			}
			const auto read = reinterpret_cast<bool (*)(int, int, int)>(base() + storage_read_rva);
			uint32_t queued = 0;
			for (size_t i = 0; i < std::size(sponsor_copy_types); ++i)
			{
				if (read(controller, sponsor_copy_types[i], 0))
				{
					queued |= 1u << i;
				}
			}
			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" guest copy: controller ");
			l.dec(static_cast<uint64_t>(controller));
			l.str(" joined - re-read queued mask ");
			l.hex(queued);
			l.str(" of 0xFF (types 11 15 20 7 9 13 18 22)");
			trace_write(l);
		}

		// ---- Client-script local-client bound ----
		// PS4 CScr_GetLocalClientNum (0x1D926A0) accepts local clients 0..3; the PC inlines the
		// check into every client-script builtin with the bound 2:
		//   call Scr_GetInt / cmp reg, 1 / jbe ok
		// For lc 2 the script error aborted the clientfield callback dispatcher before client 2's
		// queue was cleared, so it re-dispatched forever (main-thread hang). The table holds the
		// imm8 of each `cmp reg, 1` (tools/gen_csc_lc_checks.py). All-or-nothing: every byte
		// must read 0x01 before any is written.
		constexpr uint32_t csc_lc_check_imms[] = {
			0x0028A308, 0x00293969, 0x00293A1A, 0x00293A7C, 0x002B41CB, 0x002DDF33,
			0x002E5B63, 0x002E7413, 0x002E8D03, 0x002EC2A6, 0x002EC2F3, 0x002EF4FA,
			0x002EF59C, 0x002F0E6A, 0x002F0EE3, 0x003284FA, 0x0032855A, 0x00368C1A,
			0x00368FE8, 0x003694DC, 0x0036C73C, 0x0036FB2A, 0x0036FBA1, 0x00372E1A,
			0x0037AAC4, 0x0037AB6A, 0x0039294A, 0x003929AA, 0x00392A0A, 0x00395CAF,
			0x00395D2A, 0x00395D91, 0x003AC01A, 0x003AC0DA, 0x003AC13A, 0x003B24BA,
			0x003B3E16, 0x003B3F6F, 0x003B592F, 0x003B8B8A, 0x003B8C16, 0x003B8FAF,
			0x003BA90A, 0x003BC21A, 0x003BC27A, 0x003BC308, 0x003BC463, 0x003D8293,
			0x003DE4EF, 0x003DE564, 0x003DE5AA, 0x003DE60A, 0x003DE66A, 0x003DE6CA,
			0x003DE75A, 0x003DE82F, 0x003DE8E4, 0x003DE9AF, 0x003DEA54, 0x003E984C,
			0x003ED234, 0x003EEDD9, 0x003EF00A, 0x003FF0D2, 0x00412A33, 0x004209C2,
			0x00422265, 0x00423CA5, 0x004255B4, 0x004257FA, 0x00425925, 0x00425A7D,
			0x00425B61, 0x004274EA, 0x004275D1, 0x00428FE4, 0x0042924C, 0x0042AAE4,
			0x0042AB66, 0x00435E84, 0x00A3630C, 0x00A3F43C, 0x00A8444B, 0x00A8DA73,
			0x00A9A624, 0x00AA0A94, 0x00AAD334, 0x00AB686A, 0x00AB9E71, 0x00ACCA84,
			0x00ACFBB4, 0x00B582E8, 0x00B583B9, 0x00B61869, 0x00B7A497, 0x00B7EF11,
			0x00B824E1, 0x00BC53D4, 0x00BC542C, 0x00BC6CDC, 0x00BE8296, 0x00BF095B,
			0x00C07E54, 0x00C224DC, 0x00C30536, 0x00C383D1, 0x00C4041F, 0x00C6728A,
			0x00C7BC62, 0x00C7EDE2, 0x00C82289, 0x00C8252A, 0x00C86DFE, 0x00C8D28C,
			0x00C8EB2C, 0x00C9FBB8, 0x00C9FCAC, 0x00CA5F19, 0x00CA909A, 0x00CBA12E,
			0x00CBA24D, 0x00CBA304, 0x00CBA3E4, 0x00CBA4B4, 0x00CBA58E, 0x00CBA6B4,
			0x00CBA76A, 0x00CBA84F, 0x00CBA8CF, 0x00CBA93F, 0x00CBA9E3, 0x00CBCC90,
			0x00CC1873, 0x00CC3123, 0x00CCADCB, 0x00CCB38C, 0x00CE56B4, 0x00CE5723,
			0x00CE57E4, 0x00CE5834, 0x00CE588C, 0x00CE715C, 0x00CF86AB, 0x00CFD1CB,
			0x00D254CA, 0x00D2E913, 0x00D301E1, 0x00D31AB3, 0x00D333B8, 0x00D46288,
			0x00D46318, 0x00D463A8, 0x00D46488, 0x00D46518, 0x00D4DF98, 0x00D4E07C,
			0x00D4F938, 0x00D4F9F8, 0x00D4FA78, 0x00D4FAEB, 0x00D5136D, 0x00D53289,
			0x00D57C31, 0x00D5C6A8, 0x00D5C714, 0x00D5C78B, 0x00D5E1EA, 0x00D6475B,
			0x00D647DC, 0x00D6C1CC, 0x00D6DA64, 0x00D73DBC, 0x00D97681, 0x00D9F424,
			0x00D9F484, 0x00D9F504, 0x00DF9DA4, 0x00DF9E0C, 0x00DFE8AC, 0x00E00144,
			0x00E001A4, 0x00E001F4, 0x00E0025C, 0x00E04C2F, 0x00E2183E, 0x00E2946F,
			0x00E2AEA1, 0x00E2C83A, 0x00E2E186, 0x00E2FB6E, 0x00E314CD, 0x00E32DFD,
			0x00E34711, 0x00E360AA, 0x00E36226, 0x00E363D7, 0x00E36469, 0x00E44376,
			0x00E45F85, 0x00E63981, 0x00E65289, 0x00E69CFC, 0x00E6B5DC, 0x00E6CECC,
			0x00E6E7BC, 0x00E93371, 0x00EA12F3, 0x00EE3D51, 0x00EE560A, 0x00F0175C,
			0x00F0C47E, 0x00F0DD86, 0x00F29D51, 0x00F2B7D0, 0x00F2B8D5, 0x00F41B44,
			0x00F5101B,
		};
		bool csc_lc_widened = false;

		// The bound follows cl_maxLocalClients: sync_csc_lc_bound applies min(3, cl_max - 1).
		// A fixed 3 let lc 2/3 into these builtins in the lobby (cl_max 2), where they get a NULL
		// cg and crash (CScr_SetShowcaseWeaponPaintshopXUID). The sync runs right after the only
		// two writers of cl_maxLocalClients, on the main thread. Do not use an async loop: the
		// async pipelines stop during a map load and the 4-player load hung.
		void widen_csc_lc_checks()
		{
			if (csc_lc_widened)
			{
				return;
			}
			const auto b = base();
			for (const auto rva : csc_lc_check_imms)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + rva);
				if (!readable(at, 1) || *at != 0x01)
				{
					trace_line l;
					l.str("csc lc bound: 0x");
					l.hex(rva);
					l.str(" is not stock - nothing written");
					trace_write(l);
					return;
				}
			}
			// Slot-3 audit (tools/audit_csc_slot3.py): only clientUIActives' owned slot-3 head is
			// left. The bytes stay stock here; sync_csc_lc_bound writes the bound.
			csc_lc_widened = true;
		}

		// CScr_SetFilterPassEnabled compares lc against a register holding 1 (`cmp r9d, edi`), so
		// it is not in the table and players 3/4 got no screen filters. PS4 (0x14DEC0) accepts
		// 0..4. `cmp r9d, edi` becomes `cmp eax, 1` (same Scr_GetInt result; ja still rejects
		// negatives), and sync_csc_lc_bound owns the immediate from then on.
		constexpr uint32_t filter_pass_check_rva = 0x0039DB48;   // movsxd r9,eax ; cmp r9d,edi ; ja
		bool filter_pass_owned = false;

		void widen_filter_pass_lc_check()
		{
			static constexpr uint8_t stock[] = {0x4C, 0x63, 0xC8, 0x44, 0x3B, 0xCF, 0x0F, 0x87};
			static constexpr uint8_t cmp_eax_1[] = {0x83, 0xF8, 0x01};
			auto* at = reinterpret_cast<uint8_t*>(base() + filter_pass_check_rva);
			filter_pass_owned = readable(at, sizeof(stock)) && std::memcmp(at, stock, sizeof(stock)) == 0
				&& write_bytes(at + 3, cmp_eax_1, sizeof(cmp_eax_1));
			trace_line l;
			l.str(filter_pass_owned ? "SetFilterPassEnabled: local-client bound follows cl_maxLocalClients"
			                        : "SetFilterPassEnabled: bytes differ - not widened");
			trace_write(l);
		}

		uint8_t csc_lc_bound_applied = 1;

		void sync_csc_lc_bound()
		{
			if (!csc_lc_widened)
			{
				return;
			}
			const auto b = base();
			const auto max_local = *reinterpret_cast<const volatile uint32_t*>(b + cl_max_local_clients_rva);
			const uint8_t bound = max_local >= 4 ? 3 : max_local == 3 ? 2 : 1;
			if (bound == csc_lc_bound_applied)
			{
				return;
			}
			for (const auto rva : csc_lc_check_imms)
			{
				write_bytes(reinterpret_cast<uint8_t*>(b + rva), &bound, 1);
			}
			if (filter_pass_owned)
			{
				write_bytes(reinterpret_cast<uint8_t*>(b + filter_pass_check_rva + 5), &bound, 1);
			}
			trace_line l;
			l.str("csc lc bound: ");
			l.dec(csc_lc_bound_applied);
			l.str(" -> ");
			l.dec(bound);
			l.str(" (cl_maxLocalClients ");
			l.dec(max_local);
			l.str(")");
			trace_write(l);
			csc_lc_bound_applied = bound;
		}

		// AllocatePerLocalClientMemory stores cl_maxLocalClients as its last act and
		// CL_FreePerLocalClientMemory zeroes it; nothing else writes it. Neither has an Arxan
		// caller guard and ezz hooks neither, so both are detoured to resync the bound.
		utils::hook::detour alloc_per_lc_hook;
		utils::hook::detour free_per_lc_hook;

		uint64_t alloc_per_lc_stub(const int a, const int b, const int c)
		{
			const auto r = alloc_per_lc_hook.invoke<uint64_t>(a, b, c);
			sync_csc_lc_bound();
			return r;
		}

		uint64_t free_per_lc_stub(const bool a)
		{
			const auto r = free_per_lc_hook.invoke<uint64_t>(a);
			sync_csc_lc_bound();
			return r;
		}

		void install_lc_bound_hooks()
		{
			if (!csc_lc_widened)
			{
				return;
			}
			// Expected first bytes: alloc `mov [rsp+8],rbx`, free `push rbx; sub rsp,20h`
			static constexpr uint8_t alloc_head[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10};
			static constexpr uint8_t free_head[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0xE8};
			const auto b = base();
			if (!readable(reinterpret_cast<const void*>(b + 0x0135D330), sizeof(alloc_head))
				|| std::memcmp(reinterpret_cast<const void*>(b + 0x0135D330), alloc_head, sizeof(alloc_head)) != 0
				|| !readable(reinterpret_cast<const void*>(b + 0x0135DC20), sizeof(free_head))
				|| std::memcmp(reinterpret_cast<const void*>(b + 0x0135DC20), free_head, sizeof(free_head)) != 0)
			{
				csc_lc_widened = false;   // no trigger - the bound stays stock
				trace_line l;
				l.str("csc lc bound: allocator prologues differ - not widened");
				trace_write(l);
				return;
			}
			alloc_per_lc_hook.create(b + 0x0135D330, reinterpret_cast<void*>(&alloc_per_lc_stub));
			free_per_lc_hook.create(b + 0x0135DC20, reinterpret_cast<void*>(&free_per_lc_stub));
			sync_csc_lc_bound();
			trace_line l;
			l.str("csc lc bound: follows cl_maxLocalClients (allocator hooks)");
			trace_write(l);
		}

		// ---- Batch 12: zombies HUD player list ----
		// The PlayerList HUD update keeps six per-client arrays, all [2] and packed back to back:
		// scores, a second score, shown flags, client ids (-1 = none), icon names (char*) and the
		// own index. For lc 2 the icon row is the own-index array, so ints were read as string
		// pointers (crash in strcmp from the UI model string setter). The engine's reset leaves
		// set icons and ids per lc before use, so zero-filled slots 2/3 are fine.
		constexpr entcoll_site hudpl_score_sites[] = {
			{0x026A4375, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + rsi + 0x1a8759d0], eax
			{0x026A4388, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a8759d0], eax
			{0x026A74F2, 3, 7, false, 0x0000}, // mov dword ptr [rcx + rsi + 0x1a8759d0], eax
			{0x026C6E0B, 3, 7, false, 0x0000}, // lea rcx, [r10 + 0x1a8759d0]
			{0x026CC141, 4, 8, false, 0x0000}, // mov edx, dword ptr [r14 + rax + 0x1a8759d0]
		};
		constexpr entcoll_site hudpl_gap_sites[] = {
			{0x026A4396, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a10], eax
		};
		constexpr entcoll_site hudpl_flags_sites[] = {
			{0x026A4349, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + rsi + 0x1a875a50], eax
			{0x026A4362, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a50], eax
			{0x026A74D7, 3, 8, false, 0x0000}, // cmp dword ptr [rcx + rsi + 0x1a875a50], 1
			{0x026A74F9, 3, 11, false, 0x0000}, // mov dword ptr [rcx + rsi + 0x1a875a50], 0
			{0x026C6DDC, 4, 8, false, 0x0000}, // mov qword ptr [rax + r10 + 0x1a875a50], r9
			{0x026C6DE4, 4, 8, false, 0x0008}, // mov qword ptr [rax + r10 + 0x1a875a58], r9
			{0x026C6DEC, 4, 8, false, 0x0010}, // mov qword ptr [rax + r10 + 0x1a875a60], r9
			{0x026C6DFB, 4, 8, false, 0x0018}, // mov qword ptr [rax + r10 + 0x1a875a68], r9
			{0x026CC0D5, 4, 9, false, 0x0000}, // cmp dword ptr [r14 + rdx + 0x1a875a50], 0
		};
		constexpr entcoll_site hudpl_ids_sites[] = {
			{0x026A7434, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r14 + rax + 0x1a875a90]
			{0x026A74A6, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a90], ebx
			{0x026C6D58, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x18135bc1]
			{0x026C6DD2, 3, 7, false, 0x0000}, // lea rdx, [r10 + 0x1a875a90]
		};
		constexpr entcoll_site hudpl_icons_sites[] = {
			{0x026A43A2, 3, 8, false, 0x0000}, // cmp qword ptr [rsi + 0x1a875ad0], 0
			{0x026A43AA, 3, 7, false, 0x0000}, // lea rsi, [rsi + 0x1a875ad0]
			{0x026A74E1, 4, 8, false, 0x0000}, // mov qword ptr [rsi + rax*8 + 0x1a875ad0], rbx
			{0x026C6DC0, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x18135b99]
			{0x026CC0A9, 3, 7, true , 0x0000}, // lea rax, [rip + 0x181308b0]
			{0x026C6D43, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x18135c16]  (reset leaf; the generator misses it)
		};
		constexpr entcoll_site hudpl_self_sites[] = {
			{0x026A42DB, 4, 8, false, 0x0000}, // mov dword ptr [rsi + r11*4 + 0x1a875b50], eax
			{0x026CC08D, 3, 7, false, 0x0000}, // lea rax, [rdx + 0x1a875b50]
		};
		constexpr perclient_array batch12[] = {
			{"hudpl_score", 0x1A7F6A50, 0x20, hudpl_score_sites, std::size(hudpl_score_sites), 0, {}},
			{"hudpl_gap", 0x1A7F6A90, 0x20, hudpl_gap_sites, std::size(hudpl_gap_sites), 0, {}},
			{"hudpl_flags", 0x1A7F6AD0, 0x20, hudpl_flags_sites, std::size(hudpl_flags_sites), 0, {}},
			{"hudpl_ids", 0x1A7F6B10, 0x20, hudpl_ids_sites, std::size(hudpl_ids_sites), 0, {}},
			{"hudpl_icons", 0x1A7F6B50, 0x40, hudpl_icons_sites, std::size(hudpl_icons_sites), 0, {}},
			{"hudpl_self", 0x1A7F6BD0, 0x4, hudpl_self_sites, std::size(hudpl_self_sites), 0, {}},
		};
		size_t batch12_new[std::size(batch12)] = {};

		void relocate_batch12()
		{
			for (size_t i = 0; i < std::size(batch12); ++i)
			{
				if (!batch12_new[i])
				{
					batch12_new[i] = relocate_perclient(batch12[i]);
				}
			}
		}

		// ---- Batch 13: view 2's lighting and previous-frame view ----
		// Pane 3 drew a white void: two [2] per-view renderer arrays have a foreign slot 2.
		// s_sunVolumeTransitions (PS4 SunVolumeTransition[4] at 0x04546CE0; PC x 0x2BB8), used by
		// CG_SetLightingState(lc) and CG_InitView: slot 2 overwrote s_testEffect. The static
		// initializer stores 0xFFFFFFFF at +0x2BB0; slots 2/3 get the same state after the move.
		// g_prevFrameViewParmsDraw (PS4 GfxViewParms[4] x 0x290): R_RenderScene copies each
		// frame's view parms to prev[localClientNum]; slot 2 covered another renderer object.
		constexpr entcoll_site sunvol_sites[] = {
			{0x010CD118, 4, 12, false, 0x2BB0}, // mov dword ptr [rax + r14 + 0x4d2f6f0], 0xffffffff
			{0x010CD124, 4, 8, false, 0x2BA0}, // mov qword ptr [rax + r14 + 0x4d2f6e0], rcx
			{0x010CD12C, 4, 8, false, 0x2BA8}, // mov qword ptr [rax + r14 + 0x4d2f6e8], rcx
			{0x010EB4F0, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x3c41669]  (reset leaf, no .pdata)
			{0x010F30C2, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3c39a97]
			{0x02D2CC1F, 3, 7, true , 0x0830}, // lea rbx, [rip + 0x1f8714a]  (static initializer)
		};
		constexpr entcoll_site prevview_sites[] = {
			{0x01CDF3D5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xe15ffd4]
		};
		constexpr perclient_array batch13[] = {
			{"sunvol", 0x04CADB40, 0x2BB8, sunvol_sites, std::size(sunvol_sites), 0, {}},
			{"prevview", 0x0FDCC800, 0x290, prevview_sites, std::size(prevview_sites), 0, {}},
		};
		size_t batch13_new[std::size(batch13)] = {};

		// ---- Batch 14: LiveStats per-controller stat-change cache ----
		// PS4 LiveStats_SetStatChanged (0xC63D00) decodes change messages into
		// s_cachedStatsChanges[controller], [4]. The PC cache (0x100 entries, stride 0x4404,
		// count at +0x4400) is [2], so controller 2 overwrote the statics behind it, including
		// the "statReadDDLExt" cmd node (crash in Cmd_RemoveCommand at game over).
		// LiveStats_ResetCache clears all four slots on PS4; the PC memset (0x8808) clears two
		// and is widened to 0x11010 after the move (an uncleared count reaching 0x100 is
		// EXE_PATCH_STATSOVERFLOW).
		constexpr entcoll_site statscache_sites[] = {
			{0x01E94E8F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xf578eaa]  (LiveStats_ResetCache memset)
			{0x01E9893F, 2, 6, true , 0x4400}, // mov edx, dword ptr [rip + 0xf5797fb]
			{0x01E9894F, 2, 6, true , 0x4400}, // mov eax, dword ptr [rip + 0xf5797eb]
			{0x01E98959, 3, 7, true , 0x0040}, // lea r14, [rip + 0xf575420]
			{0x01E98960, 3, 7, true , 0x0000}, // lea rbp, [rip + 0xf5753d9]
			{0x01E989A3, 2, 6, true , 0x4400}, // mov eax, dword ptr [rip + 0xf579797]
			{0x01E989AD, 3, 7, true , 0x4400}, // mov dword ptr [rip + 0xf57978c], r15d
			{0x01E9952B, 3, 7, true , 0x0000}, // lea rax, [rip + 0xf57480e]  (LiveStats_SetStatChanged)
		};
		constexpr perclient_array batch14[] = {
			{"statscache", 0x1139B860, 0x4404, statscache_sites, std::size(statscache_sites), 0, {}},
		};
		size_t batch14_new[std::size(batch14)] = {};

		// ---- Batch 15: per-client UI visibility bits ----
		// The zombie HUD shows its widgets through the "UIVisibilityBit.<n>" models. PS4 keeps
		// the bits as u64[4] (sharedUiInfo +0x14240), the PC as u64[2]. Client 2's u64 lies on
		// the per-client visibility-bit model handles and the scoreboard team-model handles,
		// which CL_UpdateUIVisibilityBits(2) overwrote every frame: no HUD in panes 1 and 2.
		// Known and left: one routine clears match bits for lc 0/1 with a single 16-byte
		// and/andn; client 2's bits are recomputed every frame anyway.
		constexpr entcoll_site visbits_sites[] = {
			{0x0061D911, 3, 7, false, 0x0000}, // mov ebx, dword ptr [rbx + rsi*8 + 0x179dbdc8]
			{0x006D3CAD, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r9 + rbx*8 + 0x179dbdc8]
			{0x008635A1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rsi*8 + 0x179dbdc8]
			{0x00A141F4, 4, 8, false, 0x0000}, // mov eax, dword ptr [r13 + r15*8 + 0x179dbdc8]
			{0x00E477F9, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + rsi*8 + 0x179dbdc8]
			{0x00FB5B24, 3, 7, false, 0x0000}, // mov ecx, dword ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x0135FAF1, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x013D2030, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + rdx + 0x179dbdc8]
			{0x013D2047, 4, 12, false, 0x0000}, // mov qword ptr [r14 + rdx + 0x179dbdc8], 0
			{0x013D38B2, 4, 8, false, 0x0000}, // or qword ptr [r14 + rdx + 0x179dbdc8], rax
			{0x013D38CB, 4, 8, false, 0x0000}, // or rcx, qword ptr [r14 + rdx + 0x179dbdc8]
			{0x013D38D3, 4, 8, false, 0x0000}, // mov qword ptr [r14 + rdx + 0x179dbdc8], rcx
			{0x013D38FA, 4, 8, false, 0x0000}, // mov qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3919, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x40000000
			{0x013D3946, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3972, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3991, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x40000000
			{0x013D39A6, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x800000
			{0x013D39CD, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x1000000
			{0x013D3A12, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x2000000
			{0x013D3A54, 4, 8, false, 0x0000}, // or qword ptr [rcx + rdx + 0x179dbdc8], rax
			{0x013D3A81, 4, 8, false, 0x0000}, // or qword ptr [rcx + rdx + 0x179dbdc8], rax
			{0x013D3AAA, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3ADA, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B03, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B22, 4, 12, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], 0x4000000
			{0x013D3B3B, 4, 12, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], 0x8000000
			{0x013D3B5E, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B79, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3B94, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3BBD, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
			{0x013D3BF1, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x10000000
			{0x013D6D42, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x20000000
			{0x013D6DDF, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E04, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E26, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E44, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6E7E, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6EDC, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6EFD, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6F78, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D6F80, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x013D6FE1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x013D7033, 4, 8, false, 0x0000}, // mov qword ptr [r15 + r13 + 0x179dbdc8], rcx
			{0x013D7055, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D709D, 4, 8, false, 0x0000}, // and qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D70BF, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D70DD, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D7103, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D711E, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D7139, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D714A, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x200000
			{0x013D715F, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x400000
			{0x013D7173, 4, 8, false, 0x0000}, // mov rax, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x013D71D5, 4, 8, false, 0x0000}, // mov qword ptr [r15 + r13 + 0x179dbdc8], rax
			{0x013D72CC, 4, 8, false, 0x0000}, // mov r8, qword ptr [r15 + r13 + 0x179dbdc8]
			{0x01F23CB7, 4, 8, false, 0x0000}, // mov esi, dword ptr [rax + r12*8 + 0x179dbdc8]
			{0x01F24013, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + r12*8 + 0x179dbdc8]
			{0x01F26735, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x15aa8f0c]
			{0x01FDAA77, 3, 7, true , 0x0000}, // lea r8, [rip + 0x159f4bca]
			{0x01FF7347, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdx*8 + 0x179dbdc8]
			{0x01FF7537, 4, 8, false, 0x0000}, // mov r9, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x01FF770F, 4, 8, false, 0x0000}, // mov r9, qword ptr [rdx + rdi*8 + 0x179dbdc8]
			{0x01FF785E, 4, 8, false, 0x0000}, // mov r9, qword ptr [rcx + rdi*8 + 0x179dbdc8]
			{0x01FF79B7, 4, 8, false, 0x0000}, // mov r9, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x0200C283, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rbx*8 + 0x179dbdc8]
			{0x0201127F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x159be3c2]
			{0x0201153E, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x159be103]
			{0x0201C4B7, 5, 9, false, 0x0000}, // movzx eax, byte ptr [r13 + rdi*8 + 0x179dbdc8]
			{0x02035F70, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r11 + rsi*8 + 0x179dbdc8]
			{0x02037FE1, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x02038163, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x0203838E, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x0203A820, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdx + r12*8 + 0x179dbdc8]
			{0x020462F0, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rcx + r14*8 + 0x179dbdc8]
			{0x02054DD8, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r15*8 + 0x179dbdc8]
			{0x0205BF00, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + r13*8 + 0x179dbdc8]
			{0x020668E6, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x0206F333, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdi*8 + 0x179dbdc8]
			{0x0206F711, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rdi*8 + 0x179dbdc8]
			{0x02074E6E, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + rbx*8 + 0x179dbdc8]
			{0x0207824F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r14*8 + 0x179dbdc8]
			{0x02079CBC, 4, 8, false, 0x0000}, // mov r9, qword ptr [r12 + r14*8 + 0x179dbdc8]
			{0x0207B8ED, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r12*8 + 0x179dbdc8]
			{0x02080A84, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r14*8 + 0x179dbdc8]
			{0x02084436, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x02087672, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r10 + rdi*8 + 0x179dbdc8]
			{0x020877EE, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + rdi*8 + 0x179dbdc8]
			{0x02092456, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rax + rbx*8 + 0x179dbdc8]
			{0x02098F85, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r12 + rsi*8 + 0x179dbdc8]
			{0x0209920F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r13*8 + 0x179dbdc8]
			{0x020A0DDF, 3, 7, true , 0x0000}, // lea r9, [rip + 0x1592e862]
			{0x020A0ED7, 3, 7, true , 0x0000}, // lea r9, [rip + 0x1592e76a]
			{0x025B1223, 4, 8, true , 0x0000}, // movdqu xmm0, xmmword ptr [rip + 0x153b1a2d]
			{0x025B126B, 4, 8, true , 0x0000}, // movdqu xmmword ptr [rip + 0x153b19e5], xmm0
			{0x025D17BE, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15391493]
			{0x025D40C8, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rbx*8 + 0x179dbdc8]
			{0x026B72CE, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdi*8 + 0x179dbdc8]
			{0x026EBC6A, 4, 8, false, 0x0000}, // movzx ecx, byte ptr [rdx + rsi*8 + 0x179dbdc8]
			{0x026EF0C3, 4, 8, false, 0x0000}, // mov eax, dword ptr [rax + r13*8 + 0x179dbdc8]
			{0x026EF10A, 4, 8, false, 0x0000}, // mov eax, dword ptr [rax + r13*8 + 0x179dbdc8]
		};
		constexpr perclient_array batch15[] = {
			{"visbits", 0x1795CEC8, 0x8, visbits_sites, std::size(visbits_sites), 0, {}},
		};
		size_t batch15_new[std::size(batch15)] = {};

		void relocate_batch15()
		{
			for (size_t i = 0; i < std::size(batch15); ++i)
			{
				if (batch15_new[i])
				{
					continue;
				}
				batch15_new[i] = relocate_perclient(batch15[i]);
				if (!batch15_new[i])
				{
					continue;
				}
				// Widen the per-client reset loop that zeroes bits[lc] from 2 to 4.
				auto* bound = reinterpret_cast<uint8_t*>(base() + 0x01F26750);
				constexpr uint8_t bound_old[] = {0x83, 0xFF, 0x02};
				if (readable(bound, sizeof(bound_old)) && std::memcmp(bound, bound_old, sizeof(bound_old)) == 0)
				{
					const uint8_t four = 0x04;
					write_bytes(bound + 2, &four, 1);
				}
			}
		}

		// ---- Batch 16: console message buffers, con.messageBuffer [2] -> [4] ----
		// PS4 struct Console has MessageBuffer[4] at +0x11078, followed by color and
		// operationBuffer (the console text). The PC has MessageBuffer[2] there (stride 0x2BC0),
		// so Con_NudgeMessageWindowTimes for lc 2 walked con.color and text as a window and
		// crashed. Three reference forms, all verified before anything is written: RIP/ABS32
		// sites (conmsgbuf_sites); the end marker of Con_InitMessageBuffer's loop, retargeted to
		// &new[4]+0x2B10; con base plus a displacement inside the array (conmsgbuf_con_rel,
		// invisible to range scans), each rewritten to old + (new block - old array).
		// Nothing in the game has initialised con at post_unpack.
		constexpr uint32_t conmsgbuf_base = 0x052F87F8;
		constexpr uint32_t conmsgbuf_stride = 0x2BC0;

		constexpr entcoll_site conmsgbuf_sites[] = {
			{0x0133930F, 4, 8, false, 0x2030}, // mov qword ptr [rax + rdi + 0x5379828], rcx
			{0x01339317, 4, 8, false, 0x2038}, // mov qword ptr [rax + rdi + 0x5379830], rcx
			{0x0133931F, 4, 8, false, 0x2070}, // mov qword ptr [rax + rdi + 0x5379868], rcx
			{0x01339327, 4, 8, false, 0x2078}, // mov qword ptr [rax + rdi + 0x5379870], rcx
			{0x0133932F, 4, 8, false, 0x20B0}, // mov qword ptr [rax + rdi + 0x53798a8], rcx
			{0x01339337, 4, 8, false, 0x20B8}, // mov qword ptr [rax + rdi + 0x53798b0], rcx
			{0x0133933F, 4, 8, false, 0x20F0}, // mov qword ptr [rax + rdi + 0x53798e8], rcx
			{0x01339347, 4, 8, false, 0x20F8}, // mov qword ptr [rax + rdi + 0x53798f0], rcx
			{0x0133934F, 4, 8, false, 0x2B30}, // mov qword ptr [rbx + rdi + 0x537a328], rcx
			{0x01339357, 4, 8, false, 0x2B38}, // mov qword ptr [rbx + rdi + 0x537a330], rcx
			{0x0133A666, 3, 7, true , 0x201C}, // lea rcx, [rip + 0x403f1c7]
			{0x0133A7E0, 3, 7, true , 0x2000}, // lea rcx, [rip + 0x403f031]  Con_GetGameMsgWindow
			{0x0133AA39, 3, 7, true , 0x2B10}, // lea rdi, [rip + 0x403f8e8]  Con_InitMessageBuffer
			{0x0133D8E4, 3, 7, true , 0x2000}, // lea r13, [rip + 0x403bf2d]
			{0x0133DA8E, 3, 7, true , 0x2000}, // lea rax, [rip + 0x403bd83]
		};

		struct con_rel_site
		{
			uint32_t rva;
			uint8_t off;         // where the disp32 / imm32 sits in the instruction
			uint32_t old_value;  // offset from con it holds
		};

		constexpr con_rel_site conmsgbuf_con_rel[] = {
			{0x0133D107, 3, 0x13094}, // lea rcx, [rsi + 0x13094]            rsi = con (0x0133D0D8)
			{0x0133D172, 4, 0x13BB0}, // cmp dword ptr [rdi + rsi + 0x13bb0], r11d
			{0x0133D180, 3, 0x13BAC}, // mov eax, dword ptr [rdi + rsi + 0x13bac]
			{0x0133D18E, 3, 0x13B94}, // idiv dword ptr [rdi + rsi + 0x13b94]
			{0x0133D19C, 4, 0x13B78}, // mov rax, qword ptr [rdi + rsi + 0x13b78]
			{0x0133D1A8, 4, 0x13B80}, // mov rax, qword ptr [rdi + rsi + 0x13b80]
			{0x0133D1C3, 4, 0x13BB0}, // cmp r11d, dword ptr [rdi + rsi + 0x13bb0]
			{0x0133D22C, 3, 0x13078}, // lea rbx, [r15 + 0x13078]            r15 = con (0x0133D1FE)
			{0x0133D256, 3, 0x13B78}, // lea rcx, [r15 + 0x13b78]
			{0x0133D930, 4, 0x13B78}, // lea rdx, [r12 + 0x13b78]            r12 = con (0x0133D8CB)
			{0x0133D987, 4, 0x13B78}, // lea rdx, [r12 + 0x13b78]
			{0x0133DABC, 3, 0x13B78}, // add r8, 0x13b78                     r8 = con (0x0133DA95)
			{0x0133DB8B, 4, 0x13BB4}, // mov eax, dword ptr [rcx + r8 + 0x13bb4]   r8 = con (0x0133DB56)
			{0x0133DB93, 4, 0x13B80}, // mov rdi, qword ptr [rcx + r8 + 0x13b80]
			{0x0133DB9E, 4, 0x13B94}, // idiv dword ptr [rcx + r8 + 0x13b94]
			{0x0133DBA6, 4, 0x13BB4}, // mov dword ptr [rcx + r8 + 0x13bb4], edx
			// Con_ClearNotify (PS4 0x3F1FE0), a leaf without .pdata: clears the four game-message
			// windows, rcx = con. Missing, it cleared parts of the print queue for lc 2/3.
			{0x01339223, 4, 0x130A8}, // mov qword ptr [rax + rcx + 0x130a8], rdx
			{0x0133922B, 4, 0x130B0},
			{0x01339233, 4, 0x130E8},
			{0x0133923B, 4, 0x130F0},
			{0x01339243, 4, 0x13128},
			{0x0133924B, 4, 0x13130},
			{0x01339253, 4, 0x13168},
			{0x0133925B, 4, 0x13170},
		};

		constexpr uint32_t conmsgbuf_end_marker_rva = 0x0133AB7E; // lea rcx, [&messageBuffer[2]+0x2B10]
		constexpr uint32_t conmsgbuf_end_field = 0x2B10;

		constexpr perclient_array batch16[] = {
			{"conmsgbuf", conmsgbuf_base, conmsgbuf_stride, conmsgbuf_sites, std::size(conmsgbuf_sites), 0, {}},
		};
		size_t batch16_new[std::size(batch16)] = {};

		void relocate_batch16()
		{
			if (batch16_new[0])
			{
				return;
			}
			const auto b = base();
			for (const auto& s : conmsgbuf_con_rel)
			{
				uint32_t have = 0;
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				if (!readable(at, sizeof(have)))
				{
					return;
				}
				std::memcpy(&have, at, sizeof(have));
				if (have != s.old_value)
				{
					note("[splitscreen] conmsgbuf: 0x%08X holds 0x%X - nothing moved\n", s.rva, have);
					return;
				}
			}
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + conmsgbuf_end_marker_rva + 3);
				int32_t d32 = 0;
				if (!readable(at, sizeof(d32)))
				{
					return;
				}
				std::memcpy(&d32, at, sizeof(d32));
				if (conmsgbuf_end_marker_rva + 7 + d32
				    != conmsgbuf_base + 2 * conmsgbuf_stride + conmsgbuf_end_field)
				{
					note("[splitscreen] conmsgbuf: end marker differs - nothing moved\n");
					return;
				}
			}

			const auto fresh = relocate_perclient(batch16[0]);
			if (!fresh)
			{
				return;
			}
			batch16_new[0] = fresh;

			// allocate_near_module keeps every new value within 32 bits; checked anyway.
			const auto delta = static_cast<int64_t>(fresh) - static_cast<int64_t>(b + conmsgbuf_base);
			uint32_t rewritten = 0;
			for (const auto& s : conmsgbuf_con_rel)
			{
				const auto v = static_cast<int64_t>(s.old_value) + delta;
				if (v > INT32_MAX || v < INT32_MIN)
				{
					break;
				}
				const auto v32 = static_cast<int32_t>(v);
				if (!write_bytes(reinterpret_cast<void*>(b + s.rva + s.off), &v32, sizeof(v32)))
				{
					break;
				}
				++rewritten;
			}
			const bool marker = retarget_end_marker(conmsgbuf_end_marker_rva, 3, 7,
				conmsgbuf_base + 2 * conmsgbuf_stride + conmsgbuf_end_field,
				fresh + 4 * conmsgbuf_stride + conmsgbuf_end_field);

			trace_line l;
			l.str("conmsgbuf [2]->[4]: ");
			l.dec(std::size(conmsgbuf_sites));
			l.str(" array sites, ");
			l.dec(rewritten);
			l.str("/");
			l.dec(std::size(conmsgbuf_con_rel));
			l.str(" con-relative, end marker ");
			l.str(marker ? "moved" : "FAILED");
			trace_write(l);
		}

		// ---- uiInfoArray [2] -> [4] ---------------------------------------------
		// PS4 ui_main.cpp: uiInfoArray[4], element 0x1B68 (same size on PC). The PC
		// has [2]; client 2 wrote its menu state into foreign memory, which crashed
		// the game at exit. After the move, UI_InitUIInfos' `cmp ebp,2` loop runs
		// to 4 like PS4. widen_client_shutdown_loops depends on this move.
		constexpr entcoll_site uiinfo_sites[] = {
			{0x022304A1, 3, 7, true , 0x0000},
			{0x0223085C, 3, 7, true , 0x184C},
			{0x0223088C, 3, 7, true , 0x002C},
			{0x02230BC9, 3, 7, true , 0x0000}, // UI_UIContext_GetInfo
			{0x02231087, 3, 7, true , 0x001C}, // UI_InitUIInfos
			{0x022312C9, 3, 7, true , 0x184C},
			{0x0223157F, 3, 7, true , 0x0000},
			{0x0223268B, 3, 7, true , 0x0030},
			{0x022328F5, 3, 7, true , 0x0000},
			{0x022329A0, 3, 7, true , 0x0000},
		};
		constexpr perclient_array batch17[] = {
			{"uiinfo", 0x1795D270, 0x1B68, uiinfo_sites, std::size(uiinfo_sites), 0, {}},
		};
		size_t batch17_new[std::size(batch17)] = {};

		// ---- UI3D texture windows per local client [2] -> [4] -------------------
		// The PC saves 6 UI3D windows (0x438 bytes) per local client in
		// R_UI3D_SetupBackendData (0x01D100D0) and restores them next frame in
		// R_UI3D_PerframeInit (0x01D0FF20). With [2], client 2 overwrote the data
		// behind the array (player 3's white HUD panels in MP). PS4 has a single
		// g_ui3d_windows. Three other hits in the range are loop end markers and stay.
		constexpr entcoll_site ui3d_windows_sites[] = {
			{0x01D0FD15, 3, 7, true , 0x0000}, // lea rbx, [saved]         init, slot 0
			{0x01D0FDB0, 3, 7, true , 0x0438}, // lea rcx, [saved + 0x438] init, slot 1
			{0x01D0FF2F, 3, 7, true , 0x0000}, // lea rdx, [saved]         R_UI3D_PerframeInit
			{0x01D10373, 3, 7, true , 0x0000}, // lea rcx, [saved]         R_UI3D_SetupBackendData
		};
		constexpr perclient_array batch18[] = {
			{"ui3d_windows", 0x10B2F2F0, 0x438, ui3d_windows_sites, std::size(ui3d_windows_sites), 0, {}},
		};
		size_t batch18_new[std::size(batch18)] = {};

		void relocate_batch18()
		{
			if (!batch18_new[0])
			{
				batch18_new[0] = relocate_perclient(batch18[0]);
			}
		}

		void relocate_batch17()
		{
			if (batch17_new[0])
			{
				return;
			}
			const auto b = base();
			auto* bound = reinterpret_cast<uint8_t*>(b + 0x02231110);
			constexpr uint8_t bound_old[] = {0x83, 0xFD, 0x02};   // cmp ebp, 2
			if (!readable(bound, sizeof(bound_old)) || std::memcmp(bound, bound_old, sizeof(bound_old)) != 0)
			{
				note("[splitscreen] uiinfo: init loop bound differs - nothing moved\n");
				return;
			}
			batch17_new[0] = relocate_perclient(batch17[0]);
			if (!batch17_new[0])
			{
				return;
			}
			const uint8_t four = 0x04;
			const bool widened = write_bytes(bound + 2, &four, 1);
			trace_line l;
			l.str("uiinfo [2]->[4]: ");
			l.dec(std::size(uiinfo_sites));
			l.str(" sites, init loop ");
			l.str(widened ? "-> 4" : "FAILED");
			trace_write(l);
		}

		// ---- Light queue: records [2][1024] + counters [2] -> [4] ----------------
		// Per-client ring of light records (1024 x 0x28, stride 0xA000) with
		// read/write counters in two int[2] arrays A and B, 8 bytes apart. Client
		// 2's records overlay the counters, so on Revelations the consumer read a
		// garbage pointer. New block: records[4], A[4] at +0x28000, B[4] at +0x28010.
		// The reset's two `mov qword [rip+d],rax` become `movups [rip+d],xmm0` (same
		// length, xmm0 already zero) so all four clients' counters clear.
		constexpr uint32_t lightq_base = 0x10598670;
		constexpr uint32_t lightq_stride = 0xA000;
		constexpr uint32_t lightq_a = 0x105AC670;
		constexpr uint32_t lightq_b = 0x105AC678;
		constexpr size_t lightq_records_new = 4 * lightq_stride;   // 0x28000

		constexpr entcoll_site lightq_sites[] = {
			{0x000B15FE, 3, 7, true , 0x0000}, // restore: memset
			{0x000B1665, 3, 7, true , 0x0000}, // restore
			{0x000B1E7B, 3, 7, false, 0x0000}, // save
			{0x000B1EDE, 4, 9, false, 0x0020},
			{0x000B1EF4, 4, 8, false, 0x0000},
			{0x000B1F73, 4, 8, false, 0x0008},
			{0x000B1F8B, 4, 8, false, 0x0008},
			{0x000B1F93, 4, 8, false, 0x0010},
			{0x000B2008, 4, 8, false, 0x001C},
			{0x00436EF7, 4, 8, false, 0x0008}, // consumer
			{0x00436EFF, 4, 8, false, 0x0020},
			{0x00436F07, 4, 8, false, 0x0010},
			{0x00436F0F, 4, 8, false, 0x0000},
			{0x00436F1C, 4, 8, false, 0x0018},
			{0x01CEDEFE, 4, 8, false, 0x0010}, // producer
			{0x01CEDF09, 4, 8, false, 0x0008},
			{0x01CEDF1F, 5, 9, false, 0x0020},
			{0x01CEDF2E, 4, 8, false, 0x001C},
			{0x01CEDF3A, 4, 8, false, 0x0018},
			{0x01CEDF52, 5, 9, false, 0x0000},
			{0x01CEDF5B, 5, 10, false, 0x0020},
			{0x01CEDF6B, 4, 8, false, 0x0000},
			{0x01CEE6EE, 3, 7, false, 0x0000},
		};
		constexpr entcoll_site lightq_a_sites[] = {
			{0x000B174B, 4, 8, false, 0},
			{0x000B1DB0, 4, 8, false, 0},
			{0x00436E47, 4, 8, false, 0},
			{0x0043A80D, 4, 8, false, 0},
			{0x01CEDECE, 4, 8, false, 0},
			{0x01CEE158, 3, 7, true , 0}, // reset
			{0x01CEE6C5, 4, 8, false, 0},
		};
		constexpr entcoll_site lightq_b_sites[] = {
			{0x000B1753, 4, 8, false, 0},
			{0x000B1DB8, 3, 7, false, 0},
			{0x00436E3C, 4, 8, false, 0},
			{0x0043A7DE, 4, 8, false, 0},
			{0x0043A805, 4, 8, false, 0},
			{0x01CEDEE5, 4, 8, false, 0},
			{0x01CEE15F, 3, 7, true , 0}, // reset
			{0x01CEE6DA, 3, 7, false, 0},
		};
		bool lightq_relocated = false;

		void relocate_lightq()
		{
			if (lightq_relocated)
			{
				return;
			}
			const auto b = base();
			struct fixed_bytes { uint32_t rva; uint8_t len; uint8_t old_bytes[6]; uint8_t new_bytes[6]; };
			constexpr fixed_bytes extras[] = {
				{0x01CEE155, 3, {0x0F, 0x57, 0xC0}, {0x0F, 0x57, 0xC0}},   // xorps xmm0,xmm0 - must be there
				{0x01CEE158, 3, {0x48, 0x89, 0x05}, {0x0F, 0x11, 0x05}},   // mov qword -> movups (A)
				{0x01CEE15F, 3, {0x48, 0x89, 0x05}, {0x0F, 0x11, 0x05}},   // mov qword -> movups (B)
				{0x000B15F8, 6, {0x41, 0xB8, 0x00, 0x40, 0x01, 0x00}, {0x41, 0xB8, 0x00, 0x80, 0x02, 0x00}}, // restore memset
				{0x000B176E, 4, {0x41, 0x83, 0xFF, 0x02}, {0x41, 0x83, 0xFF, 0x03}},   // restore loop
				{0x000B2060, 4, {0x41, 0x83, 0xFD, 0x02}, {0x41, 0x83, 0xFD, 0x03}},   // save loop
			};
			for (const auto& e : extras)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + e.rva);
				if (!readable(at, e.len) || std::memcmp(at, e.old_bytes, e.len) != 0)
				{
					note("[splitscreen] lightq: bytes at 0x%08X differ - nothing moved\n", e.rva);
					return;
				}
			}

			auto* fresh = static_cast<uint8_t*>(allocate_near_module(lightq_records_new + 0x20));
			if (!fresh)
			{
				return;
			}
			std::memset(fresh, 0, lightq_records_new + 0x20);
			std::memcpy(fresh, reinterpret_cast<const void*>(b + lightq_base), 2 * lightq_stride);
			std::memcpy(fresh + lightq_records_new, reinterpret_cast<const void*>(b + lightq_a), 8);
			std::memcpy(fresh + lightq_records_new + 0x10, reinterpret_cast<const void*>(b + lightq_b), 8);

			int32_t saved_r[std::size(lightq_sites)] = {};
			int32_t saved_a[std::size(lightq_a_sites)] = {};
			int32_t saved_b[std::size(lightq_b_sites)] = {};
			const auto restore = [&](const entcoll_site* sites, const size_t n, const int32_t* saved)
			{
				for (size_t i = 0; i < n; ++i)
				{
					write_bytes(reinterpret_cast<void*>(b + sites[i].rva + sites[i].disp_off), &saved[i], sizeof(int32_t));
				}
			};
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);
			if (!rewrite_entcoll(lightq_sites, std::size(lightq_sites), lightq_base, fresh_abs, saved_r))
			{
				return;
			}
			if (!rewrite_entcoll(lightq_a_sites, std::size(lightq_a_sites), lightq_a, fresh_abs + lightq_records_new, saved_a))
			{
				restore(lightq_sites, std::size(lightq_sites), saved_r);
				return;
			}
			if (!rewrite_entcoll(lightq_b_sites, std::size(lightq_b_sites), lightq_b, fresh_abs + lightq_records_new + 0x10, saved_b))
			{
				restore(lightq_a_sites, std::size(lightq_a_sites), saved_a);
				restore(lightq_sites, std::size(lightq_sites), saved_r);
				return;
			}
			uint32_t extras_done = 0;
			for (const auto& e : extras)
			{
				if (write_bytes(reinterpret_cast<void*>(b + e.rva), e.new_bytes, e.len))
				{
					++extras_done;
				}
			}
			lightq_relocated = true;
			trace_line l;
			l.str("lightq [2]->[4]: ");
			l.dec(std::size(lightq_sites) + std::size(lightq_a_sites) + std::size(lightq_b_sites));
			l.str(" sites, ");
			l.dec(extras_done);
			l.str("/");
			l.dec(std::size(extras));
			l.str(" reset/memset/loop patches");
			trace_write(l);
		}

		// ---- Umbra occlusion culling: per-client state [2] -> [4] ---------------
		// Symptom: pane 3 drew no world geometry. The heap object sUmbra holds
		// per-client arrays (PS4: UmbraQueryParameters[4], R_Umbra_SelectTome
		// 0x945980). The PC keeps [2], and client 2 aliases the next fields:
		//   +0x12DC0C  params[2] x 0x14
		//   +0x12DC48  tome trigger[2] x 4   (-1 = none)
		//   +0x12DC50  persistent tome trigger[2] x 4
		// Fix: grow the allocation 0x470210 -> 0x470300 and put [4] copies at the
		// new tail; params at a multiple of 0x14, since the distance-scale setter
		// indexes (lc + 0xF167) * 0x14. Only possible while sUmbra is still NULL.
		struct umbra_disp
		{
			uint32_t rva;
			uint8_t off;       // where the imm32/disp32 sits in the instruction
			uint32_t old_value;
			uint32_t new_value;
		};

		constexpr uint32_t umbra_params_new = 0x470220;
		constexpr uint32_t umbra_trig_new = 0x470270;
		constexpr uint32_t umbra_ptrig_new = 0x470280;
		constexpr uint32_t umbra_params_delta = umbra_params_new - 0x12DC0C;
		static_assert(umbra_params_new % 0x14 == 0, "the distance-scale setter indexes params as (lc + bias) * 0x14");

		constexpr umbra_disp umbra_disps[] = {
			// params (field offsets 0x00..0x10 of each 0x14 entry)
			{0x01C8D417, 5, 0x12DC0C, 0x12DC0C + umbra_params_delta},
			{0x01C8D420, 5, 0x12DC10, 0x12DC10 + umbra_params_delta},
			{0x01C8D42F, 5, 0x12DC14, 0x12DC14 + umbra_params_delta},
			{0x01C8D43E, 5, 0x12DC18, 0x12DC18 + umbra_params_delta},
			{0x01C8D44D, 5, 0x12DC1C, 0x12DC1C + umbra_params_delta},
			{0x01C8DDDE, 5, 0x12DC10, 0x12DC10 + umbra_params_delta},   // SetAccurateOcclusionThreshold
			{0x01C8E045, 5, 0x12DC14, 0x12DC14 + umbra_params_delta},   // SetMinimumContributionThreshold
			{0x01C8EC55, 4, 0x12DC10, 0x12DC10 + umbra_params_delta},
			{0x01C8EFF6, 3, 0x12DC0C, umbra_params_new},                // defaults init
			{0x01C8F084, 3, 0x12DC0C, umbra_params_new},                // UmbraLevel settings x5
			{0x01C8F0D0, 3, 0x12DC0C, umbra_params_new},
			{0x01C8F11C, 3, 0x12DC0C, umbra_params_new},
			{0x01C8F16C, 3, 0x12DC0C, umbra_params_new},
			{0x01C8F1B3, 3, 0x12DC0C, umbra_params_new},
			{0x01C8E013, 2, 0xF167, umbra_params_new / 0x14},           // SetDistanceScale index bias
			// tome trigger
			{0x01C8C81D, 2, 0x12DC48, umbra_trig_new},
			{0x01C8CAFA, 3, 0x12DC48, umbra_trig_new},
			{0x01C8CBFE, 3, 0x12DC48, umbra_trig_new},
			{0x01C8CC72, 3, 0x12DC48, umbra_trig_new},
			{0x01C8DB20, 4, 0x12DC48, umbra_trig_new},
			{0x01C8DB68, 4, 0x12DC48, umbra_trig_new},
			{0x01C8F3FC, 3, 0x12DC48, umbra_trig_new},
			// persistent tome trigger
			{0x01C8CB3E, 3, 0x12DC50, umbra_ptrig_new},
			{0x01C8CBB2, 3, 0x12DC50, umbra_ptrig_new},
			{0x01C8DA64, 4, 0x12DC50, umbra_ptrig_new},
			{0x01C8F41F, 3, 0x12DC50, umbra_ptrig_new},
			// the allocation and its memset
			{0x01C8D345, 1, 0x470210, 0x470300},
			{0x01C8D35E, 2, 0x470210, 0x470300},
		};

		// init-loop end bounds: `lea reg, [base + disp8]`, disp8 at +3
		struct umbra_bound
		{
			uint32_t rva;
			uint8_t old_value;
			uint8_t new_value;
		};

		constexpr umbra_bound umbra_bounds[] = {
			{0x01C8EFFD, 0x28, 0x50},   // defaults: 2 x 0x14 -> 4 x 0x14
			{0x01C8F08B, 0x28, 0x50},
			{0x01C8F0D7, 0x28, 0x50},
			{0x01C8F123, 0x28, 0x50},
			{0x01C8F173, 0x28, 0x50},
			{0x01C8F1BA, 0x28, 0x50},
			{0x01C8F403, 0x08, 0x10},   // tome triggers: 2 x 4 -> 4 x 4
			{0x01C8F426, 0x08, 0x10},
		};

		bool umbra_grown = false;

		bool grow_umbra_client_arrays()
		{
			if (umbra_grown)
			{
				return true;
			}
			const auto b = base();
			const auto* object = reinterpret_cast<const uint64_t*>(b + 0x0AE15BF8);
			if (!readable(object, sizeof(*object)) || *object != 0)
			{
				note("[splitscreen] umbra: object already allocated - not grown\n");
				return false;
			}
			for (const auto& s : umbra_disps)
			{
				uint32_t have = 0;
				const auto* at = reinterpret_cast<const uint8_t*>(b + s.rva + s.off);
				if (!readable(at, sizeof(have)))
				{
					return false;
				}
				std::memcpy(&have, at, sizeof(have));
				if (have != s.old_value)
				{
					note("[splitscreen] umbra: 0x%08X holds 0x%X - nothing written\n", s.rva, have);
					return false;
				}
			}
			for (const auto& u : umbra_bounds)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + u.rva + 3);
				if (!readable(at, 1) || *at != u.old_value)
				{
					note("[splitscreen] umbra: bound 0x%08X differs - nothing written\n", u.rva);
					return false;
				}
			}

			size_t done_disps = 0;
			size_t done_bounds = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done_disps; ++i)
				{
					write_bytes(reinterpret_cast<void*>(b + umbra_disps[i].rva + umbra_disps[i].off),
					            &umbra_disps[i].old_value, sizeof(uint32_t));
				}
				for (size_t i = 0; i < done_bounds; ++i)
				{
					write_bytes(reinterpret_cast<void*>(b + umbra_bounds[i].rva + 3),
					            &umbra_bounds[i].old_value, 1);
				}
			};
			for (const auto& s : umbra_disps)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + s.rva + s.off), &s.new_value, sizeof(uint32_t)))
				{
					rollback();
					return false;
				}
				++done_disps;
			}
			for (const auto& u : umbra_bounds)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + u.rva + 3), &u.new_value, 1))
				{
					rollback();
					return false;
				}
				++done_bounds;
			}
			umbra_grown = true;
			return true;
		}

		// LiveStats_ResetCache: memset length `mov r8d, 0x8808` -> 0x11010,
		// only when its lea already points at the moved array.
		bool widen_statscache_reset(const size_t cache_new)
		{
			const auto b = base();
			auto* imm = reinterpret_cast<uint8_t*>(b + 0x01E94E98);
			constexpr uint8_t imm_old[] = {0x41, 0xB8, 0x08, 0x88, 0x00, 0x00};
			constexpr uint8_t imm_new[] = {0x41, 0xB8, 0x10, 0x10, 0x01, 0x00};
			const auto* lea = reinterpret_cast<const uint8_t*>(b + 0x01E94E8F);
			int32_t lea_disp = 0;
			std::memcpy(&lea_disp, lea + 3, sizeof(lea_disp));
			if (!cache_new || b + 0x01E94E96 + lea_disp != cache_new
			    || !readable(imm, sizeof(imm_old)) || std::memcmp(imm, imm_old, sizeof(imm_old)) != 0)
			{
				note("[splitscreen] statscache reset: bytes differ - not widened\n");
				return false;
			}
			return write_bytes(imm, imm_new, sizeof(imm_new));
		}

		void relocate_batch14()
		{
			for (size_t i = 0; i < std::size(batch14); ++i)
			{
				if (batch14_new[i])
				{
					continue;
				}
				batch14_new[i] = relocate_perclient(batch14[i]);
				if (batch14_new[i])
				{
					widen_statscache_reset(batch14_new[i]);
				}
			}
		}

		void relocate_batch13()
		{
			for (size_t i = 0; i < std::size(batch13); ++i)
			{
				if (batch13_new[i])
				{
					continue;
				}
				batch13_new[i] = relocate_perclient(batch13[i]);
				if (batch13_new[i] && std::strcmp(batch13[i].name, "sunvol") == 0)
				{
					// Match the static initializer: zero except dword +0x2BB0 = -1.
					for (size_t lc = 2; lc < 4; ++lc)
					{
						const uint32_t none = 0xFFFFFFFF;
						std::memcpy(reinterpret_cast<uint8_t*>(batch13_new[i]) + lc * 0x2BB8 + 0x2BB0,
						            &none, sizeof(none));
					}
				}
			}
		}

		// ---- Lens flares: disabled for local clients >= 2 ------------------------
		// PS4 FxLensFlaresManager has eight per-client arrays of 4; on the PC they
		// are arrays of 2 inside one static object, so lc 2 hits the neighbouring
		// members. The object cannot grow (the fix would re-lay the whole class),
		// so for now clients >= 2 get no lens flares. Each entry point below takes
		// lc in edx and gets a cave: `cmp edx,2 / jl original`, else return
		// (SpawnInstance: -1, its own failure value).
		struct lc_gate
		{
			uint32_t rva;
			uint8_t prologue[9];
			uint8_t len;
			bool returns_minus_one;
		};
		constexpr lc_gate lensflare_gates[] = {
			{0x014BA810, {0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08}, 9, false}, // per-client pool setup
			{0x014BAC40, {0x40, 0x55, 0x56, 0x57, 0x41, 0x54}, 6, false}, // SetPersistentData
			{0x014BB9B0, {0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54}, 6, false}, // per-client update
			{0x014BBD40, {0x48, 0x89, 0x4C, 0x24, 0x08}, 5, true},        // SpawnInstance
			{0x014BC9E0, {0x48, 0x8B, 0xC4, 0x55, 0x53}, 5, false},       // per-view render
		};
		bool lensflare_gated = false;

		void gate_lensflares_for_extra_clients()
		{
			if (lensflare_gated)
			{
				return;
			}
			const auto b = base();
			for (const auto& g : lensflare_gates)
			{
				const auto* at = reinterpret_cast<const uint8_t*>(b + g.rva);
				if (!readable(at, g.len) || std::memcmp(at, g.prologue, g.len) != 0)
				{
					trace_line l;
					l.str("lensflare gate: 0x");
					l.hex(g.rva);
					l.str(" prologue not stock - no gate installed");
					trace_write(l);
					return;
				}
			}
			uint32_t installed = 0;
			for (const auto& g : lensflare_gates)
			{
				auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
				if (!cave)
				{
					break;
				}
				std::vector<uint8_t> c;
				c.insert(c.end(), {0x83, 0xFA, 0x02});       // cmp edx, 2
				c.insert(c.end(), {0x7C, 0x00});             // jl original (patched)
				const auto jl_at = c.size() - 1;
				if (g.returns_minus_one)
				{
					c.insert(c.end(), {0xB8, 0xFF, 0xFF, 0xFF, 0xFF}); // mov eax, -1
				}
				c.insert(c.end(), {0xC3});                   // ret
				c[jl_at] = static_cast<uint8_t>(c.size() - (jl_at + 1));
				c.insert(c.end(), g.prologue, g.prologue + g.len);
				c.insert(c.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00}); // jmp [rip+0]
				const uint64_t back = b + g.rva + g.len;
				const auto* back_bytes = reinterpret_cast<const uint8_t*>(&back);
				c.insert(c.end(), back_bytes, back_bytes + 8);
				if (!write_bytes(cave, c.data(), c.size()))
				{
					break;
				}
				uint8_t patch[9];
				std::memset(patch, 0x90, sizeof(patch));
				patch[0] = 0xE9;
				const auto rel = static_cast<int32_t>(
					reinterpret_cast<size_t>(cave) - (b + g.rva + 5));
				std::memcpy(patch + 1, &rel, sizeof(rel));
				if (!write_bytes(reinterpret_cast<uint8_t*>(b + g.rva), patch, g.len))
				{
					break;
				}
				++installed;
			}
			lensflare_gated = installed == std::size(lensflare_gates);
			trace_line l;
			l.str("lensflare gate: ");
			l.dec(installed);
			l.str(" of ");
			l.dec(std::size(lensflare_gates));
			l.str(" entry points gated for local clients >= 2");
			trace_write(l);
		}

		// ---- Quit hang: lens-flare manager destructor at process exit ----------
		// Quitting skips FX_ShutdownLensFlareSystem, so at exit the destructor's
		// PMem_Free("LensFlareManager") hits an already-freed block, Com_Error fires
		// and the crash handler recurses until the stack overflows. At exit this
		// only frees memory the OS reclaims anyway, so the exit thunk returns at
		// once. The level-end shutdown takes another path and is untouched.
		constexpr uint32_t lensflare_exit_thunk_rva = 0x02EF9840;
		constexpr uint8_t lensflare_exit_thunk_expected[] = {
			0x48, 0x8D, 0x0D, 0xC9, 0x53, 0x3B, 0x00,   // lea rcx, [FxLensFlaresManager]
			0xE9, 0x24, 0x24, 0x5C, 0xFE,               // jmp Shutdown
		};

		void skip_lensflare_exit_shutdown()
		{
			auto* at = reinterpret_cast<uint8_t*>(base() + lensflare_exit_thunk_rva);
			if (!readable(at, sizeof(lensflare_exit_thunk_expected))
				|| std::memcmp(at, lensflare_exit_thunk_expected, sizeof(lensflare_exit_thunk_expected)) != 0)
			{
				note("[splitscreen] lensflare exit thunk: bytes differ - left alone\n");
				return;
			}
			const uint8_t ret = 0xC3;
			if (write_bytes(at, &ret, 1))
			{
				trace_line l;
				l.str("lensflare exit thunk: returns at process exit (quit hang)");
				trace_write(l);
			}
		}

		// ---- Per-controller UI model roots 2..3 --------------------------------
		// Engine.GetModelForController(2) was nil, so controller 2's data-bound HUD
		// widgets (ammo, scores) had nothing to subscribe to. PS4 UI_Model_Init
		// (0xD68140) creates s_controllerModel[i] = AllocateNode(global,
		// "controller%d", true) for 4 controllers; the hidden PC init makes 2.
		// Slots 2/3 are unreferenced padding, so nothing moves. A cave at the entry
		// of Com_LocalClient_LastInput_Init (run once, right after UI_Model_Init)
		// creates the missing roots with UI_Model_CreatePersistentModelFromPath;
		// they must be persistent or UI_Shutdown frees them and leaves stale handles.
		// Known hazard, not fixed: setupArmBladeTarget / setupRocketLauncherTarget
		// just before are [2] per client (PS4: 4); lc 2 would overwrite these roots.
		constexpr uint32_t lastinput_init_rva = 0x020E32E0;       // Com_LocalClient_LastInput_Init
		constexpr uint8_t lastinput_init_expected[] = {
			0x48, 0x89, 0x5C, 0x24, 0x10,                         // mov [rsp+0x10], rbx
		};
		constexpr uint32_t ui_controller_model_getter_rva = 0x0200CEE0;
		constexpr uint8_t ui_controller_model_getter_expected[] = {
			0x48, 0x63, 0xC1,                                     // movsxd rax, ecx
			0x48, 0x8D, 0x0D, 0x52, 0xF1, 0x25, 0x14,             // lea rcx, [s_controllerModel]
			0x0F, 0xB7, 0x04, 0x41,                               // movzx eax, word [rcx+rax*2]
			0xC3,
		};
		constexpr uint32_t ui_global_model_getter_rva = 0x0200CD10;
		constexpr uint8_t ui_global_model_getter_expected[] = {
			0x0F, 0xB7, 0x05, 0x21, 0xF3, 0x25, 0x14,             // movzx eax, word [global model]
			0xC3,
		};
		constexpr uint32_t ui_create_persistent_rva = 0x0200C900;
		constexpr uint8_t ui_create_persistent_expected[] = {
			0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, // prologue
			0x57, 0x48, 0x83, 0xEC, 0x70,
		};
		constexpr uint32_t ui_create_persistent_alloc_rva = 0x0200C965;
		constexpr uint8_t ui_create_persistent_alloc_expected[] = {
			0x48, 0x8D, 0x54, 0x24, 0x20,                         // lea rdx, [rsp+0x20]  (key)
			0x41, 0xB0, 0x01,                                     // mov r8b, 1           (persistent)
			0x0F, 0xB7, 0xCF,                                     // movzx ecx, di        (parent)
			0xE8, 0xCB, 0xFC, 0xFF, 0xFF,                         // call UI_Model_AllocateNode
		};
		constexpr uint32_t ui_global_model_rva = 0x1626C038;
		constexpr uint32_t ui_controller_model_rva = 0x1626C03C;  // uint16[2] on the PC
		bool controller_models_hooked = false;

		void create_extra_controller_models()
		{
			if (controller_models_hooked)
			{
				return;
			}
			const auto b = base();
			const struct
			{
				uint32_t rva;
				const uint8_t* bytes;
				size_t len;
			} checks[] = {
				{lastinput_init_rva, lastinput_init_expected, sizeof(lastinput_init_expected)},
				{ui_controller_model_getter_rva, ui_controller_model_getter_expected,
				 sizeof(ui_controller_model_getter_expected)},
				{ui_global_model_getter_rva, ui_global_model_getter_expected,
				 sizeof(ui_global_model_getter_expected)},
				{ui_create_persistent_rva, ui_create_persistent_expected,
				 sizeof(ui_create_persistent_expected)},
				{ui_create_persistent_alloc_rva, ui_create_persistent_alloc_expected,
				 sizeof(ui_create_persistent_alloc_expected)},
			};
			for (const auto& c : checks)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + c.rva);
				if (!readable(p, c.len))
				{
					note("[splitscreen] controller models: 0x%08X not readable - not hooked\n", c.rva);
					return;
				}
				if (std::memcmp(p, c.bytes, c.len) == 0)
				{
					continue;
				}
				// BOIII 1.1.0.1445 hooks UI_CreatePersistent with a 5-byte `jmp rel32`.
				// Accept that (the cave only calls it); every later byte must match.
				const bool client_hooked_entry = c.rva == ui_create_persistent_rva && c.len > 5 && p[0] == 0xE9
					&& std::memcmp(p + 5, c.bytes + 5, c.len - 5) == 0;
				if (!client_hooked_entry)
				{
					note("[splitscreen] controller models: bytes differ at 0x%08X - not hooked\n", c.rva);
					return;
				}
			}
			const auto* slots = reinterpret_cast<const uint16_t*>(b + ui_controller_model_rva);
			if (!readable(slots, 8) || slots[2] != 0 || slots[3] != 0)
			{
				note("[splitscreen] controller models: slots 2/3 not zero - not hooked\n");
				return;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return;
			}
			const auto put64 = [](std::vector<uint8_t>& v, uint64_t x)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(&x);
				v.insert(v.end(), p, p + 8);
			};
			std::vector<uint8_t> c;
			c.insert(c.end(), {0x48, 0x83, 0xEC, 0x28});                 // sub rsp, 0x28
			size_t name_fixups[2] = {};
			for (int slot = 2; slot <= 3; ++slot)
			{
				const uint64_t slot_va = b + ui_controller_model_rva + slot * 2;
				c.insert(c.end(), {0x48, 0xB8}); put64(c, slot_va);        // mov rax, &slot
				c.insert(c.end(), {0x66, 0x83, 0x38, 0x00});               // cmp word [rax], 0
				const size_t jne_at = c.size();
				c.insert(c.end(), {0x75, 0x00});                           // jne next
				c.insert(c.end(), {0x48, 0xB8}); put64(c, b + ui_global_model_rva); // mov rax, &global
				c.insert(c.end(), {0x0F, 0xB7, 0x08});                     // movzx ecx, word [rax]
				c.insert(c.end(), {0x85, 0xC9});                           // test ecx, ecx
				const size_t jz_at = c.size();
				c.insert(c.end(), {0x74, 0x00});                           // jz next
				c.insert(c.end(), {0x48, 0xBA});                           // mov rdx, name
				name_fixups[slot - 2] = c.size();
				put64(c, 0);
				c.insert(c.end(), {0x48, 0xB8}); put64(c, b + ui_create_persistent_rva); // mov rax, create
				c.insert(c.end(), {0xFF, 0xD0});                           // call rax
				c.insert(c.end(), {0x48, 0xB9}); put64(c, slot_va);        // mov rcx, &slot
				c.insert(c.end(), {0x66, 0x89, 0x01});                     // mov word [rcx], ax
				const size_t next = c.size();
				c[jne_at + 1] = static_cast<uint8_t>(next - (jne_at + 2));
				c[jz_at + 1] = static_cast<uint8_t>(next - (jz_at + 2));
			}
			c.insert(c.end(), {0x48, 0x83, 0xC4, 0x28});                 // add rsp, 0x28
			c.insert(c.end(), lastinput_init_expected,
			         lastinput_init_expected + sizeof(lastinput_init_expected)); // replayed
			c.insert(c.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});     // jmp [rip+0]
			put64(c, b + lastinput_init_rva + sizeof(lastinput_init_expected));
			static const char names[] = "controller2\0controller3";
			for (int i = 0; i < 2; ++i)
			{
				const uint64_t at = reinterpret_cast<uint64_t>(cave) + c.size();
				std::memcpy(c.data() + name_fixups[i], &at, sizeof(at));
				const char* s = names + i * 12;
				c.insert(c.end(), s, s + std::strlen(s) + 1);
			}
			if (c.size() > 0x100 || !write_bytes(cave, c.data(), c.size()))
			{
				return;
			}

			uint8_t patch[sizeof(lastinput_init_expected)];
			patch[0] = 0xE9;
			const auto rel = static_cast<int32_t>(
				reinterpret_cast<size_t>(cave) - (b + lastinput_init_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(reinterpret_cast<uint8_t*>(b + lastinput_init_rva), patch, sizeof(patch)))
			{
				return;
			}
			controller_models_hooked = true;
			trace_line l;
			l.str("controller models: roots 2/3 created at Com_LocalClient_LastInput_Init");
			trace_write(l);
		}


		// ---- Gamepad button models for controllers 2..3 -------------------------
		// Stock CoDMenu.lua joins an unused controller through its "ButtonBits.*"
		// models; controller 2 had none, so its A press never reached
		// LobbyAddLocalClient. PS4 CL_InitGamepadModels (0x3FF160) loops lc 0..3;
		// the PC stops at 2. Widened only when every array the loop touches exists
		// for lc 2/3: s_rightStickModels and s_gamepadButtons moved by batch2
		// (widening without that move crashed at launch), the model roots, and the
		// seat records.
		constexpr uint32_t gamepad_models_bound_rva = 0x01340339;
		constexpr uint8_t gamepad_models_bound_expected[] = {
			0x83, 0xFE, 0x02,                                     // cmp esi, 2
			0x0F, 0x8C, 0xBE, 0xFE, 0xFF, 0xFF,                   // jl loop head
		};

		bool gamepad_models_widened = false;

		void widen_gamepad_button_models()
		{
			if (!batch2_new[1] || !batch2_new[2] || !signin_relocated || !controller_models_hooked)
			{
				trace_line l;
				l.str("gamepad button models: stock, missing rightstick=");
				l.dec(batch2_new[1] ? 1 : 0);
				l.str(" buttons=");
				l.dec(batch2_new[2] ? 1 : 0);
				l.str(" signin=");
				l.dec(signin_relocated ? 1 : 0);
				l.str(" roots=");
				l.dec(controller_models_hooked ? 1 : 0);
				trace_write(l);
				return;
			}
			auto* p = reinterpret_cast<uint8_t*>(base() + gamepad_models_bound_rva);
			if (!readable(p, sizeof(gamepad_models_bound_expected))
				|| std::memcmp(p, gamepad_models_bound_expected, sizeof(gamepad_models_bound_expected)) != 0)
			{
				trace_line l;
				l.str("gamepad button models: stock, bytes differ at 0x01340319");
				trace_write(l);
				return;
			}
			// 4: seat records, model roots and both model arrays exist for lc 0..3.
			const uint8_t four = 0x04;
			if (write_bytes(p + 2, &four, 1))
			{
				gamepad_models_widened = true;
				trace_line l;
				l.str("gamepad button models: CL_InitGamepadModels bound 2 -> 4");
				trace_write(l);
			}
		}

		// ---- lobby_maxLocalPlayers: range max 2 -> 4 -----------------------------
		// The stock Lua join needs GetLobbyLocalClientCount < lobby_maxLocalPlayers,
		// but the PC registers the dvar with max 2 (PS4 LobbyConfig_Init 0xCC0437:
		// 1..4). Only that Lua reads the dvar. Default stays 2.
		constexpr uint32_t lobby_max_local_reg_rva = 0x01EDBF02;
		constexpr uint8_t lobby_max_local_reg_expected[] = {
			0xB9, 0x69, 0xF0, 0xD2, 0x44,                         // mov ecx, hash
			0x89, 0x5C, 0x24, 0x28,                               // mov [rsp+0x28], ebx  flags
			0x48, 0x89, 0x05, 0x2E, 0x25, 0x7F, 0x13,             // mov [rip+..], rax
			0xC7, 0x44, 0x24, 0x20, 0x02, 0x00, 0x00, 0x00,       // mov dword [rsp+0x20], 2  max
		};
		constexpr size_t lobby_max_local_max_off = 20;

		void widen_lobby_max_local_players()
		{
			if (!gamepad_models_widened)
			{
				return;   // no stock join for controller 2 - keep the stock range
			}
			auto* p = reinterpret_cast<uint8_t*>(base() + lobby_max_local_reg_rva);
			trace_line l;
			if (!readable(p, sizeof(lobby_max_local_reg_expected))
				|| std::memcmp(p, lobby_max_local_reg_expected, sizeof(lobby_max_local_reg_expected)) != 0)
			{
				l.str("lobby_maxLocalPlayers: stock, bytes differ at 0x01EE8872");
				trace_write(l);
				return;
			}
			const uint8_t four = 0x04;   // PS4's own maximum
			if (write_bytes(p + lobby_max_local_max_off, &four, 1))
			{
				l.str("lobby_maxLocalPlayers: range max 2 -> 4");
				trace_write(l);
			}
		}

		// ---- Sun shadow: bound each view's slot ----------------------------------
		// RT 5 (sun shadow depth) and RT 9 (its colour twin) have 3 slices per view
		// slot. The slot is min(splitscreen player count - 1, localClientNum); with
		// the count at 3, player 3 got a slot with no slices and crashed in d3d11
		// OMSetRenderTargets. The cave caps the slot at slices / partitions - 1,
		// read from the verified instructions. 15 straight-line bytes become
		// `jmp cave` + NOPs.
		constexpr uint32_t sun_slot_site_rva = 0x01D0D920;
		constexpr uint8_t sun_slot_site_expected[] = {
			0x41, 0x8B, 0x85, 0x98, 0x03, 0x00, 0x00,   // mov eax, [r13+0x398]
			0x3B, 0xC8,                                 // cmp ecx, eax
			0x0F, 0x4D, 0xC8,                           // cmovge ecx, eax
			0x89, 0x4D, 0x14,                           // mov [rbp+0x14], ecx
		};
		constexpr uint32_t sun_slices_rva = 0x01CD12AD;       // mov r8d, <slices>
		constexpr uint32_t sun_partitions_rva = 0x01C6FFFD;   // cmp ebx, <partitions>
		bool sun_slot_clamped = false;

		// ---- Sun shadow: 12 slices, one slot per view -----------------------------
		// With 2 slots, players 2-4 shared slot 1 and overwrote each other's
		// shadows. PS4 renders views one after another into the same slices; the
		// PC does not, so RT 5 (depth) and RT 9 (colour) grow from 6 to 12 slices.
		// - Descriptors: the two slice stores become calls to caves that store 12
		//   (the `mov r8d,6` stays; it is also RT 6's id).
		// - Depth arrays are sized from the slice count and need nothing else.
		// - Colour views live in 8 inline slots (index clamped to 7). The creation
		//   check admits 2..12, the loop stops at 6 so the inline slots stay stock,
		//   and slices 6..11 get sidecar views (maintain_sun_trans_views) that the
		//   setter and the clear pick; the clear has no clamp of its own.
		// Applied before R_Init, which builds the descriptors. History: LOG.md, 12 slices
		constexpr uint32_t sun_desc_rt5_rva = 0x01CD1357;
		constexpr uint8_t sun_desc_rt5_stock[] = {0x4C, 0x89, 0x85, 0x04, 0x0D, 0x00, 0x00};
		constexpr uint32_t sun_desc_rt9_rva = 0x01CD140A;
		constexpr uint8_t sun_desc_rt9_stock[] = {0x44, 0x89, 0x85, 0x5C, 0x0D, 0x00, 0x00};
		constexpr uint32_t sun_view_check_rva = 0x01CD5449;                           // cmp ax,5
		constexpr uint8_t sun_view_check_stock[] = {0x66, 0x83, 0xF8, 0x05};
		constexpr uint32_t sun_view_loop_rva = 0x01CD5494;                            // movzx eax,[rsi+0xA86]
		constexpr uint8_t sun_view_loop_stock[] = {0x0F, 0xB7, 0x86, 0x86, 0x0A, 0x00, 0x00};
		constexpr uint32_t sun_setter_rva = 0x01CF550D;                               // colour index clamp
		constexpr uint8_t sun_setter_stock[] = {
			0xB9, 0x07, 0x00, 0x00, 0x00, 0x3B, 0xD9, 0x44, 0x8B, 0xCB, 0x0F, 0xB7, 0xD6, 0x44, 0x0F,
			0x4D, 0xC9, 0x33, 0xC9, 0x45, 0x85, 0xC9, 0x44, 0x0F, 0x4E, 0xC9, 0x4E, 0x8B, 0x04, 0xC8,
		};
		constexpr uint32_t sun_clear_rva = 0x01CF367D;         // colour clear of the current slice
		constexpr uint8_t sun_clear_stock[] = {
			0x8B, 0x93, 0xC8, 0x80, 0x00, 0x00,   // mov edx,[rbx+0x80C8]   current slice
			0x48, 0x8B, 0x07,                     // mov rax,[rdi]
			0x4C, 0x8B, 0xC6,                     // mov r8,rsi
			0x48, 0x8B, 0x54, 0xD5, 0x00,         // mov rdx,[rbp+rdx*8]    inline[slice]
			0x48, 0x8B, 0xCF,                     // mov rcx,rdi
		};                                        // then call [rax+0x190] (ClearRenderTargetView)
		constexpr uint32_t rt_records_ptr_rva = 0x0FBBE758;   // GfxRenderTarget records, stride 0xAE0
		constexpr uint32_t rt_record_stride = 0xAE0;
		constexpr uint32_t sun_trans_rt = 9;
		constexpr uint32_t sun_slices_wanted = 12;
		bool sun_slices_grown = false;
		const char* sun_grow_result = "sun shadow 12 slices: not attempted";
		ID3D11RenderTargetView* sun_trans_extra[6] = {};      // RT 9 slices 6..11 (read by the caves)
		ID3D11RenderTargetView* sun_trans_retired[6] = {};    // released one tick later
		void* sun_trans_seen_v0 = nullptr;

		bool grow_sun_shadow_slices()
		{
			if (sun_slices_grown)
			{
				return true;
			}
			const auto b = base();
			const auto matches = [&](uint32_t rva, const uint8_t* stock, size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, stock, n) == 0;
			};
			if (!matches(sun_desc_rt5_rva, sun_desc_rt5_stock, sizeof(sun_desc_rt5_stock))
				|| !matches(sun_desc_rt9_rva, sun_desc_rt9_stock, sizeof(sun_desc_rt9_stock))
				|| !matches(sun_view_check_rva, sun_view_check_stock, sizeof(sun_view_check_stock))
				|| !matches(sun_view_loop_rva, sun_view_loop_stock, sizeof(sun_view_loop_stock))
				|| !matches(sun_setter_rva, sun_setter_stock, sizeof(sun_setter_stock))
				|| !matches(sun_clear_rva, sun_clear_stock, sizeof(sun_clear_stock)))
			{
				sun_grow_result = "sun shadow 12 slices: NOT applied - bytes differ";
				return false;
			}
			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x180));
			if (!cave)
			{
				sun_grow_result = "sun shadow 12 slices: NOT applied - allocation failed";
				return false;
			}
			const uint8_t n = static_cast<uint8_t>(sun_slices_wanted);
			// +0x00: mov qword [rbp+0xD04], 12 ; ret      (RT 5 slices, colorFormat 0)
			const uint8_t c_rt5[] = {0x48, 0xC7, 0x85, 0x04, 0x0D, 0x00, 0x00, n, 0x00, 0x00, 0x00, 0xC3};
			// +0x10: mov dword [rbp+0xD5C], 12 ; ret      (RT 9 slices)
			const uint8_t c_rt9[] = {0xC7, 0x85, 0x5C, 0x0D, 0x00, 0x00, n, 0x00, 0x00, 0x00, 0xC3};
			// +0x20: movzx eax,[rsi+0xA86] ; cmp eax,6 ; jbe +5 ; mov eax,6 ; ret
			const uint8_t c_loop[] = {0x0F, 0xB7, 0x86, 0x86, 0x0A, 0x00, 0x00, 0x83, 0xF8, 0x06, 0x76, 0x05,
			                          0xB8, 0x06, 0x00, 0x00, 0x00, 0xC3};
			// +0x40: the setter's colour pick.
			std::vector<uint8_t> s;
			s.insert(s.end(), {0x83, 0xFB, 0x06, 0x7C, 0x00});             // cmp ebx,6 ; jl orig
			const size_t j1 = s.size() - 1;
			s.insert(s.end(), {0x83, 0xFB, 0x0C, 0x73, 0x00});             // cmp ebx,12 ; jae orig
			const size_t j2 = s.size() - 1;
			s.insert(s.end(), {0x66, 0x83, 0xFE, static_cast<uint8_t>(sun_trans_rt), 0x75, 0x00});   // cmp si,9 ; jne orig
			const size_t j3 = s.size() - 1;
			s.insert(s.end(), {0x49, 0xB8});                                 // mov r8, &sun_trans_extra
			{
				const auto a = reinterpret_cast<uint64_t>(&sun_trans_extra[0]);
				const auto* p = reinterpret_cast<const uint8_t*>(&a);
				s.insert(s.end(), p, p + 8);
			}
			s.insert(s.end(), {0x4D, 0x8B, 0x44, 0xD8, 0xD0});             // mov r8,[r8+rbx*8-0x30]
			s.insert(s.end(), {0x4D, 0x85, 0xC0, 0x75, 0x00});             // test r8,r8 ; jnz done
			const size_t j4 = s.size() - 1;
			const size_t orig = s.size();
			s.insert(s.end(), sun_setter_stock, sun_setter_stock + sizeof(sun_setter_stock));
			const size_t done = s.size();
			s.insert(s.end(), {0x0F, 0xB7, 0xD6, 0x33, 0xC9});             // movzx edx,si ; xor ecx,ecx
			s.insert(s.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});       // jmp [rip+0]
			{
				const uint64_t back = b + sun_setter_rva + sizeof(sun_setter_stock);
				const auto* p = reinterpret_cast<const uint8_t*>(&back);
				s.insert(s.end(), p, p + 8);
			}
			s[j1] = static_cast<uint8_t>(orig - (j1 + 1));
			s[j2] = static_cast<uint8_t>(orig - (j2 + 1));
			s[j3] = static_cast<uint8_t>(orig - (j3 + 1));
			s[j4] = static_cast<uint8_t>(done - (j4 + 1));
			// +0x100: the clear's colour pick (rbp = view set, rbx = cmd state):
			// sidecar for RT 9 slices 6..11, otherwise inline[slice], clamped.
			std::vector<uint8_t> k;
			k.insert(k.end(), {0x8B, 0x93, 0xC8, 0x80, 0x00, 0x00});       // mov edx,[rbx+0x80C8]
			k.insert(k.end(), {0x83, 0xFA, 0x06, 0x7C, 0x00});             // cmp edx,6 ; jl orig
			const size_t k1 = k.size() - 1;
			k.insert(k.end(), {0x83, 0xFA, 0x0C, 0x73, 0x00});             // cmp edx,12 ; jae clamp
			const size_t k2 = k.size() - 1;
			k.insert(k.end(), {0x66, 0x83, 0xBB, 0xC0, 0x80, 0x00, 0x00, static_cast<uint8_t>(sun_trans_rt)});
			k.insert(k.end(), {0x75, 0x00});                                 // cmp word [rbx+0x80C0],9 ; jne clamp
			const size_t k3 = k.size() - 1;
			k.insert(k.end(), {0x48, 0xB8});                                 // mov rax, &sun_trans_extra
			{
				const auto a = reinterpret_cast<uint64_t>(&sun_trans_extra[0]);
				const auto* p = reinterpret_cast<const uint8_t*>(&a);
				k.insert(k.end(), p, p + 8);
			}
			k.insert(k.end(), {0x48, 0x8B, 0x54, 0xD0, 0xD0});             // mov rdx,[rax+rdx*8-0x30]
			k.insert(k.end(), {0x48, 0x85, 0xD2, 0x75, 0x00});             // test rdx,rdx ; jnz done
			const size_t k4 = k.size() - 1;
			const size_t kclamp = k.size();
			k.insert(k.end(), {0xBA, 0x05, 0x00, 0x00, 0x00});             // mov edx,5 (last inline view)
			const size_t korig = k.size();
			k.insert(k.end(), {0x48, 0x8B, 0x54, 0xD5, 0x00});             // mov rdx,[rbp+rdx*8]
			const size_t kdone = k.size();
			k.insert(k.end(), {0x48, 0x8B, 0x07, 0x4C, 0x8B, 0xC6, 0x48, 0x8B, 0xCF});   // rax, r8, rcx as stock
			k.insert(k.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});       // jmp [rip+0]
			{
				const uint64_t back = b + sun_clear_rva + sizeof(sun_clear_stock);
				const auto* p = reinterpret_cast<const uint8_t*>(&back);
				k.insert(k.end(), p, p + 8);
			}
			k[k1] = static_cast<uint8_t>(korig - (k1 + 1));
			k[k2] = static_cast<uint8_t>(kclamp - (k2 + 1));
			k[k3] = static_cast<uint8_t>(kclamp - (k3 + 1));
			k[k4] = static_cast<uint8_t>(kdone - (k4 + 1));
			if (0x40 + s.size() > 0x100 || 0x100 + k.size() > 0x180
				|| !write_bytes(cave + 0x00, c_rt5, sizeof(c_rt5))
				|| !write_bytes(cave + 0x10, c_rt9, sizeof(c_rt9))
				|| !write_bytes(cave + 0x20, c_loop, sizeof(c_loop))
				|| !write_bytes(cave + 0x40, s.data(), s.size())
				|| !write_bytes(cave + 0x100, k.data(), k.size()))
			{
				sun_grow_result = "sun shadow 12 slices: NOT applied - cave write failed";
				return false;
			}

			// All-or-nothing. Setter first: without sidecar views it acts as stock.
			const auto call_site = [&](uint32_t rva, size_t len, size_t cave_off, uint8_t* out)
			{
				std::memset(out, 0x90, len);
				out[0] = 0xE8;
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + cave_off) - (b + rva + 5));
				std::memcpy(out + 1, &rel, sizeof(rel));
			};
			uint8_t p_setter[sizeof(sun_setter_stock)];
			std::memset(p_setter, 0x90, sizeof(p_setter));
			p_setter[0] = 0xE9;
			{
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + 0x40) - (b + sun_setter_rva + 5));
				std::memcpy(p_setter + 1, &rel, sizeof(rel));
			}
			uint8_t p_clear[sizeof(sun_clear_stock)];
			std::memset(p_clear, 0x90, sizeof(p_clear));
			p_clear[0] = 0xE9;
			{
				const auto rel = static_cast<int32_t>(reinterpret_cast<size_t>(cave + 0x100) - (b + sun_clear_rva + 5));
				std::memcpy(p_clear + 1, &rel, sizeof(rel));
			}
			uint8_t p_loop[sizeof(sun_view_loop_stock)];
			call_site(sun_view_loop_rva, sizeof(p_loop), 0x20, p_loop);
			const uint8_t p_check[] = {0x66, 0x83, 0xF8, static_cast<uint8_t>(sun_slices_wanted - 2)};
			uint8_t p_rt5[sizeof(sun_desc_rt5_stock)];
			call_site(sun_desc_rt5_rva, sizeof(p_rt5), 0x00, p_rt5);
			uint8_t p_rt9[sizeof(sun_desc_rt9_stock)];
			call_site(sun_desc_rt9_rva, sizeof(p_rt9), 0x10, p_rt9);

			struct w { uint32_t rva; const uint8_t* stock; const uint8_t* patch; size_t n; };
			const w writes[] = {
				{sun_setter_rva, sun_setter_stock, p_setter, sizeof(p_setter)},
				{sun_clear_rva, sun_clear_stock, p_clear, sizeof(p_clear)},
				{sun_view_loop_rva, sun_view_loop_stock, p_loop, sizeof(p_loop)},
				{sun_view_check_rva, sun_view_check_stock, p_check, sizeof(p_check)},
				{sun_desc_rt5_rva, sun_desc_rt5_stock, p_rt5, sizeof(p_rt5)},
				{sun_desc_rt9_rva, sun_desc_rt9_stock, p_rt9, sizeof(p_rt9)},
			};
			size_t done_w = 0;
			for (const auto& x : writes)
			{
				if (!write_bytes(reinterpret_cast<void*>(b + x.rva), x.patch, x.n))
				{
					for (size_t k = 0; k < done_w; ++k)
					{
						write_bytes(reinterpret_cast<void*>(b + writes[k].rva), writes[k].stock, writes[k].n);
					}
					sun_grow_result = "sun shadow 12 slices: NOT applied - a site write failed (rolled back)";
					return false;
				}
				++done_w;
			}
			sun_slices_grown = true;
			sun_grow_result = "sun shadow: RT 5/9 6 -> 12 slices (4 view slots), RT 9 slices 6..11 via sidecar views";
			return true;
		}

		// Renderer loop: remakes RT 9's views for slices 6..11 whenever the target is
		// recreated. Old views are released one tick later, never while in use.
		void maintain_sun_trans_views()
		{
			for (auto*& r : sun_trans_retired)
			{
				if (r)
				{
					r->Release();
					r = nullptr;
				}
			}
			if (!sun_slices_grown)
			{
				return;
			}
			const auto b = base();
			const auto recs = *reinterpret_cast<const uint64_t*>(b + rt_records_ptr_rva);
			if (!recs)
			{
				return;
			}
			const auto* rec = reinterpret_cast<const uint8_t*>(recs + sun_trans_rt * rt_record_stride);
			if (!readable(rec, rt_record_stride))
			{
				return;
			}
			const uint16_t slices = *reinterpret_cast<const uint16_t*>(rec + 0xA86);
			auto* v0 = *reinterpret_cast<ID3D11RenderTargetView* const*>(rec + 8);
			if (v0 == sun_trans_seen_v0)
			{
				return;
			}
			for (size_t i = 0; i < std::size(sun_trans_extra); ++i)
			{
				sun_trans_retired[i] = sun_trans_extra[i];
				sun_trans_extra[i] = nullptr;
			}
			sun_trans_seen_v0 = v0;
			if (!v0 || slices <= 6)
			{
				return;
			}
			D3D11_RENDER_TARGET_VIEW_DESC d{};
			v0->GetDesc(&d);
			if (d.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
			{
				return;
			}
			ID3D11Resource* res = nullptr;
			ID3D11Device* dev = nullptr;
			v0->GetResource(&res);
			v0->GetDevice(&dev);
			uint32_t made = 0;
			if (res && dev)
			{
				for (uint32_t sl = 6; sl < slices && sl < 6 + std::size(sun_trans_extra); ++sl)
				{
					d.Texture2DArray.FirstArraySlice = sl;
					d.Texture2DArray.ArraySize = 1;
					ID3D11RenderTargetView* v = nullptr;
					if (SUCCEEDED(dev->CreateRenderTargetView(res, &d, &v)) && v)
					{
						sun_trans_extra[sl - 6] = v;
						++made;
					}
				}
			}
			if (res)
			{
				res->Release();
			}
			if (dev)
			{
				dev->Release();
			}
			trace_line l;
			l.str("sun shadow: RT 9 sidecar views for slices 6..");
			l.dec(slices - 1u);
			l.str(": ");
			l.dec(made);
			l.str(" made");
			trace_write(l);
		}

		void clamp_sun_shadow_slot()
		{
			if (sun_slot_clamped)
			{
				return;
			}
			const auto b = base();
			const auto* slices_at = reinterpret_cast<const uint8_t*>(b + sun_slices_rva);
			const auto* parts_at = reinterpret_cast<const uint8_t*>(b + sun_partitions_rva);
			auto* site = reinterpret_cast<uint8_t*>(b + sun_slot_site_rva);
			if (!readable(slices_at, 6) || slices_at[0] != 0x41 || slices_at[1] != 0xB8
				|| !readable(parts_at, 3) || parts_at[0] != 0x83 || parts_at[1] != 0xFB
				|| !readable(site, sizeof(sun_slot_site_expected))
				|| std::memcmp(site, sun_slot_site_expected, sizeof(sun_slot_site_expected)) != 0)
			{
				note("[splitscreen] sun shadow slot: bytes differ - not clamped\n");
				return;
			}
			uint32_t slices = 0;
			std::memcpy(&slices, slices_at + 2, sizeof(slices));
			// grow_sun_shadow_slices() leaves that immediate at 6 and stores 12 itself.
			if (sun_slices_grown)
			{
				slices = sun_slices_wanted;
			}
			const uint32_t partitions = parts_at[2];
			if (partitions == 0 || slices < partitions || slices % partitions != 0
				|| slices / partitions > 0x80)
			{
				note("[splitscreen] sun shadow slot: %u slices / %u partitions - not clamped\n",
				     slices, partitions);
				return;
			}
			uint32_t max_slot = slices / partitions - 1;
			// Debug switch: BO3_SUN_SLOT_MAX=<digit> lowers the bound. 0 puts every
			// view into slot 0, like PS4 R_DrawSunShadowMapCallback (0x9589B0).
			{
				char env[8] = {};
				GetEnvironmentVariableA("BO3_SUN_SLOT_MAX", env, sizeof(env));
				if (env[0] >= '0' && env[0] <= '9' && env[1] == 0
					&& static_cast<uint32_t>(env[0] - '0') < max_slot)
				{
					max_slot = static_cast<uint32_t>(env[0] - '0');
				}
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x40));
			if (!cave)
			{
				return;
			}
			std::vector<uint8_t> c(sun_slot_site_expected, sun_slot_site_expected + 12);
			c.insert(c.end(), {0x83, 0xF9, static_cast<uint8_t>(max_slot)}); // cmp ecx, max_slot
			c.insert(c.end(), {0x7E, 0x05});                                 // jle +5
			c.insert(c.end(), {0xB9});                                       // mov ecx, max_slot
			{
				const auto* p = reinterpret_cast<const uint8_t*>(&max_slot);
				c.insert(c.end(), p, p + 4);
			}
			c.insert(c.end(), {0x89, 0x4D, 0x14});                           // mov [rbp+0x14], ecx
			c.insert(c.end(), {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});         // jmp [rip+0]
			const uint64_t back = b + sun_slot_site_rva + sizeof(sun_slot_site_expected);
			const auto* back_bytes = reinterpret_cast<const uint8_t*>(&back);
			c.insert(c.end(), back_bytes, back_bytes + 8);
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return;
			}

			uint8_t patch[sizeof(sun_slot_site_expected)];
			std::memset(patch, 0x90, sizeof(patch));
			patch[0] = 0xE9;
			const auto rel = static_cast<int32_t>(
				reinterpret_cast<size_t>(cave) - (b + sun_slot_site_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return;
			}
			sun_slot_clamped = true;
			trace_line l;
			l.str("sun shadow slot: bounded to 0..");
			l.dec(max_slot);
			l.str(" (");
			l.dec(slices);
			l.str(" slices / ");
			l.dec(partitions);
			l.str(" partitions)");
			trace_write(l);
		}

		bool install_pane_counts_and_bounds()
		{
			if (pane_counts_installed)
			{
				return true;
			}
			// clientUIActives cannot move, so its IsActive gate is caved. Every other
			// array the pane path indexes at 2 must be relocated first, or the wider
			// bounds corrupt foreign globals.
			if (!isactive_caved || !view_params_relocated
				|| !scrplace_relocated || !perclient54_relocated
				|| !aaglob_relocated)
			{
				return false;
			}
			const auto b = base();

			auto* fn = reinterpret_cast<uint8_t*>(b + get_active_count_rva);
			if (std::memcmp(fn, get_active_count_expected,
			                sizeof(get_active_count_expected)) != 0)
			{
				return false;
			}
			for (const auto& f : pane_bounds)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + f.rva);
				if (!readable(p, f.expect_len)
					|| std::memcmp(p, f.expect, f.expect_len) != 0)
				{
					return false;
				}
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x80));
			if (!cave)
			{
				return false;
			}
			const auto cave_addr = reinterpret_cast<size_t>(cave);
			std::vector<uint8_t> c;
			const auto rip32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};
			// CL_LocalClient_GetActiveCount: PS4 (0x1516A20) sums IsActive(i) for
			// i < 4, the PC unrolls two elements. The cave adds elements 2/3 under the
			// IsActive cave's rule (i < cl_maxLocalClients). It counts active flags,
			// not seats: seats stay set in the lobby after GAME OVER, which kept three
			// panes open.
			c.insert(c.end(), {0x33, 0xC0});                   // xor eax, eax
			c.insert(c.end(), {0x48, 0x8D, 0x0D});             // lea rcx, [clientUIActives]
			rip32(b + uia_base_rva);
			for (uint32_t i = 0; i < 4; ++i)
			{
				const uint32_t off = i * uia_stride;
				if (i >= 2)
				{
					c.insert(c.end(), {0x83, 0x3D});           // cmp dword [cl_maxLocalClients], i
					{
						const auto v = static_cast<int32_t>((b + cl_max_local_clients_rva)
							- (cave_addr + c.size() + 4 + 1));
						const auto* p = reinterpret_cast<const uint8_t*>(&v);
						c.insert(c.end(), p, p + 4);
					}
					c.insert(c.end(), {static_cast<uint8_t>(i)});
					c.insert(c.end(), {0x7E, 0x0B});           // jle next (skips the 11 bytes below)
				}
				if (off == 0)
				{
					c.insert(c.end(), {0x8B, 0x11});           // mov edx, [rcx]
				}
				else
				{
					c.insert(c.end(), {0x8B, 0x91});           // mov edx, [rcx+off]
					const auto* p = reinterpret_cast<const uint8_t*>(&off);
					c.insert(c.end(), p, p + 4);
				}
				c.insert(c.end(), {0x83, 0xE2, 0x01});         // and edx, 1
				c.insert(c.end(), {0x03, 0xC2});               // add eax, edx
			}
			c.insert(c.end(), {0xC3});                          // ret
			if (c.size() > 0x80)
			{
				note("[splitscreen] pane count: cave too small (%zu)\n", c.size());
				return false;
			}
			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}

			uint8_t patch[5] = {0xE9};
			const auto rel = static_cast<int32_t>(
				cave_addr - (b + get_active_count_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			uint8_t old_head[5] = {};
			std::memcpy(old_head, fn, sizeof(old_head));
			if (!write_bytes(fn, patch, sizeof(patch)))
			{
				return false;
			}

			uint32_t applied = 0;
			for (const auto& f : pane_bounds)
			{
				auto* site = reinterpret_cast<uint8_t*>(b + f.rva + f.offset);
				if (*site != f.from || !write_bytes(site, &f.to, sizeof(f.to)))
				{
					// roll the whole transaction back
					for (uint32_t i = 0; i < applied; ++i)
					{
						auto* undo = reinterpret_cast<uint8_t*>(
							b + pane_bounds[i].rva + pane_bounds[i].offset);
						write_bytes(undo, &pane_bounds[i].from, 1);
					}
					write_bytes(fn, old_head, sizeof(old_head));
					return false;
				}
				++applied;
			}

			pane_counts_installed = true;
			return true;
		}

		// --- Unnamed per-client CG/UI context array [2] -> [4] --------------------
		// Element 0x2BB8, indexed by localClientNum; no PS4 match by size. Slot 2
		// overlaps live globals (47 refs); a client-2 write turned a pointer into
		// floats and crashed. The array is not statically zero, so slots 0/1 are
		// copied. The per-map reset walker (`mov ebp,1 / dec / jns`, two elements)
		// is set to 3 only after the move.
		constexpr uint32_t percg_base_rva = 0x04CADB40;
		constexpr uint32_t percg_stride = 0x2BB8;
		constexpr uint32_t percg_old_slots = 2;
		constexpr uint32_t percg_new_slots = 4;
		constexpr uint32_t percg_old_size = percg_old_slots * percg_stride;  // 0x5770
		constexpr uint32_t percg_new_size = percg_new_slots * percg_stride;  // 0xAEE0

		constexpr uint32_t percg_walker_count_rva = 0x02D2CC1A;
		constexpr uint8_t percg_walker_expected[] = {0xBD, 0x01, 0x00, 0x00, 0x00};

		struct percg_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint32_t delta;   // byte offset within element 0
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[12];
		};

		constexpr percg_ref percg_refs[] = {
			{0x010EB4F0, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0x49, 0x26, 0xBC, 0x03}},
			{0x010F30C2, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x77, 0xAA, 0xBB, 0x03}},
			{0x02D2CC1F, 7, 3, 0x830, true, {0x48, 0x8D, 0x1D, 0x4A, 0x17, 0xF8, 0x01}},
			{0x010CD118, 12, 4, 0x2BB0, false, {0x42, 0xC7, 0x84, 0x30, 0xF0, 0x06, 0xCB, 0x04, 0xFF, 0xFF, 0xFF, 0xFF}},
			{0x010CD124, 8, 4, 0x2BA0, false, {0x4A, 0x89, 0x8C, 0x30, 0xE0, 0x06, 0xCB, 0x04}},
			{0x010CD12C, 8, 4, 0x2BA8, false, {0x4A, 0x89, 0x8C, 0x30, 0xE8, 0x06, 0xCB, 0x04}},
		};

		bool percg_relocated = false;
		size_t percg_new_base_rva = 0;

		bool relocate_percg_context()
		{
			if (percg_relocated)
			{
				return true;
			}

			const auto b = base();

			for (const auto& r : percg_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}
			if (!readable(reinterpret_cast<const void*>(b + percg_walker_count_rva),
			              sizeof(percg_walker_expected))
				|| std::memcmp(reinterpret_cast<const void*>(b + percg_walker_count_rva),
				               percg_walker_expected, sizeof(percg_walker_expected)) != 0)
			{
				return false;
			}

			auto* destination = allocate_near_module(percg_new_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			if (!readable(reinterpret_cast<const void*>(b + percg_base_rva), percg_old_size))
			{
				return false;
			}
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + percg_base_rva),
			            percg_old_size);

			int32_t old_values[std::size(percg_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + percg_refs[i].rva + percg_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			// rip-relative counts from the end of the instruction; the ABS32 form
			// is `[reg + disp32]` with reg = image base, so disp32 is the RVA.
			const auto field_value = [&](const percg_ref& r)
			{
				const size_t tgt = new_base + r.delta;
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			for (size_t i = 0; i < std::size(percg_refs); ++i)
			{
				const auto& r = percg_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			for (size_t i = 0; i < std::size(percg_refs); ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + percg_refs[i].rva + percg_refs[i].disp_off),
				            sizeof(seen));
				if (seen != field_value(percg_refs[i]))
				{
					rollback();
					return false;
				}
			}

			// Only now widen the walker: elements 2/3 land in the new block.
			const uint8_t three = 0x03;
			auto* count = reinterpret_cast<void*>(b + percg_walker_count_rva + 1);
			const uint8_t old_count = *reinterpret_cast<const uint8_t*>(count);
			if (!write_bytes(count, &three, sizeof(three)))
			{
				rollback();
				return false;
			}
			uint8_t count_seen = 0;
			std::memcpy(&count_seen, count, sizeof(count_seen));
			if (count_seen != three)
			{
				write_bytes(count, &old_count, sizeof(old_count));
				rollback();
				return false;
			}

			percg_new_base_rva = new_base;
			percg_relocated = true;
			// No status slot free; verify from outside with tools/verify_percg.py.
			return true;
		}

		// --- Per-client LUI root array [2] -> [4], flat relocation -----------------
		// Element 0xB0; the init loop builds "UIRoot%d" only for ebx < 2. Element 0
		// starts at the lower of the loop's two cursors (0x80 apart); an earlier
		// relocation used the upper one and was retracted. The bound cannot be
		// widened in place: element 2 would land on s_perController. So the array
		// moves first and the bound follows, in one transaction, at post_unpack
		// before the loop runs. If the old startup int3 at 0x01D492DB returns, the
		// move is wrong, not the timing: revert rather than retry at menu time.
		// History: LOG.md, s_rootData
		constexpr uint32_t uiroot_base_rva = 0x162631B0;
		constexpr uint32_t uiroot_stride = 0xB0;
		constexpr uint32_t uiroot_old_slots = 2;
		constexpr uint32_t uiroot_new_slots = 4;
		constexpr uint32_t uiroot_old_size = uiroot_old_slots * uiroot_stride;   // 0x160
		constexpr uint32_t uiroot_new_size = uiroot_new_slots * uiroot_stride;   // 0x2C0

		// The init loop bound `cmp ebx,2` (83 FB 02, immediate at +2).
		constexpr uint32_t uiroot_bound_rva = 0x01F1CC37;
		constexpr uint8_t uiroot_bound_expected[] = {0x83, 0xFB, 0x02};

		// Second `cmp ebx,2`: the per-client UI-context setup loop. At 2, context 2
		// is never built and later reads of it deref NULL. Uses the LUI roots, so
		// it is widened in the same function, after they moved.
		constexpr uint32_t uiroot_bound2_rva = 0x01F1CCDB;
		constexpr uint8_t uiroot_bound2_expected[] = {0x83, 0xFB, 0x02};

		struct uiroot_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint8_t delta;    // byte offset within element 0
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[9];
		};

		constexpr uiroot_ref uiroot_refs[] = {
			{0x01F1C1CC, 7, 3, 0x00, true, {0x48, 0x8D, 0x0D, 0xDD, 0x6F, 0x34, 0x14}},
			{0x01F1C3D5, 8, 3, 0xAC, false, {0x80, 0xBC, 0x38, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F1C3DF, 7, 3, 0x8C, false, {0x4C, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F1C46C, 8, 3, 0xAC, false, {0x80, 0xBC, 0x38, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F1C476, 7, 3, 0x8C, false, {0x4C, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F1C5E6, 8, 3, 0xAC, false, {0x80, 0xBC, 0x28, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F1C5F0, 7, 3, 0x8C, false, {0x48, 0x8D, 0x9D, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F1C79E, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x0B, 0x6A, 0x34, 0x14}},
			{0x01F1C9E8, 7, 3, 0x00, true, {0x48, 0x8D, 0x2D, 0xC1, 0x67, 0x34, 0x14}},
			{0x01F1CB8E, 7, 3, 0x80, true, {0x48, 0x8D, 0x35, 0x9B, 0x66, 0x34, 0x14}},
			{0x01F1CD35, 7, 3, 0xAC, true, {0x48, 0x8D, 0x1D, 0x20, 0x65, 0x34, 0x14}},
			{0x01F1CD3C, 7, 3, 0x8C, true, {0x48, 0x8D, 0x35, 0xF9, 0x64, 0x34, 0x14}},
			{0x01F1D832, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x77, 0x59, 0x34, 0x14}},
			{0x01F21A19, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x90, 0x17, 0x34, 0x14}},
			{0x01F25915, 7, 3, 0x00, true, {0x48, 0x8D, 0x15, 0x94, 0xD8, 0x33, 0x14}},
			{0x01F25D13, 9, 4, 0xAC, false, {0x42, 0x80, 0xBC, 0x39, 0x5C, 0x32, 0x26, 0x16, 0x00}},
			{0x01F25D1E, 7, 3, 0x8C, false, {0x4D, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
			{0x01F26428, 7, 3, 0x00, true, {0x4C, 0x8D, 0x2D, 0x81, 0xCD, 0x33, 0x14}},
			{0x01F26AB4, 7, 3, 0xAC, true, {0x48, 0x8D, 0x3D, 0xA1, 0xC7, 0x33, 0x14}},
			{0x01F26ABB, 7, 3, 0x8C, true, {0x48, 0x8D, 0x35, 0x7A, 0xC7, 0x33, 0x14}},
		};

		bool lui_roots_relocated = false;
		size_t uiroot_new_base_rva = 0;

		bool relocate_lui_roots()
		{
			if (lui_roots_relocated)
			{
				return true;
			}

			const auto b = base();

			// Every site must match its recorded bytes, or nothing is written.
			for (const auto& r : uiroot_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}
			if (!readable(reinterpret_cast<const void*>(b + uiroot_bound_rva),
			              sizeof(uiroot_bound_expected))
				|| std::memcmp(reinterpret_cast<const void*>(b + uiroot_bound_rva),
				               uiroot_bound_expected, sizeof(uiroot_bound_expected)) != 0)
			{
				return false;
			}

			auto* destination = allocate_near_module(uiroot_new_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			// Static content: copy slots 0/1; the init loop fills slots 2/3 later.
			if (!readable(reinterpret_cast<const void*>(b + uiroot_base_rva), uiroot_old_size))
			{
				return false;
			}
			std::memcpy(destination,
			            reinterpret_cast<const void*>(b + uiroot_base_rva),
			            uiroot_old_size);

			int32_t old_values[std::size(uiroot_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + uiroot_refs[i].rva + uiroot_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			// rip-relative counts from the end of the instruction; the ABS32 form
			// is `[reg + disp32]` with reg = image base, so disp32 is the RVA.
			const auto field_value = [&](const uiroot_ref& r)
			{
				const size_t tgt = new_base + r.delta;
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			for (size_t i = 0; i < std::size(uiroot_refs); ++i)
			{
				const auto& r = uiroot_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			for (size_t i = 0; i < std::size(uiroot_refs); ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + uiroot_refs[i].rva + uiroot_refs[i].disp_off),
				            sizeof(seen));
				if (seen != field_value(uiroot_refs[i]))
				{
					rollback();
					return false;
				}
			}

			// Only now widen the bound: element 2 no longer lands on s_perController.
			const uint8_t four = 0x04;
			auto* bound = reinterpret_cast<void*>(b + uiroot_bound_rva + 2);
			const uint8_t old_bound = *reinterpret_cast<const uint8_t*>(bound);
			if (!write_bytes(bound, &four, sizeof(four)))
			{
				rollback();
				return false;
			}
			uint8_t bound_seen = 0;
			std::memcpy(&bound_seen, bound, sizeof(bound_seen));
			if (bound_seen != four)
			{
				write_bytes(bound, &old_bound, sizeof(old_bound));
				rollback();
				return false;
			}

			// The second bound; on a byte mismatch only this site is skipped.
			if (readable(reinterpret_cast<const void*>(b + uiroot_bound2_rva),
			             sizeof(uiroot_bound2_expected))
				&& std::memcmp(reinterpret_cast<const void*>(b + uiroot_bound2_rva),
				               uiroot_bound2_expected,
				               sizeof(uiroot_bound2_expected)) == 0)
			{
				auto* bound2 = reinterpret_cast<void*>(b + uiroot_bound2_rva + 2);
				const uint8_t old_bound2 = *reinterpret_cast<const uint8_t*>(bound2);
				if (write_bytes(bound2, &four, sizeof(four)))
				{
					uint8_t seen2 = 0;
					std::memcpy(&seen2, bound2, sizeof(seen2));
					if (seen2 != four)
					{
						write_bytes(bound2, &old_bound2, sizeof(old_bound2));
					}
				}
			}

			// Sibling UI loops, same value. `cmp ebx,2` broadcasts "update_safe_area"
			// and steps into no new slot. `cmp edi,2` adds a HUD menu per active
			// controller (PS4 UI_CoD_Init 0xD04C0F). It needs root 2 in use (the count
			// stub no longer holds it at inUse = 0): otherwise HUD(2) lands in
			// "UIRootFull", the primary root, and swallows the other roots'
			// first_snapshot events. The registrar loop is widened later, in
			// widen_ui_registrar_bound.
			struct ui_bound { uint32_t rva; uint8_t modrm; };
			constexpr ui_bound ui_bounds[] = {
				{0x01F26B4D, 0xFB},   // cmp ebx,2
				{0x01F1CD94, 0xFF},   // cmp edi,2  HUD menus
			};
			for (const auto& ub : ui_bounds)
			{
				const uint8_t want[] = {0x83, ub.modrm, 0x02};
				auto* at = reinterpret_cast<uint8_t*>(b + ub.rva);
				if (!readable(at, sizeof(want))
					|| std::memcmp(at, want, sizeof(want)) != 0)
				{
					continue;
				}
				auto* imm = reinterpret_cast<uint8_t*>(b + ub.rva + 2);
				if (write_bytes(imm, &four, sizeof(four)))
				{
					uint8_t back = 0;
					std::memcpy(&back, imm, sizeof(back));
					if (back != four)
					{
						const uint8_t two = 0x02;
						write_bytes(imm, &two, sizeof(two));
					}
				}
			}

			uiroot_new_base_rva = new_base;
			lui_roots_relocated = true;
			// No status slot free; verify from outside with tools/verify_uiroot.py.
			return true;
		}

		// ---- s_perController (LUI_CoD) [2] -> [4] --------------------------------
		// Symptom: pane 4 turned grey and blurred mid-round in 4-player MP. PS4
		// CL_IsUIActive 0x415330 checks s_perController[ctrl] byte +1; PS4 has [4] x
		// 0x14, the PC clears only 0x28 bytes (= [2]). Slots 2/3 lie in the
		// button-glyph buffer that follows, so a glyph name marked player 4 as "in
		// a menu". 12 references into slots 0/1 are rewritten; 5 more address the
		// glyph buffer and stay. Moved at startup, so the engine's init clear
		// (widened 0x28 -> 0x50) initialises slots 2/3.
		constexpr uint32_t perctrl_base = 0x16263310;
		constexpr uint32_t perctrl_stride = 0x14;
		constexpr uint32_t perctrl_old_count = 2;
		constexpr uint32_t perctrl_new_count = 4;
		constexpr entcoll_site perctrl_sites[] = {
			{0x01F14F0D, 3, 7, true , 0x8},    // lea rcx,[+8]    subscribers++
			{0x01F1A550, 3, 7, true , 0x4},    // lea rcx,[+4]    float getter
			{0x01F1C68C, 5, 9, false, 0xC},    // mulss xmm1,[rbp+rbx*4+RVA+0xC]
			{0x01F1C6B8, 5, 9, false, 0x10},   // mulss xmm1,[rbp+rbx*4+RVA+0x10]
			{0x01F1CA11, 3, 7, true , 0x0},    // lea r14,[base]  init clear
			{0x01F1D09E, 3, 8, false, 0x0},    // cmp byte [rcx+rax*4+RVA],0
			{0x01F1D0E0, 3, 7, true , 0x1},    // lea rcx,[+1]    UI_CoD_IsUIActive
			{0x01F1F66E, 3, 7, true , 0x0},    // lea rax,[base]  flag setter
			{0x01F21363, 6, 10, false, 0xC},   // movss [r14+rax*4+RVA+0xC],xmm6
			{0x01F2136D, 6, 10, false, 0x10},  // movss [r14+rax*4+RVA+0x10],xmm7
			{0x01F2197D, 3, 7, true , 0x8},    // lea rcx,[+8]    subscribers--
			{0x01F265E7, 3, 7, true , 0x1},    // lea rax,[+1]
		};
		constexpr uint32_t perctrl_clear_rva = 0x01F1CA0D;                    // lea r8d,[rdx+0x28]
		constexpr uint8_t perctrl_clear_stock[] = {0x44, 0x8D, 0x42, 0x28};
		const char* perctrl_result = "s_perController: not attempted";
		size_t perctrl_new = 0;

		bool relocate_per_controller()
		{
			if (perctrl_new)
			{
				return true;
			}
			const auto b = base();
			auto* clear = reinterpret_cast<uint8_t*>(b + perctrl_clear_rva);
			if (!readable(clear, sizeof(perctrl_clear_stock))
				|| std::memcmp(clear, perctrl_clear_stock, sizeof(perctrl_clear_stock)) != 0)
			{
				perctrl_result = "s_perController: NOT moved - init clear bytes differ";
				return false;
			}
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(perctrl_new_count * perctrl_stride));
			if (!fresh)
			{
				perctrl_result = "s_perController: NOT moved - allocation failed";
				return false;
			}
			const auto* old = reinterpret_cast<const uint8_t*>(b + perctrl_base);
			std::memcpy(fresh, old, perctrl_old_count * perctrl_stride);   // slots 2/3 stay zero
			const auto fresh_abs = reinterpret_cast<size_t>(fresh);
			static int32_t saved[std::size(perctrl_sites)]{};
			if (!rewrite_entcoll(perctrl_sites, std::size(perctrl_sites), perctrl_base, fresh_abs, saved))
			{
				perctrl_result = "s_perController: NOT moved - a reference did not match";
				return false;
			}
			const uint8_t len = static_cast<uint8_t>(perctrl_new_count * perctrl_stride);   // 0x50
			if (!write_bytes(clear + 3, &len, 1))
			{
				for (size_t j = 0; j < std::size(perctrl_sites); ++j)
				{
					auto* insn = reinterpret_cast<uint8_t*>(b + perctrl_sites[j].rva);
					write_bytes(insn + perctrl_sites[j].disp_off, &saved[j], sizeof(int32_t));
				}
				perctrl_result = "s_perController: NOT moved - init clear write failed (rolled back)";
				return false;
			}
			perctrl_new = fresh_abs;
			perctrl_result = "s_perController [2] -> [4] (12 sites, init clear 0x28 -> 0x50)";
			note("[splitscreen] s_perController [2] -> [4] at RVA 0x%08X\n",
			     static_cast<uint32_t>(fresh_abs - b));
			return true;
		}

		// ============ LUI target tables [2 clients] -> [4] ============
		// Four per-client LUI tables (weakpoints, reticle, rocket launcher, arm blade)
		// are [2] on PC and [4] on PS4. Clients 2/3 overran them into the next table
		// and the UI model globals, crashing 4-player MP with bots. Moves all four
		// (48 refs) and widens two clear-loop bounds, at startup.
		struct lui_table
		{
			const char* name;
			uint32_t base;
			uint32_t stride;
			uint32_t old_count;
			uint32_t new_count;
			const entcoll_site* sites;
			size_t site_count;
		};
		constexpr entcoll_site lui_armblade_sites[] = {
			{0x01FF9FF3, 3, 7, true, 0x10}, {0x01FFA053, 3, 7, true, 0x0}, {0x01FFA0D7, 3, 7, true, 0x0},
			{0x01FFA157, 3, 7, true, 0x0}, {0x01FFBA12, 4, 8, false, 0x4}, {0x01FFEB9D, 5, 9, false, 0x8},
			{0x01FFEBAB, 5, 9, false, 0xC}, {0x01FFEBB4, 5, 9, false, 0x10}, {0x01FFEBC5, 4, 8, false, 0x0},
			{0x01FFEBE7, 3, 7, false, 0x14}, {0x01FFEBEE, 3, 7, false, 0x14}, {0x01FFEC0A, 3, 11, false, 0x4},
			{0x01FFEC15, 3, 11, false, 0x14}, {0x01FFEC20, 4, 8, false, 0x8}, {0x0200C192, 3, 7, true, 0x0},
			{0x0200C3E6, 3, 7, true, 0x0},
		};
		constexpr entcoll_site lui_rocket_sites[] = {
			{0x01FFA1A7, 3, 7, true, 0x0}, {0x0200BFDE, 4, 8, false, 0x0}, {0x0200BFFF, 4, 8, false, 0x2},
			{0x0200C25E, 5, 9, false, 0x0}, {0x0200C343, 3, 7, true, 0x0}, {0x0200C5C9, 3, 7, true, 0x0},
		};
		constexpr entcoll_site lui_reticle_sites[] = {
			{0x01FFA187, 3, 7, true, 0x0}, {0x02000639, 4, 9, false, 0x0}, {0x02000689, 6, 10, false, 0x4},
			{0x020006B7, 4, 8, false, 0x0}, {0x020006C1, 6, 10, false, 0x4}, {0x020006DA, 6, 10, false, 0x4},
			{0x020006E6, 4, 8, false, 0x0}, {0x020006F0, 6, 10, false, 0x4}, {0x0200071B, 6, 10, false, 0x4},
			{0x02000760, 4, 8, false, 0x4}, {0x0200078E, 6, 10, false, 0x4}, {0x020007C1, 6, 10, false, 0x4},
		};
		constexpr entcoll_site lui_weakpoint_sites[] = {
			{0x01FDF700, 3, 7, true, 0x0}, {0x01FED34C, 5, 9, false, 0x4}, {0x01FED355, 4, 8, false, 0x0},
			{0x01FEEDC5, 4, 8, false, 0x0}, {0x01FEEE37, 4, 8, false, 0x0}, {0x01FEEEAD, 6, 10, false, 0x8},
			{0x01FEEEC9, 6, 10, false, 0xC}, {0x01FEEF28, 5, 9, false, 0x4}, {0x01FEEF50, 5, 9, false, 0x4},
			{0x01FEEF65, 4, 8, false, 0x0}, {0x01FF2847, 5, 9, false, 0x10}, {0x01FF2868, 5, 9, false, 0x10},
			{0x01FF96E5, 3, 7, true, 0x6}, {0x01FF9734, 3, 7, true, 0x0},
		};
		constexpr lui_table lui_tables[] = {
			{"weakpoints", 0x1626BDB0, 0x14, 20, 40, lui_weakpoint_sites, std::size(lui_weakpoint_sites)},
			{"reticle", 0x1626BF40, 0x8, 2, 4, lui_reticle_sites, std::size(lui_reticle_sites)},
			{"rocket launcher", 0x1626BF58, 0x4, 2, 4, lui_rocket_sites, std::size(lui_rocket_sites)},
			{"arm blade", 0x1626BF60, 0x18, 8, 16, lui_armblade_sites, std::size(lui_armblade_sites)},
		};
		struct lui_bound
		{
			uint32_t rva;
			uint8_t stock[4];
			uint8_t value;
		};
		constexpr lui_bound lui_bounds[] = {
			{0x01FFA01E, {0x48, 0x83, 0xF8, 0x08}, 0x10},   // arm-blade clear: cmp rax,8
			{0x0200C35C, {0x48, 0x83, 0xF9, 0x02}, 0x04},   // rocket-launcher clear: cmp rcx,2
		};
		const char* lui_tables_result = "LUI target tables: not attempted";
		bool lui_tables_moved = false;

		bool relocate_lui_target_tables()
		{
			if (lui_tables_moved)
			{
				return true;
			}
			const auto b = base();
			for (const auto& bd : lui_bounds)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + bd.rva);
				if (!readable(p, sizeof(bd.stock)) || std::memcmp(p, bd.stock, sizeof(bd.stock)) != 0)
				{
					lui_tables_result = "LUI target tables: NOT moved - a loop bound differs";
					return false;
				}
			}
			// All four or none: a table moved alone is still overrun by its neighbour.
			static int32_t saved[std::size(lui_tables)][16]{};
			size_t moved = 0;
			const auto rollback = [&]
			{
				for (size_t t = 0; t < moved; ++t)
				{
					for (size_t j = 0; j < lui_tables[t].site_count; ++j)
					{
						auto* insn = reinterpret_cast<uint8_t*>(b + lui_tables[t].sites[j].rva);
						write_bytes(insn + lui_tables[t].sites[j].disp_off, &saved[t][j], sizeof(int32_t));
					}
				}
			};
			for (const auto& t : lui_tables)
			{
				const size_t bytes = static_cast<size_t>(t.new_count) * t.stride;
				auto* fresh = static_cast<uint8_t*>(allocate_near_module(bytes));
				if (!fresh || t.site_count > 16)
				{
					rollback();
					lui_tables_result = "LUI target tables: NOT moved - allocation failed";
					return false;
				}
				// Start empty; the arm blade marks a free record with entity 0x3FF.
				std::memset(fresh, 0, bytes);
				if (t.base == 0x1626BF60)
				{
					for (uint32_t r = 0; r < t.new_count; ++r)
					{
						const int32_t none = 0x3FF;
						std::memcpy(fresh + r * t.stride + 4, &none, sizeof(none));
					}
				}
				if (!rewrite_entcoll(t.sites, t.site_count, t.base, reinterpret_cast<size_t>(fresh), saved[moved]))
				{
					rollback();
					lui_tables_result = "LUI target tables: NOT moved - a reference did not match (rolled back)";
					return false;
				}
				++moved;
			}
			size_t bounds_done = 0;
			for (const auto& bd : lui_bounds)
			{
				if (!write_bytes(reinterpret_cast<uint8_t*>(b + bd.rva + 3), &bd.value, 1))
				{
					for (size_t k = 0; k < bounds_done; ++k)
					{
						write_bytes(reinterpret_cast<uint8_t*>(b + lui_bounds[k].rva + 3), &lui_bounds[k].stock[3], 1);
					}
					rollback();
					lui_tables_result = "LUI target tables: NOT moved - a bound write failed (rolled back)";
					return false;
				}
				++bounds_done;
			}
			lui_tables_moved = true;
			lui_tables_result = "LUI target tables -> [4 clients]: weakpoints 20->40, reticle 2->4, rocket 2->4, "
			                    "arm blade 8->16 (48 sites, 2 bounds)";
			return true;
		}

		// ============ per-client marker blocks [2] -> [4] ============
		// 32 records of 0x50 per client at lc*0xA00. Clients 2/3 wrote over the
		// cgame media table that follows and crashed 4-player MP.
		constexpr uint32_t cg_marks_base = 0x04748220;
		constexpr uint32_t cg_marks_block = 0xA00;
		constexpr entcoll_site cg_marks_sites[] = {
			{0x00220447, 3, 7, true, 0x38},   // lea rax,[+0x38]   per-client init
			{0x002287DD, 3, 7, true, 0x8},    // lea rax,[+0x8]
			{0x00228863, 3, 7, true, 0x0},    // lea rcx,[base]    block getter
			{0x00228883, 3, 7, true, 0x0},    // lea rcx,[base]    find by entity
			{0x00233BA4, 3, 7, false, 0x0},   // lea rbx,[rax+RVA]
		};
		const char* cg_marks_result = "cg marker blocks: not attempted";
		bool cg_marks_moved = false;

		bool relocate_cg_marker_blocks()
		{
			if (cg_marks_moved)
			{
				return true;
			}
			const auto b = base();
			auto* fresh = static_cast<uint8_t*>(allocate_near_module(4 * cg_marks_block));
			if (!fresh)
			{
				cg_marks_result = "cg marker blocks: NOT moved - allocation failed";
				return false;
			}
			std::memset(fresh, 0, 4 * cg_marks_block);   // blocks 2/3: the per-client init sets them up
			if (!readable(reinterpret_cast<const void*>(b + cg_marks_base), 2 * cg_marks_block))
			{
				cg_marks_result = "cg marker blocks: NOT moved - old blocks unreadable";
				return false;
			}
			std::memcpy(fresh, reinterpret_cast<const void*>(b + cg_marks_base), 2 * cg_marks_block);
			static int32_t saved[std::size(cg_marks_sites)]{};
			if (!rewrite_entcoll(cg_marks_sites, std::size(cg_marks_sites), cg_marks_base,
			                     reinterpret_cast<size_t>(fresh), saved))
			{
				cg_marks_result = "cg marker blocks: NOT moved - a reference did not match";
				return false;
			}
			cg_marks_moved = true;
			cg_marks_result = "cg marker blocks [2] -> [4] x 0xA00 (5 sites)";
			return true;
		}

		// --- cg_localEntities: [2] -> [4], a flat relocation ----------------
		// CG_InitLocalEntities(lc) memsets cg_localEntities[lc] (0x7C00 bytes); for
		// lc 2 that wiped the clientfield system and broke the round start.
		// PS4 CG_InitLocalEntities 0x0021ED10 uses the same length, so 128 entities of
		// 0xF8 bytes hold on PC; only the client dimension differs ([4] vs [2]).
		// The client index is outside the element (lc*0x7C00, lc*0xF8, lc*8), so no
		// field offset changes, and the only client bound reads cl_maxLocalClients.
		// Sites are matched by full expected bytes, never by pattern.
		// History: LOG.md, cg_localEntities
		constexpr uint32_t le_active_rva = 0x0494A900;
		constexpr uint32_t le_free_rva = 0x0494AAF0;
		constexpr uint32_t le_pool_rva = 0x0494AB10;
		constexpr uint32_t le_entity_size = 0xF8;
		constexpr uint32_t le_entities_per_client = 128;
		constexpr uint32_t le_clients = 4;

		constexpr uint32_t le_old_active_size = 2 * le_entity_size;   // 0x1F0
		constexpr uint32_t le_old_free_size = 2 * 8;                  // 0x10
		constexpr uint32_t le_old_pool_size =
			2 * le_entities_per_client * le_entity_size;              // 0xF800

		constexpr uint32_t le_new_active_size = le_clients * le_entity_size; // 0x3E0
		constexpr uint32_t le_new_free_size = le_clients * 8;                // 0x20
		constexpr uint32_t le_new_pool_size =
			le_clients * le_entities_per_client * le_entity_size;            // 0x1F000

		// The pool sits at 0x400 to keep 1024-byte alignment (SIMD fields).
		constexpr uint32_t le_off_active = 0;
		constexpr uint32_t le_off_free = le_new_active_size;           // 0x3E0
		constexpr uint32_t le_off_pool = 0x400;
		constexpr uint32_t le_block_size = le_off_pool + le_new_pool_size; // 0x1F400

		enum le_array_id : uint8_t { le_active = 0, le_free = 1, le_pool = 2 };

		struct le_ref
		{
			uint32_t rva;
			uint8_t len;
			uint8_t disp_off;
			uint8_t arr;
			uint32_t delta;   // byte offset within that array
			bool rip;         // true = rip-relative, false = ABS32 off the image base
			uint8_t expected[9];
		};

		constexpr le_ref le_refs[] = {
			{0x0083B986, 7, 3, le_active, 0x0, true, {0x48, 0x8D, 0x05, 0x73, 0xEF, 0x10, 0x04}},
			{0x0083D20B, 8, 4, le_free, 0x0, false, {0x4B, 0x8B, 0x84, 0xE5, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D217, 8, 4, le_free, 0x0, false, {0x4B, 0x89, 0x9C, 0xE5, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D519, 9, 4, le_free, 0x0, false, {0x48, 0x83, 0xBC, 0xFE, 0xF0, 0xAA, 0x94, 0x04, 0x00}},
			{0x0083D52E, 8, 4, le_active, 0x0, false, {0x48, 0x8B, 0x94, 0x32, 0x00, 0xA9, 0x94, 0x04}},
			{0x0083D53B, 8, 4, le_free, 0x0, false, {0x48, 0x8B, 0x9C, 0xFE, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D552, 8, 4, le_free, 0x0, false, {0x48, 0x89, 0x84, 0xFE, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D569, 8, 4, le_active, 0x8, false, {0x48, 0x8B, 0x8C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D575, 7, 3, le_active, 0x0, false, {0x48, 0x8D, 0x8E, 0x00, 0xA9, 0x94, 0x04}},
			{0x0083D582, 8, 4, le_active, 0x8, false, {0x48, 0x8B, 0x8C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D58D, 8, 4, le_active, 0x8, false, {0x48, 0x89, 0x9C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D5F9, 7, 3, le_free, 0x0, true, {0x48, 0x8D, 0x0D, 0xF0, 0xD4, 0x10, 0x04}},
			{0x0083D632, 7, 3, le_pool, 0x0, true, {0x48, 0x8D, 0x35, 0xD7, 0xD4, 0x10, 0x04}},
			{0x0083D667, 7, 3, le_active, 0x0, false, {0x48, 0x8D, 0x82, 0x00, 0xA9, 0x94, 0x04}},
			{0x0083D66E, 8, 4, le_free, 0x0, false, {0x48, 0x89, 0x9C, 0xFA, 0xF0, 0xAA, 0x94, 0x04}},
			{0x0083D67D, 8, 4, le_active, 0x8, false, {0x48, 0x89, 0x84, 0x11, 0x08, 0xA9, 0x94, 0x04}},
			{0x0083D68B, 7, 3, le_pool, 0x8, true, {0x48, 0x8D, 0x0D, 0x86, 0xD4, 0x10, 0x04}},
		};

		bool local_entities_relocated = false;
		size_t le_new_base_rva = 0;

		bool relocate_local_entities()
		{
			if (local_entities_relocated)
			{
				return true;
			}

			const auto b = base();

			// Every site must match its recorded bytes, or nothing is written.
			for (const auto& r : le_refs)
			{
				const auto* p = reinterpret_cast<const void*>(b + r.rva);
				if (!readable(p, r.len) || std::memcmp(p, r.expected, r.len) != 0)
				{
					return false;
				}
			}

			// cg_freeLocalEntities holds pointers into the pool, so nothing is copied:
			// refuse unless all three arrays are still zero (before any map).
			const auto all_zero = [&](const uint32_t rva, const uint32_t size)
			{
				const auto* p = reinterpret_cast<const uint8_t*>(b + rva);
				if (!readable(p, size))
				{
					return false;
				}
				for (uint32_t i = 0; i < size; ++i)
				{
					if (p[i] != 0)
					{
						return false;
					}
				}
				return true;
			};
			if (!all_zero(le_active_rva, le_old_active_size)
				|| !all_zero(le_free_rva, le_old_free_size)
				|| !all_zero(le_pool_rva, le_old_pool_size))
			{
				return false;
			}

			auto* destination = allocate_near_module(le_block_size);
			if (!destination)
			{
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;

			const auto target_rva = [&](const le_ref& r) -> size_t
			{
				const uint32_t arr_off = r.arr == le_active
					                         ? le_off_active
					                         : (r.arr == le_free ? le_off_free : le_off_pool);
				return new_base + arr_off + r.delta;
			};
			// rip-relative counts from the end of the instruction; the ABS32 form is
			// [reg + disp32] with reg = image base, so its displacement is the RVA.
			const auto field_value = [&](const le_ref& r)
			{
				const auto tgt = target_rva(r);
				return r.rip
					       ? static_cast<int32_t>(tgt - (r.rva + r.len))
					       : static_cast<int32_t>(tgt);
			};

			int32_t old_values[std::size(le_refs)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + le_refs[i].rva + le_refs[i].disp_off),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			for (size_t i = 0; i < std::size(le_refs); ++i)
			{
				const auto& r = le_refs[i];
				const int32_t value = field_value(r);
				auto* field = reinterpret_cast<void*>(b + r.rva + r.disp_off);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &value, sizeof(value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			// Read every field back before declaring success.
			for (size_t i = 0; i < std::size(le_refs); ++i)
			{
				const auto& r = le_refs[i];
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + r.rva + r.disp_off), sizeof(seen));
				if (seen != field_value(r))
				{
					rollback();
					return false;
				}
			}

			le_new_base_rva = new_base;
			local_entities_relocated = true;
			// No status slot is free; tools/verify_localentities.py checks the live bytes.
			return true;
		}

		// --- RadiantExploderData: [2] -> [4], with a layout change ----------
		// Each 0x3930-byte record holds effectCount[2] at +0x19E8 and effects[2][500]
		// at +0x19F0 with no slack, so writing effectCount[2] crashed a round.
		// New: effectCount[4] at +0x19E8, effects[4][500] at +0x19F8, stride 0x5878.
		// No record transform: the array is per-map data and still empty here.
		// +0x19E8 / +0x19F0 also occur in unrelated structures, so every site is an
		// explicit address with expected bytes.
		constexpr uint32_t exploder_base_rva = 0x043016E0;
		constexpr uint32_t exploder_count_rva = 0x043016C4;
		constexpr uint32_t exploder_new_size = 0x587800;
		constexpr uint32_t exploder_new_stride = 0x5878;
		constexpr uint32_t exploder_counts_off = 0x19E8;   // unchanged
		constexpr uint32_t exploder_new_effects_off = 0x19F8;

		// Expected original bytes; a different build fails closed.
		constexpr uint8_t exploder_expect_A49[] = {0x48, 0x69, 0xC9, 0x30, 0x39, 0x00, 0x00};
		constexpr uint8_t exploder_expect_A5C[] = {0x48, 0x81, 0xC6, 0x30, 0x39, 0x00, 0x00};
		constexpr uint8_t exploder_expect_703[] = {0x49, 0x81, 0xC0, 0x30, 0x39, 0x00, 0x00};
		constexpr uint8_t exploder_expect_7A4[] = {0x41, 0xB8, 0x00, 0x30, 0x39, 0x00};
		constexpr uint8_t exploder_expect_AF7[] = {0x48, 0x89, 0xAC, 0xC6, 0xF0, 0x19, 0x00, 0x00};
		constexpr uint8_t exploder_expect_AC9[] = {0x48, 0x8D, 0x48, 0x08};
		constexpr uint8_t exploder_expect_589[] = {0x48, 0x8D, 0x3D, 0x40, 0xDB, 0x0F, 0x04};
		constexpr uint8_t exploder_expect_5D7[] = {0x48, 0x8D, 0x3D, 0xF2, 0xDA, 0x0F, 0x04};
		constexpr uint8_t exploder_expect_6E3[] = {0x48, 0x8D, 0x3D, 0xE6, 0xD9, 0x0F, 0x04};
		constexpr uint8_t exploder_expect_5CF[] = {0x48, 0x63, 0x84, 0x87, 0xC8, 0x30, 0x30, 0x04};
		constexpr uint8_t exploder_expect_79B[] = {0x48, 0x8D, 0x0D, 0x3E, 0x3F, 0x10, 0x04};
		constexpr uint8_t exploder_expect_A38[] = {0x48, 0x8D, 0x35, 0xA1, 0x0C, 0x10, 0x04};

		bool exploders_relocated = false;
		size_t exploder_new_base_rva = 0;

		bool relocate_radiant_exploders()
		{
			if (exploders_relocated)
			{
				return true;
			}

			const auto b = base();

			// Verify every original site first; one mismatch and nothing is written.
			const auto expect = [&](const uint32_t rva, const uint8_t* want,
			                        const size_t n)
			{
				const auto* p = reinterpret_cast<const void*>(b + rva);
				return readable(p, n) && std::memcmp(p, want, n) == 0;
			};
			if (!expect(0x00200A49, exploder_expect_A49, sizeof(exploder_expect_A49))
				|| !expect(0x00200A5C, exploder_expect_A5C, sizeof(exploder_expect_A5C))
				|| !expect(0x00205703, exploder_expect_703, sizeof(exploder_expect_703))
				|| !expect(0x001FD7A4, exploder_expect_7A4, sizeof(exploder_expect_7A4))
				|| !expect(0x00200AF7, exploder_expect_AF7, sizeof(exploder_expect_AF7))
				|| !expect(0x00200AC9, exploder_expect_AC9, sizeof(exploder_expect_AC9))
				|| !expect(0x00205589, exploder_expect_589, sizeof(exploder_expect_589))
				|| !expect(0x002055D7, exploder_expect_5D7, sizeof(exploder_expect_5D7))
				|| !expect(0x002056E3, exploder_expect_6E3, sizeof(exploder_expect_6E3))
				|| !expect(0x002055CF, exploder_expect_5CF, sizeof(exploder_expect_5CF))
				|| !expect(0x001FD79B, exploder_expect_79B, sizeof(exploder_expect_79B))
				|| !expect(0x00200A38, exploder_expect_A38, sizeof(exploder_expect_A38)))
			{
				return false;
			}

			// A non-zero live count means records exist that a zeroed copy would lose.
			uint32_t live_records = 0;
			std::memcpy(&live_records,
			            reinterpret_cast<const void*>(b + exploder_count_rva),
			            sizeof(live_records));
			if (live_records != 0)
			{
				return false;
			}

			auto* destination = allocate_near_module(exploder_new_size);
			if (!destination)
			{
				// No reachable 5.8 MB hole within a 32-bit displacement.
				return false;
			}
			const auto new_base = reinterpret_cast<size_t>(destination) - b;
			// VirtualAlloc zero-fills and the source is all zeros: nothing to copy.

			struct pending
			{
				uint32_t rva;
				uint8_t disp_offset;
				int32_t value;
			};

			const auto rip_to = [&](const size_t insn, const size_t len,
			                        const size_t target)
			{
				return static_cast<int32_t>(target - (insn + len));
			};

			const pending writes[] = {
				// base references
				{0x001FD79B, 3, rip_to(0x001FD79B, 7, new_base)},
				{0x00200A38, 3, rip_to(0x00200A38, 7, new_base)},
				// &effects[0][0] moves to the new field offset, not the old one
				{0x00205589, 3, rip_to(0x00205589, 7, new_base + exploder_new_effects_off)},
				{0x002055D7, 3, rip_to(0x002055D7, 7, new_base + exploder_new_effects_off)},
				{0x002056E3, 3, rip_to(0x002056E3, 7, new_base + exploder_new_effects_off)},
				// &effectCount[0]: ABS32 displacement off rdi, which holds the image base.
				{0x002055CF, 4, static_cast<int32_t>(new_base + exploder_counts_off)},
				// strides and sizes
				{0x00200A49, 3, static_cast<int32_t>(exploder_new_stride)},
				{0x00200A5C, 3, static_cast<int32_t>(exploder_new_stride)},
				{0x00205703, 3, static_cast<int32_t>(exploder_new_stride)},
				{0x001FD7A4, 2, static_cast<int32_t>(exploder_new_size)},
				// the one effects displacement that belongs to this array
				{0x00200AF7, 4, static_cast<int32_t>(exploder_new_effects_off)},
			};

			int32_t old_values[std::size(writes)] = {};
			size_t done = 0;
			const auto rollback = [&]
			{
				for (size_t i = 0; i < done; ++i)
				{
					write_bytes(reinterpret_cast<void*>(
						            b + writes[i].rva + writes[i].disp_offset),
					            &old_values[i], sizeof(old_values[i]));
				}
			};

			for (size_t i = 0; i < std::size(writes); ++i)
			{
				auto* field = reinterpret_cast<void*>(
					b + writes[i].rva + writes[i].disp_offset);
				std::memcpy(&old_values[i], field, sizeof(int32_t));
				if (!write_bytes(field, &writes[i].value, sizeof(writes[i].value)))
				{
					rollback();
					return false;
				}
				++done;
			}

			// `lea rcx,[rax+8]` -> `[rax+0x10]`: the init loop now clears four counts.
			const uint8_t clear_len = 0x10;
			auto* clear_field = reinterpret_cast<void*>(b + 0x00200AC9 + 3);
			const uint8_t old_clear = *reinterpret_cast<const uint8_t*>(clear_field);
			if (!write_bytes(clear_field, &clear_len, sizeof(clear_len)))
			{
				rollback();
				return false;
			}

			// Read every field back before declaring success.
			for (size_t i = 0; i < std::size(writes); ++i)
			{
				int32_t seen = 0;
				std::memcpy(&seen, reinterpret_cast<const void*>(
					            b + writes[i].rva + writes[i].disp_offset),
				            sizeof(seen));
				if (seen != writes[i].value)
				{
					write_bytes(clear_field, &old_clear, sizeof(old_clear));
					rollback();
					return false;
				}
			}

			exploder_new_base_rva = new_base;
			exploders_relocated = true;
			// No status slot is free. Verify from outside by reading the patched bytes:
			// 0x00200A49+3 == 0x5878, 0x00200AF7+4 == 0x19F8, 0x00200AC9+3 == 0x10.
			return true;
		}

		// --- Put the native guest into the real lobby -----------------------
		// Repeating the plural LobbyHost_AddLocalClients late is unsafe (it bumps
		// session+0xF0 again for an existing XUID), so the single-client
		// LobbyHost_AddLocalClient(actionId, ci, type) is used (PS4 0xCA5C70).
		// actionId 0 only tags the UI result.
		constexpr uint32_t lobby_host_add_local_rva = 0x01ECAAF0;
		constexpr uint32_t lobby_get_session_rva = 0x01ED03E0;
		constexpr uint32_t lobby_get_client_by_xuid_rva = 0x01EF3920;
		constexpr uint32_t live_user_get_xuid_rva = 0x01EBA880;
		constexpr uint32_t mutable_client_info_rva = 0x01EBEB00;
		constexpr uint32_t lobby_update_client_rva = 0x01EF5590;
		constexpr int game_lobby_type = 1;

		constexpr uint8_t lobby_host_add_local_bytes[] = {
			0xE9, 0xDB, 0xC7, 0x00, 0x00,
		};
		constexpr uint8_t lobby_get_session_bytes[] = {
			0x83, 0xF9, 0x01, 0x77, 0x16, 0x48, 0x63, 0xC1,
		};
		constexpr uint8_t lobby_get_client_bytes[] = {
			0x4C, 0x8D, 0x89, 0xF8, 0x00, 0x00, 0x00, 0x33, 0xC0,
		};
		constexpr uint8_t live_user_get_xuid_bytes[] = {
			0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xD9,
		};
		constexpr uint8_t mutable_client_info_bytes[] = {
			0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x55, 0x41, 0x56,
		};
		constexpr uint8_t lobby_update_client_bytes[] = {
			0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
			0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
		};

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
		constexpr uint32_t lobby_get_network_mode_rva = 0x01EDB7F0;
		constexpr uint8_t lobby_get_network_mode_bytes[] = {
			0x8B, 0x05, 0x26, 0x2B, 0x7F, 0x13, 0xC3,
		};
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
			uint32_t publish_attempts = 0;
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
				report87(47);
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
				report87(48);
				return false;
			}

			if (get_client(session, xuid))
			{
				enrolled = true;
				report87(50);
				return true;
			}

			// Never ahead of the host and native guest (join order); waiting costs no attempt.
			if (!host_and_guest_in_game_lobby(session, controller))
			{
				report87(55);
				return false;
			}

			const in_progress_guard guard(lobby_enrollment_in_progress);
			++attempts;
			report87(49);
			add_local(0, controller, game_lobby_type);

			enrolled = get_client(session, xuid) != nullptr;
			if (enrolled)
			{
				report87(50);
			}
			return enrolled;
		}

		bool guest2_profile_published = false;
		bool profile_publish_in_progress = false;
		uint32_t profile_publish_attempts = 0;

		bool refresh_guest_lobby_profile(int controller, bool enrolled, bool& published,
		                                 uint32_t& attempts);

		bool refresh_guest2_lobby_profile()
		{
			return refresh_guest_lobby_profile(2, guest2_lobby_enrolled, guest2_profile_published,
			                                   profile_publish_attempts);
		}

		bool refresh_guest_lobby_profile(const int controller, const bool enrolled, bool& published,
		                                 uint32_t& attempts)
		{
			if (published)
			{
				return true;
			}
			if (!enrolled || profile_publish_in_progress)
			{
				return false;
			}
			if (!profile_publish_api_matches())
			{
				report87(51);
				return false;
			}

			using mutable_info_fn = void (*)(int, void*);
			using get_session_fn = void* (*)(int);
			using get_xuid_fn = uint64_t (*)(int);
			using get_client_fn = void* (*)(void*, uint64_t);
			using update_client_fn = bool (*)(void*, uint64_t, const void*, bool*);

			const in_progress_guard guard(profile_publish_in_progress);
			++attempts;
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
				report87(52);
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
				report87(53);
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
				refresh_guest_lobby_profile(3, g.enrolled, g.published, g.publish_attempts);
			}
		}

		// Defined further down: player 3's own controller (A joins, B / unplug leaves).
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
		constexpr uint32_t lobby_add_all_rva = 0x01ECAB00;
		constexpr uint8_t lobby_add_all_prologue[] = {
			0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54, 0x41, 0x55,
			0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xB0,
		};
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
		// 3's own controller. The sign-in function stays detoured to log every
		// splitscreen sign-in/out to splitscreen_ui_trace.txt.
		// History: LOG.md, DEACTIVATE SPLITSCREEN
		constexpr uint32_t lobbyvm_local_leave_rva = 0x01EE3B40;
		constexpr uint8_t lobbyvm_local_leave_prologue[] = {
			0x40, 0x57,                                  // push rdi
			0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00,    // sub rsp, 0xB0
			0x48, 0xC7, 0x44, 0x24, 0x38, 0xFE, 0xFF, 0xFF, 0xFF,
		};
		constexpr uint8_t guest_signin_prologue[] = {
			0x48, 0x89, 0x5C, 0x24, 0x08,                // mov [rsp+8], rbx
			0x48, 0x89, 0x6C, 0x24, 0x18,                // mov [rsp+18h], rbp
			0x48, 0x89, 0x74, 0x24, 0x20,                // mov [rsp+20h], rsi
			0x57, 0x41, 0x56, 0x41, 0x57,                // push rdi / r14 / r15
		};
		constexpr uint32_t lobby_host_is_host_rva = 0x01ECC700;
		constexpr uint8_t lobby_host_is_host_bytes[] = {
			0x48, 0x83, 0xEC, 0x28, 0xE8, 0xD7, 0x3C, 0x00, 0x00,   // call 0x01EDCDD0
		};
		constexpr uint32_t lobby_host_remove_client_rva = 0x01ECD250;
		constexpr uint8_t lobby_host_remove_client_bytes[] = {
			0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
			0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20,
		};
		constexpr uint32_t local_client_left_reason_rva = 0x02FB1758;
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
			profile_publish_attempts = 0;
		}

		// Take a component-seated controller out of every lobby we host. Only when
		// no lobby lists him may he be signed out: a seat without a lobby entry, or
		// the reverse, is the "Failed to host lobby" state.
		bool remove_guest_from_lobbies(const int controller, trace_line& l)
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
				l.str(" lobbies=bytes-mismatch");
				return false;
			}

			const auto network_mode = reinterpret_cast<int (*)()>(
				base() + lobby_get_network_mode_rva)();
			const auto xuid = reinterpret_cast<uint64_t (*)(int)>(
				base() + live_user_get_xuid_rva)(controller);
			l.str(" mode=");
			l.dec(static_cast<uint64_t>(network_mode));
			if ((network_mode != lobby_network_local && network_mode != lobby_network_lan)
				|| !xuid)
			{
				l.str(" lobbies=refused");
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
				l.str(lobby_type == game_lobby_type ? " game=" : " party=");
				if (!member)
				{
					l.str("absent");
					continue;
				}
				if (!is_host(lobby_type))
				{
					l.str("not-host");
					failed = true;
					continue;
				}
				const bool removed = remove(lobby_type, xuid, reason);
				l.str(removed ? "removed" : "remove-failed");
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

			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" local_leave ci=");
			l.dec(static_cast<uint64_t>(controller));
			l.str(" lua=");
			l.dec(lua_result ? 1 : 0);

			const bool result = lua_result || remove_guest_from_lobbies(controller, l);
			if (result && controller == 2)
			{
				reset_guest2_join();
			}
			l.str(" signout=");
			l.dec(result ? 1 : 0);
			trace_write(l);
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

			{
				trace_line l;
				trace_head(l, "splitscreen_signin", call_site_of(_ReturnAddress()));
				l.str(" ci=");
				l.dec(static_cast<uint64_t>(controller));
				l.str(" add=");
				l.dec(signin ? 1 : 0);
				l.str(" a3=");
				l.dec(arg3 ? 1 : 0);
				l.str(" seats=");
				l.hex(before);
				l.str("->");
				l.hex(after);
				trace_stack(l);
				trace_write(l);
			}

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

		// ---- SwapClients (PS4 0xE356F0): trace and slot-3 guard ----
		// Logs both indices, cl_maxLocalClients, whether the connection block
		// exists, and both clients' UI flags before and after.
		constexpr uint32_t swap_clients_rva = 0x020E39D0;
		constexpr uint8_t swap_clients_prologue[] = {
			0x89, 0x54, 0x24, 0x10,                               // mov [rsp+0x10], edx
			0x89, 0x4C, 0x24, 0x08,                               // mov [rsp+8], ecx
			0x53, 0x55, 0x56, 0x57, 0x41, 0x54,                   // push rbx/rbp/rsi/rdi/r12
		};
		utils::hook::detour swap_clients_hook;

		void swap_clients_stub(const int a, const int b)
		{
			const auto ui_flags = [](const int lc) -> uint32_t
			{
				if (lc < 0 || lc > 3)   // lc 3: the owned head of clientUIActives[3]
				{
					return 0xFFFFFFFF;
				}
				return *reinterpret_cast<const volatile uint32_t*>(
					base() + 0x05359BC0 + static_cast<size_t>(lc) * 0x1078);
			};
			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(" SwapClients a=");
			l.dec(static_cast<uint64_t>(a));
			l.str(" b=");
			l.dec(static_cast<uint64_t>(b));
			l.str(" cl_max=");
			l.dec(*reinterpret_cast<const volatile uint32_t*>(base() + cl_max_local_clients_rva));
			l.str(" conn=");
			l.dec(*reinterpret_cast<const volatile uint64_t*>(base() + 0x05359BB8) ? 1 : 0);
			l.str(" flags ");
			l.hex(ui_flags(a));
			l.str("/");
			l.hex(ui_flags(b));

			// clientUIActives[3] owns only its first 0x3F0 bytes; the rest overlaps live
			// client globals and cls, which cannot be moved (CLAUDE.md dead end). On PS4
			// everything past +0x18 is online host migration data and voice counters, so
			// a swap involving slot 3 keeps the foreign tail [+0x3F0, +0x1078) in place
			// on both sides.
			constexpr size_t swap_uia_base = 0x05359BC0, swap_uia_stride = 0x1078;
			constexpr size_t window_off = 0x3F0, window_len = 0x1078 - 0x3F0;
			const bool guard = (a == 3 || b == 3) && a >= 0 && b >= 0 && a <= 3 && b <= 3;
			static uint8_t keep_a[window_len], keep_b[window_len];
			auto* win_a = reinterpret_cast<uint8_t*>(base() + swap_uia_base + a * swap_uia_stride + window_off);
			auto* win_b = reinterpret_cast<uint8_t*>(base() + swap_uia_base + b * swap_uia_stride + window_off);
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
				l.str(" window-held");
			}
			l.str(" -> ");
			l.hex(ui_flags(a));
			l.str("/");
			l.hex(ui_flags(b));
			trace_write(l);
		}

		// --- Player 3's own controller: A joins, B / unplugging leaves ---------
		// On PC the lobby menu only listens to controllers below
		// GetMaxLocalControllers() (2), so the component reads controller 2's buttons
		// itself, frontend only. Console runs the same join/leave from Lua.
		// Button bits: gamepad record +0x08; A = 0x100, B = 0x200.
		constexpr size_t gamepad_buttons = 0x08;
		constexpr uint32_t game_button_a = 0x00000100;
		constexpr uint32_t game_button_b = 0x00000200;
		constexpr uint64_t guest2_join_request_ms = 3000;
		constexpr uint32_t guest2_unplug_frames = 30;
		uint32_t guest2_prev_buttons = 0;
		bool guest2_join_requested = false;
		uint64_t guest2_join_request_tick = 0;
		uint32_t guest2_unplugged_frames = 0;
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
		void guest_leave(int controller, const char* why);

		void guest2_leave(const char* why)
		{
			guest_leave(2, why);
		}

		void guest_leave(const int controller, const char* why)
		{
			if (guest2_leave_in_progress)
			{
				return;
			}
			const in_progress_guard guard(guest2_leave_in_progress);
			trace_line l;
			l.str("t=");
			l.dec(GetTickCount64());
			l.str(controller == 2 ? " guest2_leave why=" : " guest3_leave why=");
			l.str(why);
			if (remove_guest_from_lobbies(controller, l))
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
				l.str(out ? " signout=1" : " signout=seat-still-set");
			}
			else
			{
				l.str(" signout=0");
			}
			l.str(" seats=");
			l.hex(seat_bits());
			trace_write(l);
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
				guest2_unplugged_frames = 0;
				return;
			}

			if (!controller_seated(2))
			{
				guest2_unplugged_frames = 0;
				// Seat lost but still listed: finish the leave (no member without a seat).
				if (guest2_lobby_enrolled && guest2_listed_in_game_lobby())
				{
					guest2_leave("seat-lost");
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
					trace_line l;
					l.str("t=");
					l.dec(guest2_join_request_tick);
					l.str(" guest2_join_request A");
					trace_write(l);
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
				guest2_leave("B");
				return;
			}
			if (!connected)
			{
				if (++guest2_unplugged_frames >= guest2_unplug_frames)
				{
					guest2_unplugged_frames = 0;
					guest2_leave("unplugged");
				}
				return;
			}
			guest2_unplugged_frames = 0;
		}

		// Player 4's controller: A and B go through the stock Lua. Leaving on unplug,
		// or when the seat is gone but the lobby still lists him, is done here.
		uint32_t guest3_unplugged_frames = 0;

		void guest3_input_frame()
		{
			if (!gamepads_activated || !game::Com_IsRunningUILevel())
			{
				guest3_unplugged_frames = 0;
				return;
			}
			if (!controller_seated(3))
			{
				guest3_unplugged_frames = 0;
				if (guest3_lobby.enrolled && guest_listed_in_game_lobby(3))
				{
					guest_leave(3, "seat-lost");
				}
				return;
			}
			if (!gamepad_connected(3))
			{
				if (++guest3_unplugged_frames >= guest2_unplug_frames)
				{
					guest3_unplugged_frames = 0;
					guest_leave(3, "unplugged");
				}
				return;
			}
			guest3_unplugged_frames = 0;
		}

		void cl_init_watch()
		{
			patch_probe_once();   // diagnostic, BO3_PATCH_PROBE=1 only

			// Not cl_init2_done alone: the widens are deferred until the allocation is
			// real, so keep re-entering until both halves are done.
			if (lc2_fully_done())
			{
				return;
			}
			// Breadcrumbs in status slot 87, so a run that does nothing says why.
			if (!signin_relocated)
			{
				report87(10);
				return;
			}
			if (!raise_local_client_count)
			{
				report87(11);
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
				return;
			}
			report87(20 + seats);
		}

		// Trigger for CL_Init(2): CL_LocalClients_SetAllUsedActive. It sets the
		// active bit inline and is known to run, while the connect loop's
		// CL_LocalClient_SetActive call never runs before the crash. PS4
		// CL_SetupClientsForIngame calls it right before the per-client allocation
		// and the connect loop. History: LOG.md, SetAllUsedActive
		constexpr uint32_t set_active_rva = 0x027C19C0;
		constexpr uint8_t set_active_prologue[] = {
			0x48, 0x89, 0x5C, 0x24, 0x08, // mov [rsp+8], rbx
			0x48, 0x89, 0x74, 0x24, 0x10, // mov [rsp+0x10], rsi
			0x57,                         // push rdi
			0x48, 0x83, 0xEC, 0x20,       // sub rsp, 0x20
		};
		utils::hook::detour set_active_hook;

		void run_cl_init_for_local_client2();


		// SetAllUsedActive takes no arguments (PS4 0x1517020: for i in 0..3
		// SetActive(i, IsBeingUsed(i))). Run the engine's pass first, then ours.
		void set_active_stub()
		{
			set_active_hook.invoke<void>();


			// Counted before any gate, so the number means "our hook ran".
			++set_active_calls;
			report87(0);

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

		// ---- Player 4: the per-frame loops that need four allocated clients ------
		// The client-2 path widens these to 3. Each indexes per-client heap memory
		// sized by cl_maxLocalClients, so it may reach index 3 only once the engine
		// has allocated four. Each site must still read 03; anything else is left
		// alone and counted.
		bool round4_widened = false;

		void widen_round_for_four()
		{
			if (round4_widened || !lc2_fully_done())
			{
				return;
			}
			const auto b = base();
			const auto max_local = *reinterpret_cast<const volatile uint32_t*>(b + cl_max_local_clients_rva);
			if (max_local < 4)
			{
				return;
			}
			round4_widened = true;
			const uint8_t four = 0x04;
			uint32_t done = 0, skipped = 0;
			const auto bump = [&](const uint32_t rva)
			{
				auto* at = reinterpret_cast<uint8_t*>(b + rva);
				if (readable(at, 1) && *at == 0x03 && write_bytes(at, &four, 1))
				{
					++done;
				}
				else
				{
					++skipped;
				}
			};
			bump(cl_frame_pump_imm_rva);
			bump(netchan_poll_imm_rva);
			for (const auto rva : cg_frame_imms)
			{
				bump(rva);
			}
			// LUI context bound: `41 83 FF 03 90 90 90` (hold_lui_context_count)
			bump(lui_ctx_bound_rva + 3);
			trace_line l;
			l.str("player 4 round: per-frame loops 3 -> 4, written ");
			l.dec(done);
			l.str(", not at 3 (left) ");
			l.dec(skipped);
			l.str(", cl_max ");
			l.dec(max_local);
			trace_write(l);
		}

		int splitscreen_player_count_stub()
		{
			if (signin_relocated)
			{
				// Console semantics: PS4 CL_SplitscreenPlayerCount only reads the
				// splitscreen_playerCount dvar. The PC re-seats guests during map load and
				// the reallocation asks mid re-seat, when seats look signed out. So treat the
				// dvar as the committed count and commit seats upward into it here.
				// Panes are still gated by IsActive, which reads the live seats.
				const uint32_t seats = bridged_seat_count();
				uint32_t n = seats;

				// Hold the per-client HUD gate (roots + lc*0xB0 + 0xAC) at 0 for clients
				// without a HUD root; set, it lets the HUD draw resolve to the NULL sentinel.
				// Hot path: this stub answers over a million queries per session, so no
				// readable()/write_bytes() per call (it collapsed the menu frame rate).
				// Validate once, then plain reads; the roots block is component-owned RW.
				if (lui_roots_relocated && uiroot_new_base_rva)
				{
					static bool hud_flags_checked = false;
					static bool hud_flags_ok = false;
					if (!hud_flags_checked)
					{
						hud_flags_checked = true;
						hud_flags_ok = readable(
							reinterpret_cast<const uint8_t*>(
								base() + uiroot_new_base_rva + 2 * 0xB0 + 0xAC), 0xB0 + 1);
					}
					if (hud_flags_ok)
					{
						// Root 2 is always released, root 3 once four seats are in use. A held root
						// makes UI_CoD_GetRootNameForController answer UIRootFull, and a HUD there
						// swallows every other root's first_snapshot.
						const uint32_t held_from = seat_count() >= 4 ? 4u : 3u;
						for (uint32_t lc = held_from; lc < 4; ++lc)
						{
							auto* flag = reinterpret_cast<uint8_t*>(
								base() + uiroot_new_base_rva + lc * 0xB0 + 0xAC);
							if (*flag != 0)
							{
								*flag = 0;   // component-owned RW, no VirtualProtect
							}
						}
					}
				}

				// Commit the allocation floor: max(count, floor) at alloc_floor_rva feeds
				// every per-client allocation and cl_maxLocalClients. Once three (four) seats
				// have genuinely seated it stays 3 (4) for the session, so the map-load
				// reallocation cannot shrink below the party. Race-free: our caller is the
				// allocator itself. Only 02 -> 03 -> 04, one-way.
				if (raise_local_client_count)
				{
					const uint32_t constituted = seat_count();
					if (constituted > committed_seats)
					{
						committed_seats = constituted;
					}
					if (committed_seats >= 3)
					{
						// Plain read; VirtualProtect only on the actual edge.
						auto* floor_imm = reinterpret_cast<uint8_t*>(
							base() + alloc_floor_rva);
						const uint8_t target = committed_seats >= 4 ? 0x04 : 0x03;
						if (*floor_imm == 0x02 || (*floor_imm == 0x03 && target == 0x04))
						{
							write_bytes(floor_imm, &target, sizeof(target));
							note("[splitscreen] allocation floor committed to %u\n", target);
						}
						if (n < committed_seats)
						{
							// the same commitment, answered directly
							n = committed_seats;
						}
					}
				}
				uint64_t dvar = 0;
				std::memcpy(&dvar,
				            reinterpret_cast<const void*>(
					            base() + splitscreen_player_count_dvar_rva),
				            sizeof(dvar));
				if (dvar)
				{
					auto* current = reinterpret_cast<uint32_t*>(dvar + dvar_current_offset);
					static bool dvar_checked = false;
					static bool dvar_ok = false;
					static uint32_t dvar_pushes = 0;
					if (!dvar_checked)
					{
						dvar_checked = true;
						dvar_ok = readable(current, sizeof(uint32_t));
					}
					if (dvar_ok)
					{
						// Bounded: the engine's updater pushes this back down during the re-seat
						// window, and pushing on every query cost the frame rate. The allocation
						// floor guarantees the size anyway. Plain dvar field, no VirtualProtect.
						if (seats >= 2 && seats > *current && dvar_pushes < 64)
						{
							*current = seats;
							++dvar_pushes;
						}
						if (*current > n)
						{
							n = *current;
						}
					}
				}
				if (n > 0)
				{
					// No set_status here (hot query, VirtualProtect); the async publisher reports it.
					++player_count_queries;
					player_count_last = n;

					// The scheduler loops stop after a splitscreen sign-in, so CL_Init(2) is
					// triggered here: the allocator calls this stub at map load, on the game
					// thread, before the connect loop. cl_init2_done is set first in the callee,
					// so this cannot recurse.
					if (n >= 3 && !lc2_fully_done() && raise_local_client_count)
					{
						run_cl_init_for_local_client2();
					}
					if (n >= 4 && raise_local_client_count)
					{
						widen_round_for_four();
					}
					return static_cast<int>(n);
				}
			}
			return splitscreen_player_count_hook.invoke<int>();
		}

		utils::hook::detour start_op_hook;
		uint32_t start_op_counts[4] = {};

		void start_op_stub(const int controller, const int operation, void* files)
		{
			// clientGameStates is not relocated: moved at post_unpack nothing signs in,
			// moved at the first StartOp the game crashes at startup. Menu time (chain4)
			// works but is too late for the boot storage read. History: LOG.md, clientGameStates

			if (controller >= 0 && controller < 4)
			{
				set_status(37 + controller, ++start_op_counts[controller]);
			}
			else
			{
				set_status(41, static_cast<uint32_t>(controller));
			}
			start_op_hook.invoke<void>(controller, operation, files);
		}

		constexpr uint32_t clear_storage_rva = 0x02218F80;
		constexpr uint8_t clear_storage_prologue[] = {0x48, 0x89, 0x6C, 0x24, 0x20, 0x56};
		bool clear_storage_ok = false;
		bool guests_cleared = false;

		void clear_guest_storage()
		{
			if (guests_cleared || !clear_storage_ok || !guests_filled)
			{
				return;
			}
			guests_cleared = true;
			const auto fn = reinterpret_cast<void(*)(int)>(base() + clear_storage_rva);
			fn(2);
			fn(3);
			set_status(35, 1);
		}

		constexpr uint32_t per_controller_update_rva = 0x01E19AE0;
		constexpr uint8_t per_controller_update_prologue[] = {0x48, 0x8B, 0xC4, 0x55, 0x41, 0x54};
		utils::hook::detour per_controller_update_hook;
		bool per_controller_update_hooked = false;

		// Task list layout (from TaskIsInProgress and the gamer-profile handlers):
		//   +0x00 next  +0x08 definition  +0x10 state  +0x48 opData  +0x51 flag
		//   in progress = state in {2,4,5} && flag == 0
		// opData points into s_localFileOpData: (opData - base) / 0x1820 = controller.
		constexpr uint32_t task_head_rva = 0x17A12A30;
		constexpr uint32_t gamerprofile_def_rva = 0x02FD3E08;

		// Which guest controller has a wedged gamer-profile task, or -1.
		// Calling ProcessTasks blindly for controllers 2 and 3 crashed startup
		// intermittently; the game never calls it for a controller with nothing to
		// process, so look first and act only on the one with a stuck task.
		int wedged_guest_controller()
		{
			if (!storage_base_rva)
			{
				return -1;
			}
			const auto module_base = base();
			const auto def = module_base + gamerprofile_def_rva;
			if (!localfileop_new_rva)
			{
				return -1;
			}
			const auto lfo = module_base + localfileop_new_rva;

			size_t node = 0;
			std::memcpy(&node, reinterpret_cast<const void*>(module_base + task_head_rva),
			            sizeof(node));

			for (int guard = 0; node && guard < 64; ++guard)
			{
				size_t next = 0;
				size_t definition = 0;
				int32_t state = 0;
				size_t opdata = 0;
				uint8_t flag = 0;
				std::memcpy(&next, reinterpret_cast<const void*>(node), sizeof(next));
				std::memcpy(&definition, reinterpret_cast<const void*>(node + 0x08),
				            sizeof(definition));
				std::memcpy(&state, reinterpret_cast<const void*>(node + 0x10), sizeof(state));
				std::memcpy(&opdata, reinterpret_cast<const void*>(node + 0x48), sizeof(opdata));
				std::memcpy(&flag, reinterpret_cast<const void*>(node + 0x51), sizeof(flag));

				const bool in_progress = (state >= 2 && state <= 5 && state != 3 && flag == 0);
				if (definition == def && in_progress && opdata >= lfo)
				{
					const auto index = (opdata - lfo) / localfileop_elem;
					if (index == 2 || index == 3)
					{
						return static_cast<int>(index);
					}
				}
				node = next;
			}
			return -1;
		}

		void reap_guest_tasks()
		{
			if (!process_tasks_ok)
			{
				return;
			}
			const auto who = wedged_guest_controller();
			if (who < 0)
			{
				return;
			}
			set_status(32, static_cast<uint32_t>(who));
			reinterpret_cast<void(*)(int)>(base() + process_tasks_rva)(who);
		}

		void storage_pump_stub(const int controller)
		{
			storage_pump_hook.invoke<void>(controller);

			// Once storage has a real CAC root, retry only the publisher/update half;
			// it runs no completion handler, so the outer storage walk stays valid.
			if (guest2_lobby_enrolled && !guest2_profile_published)
			{
				refresh_guest2_lobby_profile();
			}

			// Re-entry guard: pump_guest_storage calls back through here.
			if (inside_guest_pump || controller != 1)
			{
				return;
			}
			set_status(21, ++ticks_main); // times the game pumped controller 1

			// No reaping and no guest pumping here: Storage_Pump's caller is still on
			// the stack holding storage pointers, and completion handlers re-enter
			// storage. With s_targets widened the game pumps every controller itself;
			// the reap is in per_controller_update_stub. History: LOG.md, "reaper".
		}

		// Per-controller update detour: advances the guest joins, then reaps wedged
		// guest storage tasks. The reap is the loadout fix: controller 2's
		// gamer-profile task stays DONE, the 'hdd' busy query checks one global
		// task, so its reads never complete and the lobby draws no loadout.
		void per_controller_update_stub(const int controller)
		{
			per_controller_update_hook.invoke<void>(controller);

			// Guest sign-in: on the game's own thread (the renderer pipeline
			// answers engine predicates wrongly, see cl_init_watch) and outside
			// the Storage_Pump detour.
			if (controller == 2)
			{
				advance_guest2_join();
			}
			else if (controller == 3)
			{
				advance_guest3_join();
			}

			// Reap on the last controller, after the whole sweep's storage work.
			if (controller != 3)
			{
				return;
			}
			// Once per frame; set_status is a VirtualProtect pair, so every 256th pass.
			if ((++update_calls & 0xFF) == 1)
			{
				set_status(34, update_calls);
			}

			// Do not call clear_guest_storage() here: it zeroes the xuid, and the
			// re-assign never comes while the global 'hdd' task is busy.
		}

		uint32_t link_client_objects(const size_t table_slot, const size_t array_slot)
		{
			if (!new_base_rva[table_slot] || !new_base_rva[array_slot])
			{
				return 0;
			}

			const auto table = base() + new_base_rva[table_slot];
			const auto array = base() + new_base_rva[array_slot];
			guest_array_rva = new_base_rva[array_slot];

			uint32_t written = 0;
			for (size_t i = 0; i < 4; ++i)
			{
				const auto object = array + i * client_ui_stride;
				if (write_bytes(reinterpret_cast<void*>(table + i * sizeof(size_t)),
				                &object, sizeof(object)))
				{
					++written;
				}
			}
			return written;
		}

		// Verify the expected original byte before writing; skip with a log line on
		// mismatch.
		template <size_t N>
		uint32_t apply_byte_patches(const byte_patch (&patches)[N])
		{
			uint32_t done = 0;
			for (const auto& p : patches)
			{
				auto* site = reinterpret_cast<uint8_t*>(base() + p.rva);
				if (*site != p.expect)
				{
					note("[splitscreen] %s: 0x%zX holds 0x%02X, expected 0x%02X - skipped\n",
					       p.what, p.rva, *site, p.expect);
					continue;
				}
				if (write_bytes(site, &p.value, 1))
				{
					++done;
					note("[splitscreen] %s\n", p.what);
				}
			}
			return done;
		}

		uint32_t apply_storage_patches()
		{
			return apply_byte_patches(storage_patches);
		}

		// BO3_SS_SKIP=floor: leave the allocation floor (the `mov r14d, 2` of
		// max(CL_SplitscreenPlayerCount(), 2) in CL_AllocatePerLocalClientMemory,
		// PS4 0x416A32) at 2 while the rest of the count group stays on.
		bool skip_alloc_floor = false;

		uint32_t apply_local_client_count_patches()
		{
			if (!raise_local_client_count)
			{
				return 0;
			}
			uint32_t done = 0;
			for (const auto& p : local_client_count_patches)
			{
				if (skip_alloc_floor && p.rva == alloc_floor_rva)
				{
					note("[splitscreen] allocation floor left at 2 (BO3_SS_SKIP=floor)\n");
					continue;
				}
				const byte_patch one[1] = {p};
				done += apply_byte_patches(one);
			}
			return done;
		}

		// clientUIActives walker end-bounds: twelve loops end at
		// `lea reg, [clientUIActives[2]]` (four at field +8), among them the inlined
		// CL_AnyLocalClientsRunning. Unwidened, clients 2/3 miss the per-frame
		// upkeep. A thirteenth reference is an Arxan code copy and is left alone.
		struct end_bound_fix
		{
			uint32_t insn_rva;
			uint8_t expect[7];
		};

		constexpr end_bound_fix client_ui_end_bounds[] = {
			{0x0134B907, {0x48, 0x8D, 0x15, 0xA2, 0x03, 0x01, 0x04}},
			{0x0135950F, {0x48, 0x8D, 0x0D, 0x9A, 0x27, 0x00, 0x04}},
			{0x01359B92, {0x48, 0x8D, 0x0D, 0x17, 0x21, 0x00, 0x04}},
			{0x0135A27B, {0x48, 0x8D, 0x15, 0x2E, 0x1A, 0x00, 0x04}},
			{0x0135D1BB, {0x48, 0x8D, 0x15, 0xEE, 0xEA, 0xFF, 0x03}},
			{0x027C171E, {0x4C, 0x8D, 0x0D, 0x8B, 0xA5, 0xB9, 0x02}},
			{0x027C1789, {0x48, 0x8D, 0x15, 0x20, 0xA5, 0xB9, 0x02}},
			{0x027C1890, {0x48, 0x8D, 0x0D, 0x19, 0xA4, 0xB9, 0x02}},
			{0x020ECA0F, {0x48, 0x8D, 0x15, 0xA2, 0xF2, 0x26, 0x03}},
			{0x027C1617, {0x48, 0x8D, 0x0D, 0x9A, 0xA6, 0xB9, 0x02}},
			{0x027C1650, {0x48, 0x8D, 0x0D, 0x61, 0xA6, 0xB9, 0x02}},
			{0x027C1690, {0x48, 0x8D, 0x0D, 0x21, 0xA6, 0xB9, 0x02}},
		};

		// IsActive tracer (read-only diagnostic): CL_LocalClient_IsActive jumps to a
		// cave that computes the same answer and records, per local client, the
		// last caller, the call count and the answer (read by activation_watch.py).
		constexpr uint32_t is_active_rva = 0x027C18E0;
		constexpr uint32_t client_ui_actives_rva = 0x05359BC0;
		constexpr uint8_t is_active_bytes[] = {
			0x48, 0x63, 0xC1,                         // movsxd rax, ecx
			0x48, 0x8D, 0x0D,                         // lea rcx, [clientUIActives]
		};

		// Instrumented at the function, not a call site: IsActive has 93 callers
		// and Arxan flattening hides which loop really runs.
		bool install_is_active_tracer()
		{
			auto* fn = reinterpret_cast<uint8_t*>(base() + is_active_rva);
			if (std::memcmp(fn, is_active_bytes, sizeof(is_active_bytes)) != 0)
			{
				note("[splitscreen] IsActive tracer: unexpected prologue\n");
				return false;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return false;
			}
			// per index i: +0 last caller (qword), +8 calls, +12 last answer
			auto* slots = cave + 0x80;
			std::memset(slots, 0, 0x40);
			const auto cave_addr = reinterpret_cast<size_t>(cave);

			std::vector<uint8_t> c;
			const auto rip32 = [&](const size_t tgt)
			{
				const auto v = static_cast<int32_t>(tgt - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&v);
				c.insert(c.end(), p, p + 4);
			};

			c.insert(c.end(), {0x4C, 0x8B, 0x04, 0x24});             // mov r8, [rsp]
			c.insert(c.end(), {0x48, 0x63, 0xC1});                   // movsxd rax, ecx
			c.insert(c.end(), {0x48, 0x69, 0xC0, 0x78, 0x10, 0x00, 0x00}); // imul rax,0x1078
			c.insert(c.end(), {0x48, 0x8D, 0x15});                   // lea rdx, [uiactives]
			// Read the array where the engine reads it (relocated or not).
			rip32(base() + (client_ui_actives_relocated
				                ? uia_new_base_rva
				                : client_ui_actives_rva));
			c.insert(c.end(), {0x8B, 0x04, 0x10});                   // mov eax, [rax+rdx]
			c.insert(c.end(), {0x83, 0xE0, 0x01});                   // and eax, 1
			c.insert(c.end(), {0x83, 0xF9, 0x04});                   // cmp ecx, 4
			c.insert(c.end(), {0x73, 0x1C});                         // jae +28 -> ret
			c.insert(c.end(), {0x4C, 0x63, 0xC9});                   // movsxd r9, ecx
			c.insert(c.end(), {0x49, 0xC1, 0xE1, 0x04});             // shl r9, 4
			c.insert(c.end(), {0x48, 0x8D, 0x15});                   // lea rdx, [slots]
			rip32(reinterpret_cast<size_t>(slots));
			c.insert(c.end(), {0x4C, 0x03, 0xCA});                   // add r9, rdx
			c.insert(c.end(), {0x4D, 0x89, 0x01});                   // mov [r9], r8
			c.insert(c.end(), {0x41, 0xFF, 0x41, 0x08});             // inc dword [r9+8]
			c.insert(c.end(), {0x41, 0x89, 0x41, 0x0C});             // mov [r9+12], eax
			c.insert(c.end(), {0xC3});                               // ret

			if (!write_bytes(cave, c.data(), c.size()))
			{
				return false;
			}

			// Jump the function into the cave; its callers see the same value.
			uint8_t patch[5] = {0xE9};
			const auto rel = static_cast<int32_t>(
				cave_addr - (base() + is_active_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));
			return write_bytes(fn, patch, sizeof(patch));
		}

		uint32_t widen_client_ui_walker_bounds()
		{
			uint32_t done = 0;
			for (const auto& f : client_ui_end_bounds)
			{
				auto* insn = reinterpret_cast<uint8_t*>(base() + f.insn_rva);
				if (std::memcmp(insn, f.expect, sizeof(f.expect)) != 0)
				{
					note("[splitscreen] walker end-bound at 0x%X: unexpected "
					     "bytes - skipped\n", f.insn_rva);
					continue;
				}
				// The leas point at clientUIActives[2], the start of a foreign
				// array. After a relocation, aim them at the new one-past-the-end.
				int32_t new_disp = 0;
				if (client_ui_actives_relocated)
				{
					const size_t end = uia_new_base_rva + 4 * uia_stride;
					new_disp = static_cast<int32_t>(
						end - (f.insn_rva + sizeof(f.expect)));
				}
				else
				{
					// Player 4: end at &[4]. Walkers read only +0 (flags) or +8
					// (connectionState) of an element, never the end address.
					int32_t disp = 0;
					std::memcpy(&disp, f.expect + 3, sizeof(disp));
					new_disp = disp + 2 * 0x1078;
				}
				if (write_bytes(insn + 3, &new_disp, sizeof(new_disp)))
				{
					++done;
				}
			}
			return done;
		}

		// End-of-match client loops. PS4 shuts down every local client:
		//   Com_ShutdownInternal (0xE47020):       CL_Disconnect(lc, false), lc < 4
		//   CL_ShutdownAllClientsCGame (0x3EADA0): CL_ShutdownCGame(lc), lc = 3..0
		// The PC copies stop at two, so client 2 kept its cgame at GAME OVER and the
		// next frame crashed on a NULL cg array. Both loops only act on a client
		// that is up, so two-player sessions are unaffected.
		uint32_t widen_client_shutdown_loops()
		{
			uint32_t done = 0;
			const auto b = base();

			auto* disconnect_bound = reinterpret_cast<uint8_t*>(b + 0x020F0797);
			constexpr uint8_t disconnect_old[] = {0x83, 0xFF, 0x02};
			if (readable(disconnect_bound, sizeof(disconnect_old))
			    && std::memcmp(disconnect_bound, disconnect_old, sizeof(disconnect_old)) == 0)
			{
				// Player 4: CL_Disconnect(3) returns unless flags bit 1 is set.
				const uint8_t four = 0x04;
				if (write_bytes(disconnect_bound + 2, &four, 1))
				{
					++done;
				}
			}
			else
			{
				note("[splitscreen] shutdown disconnect loop: bytes differ - skipped\n");
			}

			// The real Com_ShutdownInternal loop (passes false) and its inlined copy
			// also stopped at two; client 2 then stayed CA_ACTIVE through
			// CL_FreePerLocalClientMemory and hung the next round at CA_CONNECTED.
			// Their UI close loops (PS4 UI_CloseAll i < 4) index uiInfoArray, so they
			// widen only when relocate_batch17 moved it.
			struct shutdown_site { uint32_t rva; uint8_t modrm; const char* what; bool needs_uiinfo; };
			constexpr shutdown_site com_shutdown_sites[] = {
				{0x020F11CE, 0xFF, "Com_ShutdownInternal disconnect loop", false},
				{0x020F188B, 0xFB, "inlined Com_ShutdownInternal disconnect loop", false},
				{0x020F1219, 0xFB, "Com_ShutdownInternal UI close loop", true},
				{0x020F18DC, 0xFB, "inlined Com_ShutdownInternal UI close loop", true},
			};
			for (const auto& s : com_shutdown_sites)
			{
				if (s.needs_uiinfo && !batch17_new[0])
				{
					note("[splitscreen] %s: uiInfoArray not moved - left at 2\n", s.what);
					continue;
				}
				auto* at = reinterpret_cast<uint8_t*>(b + s.rva);
				const uint8_t want[] = {0x83, s.modrm, 0x02};
				if (!readable(at, sizeof(want)) || std::memcmp(at, want, sizeof(want)) != 0)
				{
					note("[splitscreen] %s: bytes differ - skipped\n", s.what);
					continue;
				}
				const uint8_t four = 0x04;   // player 4: uiInfoArray [4], seat record 3
				if (write_bytes(at + 2, &four, 1))
				{
					++done;
				}
			}

			auto* start = reinterpret_cast<uint8_t*>(b + 0x0132E31A);
			constexpr uint8_t start_old[] = {0xBF, 0x01, 0x00, 0x00, 0x00};
			// Player 4: start at client 3; the walk tests flags & 0x10 (cgame up).
			constexpr uint8_t start_new[] = {0xBF, 0x03, 0x00, 0x00, 0x00};
			auto* cursor = reinterpret_cast<uint8_t*>(b + 0x0132E31F);
			constexpr uint8_t cursor_old[] = {0x48, 0x8D, 0x1D, 0x12, 0xC9, 0x02, 0x04};
			if (!readable(start, sizeof(start_old)) || std::memcmp(start, start_old, sizeof(start_old)) != 0
			    || !readable(cursor, sizeof(cursor_old)) || std::memcmp(cursor, cursor_old, sizeof(cursor_old)) != 0)
			{
				note("[splitscreen] cgame shutdown loop: bytes differ - skipped\n");
				return done;
			}
			int32_t disp = 0;
			std::memcpy(&disp, cursor_old + 3, sizeof(disp));
			disp += 2 * 0x1078;   // clientUIActives[1] -> clientUIActives[3]
			if (!write_bytes(cursor + 3, &disp, sizeof(disp)))
			{
				return done;
			}
			if (!write_bytes(start, start_new, sizeof(start_new)))
			{
				write_bytes(cursor, cursor_old, sizeof(cursor_old));
				return done;
			}
			return done + 1;
		}

		// Seed cl_maxLocalClients for the frontend: it stays at the static 2 until
		// CL_AllocatePerLocalClientMemory runs at map load, which blocks a third
		// controller. Safe before the memory exists: PS4 CG_GetLocalClientGlobals
		// (0x1D76850) checks cgArray == NULL before the index bound.
		bool seed_cl_max_local_clients()
		{
			auto* v = reinterpret_cast<uint32_t*>(base() + cl_max_local_clients_rva);
			if (*v != 2)
			{
				note("[splitscreen] cl_maxLocalClients holds %u, expected 2 - not seeding\n", *v);
				return false;
			}
			return write_bytes(v, &seed_max_local_clients, sizeof(seed_max_local_clients));
		}

		// Stride-fix cave counters (status 76..81): +0x00 substitutions,
		// +0x04 executions, +0x08 last delivered rax, +0x10 last substituted value.
		uint8_t* stride_slots = nullptr;

		// Active count fix ("only two screens"). PS4 SetAllUsedActive sets
		// splitscreen_playerCount from CL_LocalClient_GetActiveCount (0x1516A20,
		// i < 4); the PC inlines it unrolled to two elements, so the dvar and the
		// allocator never exceed 2. These 26 bytes jump to a cave that counts
		// Com_LocalClient_IsBeingUsed(lc) for lc = 0..3 and returns into the
		// engine's own Dvar_SetInt. The activation loop bound stays at 2: widening
		// it would write into clientUIActives[2].
		constexpr size_t active_count_rva = 0x027C1A0D;
		constexpr uint8_t active_count_bytes[] = {
			0x40, 0x84, 0x35, 0xAC, 0x81, 0xB9, 0x02, // test byte [rip+..], sil
			0xB8, 0x00, 0x00, 0x00, 0x00,             // mov eax, 0
			0x0F, 0x45, 0xC6,                         // cmovne eax, esi
			0x40, 0x84, 0x35, 0x15, 0x92, 0xB9, 0x02, // test byte [rip+..], sil
			0x74, 0x02,                               // je +2
			0xFF, 0xC0,                               // inc eax
		};


		bool install_active_count_fix()
		{
			auto* site = reinterpret_cast<uint8_t*>(base() + active_count_rva);
			// The clientUIActives relocation rewrites the second displacement
			// (offset 18), so rebuild the expected bytes from the live target.
			uint8_t expect[sizeof(active_count_bytes)];
			std::memcpy(expect, active_count_bytes, sizeof(expect));
			if (client_ui_actives_relocated)
			{
				const size_t tgt = base() + uia_new_base_rva + uia_stride;
				const int32_t disp = static_cast<int32_t>(
					tgt - (base() + active_count_rva + 18 + 4 + 1));
				std::memcpy(expect + 18, &disp, sizeof(disp));
			}
			if (std::memcmp(site, expect, sizeof(expect)) != 0)
			{
				note("[splitscreen] active count: unexpected bytes at 0x%zX\n",
				     active_count_rva);
				return false;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return false;
			}

			auto* slots = cave + 0x80; // +0 index, +4 count, +8 executions, +12 last
			const auto cave_addr = reinterpret_cast<size_t>(cave);

			std::vector<uint8_t> c;
			// `extra` = bytes after the displacement in the same instruction (an
			// immediate); getting it wrong aims the access off by that much.
			const auto rip32 = [&](const size_t target, const size_t extra = 0)
			{
				const auto value = static_cast<int32_t>(
					target - (cave_addr + c.size() + 4 + extra));
				const auto* p = reinterpret_cast<const uint8_t*>(&value);
				c.insert(c.end(), p, p + 4);
			};

			const auto slot = [&](const size_t i) { return reinterpret_cast<size_t>(slots + i); };

			// inc dword [executions]
			c.insert(c.end(), {0xFF, 0x05});
			rip32(slot(8));
			// mov dword [index], 0
			c.insert(c.end(), {0xC7, 0x05});
			rip32(slot(0), 4);
			c.insert(c.end(), {0x00, 0x00, 0x00, 0x00});
			// mov dword [count], 0
			c.insert(c.end(), {0xC7, 0x05});
			rip32(slot(4), 4);
			c.insert(c.end(), {0x00, 0x00, 0x00, 0x00});

			const auto loop_start = c.size();
			// mov ecx, dword [index]
			c.insert(c.end(), {0x8B, 0x0D});
			rip32(slot(0));
			// call Com_LocalClient_IsBeingUsed
			c.insert(c.end(), {0xE8});
			rip32(base() + is_being_used_rva);
			// test al, al ; je +6
			c.insert(c.end(), {0x84, 0xC0});
			c.insert(c.end(), {0x74, 0x06});
			// inc dword [count]            (6 bytes - the je above skips exactly this)
			c.insert(c.end(), {0xFF, 0x05});
			rip32(slot(4));
			// inc dword [index]
			c.insert(c.end(), {0xFF, 0x05});
			rip32(slot(0));
			// cmp dword [index], 4
			c.insert(c.end(), {0x83, 0x3D});
			rip32(slot(0), 1);
			c.insert(c.end(), {0x04});
			// jl loop
			c.insert(c.end(), {0x0F, 0x8C});
			{
				const auto target = cave_addr + loop_start;
				const auto value = static_cast<int32_t>(target - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&value);
				c.insert(c.end(), p, p + 4);
			}
			// mov eax, dword [count]   -> the value the engine then uses
			c.insert(c.end(), {0x8B, 0x05});
			rip32(slot(4));
			// mov dword [last], eax    (publishable)
			c.insert(c.end(), {0x89, 0x05});
			rip32(slot(12));
			// jmp back, past the 26 replaced bytes
			c.insert(c.end(), {0xE9});
			rip32(base() + active_count_rva + sizeof(active_count_bytes));

			if (c.size() > 0x80)
			{
				note("[splitscreen] active count: cave too small (%zu)\n", c.size());
				return false;
			}

			std::memcpy(cave, c.data(), c.size());
			std::memset(slots, 0, 0x30);

			uint8_t patch[sizeof(active_count_bytes)];
			std::memset(patch, 0x90, sizeof(patch)); // nop the remainder
			patch[0] = 0xE9;
			const auto rel = static_cast<int32_t>(cave_addr - (base() + active_count_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));

			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return false;
			}

			active_count_slots = slots;
			note("[splitscreen] active count fix installed at 0x%zX\n", active_count_rva);
			return true;
		}

		void publish_active_count()
		{
			if (!active_count_slots)
			{
				return;
			}
			uint32_t execs = 0;
			uint32_t last = 0;
			std::memcpy(&execs, active_count_slots + 8, sizeof(execs));
			std::memcpy(&last, active_count_slots + 12, sizeof(last));
			set_status(86, execs);
			set_status(87, last);
			set_status(93, player_count_queries);
			set_status(94, player_count_last);
		}

		bool install_stride_fix()
		{
			auto* site = reinterpret_cast<uint8_t*>(base() + stride_site_rva);
			if (std::memcmp(site, stride_site_bytes, sizeof(stride_site_bytes)) != 0)
			{
				note("[splitscreen] stride fix: unexpected bytes at 0x%zX\n", stride_site_rva);
				return false;
			}

			auto* cave = static_cast<uint8_t*>(allocate_near_module(0x100));
			if (!cave)
			{
				return false;
			}

			auto* slots = cave + 0x80; // count, delivered, substituted
			const auto cave_addr = reinterpret_cast<size_t>(cave);

			std::vector<uint8_t> c;
			const auto rip32 = [&](const size_t target)
			{
				const auto value = static_cast<int32_t>(target - (cave_addr + c.size() + 4));
				const auto* p = reinterpret_cast<const uint8_t*>(&value);
				c.insert(c.end(), p, p + 4);
			};

			// inc dword [executions] first, so every entry is counted
			c.insert(c.end(), {0xFF, 0x05});
			rip32(reinterpret_cast<size_t>(slots + 4));
			// mov [delivered], rax
			c.insert(c.end(), {0x48, 0x89, 0x05});
			rip32(reinterpret_cast<size_t>(slots + 8));
			// Repair only a value that is clearly not a pointer (top 16 bits set);
			// substituting unconditionally killed the process with the menu up.
			c.insert(c.end(), {0x50}); // push rax
			c.insert(c.end(), {0x48, 0xC1, 0xE8, 0x30}); // shr rax, 48
			c.insert(c.end(), {0x85, 0xC0}); // test eax, eax
			c.insert(c.end(), {0x58}); // pop rax  (does not touch flags)
			c.insert(c.end(), {0x74, 20}); // jz -> skip the 20-byte repair
			// mov rax, [base_table]
			c.insert(c.end(), {0x48, 0x8B, 0x05});
			rip32(base() + base_table_rva);
			// inc dword [count]
			c.insert(c.end(), {0xFF, 0x05});
			rip32(reinterpret_cast<size_t>(slots));
			// mov [substituted], rax
			c.insert(c.end(), {0x48, 0x89, 0x05});
			rip32(reinterpret_cast<size_t>(slots + 16));
			// The displaced imul last, so the flags seen afterwards are unchanged.
			c.insert(c.end(), std::begin(stride_site_bytes), std::end(stride_site_bytes));
			// jmp back
			c.insert(c.end(), {0xE9});
			rip32(base() + stride_site_rva + sizeof(stride_site_bytes));

			std::memcpy(cave, c.data(), c.size());
			std::memset(slots, 0, 0x30);

			uint8_t patch[7] = {0xE9, 0, 0, 0, 0, 0x90, 0x90};
			const auto rel = static_cast<int32_t>(cave_addr - (base() + stride_site_rva + 5));
			std::memcpy(patch + 1, &rel, sizeof(rel));

			if (!write_bytes(site, patch, sizeof(patch)))
			{
				return false;
			}

			stride_slots = slots;
			note("[splitscreen] stride fix installed at 0x%zX\n", stride_site_rva);
			return true;
		}

		// Copies the stride-fix counters to status 76..81.
		void publish_stride_counters()
		{
			if (!stride_slots)
			{
				return;
			}

			uint32_t substitutions = 0;
			uint32_t executions = 0;
			uint64_t delivered = 0;
			uint64_t substituted = 0;
			std::memcpy(&substitutions, stride_slots, sizeof(substitutions));
			std::memcpy(&executions, stride_slots + 4, sizeof(executions));
			std::memcpy(&delivered, stride_slots + 8, sizeof(delivered));
			std::memcpy(&substituted, stride_slots + 16, sizeof(substituted));

			set_status(76, executions);
			set_status(77, substitutions);
			set_status(78, static_cast<uint32_t>(delivered));
			set_status(79, static_cast<uint32_t>(delivered >> 32));
			set_status(80, static_cast<uint32_t>(substituted));
			set_status(81, static_cast<uint32_t>(substituted >> 32));
		}

		// The injected local client has no message channel, so the launch stalls
		// on the lobby state message. Hold its gate fields and ack equal to a real
		// local guest (slot 1) for the whole launch; this copies true state.
		void hold_injected_clients()
		{
			const auto session = base() + lobby_pool_rva + game_lobby_index * lobby_pool_stride;
			const auto sequence = *reinterpret_cast<uint32_t*>(base() + launch_sequence_rva);

			uint32_t target_ack = sequence + 1;
			for (size_t i = 0; i < 2; ++i)
			{
				const auto xuid = *reinterpret_cast<uint64_t*>(
					session + session_clients_offset + i * session_client_stride);
				if (!xuid)
				{
					continue;
				}
				const auto ack = *reinterpret_cast<uint32_t*>(session + acks_offset + i * 4);
				target_ack = std::max(target_ack, ack);
			}

			for (const auto slot : injected_slots)
			{
				const auto xuid = *reinterpret_cast<uint64_t*>(
					session + session_clients_offset + slot * session_client_stride);
				if (!xuid)
				{
					continue; // not signed in, nothing to hold
				}

				for (const auto gate : gate_arrays)
				{
					const auto ref = session + gate + reference_slot * session_client_stride;
					const auto dst = session + gate + slot * session_client_stride;
					for (const auto field : copy_fields)
					{
						auto* d = reinterpret_cast<uint32_t*>(dst + field);
						const auto v = *reinterpret_cast<uint32_t*>(ref + field);
						if (*d != v)
						{
							*d = v;
						}
					}
				}

				auto* ack = reinterpret_cast<uint32_t*>(session + acks_offset + slot * 4);
				if (*ack < target_ack)
				{
					*ack = target_ack;
				}
			}
		}

		// Status block for external readers (there is no console), in the reserved
		// .data window (LOG.md, "RESERVED `.data`").
		constexpr size_t status_rva = 0x1A828D00;
		constexpr uint32_t status_magic = 0xB03C0FFE;

		void set_status(const size_t index, const uint32_t value)
		{
			// 0x180 covers slots 0..95, short of trace_null_caller's cave.
			auto* s = reinterpret_cast<uint32_t*>(base() + status_rva);
			DWORD old{};
			if (VirtualProtect(s, 0x180, PAGE_READWRITE, &old))
			{
				s[index] = value;
				DWORD tmp{};
				VirtualProtect(s, 0x180, old, &tmp);
			}
		}

		// True if every reference still holds the value the table expects.
		bool table_matches(const reloc_table& t)
		{
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
					return false;
				}
			}
			return true;
		}

		// Only the stride site is checked here; relocate() dry-runs each table in
		// full and writes nothing unless every reference matches.
		bool ready()
		{
			return std::memcmp(reinterpret_cast<void*>(base() + stride_site_rva),
			                   stride_site_bytes, sizeof(stride_site_bytes)) == 0;
		}

		uint32_t attempts = 0;

		// BO3_SPLITSCREEN selects how much is applied (cumulative):
		//   off      nothing at all
		//   reloc    the container relocations only
		//   storage  + the storage byte patches and the stride fix
		//   signin   + clientGameStates, the seat, the guest fill
		//   full     + every detour and the count patches   (default)
		// Status 3 = 0xFF when off; status 74 = the level in force.
		enum apply_level
		{
			level_off = 0,
			level_reloc = 1,
			level_storage = 2,
			level_signin = 3,
			level_full = 4,
		};

		apply_level current_level()
		{
			char buf[16]{};
			const auto n = GetEnvironmentVariableA("BO3_SPLITSCREEN", buf, sizeof(buf));
			if (n == 0 || n >= sizeof(buf))
			{
				return level_full;
			}
			if (_stricmp(buf, "off") == 0 || _stricmp(buf, "0") == 0) { return level_off; }
			if (_stricmp(buf, "reloc") == 0) { return level_reloc; }
			if (_stricmp(buf, "storage") == 0) { return level_storage; }
			if (_stricmp(buf, "signin") == 0) { return level_signin; }
			return level_full;
		}

		apply_level level = level_full;

		bool at_least(const apply_level want)
		{
			return level >= want;
		}

		// BO3_SS_SKIP removes exactly one group (counts, settings, signin, storage,
		// readfilter, stride, floor) while all others stay on; the cumulative
		// levels cannot isolate a group because later groups make earlier ones
		// safe. Status 75 = the skipped group.
		enum skip_group
		{
			skip_none = 0,
			skip_counts = 1,
			skip_settings = 2,
			skip_signin = 3,
			skip_storage = 4,
			skip_readfilter = 5,
			skip_stride = 6,
			skip_floor = 7,
		};

		skip_group skipped = skip_none;

		skip_group current_skip()
		{
			char buf[16]{};
			const auto n = GetEnvironmentVariableA("BO3_SS_SKIP", buf, sizeof(buf));
			if (n == 0 || n >= sizeof(buf))
			{
				return skip_none;
			}
			if (_stricmp(buf, "counts") == 0) { return skip_counts; }
			if (_stricmp(buf, "settings") == 0) { return skip_settings; }
			if (_stricmp(buf, "signin") == 0) { return skip_signin; }
			if (_stricmp(buf, "storage") == 0) { return skip_storage; }
			if (_stricmp(buf, "readfilter") == 0) { return skip_readfilter; }
			if (_stricmp(buf, "stride") == 0) { return skip_stride; }
			if (_stricmp(buf, "floor") == 0) { return skip_floor; }
			return skip_none;
		}

		bool alloc_floor_requested()
		{
			char buf[8]{};
			const auto n = GetEnvironmentVariableA("BO3_SS_FLOOR", buf, sizeof(buf));
			if (n == 0 || n >= sizeof(buf))
			{
				return false;
			}
			return _stricmp(buf, "on") == 0 || _stricmp(buf, "1") == 0;
		}

		bool group_enabled(const skip_group g)
		{
			return skipped != g;
		}

		bool try_apply()
		{
			set_status(0, status_magic);
			set_status(4, ++attempts);

			level = current_level();
			skipped = current_skip();
			set_status(74, static_cast<uint32_t>(level));
			set_status(75, static_cast<uint32_t>(skipped));
			if (level == level_off)
			{
				set_status(3, 0xFF);
				return true; // inert on purpose - do not retry, do not patch
			}

			if (!ready())
			{
				return false; // image not settled yet - try again
			}

			// Behind ezz BOIII: controllers 2/3 get the engine's own XUID and
			// name code back, local clients 2/3 get cgame memory of their own
			// (splitscreen_ezz.hpp). Nothing happens on official BOIII.
			{
				const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base());
				const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base() + dos->e_lfanew);
				ezz::install(base());
				ezz::install_pools(base(), nt->OptionalHeader.SizeOfImage, &allocate_near_module);
				const auto mask = ezz::install_client_command_guard(base());
				note("host chain: mask 0x%X (1 GetXuid, 2 UserGetXuid, 4 GetClientName, "
				     "16/32/64 cg/cgs/viewmodel pools, 128 entity pools, 256 ClientCommand guard)", mask);
			}

			// Reserve the s_gamePads region now, but do not repoint it: that crashed
			// pre-menu gamepad init. A lobby-time tool activates it.
			const bool gamepads_reserved = reserve_gamepads_region();
			set_status(56, gamepads_reserved ? 1u : 2u);
			set_status(57, static_cast<uint32_t>(gamepads_reserved_rva));
			set_status(58, 0); // lobby tool: references activated (expect 38)
			set_status(59, 0); // lobby tool: bounds activated (expect 6)

			// Status 9: 1 = before Storage_Init (pool still null), 2 = too late.
			const auto pool_before = *reinterpret_cast<uint64_t*>(base() + storage_pool_rva);
			const bool early = pool_before == 0;
			set_status(9, early ? 1u : 2u);

			// The storage relocation and byte patches are only safe before
			// Storage_Init; later, records 2/3 stay uninitialised and the game dies.
			uint32_t ok = 0;
			bool storage_moved = false;
			size_t slot = 0;
			for (const auto& t : reloc_tables)
			{
				const auto this_slot = slot++;
				const bool is_storage = std::strcmp(t.name, "storage") == 0;
				if (is_storage && !early)
				{
					note("[splitscreen] storage: Storage_Init already ran "
					       "(pool 0x%llX) - skipping the relocation and the byte "
					       "patches, they are only safe before it\n",
					       static_cast<unsigned long long>(pool_before));
					continue;
				}

				if (relocate(t, this_slot))
				{
					++ok;
					if (is_storage)
					{
						storage_moved = true;
					}
					if (std::strcmp(t.name, "voice_comm") == 0)
					{
						// The vacated original is clientUIActives slot 2 (plus the
						// head of slot 3): zero it so the activation loop does not
						// start from stale voice data.
						std::vector<uint8_t> zeros(t.old_size, 0);
						write_bytes(reinterpret_cast<void*>(base() + t.base_rva),
						            zeros.data(), zeros.size());
					}
				}
			}
			set_status(13, 99); // reached the end of the relocation loop
			set_status(1, ok);
			complete_players_kb();

			// RadiantExploderData changes its internal layout, so it has its own
			// transaction instead of a reloc_tables entry.
			// BO3_SKIP_FIX=<csv> disables individual [2]->[4] relocations below.
			char skip_fix[128] = {};
			GetEnvironmentVariableA("BO3_SKIP_FIX", skip_fix, sizeof(skip_fix));
			const auto fix_enabled = [&](const char* name)
			{
				return std::strstr(skip_fix, name) == nullptr;
			};

			if (fix_enabled("exploder"))
			{
				relocate_radiant_exploders();
			}

			// cg_localEntities and friends are [2] (PS4 [4]); slot 2 covered the
			// clientfield system, which CG_InitLocalEntities(2) zeroed.
			if (fix_enabled("localentities"))
			{
				relocate_local_entities();
			}

			// The per-client LUI root array is [2] and element 2 would land on
			// s_perController, so it moves before its bound is widened.
			if (fix_enabled("uiroot"))
			{
				relocate_lui_roots();
			}

			// Per-controller LUI state is [2]; controller 3's "UI active" byte
			// sat in the button-glyph text buffer (pane 4 went grey mid-round).
			if (fix_enabled("perctrl"))
			{
				relocate_per_controller();
				trace_line pc;
				pc.str(perctrl_result);
				trace_write(pc);
			}

			// Weakpoint / reticle / rocket-launcher / arm-blade HUD tables are sized
			// for two clients; clients 2/3 overran them into the UI model globals.
			if (fix_enabled("luitables"))
			{
				relocate_lui_target_tables();
				trace_line lt;
				lt.str(lui_tables_result);
				trace_write(lt);
			}
			// Per-client 32-entity marker blocks: clients 2/3 wrote the media table.
			if (fix_enabled("cgmarks"))
			{
				relocate_cg_marker_blocks();
				trace_line cm;
				cm.str(cg_marks_result);
				trace_write(cm);
			}

			// The per-client CG/UI context array (stride 0x2BB8) is [2]; client 2
			// sprayed float defaults over the globals in its slot 2.
			if (fix_enabled("percg"))
			{
				relocate_percg_context();
			}

			// Pane fix. The clientUIActives relocation is research only (black
			// frontend at launch, a closed dead end in CLAUDE.md) and runs only
			// with BO3_PANES=storage or full.
			char panes_mode[32] = {};
			GetEnvironmentVariableA("BO3_PANES", panes_mode, sizeof(panes_mode));
			const bool want_storage = std::strcmp(panes_mode, "storage") == 0
				|| std::strcmp(panes_mode, "full") == 0;

			if (want_storage)
			{
				relocate_client_ui_actives();
			}
			// Third screen, part 1: geometry. The IsActive cave comes later.
			relocate_view_params();
			// The two [2] arrays the pane path indexes at 2 (scrPlaceView overflowed).
			// The pane bounds refuse without them, so a failure means two panes.
			if (fix_enabled("scrplace"))
			{
				relocate_flat24("scrPlaceView", 0x0577B800, 0x7C,
				                scrplace_sites, std::size(scrplace_sites),
				                scrplace_relocated, scrplace_new_rva);
			}
			if (fix_enabled("perclient54"))
			{
				relocate_flat24("perclient54", 0x04CB32C0, 0x54,
				                perclient54_sites, std::size(perclient54_sites),
				                perclient54_relocated, perclient54_new_rva);
			}
			// AimAssist globals: CG_SetView(2) reads and writes slot 2, so this must
			// land before the pane bound widens (full 50-site table).
			if (fix_enabled("aaglob") && !aaglob_relocated)
			{
				const auto fresh = relocate_perclient(aaglob_array);
				if (fresh)
				{
					aaglob_relocated = true;
					aaglob_new_rva = fresh - base();
				}
				trace_line aa_line;
				aa_line.str(fresh ? "aaGlobArray [2] -> [4] (complete table: 50 sites)"
				                  : "aaGlobArray: NOT moved - a site did not match");
				trace_write(aa_line);
			}
			// The UI element-handle word array: prerequisite for widening the
			// registrar loop below.
			if (fix_enabled("uielem"))
			{
				relocate_flat24("uiElemHandles", 0x1795CED8, 0x2,
				                uielem_sites, std::size(uielem_sites),
				                uielem_relocated, uielem_new_rva);
				retarget_uielem_reader();
			}
			// Needs the array above and the LUI roots relocation, both done by now.
			widen_ui_registrar_bound();
			// Keep the LUI renderer at two contexts - see hold_lui_context_count.
			hold_lui_context_count();
			// And make the HUD-refresh reader survive a client with no snapshot.
			install_snapguard_cave();
			// Move scene buffer B out of A[2]/A[3] before anything reads them.
			relocate_scene_buffer_b();

			// Entity-collision group, opt-in with BO3_CG_FRAME=on like the frame loop
			// that needs it; it is only reachable once client 2's cgame ticks.
			{
				char entcoll_env[16] = {};
				GetEnvironmentVariableA("BO3_CG_FRAME", entcoll_env, sizeof(entcoll_env));
				if (std::strcmp(entcoll_env, "on") == 0)
				{
					relocate_entity_collision();
					// BO3_CF=off leaves the clientfield callback array stock, for bisecting.
					char cf_env[16] = {};
					GetEnvironmentVariableA("BO3_CF", cf_env, sizeof(cf_env));
					if (std::strcmp(cf_env, "off") != 0)
					{
						relocate_clientfield_callbacks();
					}
					else
					{
						note("[splitscreen] clientfield relocation SKIPPED (BO3_CF=off)\n");
					}
					// Independent of the clientfield fix: keep outside the BO3_CF switch.
					relocate_entword_table();
					{
						trace_line ow;
						ow.str(entword_result);
						trace_write(ow);
					}
					relocate_exposure_adaptions();
					{
						trace_line ex;
						ex.str(exposure_result);
						trace_write(ex);
					}
					relocate_sst_ring();
					{
						trace_line sr;
						sr.str(sst_result);
						trace_write(sr);
					}
					// 3 and 4 players in MP: every player's ChooseClass builds ~11.1k
					// model nodes; the stock pool (0x9000) holds two.
					relocate_ui_model_pool();
					{
						trace_line mp;
						mp.str(model_pool_result);
						trace_write(mp);
					}
					// Entering a mode with 3-4 players seated: the party join needs
					// every member's agreement over the lobby message loop.
					relocate_join_clients();
					{
						trace_line jc;
						jc.str(joinclient_result);
						trace_write(jc);
					}
					// MP HUD players 3/4: Engine.GetClientNum answered -1 for them.
					widen_lua_controller_checks();
					{
						trace_line lc;
						lc.str(lua_ctrl_result);
						trace_write(lc);
					}
					// cl_voiceCommunication is moved by reloc_tables' voice_comm.
					relocate_cgdc();
					relocate_playerkeys();
					relocate_notetracklerps();
					relocate_batch1b();
					relocate_batch2();
					relocate_batch3();
					relocate_batch4();
					{
						trace_line ik;
						ik.str(ikstates_new ? "ikStates [3] -> [5] (9 sites + reset end marker)"
						                    : "ikStates: NOT moved - reset loop widened one slot (3 players only)");
						trace_write(ik);
					}
					relocate_batch5();
				}
			}
			// Not behind BO3_CG_FRAME: CG_Init(2) runs whenever player 3's cgame
			// initialises at map load.
			{
				trace_line sm;
				sm.str(relocate_session_members()
				       ? "session members [2][18] x 0x132 -> [4] (4 sites, clear 0x5610)"
				       : "session members: NOT moved (bytes differ)");
				trace_write(sm);
			}
			relocate_batch6();
			// Also ungated: the 190 MB slide happened with the third pane off too.
			relocate_batch7();
			// Before install_perclient_buffer_guard(): its cave bakes C's base.
			relocate_batch8();
			relocate_batch9();
			relocate_batch10();
			relocate_batch11();
			relocate_batch12();
			relocate_batch13();
			relocate_batch14();
			relocate_batch15();
			relocate_batch16();
			relocate_batch17();
			relocate_batch18();
			relocate_lightq();
			// Before R_Init allocates the culler object (see grow_umbra_client_arrays).
			grow_umbra_client_arrays();
			install_perclient_buffer_guard();
			install_ui_trace();
			install_guest_copy();
			widen_csc_lc_checks();
			widen_filter_pass_lc_check();
			install_lc_bound_hooks();
			gate_lensflares_for_extra_clients();
			// Before the clamp, which bounds the slot by these slices.
			// BO3_SUN4=off keeps the shared slot 1.
			char sun4_env[8] = {};
			GetEnvironmentVariableA("BO3_SUN4", sun4_env, sizeof(sun4_env));
			if (std::strcmp(sun4_env, "off") != 0)
			{
				grow_sun_shadow_slices();
				trace_line sg;
				sg.str(sun_grow_result);
				trace_write(sg);
			}
			clamp_sun_shadow_slot();
			skip_lensflare_exit_shutdown();
			// Before Com_Init runs Com_LocalClient_LastInput_Init.
			create_extra_controller_models();
			fix_gamepad_type_selectors();

			// Link the two client tables by name, not index, so reordering tables
			// cannot point this at the wrong array.
			size_t table_slot = SIZE_MAX;
			size_t array_slot = SIZE_MAX;
			for (size_t i = 0; i < std::size(reloc_tables); ++i)
			{
				if (std::strcmp(reloc_tables[i].name, "client_objs") == 0)
				{
					table_slot = i;
				}
				else if (std::strcmp(reloc_tables[i].name, "client_ui") == 0)
				{
					array_slot = i;
				}
			}
			set_status(14, (table_slot != SIZE_MAX && array_slot != SIZE_MAX)
				               ? link_client_objects(table_slot, array_slot)
				               : 0u);

			// Publish where each table landed (it differs per run): status 16..19 =
			// bit_array, storage, client_objs, client_ui. Status 13/14 hold the
			// loop/link results, not mark() stage breadcrumbs.
			for (size_t i = 0; i < 4; ++i)
			{
				set_status(16 + i, static_cast<uint32_t>(new_base_rva[i]));
			}

			// By name, for the same reason as above.
			for (size_t i = 0; i < std::size(reloc_tables); ++i)
			{
				if (std::strcmp(reloc_tables[i].name, "storage") == 0)
				{
					storage_base_rva = new_base_rva[i];
				}
				// Status 20 is taken, so netchan's base goes to status 73.
				else if (std::strcmp(reloc_tables[i].name, "netchan") == 0)
				{
					set_status(73, static_cast<uint32_t>(new_base_rva[i]));
				}
			}

			// Local clients 2/3 get their command buffers (MP class choice,
			// every "cmd" a guest sends) - needs the relocated cbuf records above.
			install_cbuf_for_players34();
			note("[splitscreen] %s\n", cbuf34_result);

			// These reach s_storage[2]/[3], which are ours only after the relocation.
			const bool do_storage = at_least(level_storage) && group_enabled(skip_storage);
			set_status(10, storage_moved ? 1u : 0u);
			set_status(8, (storage_moved && do_storage) ? apply_storage_patches() : 0u);

			// The stride fix is its own skip group and is not tied to s_storage.
			const bool do_stride = at_least(level_storage) && group_enabled(skip_stride);
			set_status(2, (do_stride && install_stride_fix()) ? 1u : 0u);

			// Count patches only after every container relocation succeeded: raising
			// the count over a still-[2] array is the known crash family.
			// Status 60 = count patches applied (expect 3).
			const auto counts_ok = (ok == std::size(reloc_tables)) && at_least(level_full)
			                       && group_enabled(skip_counts);

			// The cl_maxLocalClients hold belongs to the count group too.
			raise_local_client_count = counts_ok;

			// Let the engine's own active count reach 3 and 4 (count group).
			// Status 85 = installed, 86 = executions, 87 = last count.
			set_status(85, (counts_ok && install_active_count_fix()) ? 1u : 2u);

			// The allocation floor is off by default: it told the allocator four while
			// splitscreen_playerCount said one, and a solo round crashed at start.
			// hold_splitscreen_player_count() sets the real count instead;
			// BO3_SS_FLOOR=on restores the floor.
			skip_alloc_floor = !alloc_floor_requested() || !group_enabled(skip_floor);

			set_status(60, counts_ok ? apply_local_client_count_patches() : 0u);
			// Walker end-bounds: safe only with voice_comm moved (implied by counts_ok).
			set_status(95, counts_ok ? widen_client_ui_walker_bounds() : 0u);
			// Same gate: the end-of-match loops walk clientUIActives slot 2.
			if (counts_ok)
			{
				widen_client_shutdown_loops();
			}

			// cl_maxLocalClients is not seeded here (not yet 2 at post_unpack); the
			// async probe seeds it once, when it first reads 2 (status 62).

			// Relocate clientGameStates so Com_ControllerIndex_GetLocalClientNum(2)
			// returns 2, not -1. On PS4 that gates the gumball row, guest menu input
			// and the per-player UI models. Status 43: 1 relocated, 2 refused.
			set_status(43, (at_least(level_signin) && group_enabled(skip_signin))
			                   ? (relocate_signin_field() ? 1u : 2u)
			                   : 0u);

			// Third screen, part 2. install_isactive_cave() answers IsActive(lc >= 2)
			// from the relocated clientGameStates, so it must run after
			// relocate_signin_field(). Pane counts and bounds come last.
			install_isactive_cave();
			install_pane_counts_and_bounds();
			// Also after relocate_signin_field(): reads seat record 2.
			widen_gamepad_button_models();
			widen_lobby_max_local_players();

			// Same function as the cave: the tracer runs only when the cave is absent.
			if (counts_ok && !isactive_caved)
			{
				install_is_active_tracer();
			}

			// Hold the injected clients on both pipelines: async stops during a
			// launch, and a hold registered only there hung the load. The hold only
			// moves values forward, so running it twice is idempotent.
			scheduler::loop(hold_injected_clients, scheduler::pipeline::async, 5ms);
			scheduler::loop(hold_injected_clients, scheduler::pipeline::renderer, 5ms);

			// Fill the guest records once element 1 is a signed-in profile. Fast
			// tick: the window closes when the boot storage pass runs.
			scheduler::loop(fill_guests_when_ready, scheduler::pipeline::async, 5ms);

			// Storage_Pump must run on the game's main thread, never async.
			scheduler::loop(pump_on_renderer, scheduler::pipeline::renderer, 250ms);
			// Renderer pipeline: it keeps running through a launch, when client 2
			// still has no scene buffers.
			scheduler::loop(fill_scene_buffers, scheduler::pipeline::renderer, 100ms);
			scheduler::loop(maintain_sun_trans_views, scheduler::pipeline::renderer, 100ms);
			scheduler::loop(cl_init_watch, scheduler::pipeline::renderer, 250ms);
			scheduler::loop(count_async_ticks, scheduler::pipeline::async, 250ms);
			scheduler::loop(publish_active_count, scheduler::pipeline::async, 250ms);
			scheduler::loop(publish_stride_counters, scheduler::pipeline::async, 250ms);
			scheduler::loop(mirror_signin_state, scheduler::pipeline::async, 50ms);
			scheduler::loop(maintain_signin_seats, scheduler::pipeline::async, 50ms);

			// Diagnostic (BO3_IDATA_TRAP=on): makes .idata read-only after 30 s so the
			// writer that once shifted it by 8 bytes faults at the culprit
			// instruction. History: LOG.md, "BO3_IDATA_TRAP".
			{
				char trap_env[8] = {};
				GetEnvironmentVariableA("BO3_IDATA_TRAP", trap_env, sizeof(trap_env));
				if (std::strcmp(trap_env, "on") == 0)
				{
					std::thread([]
					{
						std::this_thread::sleep_for(std::chrono::seconds(30));
						DWORD old{};
						VirtualProtect(reinterpret_cast<void*>(base() + 0x1AA67000), 0x4000, PAGE_READONLY, &old);
					}).detach();
				}
			}

			// pump_guests_when_quiet stays unregistered: pumping storage from the
			// async pipeline killed the client at startup.

			// s_targets must be widened before anything pumps controller 2 or 3.
			const auto targets_ok = widen_storage_targets();
			set_status(28, targets_ok ? 1u : 0u);

			// Before controller 2 does any local-file work.
			set_status(30, widen_local_file_ops() ? 1u : 0u);

			// Verify the prologue before ever calling it.
			process_tasks_ok = std::memcmp(
				reinterpret_cast<const void*>(base() + process_tasks_rva),
				process_tasks_prologue, sizeof(process_tasks_prologue)) == 0;
			set_status(31, process_tasks_ok ? 1u : 2u);

			clear_storage_ok = std::memcmp(
				reinterpret_cast<const void*>(base() + clear_storage_rva),
				clear_storage_prologue, sizeof(clear_storage_prologue)) == 0;
			set_status(36, clear_storage_ok ? 1u : 2u);

			// The StartOp-time relocation (see start_op_stub) is not done here:
			// done at post_unpack, nothing signs in afterwards.
			// All detours below are level `full` only.
			if (at_least(level_full))
			{
			const auto sread = base() + storage_read_rva;
			if (std::memcmp(reinterpret_cast<const void*>(sread), storage_read_prologue,
			                sizeof(storage_read_prologue)) == 0)
			{
				if (group_enabled(skip_readfilter))
				{
					storage_read_hook.create(reinterpret_cast<void*>(sread), storage_read_stub);
					set_status(48, 1);
				}
			}
			else
			{
				set_status(48, 2);
			}

			// Settings completion, neutered for guests only. Without this hook file 0
			// must stay out of guest_seated_file_types.
			const auto srr = base() + settings_read_result_rva;
			if (std::memcmp(reinterpret_cast<const void*>(srr), settings_read_result_prologue,
			                sizeof(settings_read_result_prologue)) == 0)
			{
				if (group_enabled(skip_settings))
				{
					settings_read_result_hook.create(reinterpret_cast<void*>(srr),
					                                 settings_read_result_stub);
					settings_result_neutered = true;
					set_status(67, 1);
				}
			}
			else
			{
				set_status(67, 2);
			}

			const auto scrr = base() + shoutcaster_read_result_rva;
			if (std::memcmp(reinterpret_cast<const void*>(scrr), shoutcaster_read_result_prologue,
			                sizeof(shoutcaster_read_result_prologue)) == 0)
			{
				if (group_enabled(skip_settings))
				{
					shoutcaster_read_result_hook.create(reinterpret_cast<void*>(scrr),
					                                    shoutcaster_read_result_stub);
					shoutcaster_result_neutered = true;
					set_status(71, 1);
				}
			}
			else
			{
				set_status(71, 2);
			}

			// Count group.
			if (group_enabled(skip_counts))
			{
				const auto spc = base() + splitscreen_player_count_rva;
				if (std::memcmp(reinterpret_cast<const void*>(spc),
				                splitscreen_player_count_prologue,
				                sizeof(splitscreen_player_count_prologue)) == 0)
				{
					splitscreen_player_count_hook.create(reinterpret_cast<void*>(spc),
					                                     splitscreen_player_count_stub);
					set_status(90, 1);
				}
				else
				{
					set_status(90, 2);
				}

				// CL_LocalClient_SetActive: the trigger for CL_Init(2) (see
				// set_active_stub).
				const auto sa = base() + set_active_rva;
				if (std::memcmp(reinterpret_cast<void*>(sa), set_active_prologue,
				                sizeof(set_active_prologue)) == 0)
				{
					set_active_hook.create(reinterpret_cast<void*>(sa), set_active_stub);
					set_status(89, 1);
				}
				else
				{
					set_status(89, 2);
				}
			}

			const auto sop = base() + start_op_rva;
			if (std::memcmp(reinterpret_cast<const void*>(sop), start_op_prologue,
			                sizeof(start_op_prologue)) == 0)
			{
				start_op_hook.create(reinterpret_cast<void*>(sop), start_op_stub);
				set_status(42, 1);
			}
			else
			{
				set_status(42, 2);
			}

			// Hook the plural lobby add early, so a ready controller-2 sign-in
			// joins the original loop.
			const auto laa = base() + lobby_add_all_rva;
			if (std::memcmp(reinterpret_cast<const void*>(laa),
			                lobby_add_all_prologue,
			                sizeof(lobby_add_all_prologue)) == 0)
			{
				lobby_add_all_hook.create(reinterpret_cast<void*>(laa),
				                          lobby_add_all_stub);
			}

			// Deactivating splitscreen must take player 3 out too (guest_signin_stub).
			const auto lll = base() + lobbyvm_local_leave_rva;
			if (std::memcmp(reinterpret_cast<const void*>(lll),
			                lobbyvm_local_leave_prologue,
			                sizeof(lobbyvm_local_leave_prologue)) == 0)
			{
				lobbyvm_local_leave_hook.create(reinterpret_cast<void*>(lll),
				                                lobbyvm_local_leave_stub);
			}
			const auto gsi = base() + guest_signin_rva;
			if (std::memcmp(reinterpret_cast<const void*>(gsi),
			                guest_signin_prologue,
			                sizeof(guest_signin_prologue)) == 0)
			{
				guest_signin_hook.create(reinterpret_cast<void*>(gsi),
				                         guest_signin_stub);
			}
			const auto swc = base() + swap_clients_rva;
			if (std::memcmp(reinterpret_cast<const void*>(swc),
			                swap_clients_prologue,
			                sizeof(swap_clients_prologue)) == 0)
			{
				swap_clients_hook.create(reinterpret_cast<void*>(swc), swap_clients_stub);
			}

			// Reap the guests' finished tasks once the whole sweep has returned.
			const auto pcu = base() + per_controller_update_rva;
			if (process_tasks_ok && std::memcmp(reinterpret_cast<const void*>(pcu),
			                                    per_controller_update_prologue,
			                                    sizeof(per_controller_update_prologue)) == 0)
			{
				per_controller_update_hook.create(reinterpret_cast<void*>(pcu),
				                                  per_controller_update_stub);
				per_controller_update_hooked = true;
				set_status(33, 1);
			}
			else
			{
				set_status(33, 2);
			}

			// ezz BOIII detours Storage_Pump itself; its 5-byte jump is accepted too,
			// and invoke() then runs ezz's locked pump.
			const auto pump = base() + storage_pump_rva;
			static constexpr uint8_t storage_pump_after_host_jump[] = {0x40, 0x48, 0x63, 0xF9, 0x8B, 0xCF, 0xE8};
			const bool pump_is_engine = std::memcmp(reinterpret_cast<const void*>(pump), storage_pump_prologue,
			                                        sizeof(storage_pump_prologue)) == 0;
			const bool pump_is_hosted = !pump_is_engine
				&& ezz::host_jump_then(base(), storage_pump_rva, storage_pump_after_host_jump,
				                       sizeof(storage_pump_after_host_jump));
			if (targets_ok && (pump_is_engine || pump_is_hosted))
			{
				if (pump_is_hosted)
				{
					ezz::chained |= 8;
					note("host chain: Storage_Pump stacked on the host's detour");
				}
				storage_pump_hook.create(reinterpret_cast<void*>(pump), storage_pump_stub);
				storage_pump_hooked = true;
				set_status(22, 1);

				// No async fallback pump: it would race the game's own pumping.
			}
			else
			{
				set_status(22, 2); // prologue mismatch - refused to hook
			}
			} // end of the level_full detour block
			set_status(3, 1);
			set_status(5, alloc_regions_seen);
			set_status(6, alloc_free_seen);
			set_status(7, alloc_last_error);
			return true;
		}
	}

	// ---- Silent launch death: who calls exit? ----
	// BOIII routes ExitProcess through pre_destroy(). Log the unwound stack plus
	// stack words pointing into the game image (Arxan code does not always
	// unwind). History: LOG.md, "silent launch death".
	uint64_t component_start_tick = 0;

	void trace_process_exit()
	{
		trace_line l;
		l.str("t=");
		l.dec(GetTickCount64());
		l.str(" PROCESS EXIT after ");
		l.dec(component_start_tick ? (GetTickCount64() - component_start_tick) / 1000 : 0);
		l.str(" s");
		trace_stack(l);
		l.str(" raw:");
		const auto b = base();
		const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
		const auto* word = reinterpret_cast<const uint64_t*>(&l);
		const auto* top = static_cast<const uint64_t*>(tib->StackBase);
		int found = 0;
		for (; word < top && found < 24; ++word)
		{
			const auto v = *word;
			if (v > b + 0x1000 && v < b + 0x1FAB7000)
			{
				l.str(" ");
				l.hex(v - b);
				++found;
			}
		}
		trace_write(l);
	}

	class component final : public generic_component
	{
	public:
		void pre_destroy() override
		{
			if (!game::is_server())
			{
				trace_process_exit();
			}
		}

		void post_unpack() override
		{
			if (game::is_server())
			{
				return;
			}
			component_start_tick = GetTickCount64();

			// Apply immediately: by the time the scheduler runs, Storage_Init has
			// allocated for two controllers. The retry is only a fallback.
			if (!try_apply())
			{
				scheduler::schedule([]
				{
					return try_apply() ? scheduler::cond_end : scheduler::cond_continue;
				}, scheduler::pipeline::async, 500ms);
			}
		}
	};
}

REGISTER_COMPONENT(splitscreen::component)
