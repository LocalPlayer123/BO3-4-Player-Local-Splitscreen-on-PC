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
		#include "splitscreen/01_core.inl"
		#include "splitscreen/02_guest_storage.inl"
		#include "splitscreen/03_signin_seats.inl"
		#include "splitscreen/04_panes.inl"
		#include "splitscreen/05_renderer_scene.inl"
		#include "splitscreen/06_relocations_a.inl"
		#include "splitscreen/07_trace.inl"
		#include "splitscreen/08_guest_copy.inl"
		#include "splitscreen/09_script_bounds.inl"
		#include "splitscreen/10_relocations_b.inl"
		#include "splitscreen/11_sun_shadow.inl"
		#include "splitscreen/12_relocations_ui.inl"
		#include "splitscreen/13_lobby_join.inl"
		#include "splitscreen/14_player4_fixes.inl"

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
