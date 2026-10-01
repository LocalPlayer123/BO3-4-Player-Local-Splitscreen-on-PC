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
#include "splitscreen_addresses.hpp"
#include "splitscreen_midhook.hpp"

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
//      whatever the linker put next (tables in splitscreen_addresses.hpp)
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
// on a mismatch; note() says so in the trace file (diagnostic build only).
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
		#include "splitscreen/15_stats_cache.inl"

		// Marks the image as patched: runtime.cpp's patched-twice check reads this
		// magic and stands down instead of applying the component a second time.
		void publish_status_magic()
		{
			auto* s = reinterpret_cast<uint32_t*>(base() + status_rva);
			DWORD old{};
			if (VirtualProtect(s, sizeof(*s), PAGE_READWRITE, &old))
			{
				*s = status_magic;
				DWORD tmp{};
				VirtualProtect(s, sizeof(*s), old, &tmp);
			}
		}

		// Only the stride site is checked here; relocate() dry-runs each table in
		// full and writes nothing unless every reference matches.
		bool ready()
		{
			return std::memcmp(reinterpret_cast<void*>(base() + stride_site_rva),
			                   stride_site_bytes, sizeof(stride_site_bytes)) == 0;
		}

		bool try_apply()
		{
			publish_status_magic();

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
			reserve_gamepads_region();

			// Before Storage_Init the storage pool is still null.
			const auto pool_before = *reinterpret_cast<uint64_t*>(base() + storage_pool_rva);
			const bool early = pool_before == 0;

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
			complete_players_kb();

			// RadiantExploderData changes its internal layout, so it has its own
			// transaction instead of a reloc_tables entry.
			relocate_radiant_exploders();

			// cg_localEntities and friends are [2] (PS4 [4]); slot 2 covered the
			// clientfield system, which CG_InitLocalEntities(2) zeroed.
			relocate_local_entities();

			// The per-client LUI root array is [2] and element 2 would land on
			// s_perController, so it moves before its bound is widened.
			relocate_lui_roots();

			// Per-controller LUI state is [2]; controller 3's "UI active" byte
			// sat in the button-glyph text buffer (pane 4 went grey mid-round).
			relocate_per_controller();

			// Weakpoint / reticle / rocket-launcher / arm-blade HUD tables are sized
			// for two clients; clients 2/3 overran them into the UI model globals.
			relocate_lui_target_tables();
			// Per-client 32-entity marker blocks: clients 2/3 wrote the media table.
			relocate_cg_marker_blocks();

			// The per-client CG/UI context array (stride 0x2BB8) is [2]; client 2
			// sprayed float defaults over the globals in its slot 2.
			relocate_percg_context();

			// Third screen, part 1: geometry. The IsActive cave comes later.
			relocate_view_params();
			// The two [2] arrays the pane path indexes at 2 (scrPlaceView overflowed).
			// The pane bounds refuse without them, so a failure means two panes.
			relocate_flat24("scrPlaceView", scrplaceview_rva, scrplaceview_stride,
			                scrplace_sites, std::size(scrplace_sites),
			                scrplace_relocated, scrplace_new_rva);
			relocate_flat24("perclient54", perclient54_rva, perclient54_stride,
			                perclient54_sites, std::size(perclient54_sites),
			                perclient54_relocated, perclient54_new_rva);
			// AimAssist globals: CG_SetView(2) reads and writes slot 2, so this must
			// land before the pane bound widens (full 50-site table).
			if (!aaglob_relocated)
			{
				const auto fresh = relocate_perclient(aaglob_array);
				if (fresh)
				{
					aaglob_relocated = true;
				}
				else
				{
					note("aaGlobArray: NOT moved - a site did not match");
				}
			}
			// The UI element-handle word array: prerequisite for widening the
			// registrar loop below.
			relocate_flat24("uiElemHandles", uielemhandles_rva, uielemhandles_stride,
			                uielem_sites, std::size(uielem_sites),
			                uielem_relocated, uielem_new_rva);
			retarget_uielem_reader();
			// Needs the array above and the LUI roots relocation, both done by now.
			widen_ui_registrar_bound();
			// Keep the LUI renderer at two contexts - see hold_lui_context_count.
			hold_lui_context_count();
			// And make the HUD-refresh reader survive a client with no snapshot.
			install_snapguard();
			// Move scene buffer B out of A[2]/A[3] before anything reads them.
			relocate_scene_buffer_b();

			// Entity-collision group and the rest of the per-client arrays client 2's
			// cgame reaches once its frame loop ticks.
			relocate_entity_collision();
			relocate_clientfield_callbacks();
			relocate_entword_table();
			relocate_exposure_adaptions();
			relocate_sst_ring();
			// 3 and 4 players in MP: every player's ChooseClass builds ~11.1k
			// model nodes; the stock pool (0x9000) holds two.
			relocate_ui_model_pool();
			// Entering a mode with 3-4 players seated: the party join needs
			// every member's agreement over the lobby message loop.
			relocate_join_clients();
			// MP HUD players 3/4: Engine.GetClientNum answered -1 for them.
			widen_lua_controller_checks();
			// cl_voiceCommunication is moved by reloc_tables' voice_comm.
			// Batches 1-18 and the light queue, in perclient_rows order (10_relocations_b.inl).
			// Before install_perclient_buffer_guard(): its cave bakes C's base.
			relocate_perclient_rows();
			// cg_zbarriers/numcgZBarriers: the two functions that use them, on four rows of ours.
			replace_zbarrier_functions();
			// s_cachedStatsChanges: SetStatChanged and the cache reset over four slots.
			install_stats_cache();
			// A client validated without a stats context must not crash the snapshot.
			hook_if_stock(sv_add_modified_stats_hook, sv_add_modified_stats_rva,
			              sv_add_modified_stats_prologue, sv_add_modified_stats_stub);
			// Before R_Init allocates the culler object (see grow_umbra_client_arrays).
			grow_umbra_client_arrays();
			install_perclient_buffer_guard();
			install_guest_copy();
			widen_csc_lc_checks();
			widen_odd_lc_checks();
			install_lc_bound_hooks();
			// Lens flares for clients 2/3: a second manager, else off for them.
			if (!route_lensflares_for_extra_clients())
			{
				gate_lensflares_for_extra_clients();
			}
			// Before the clamp, which bounds the slot by these slices.
			grow_sun_shadow_slices();
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
			if (table_slot != SIZE_MAX && array_slot != SIZE_MAX)
			{
				link_client_objects(table_slot, array_slot);
			}

			// Local clients 2/3 get their command buffers (MP class choice,
			// every "cmd" a guest sends) - needs the relocated cbuf records above.
			install_cbuf_for_players34();

			// These reach s_storage[2]/[3], which are ours only after the relocation.
			if (storage_moved)
			{
				apply_storage_patches();
			}

			// The stride fix is not tied to s_storage.
			install_stride_fix();

			// Count patches only after every container relocation succeeded: raising
			// the count over a still-[2] array is the known crash family.
			const auto counts_ok = ok == std::size(reloc_tables);

			// The cl_maxLocalClients hold has the same precondition.
			raise_local_client_count = counts_ok;

			if (counts_ok)
			{
				// Let the engine's own active count reach 3 and 4.
				install_active_count_fix();

				apply_local_client_count_patches();
				// Walker end-bounds: safe only with voice_comm moved (implied by counts_ok).
				widen_client_ui_walker_bounds();
				// Same gate: the end-of-match loops walk clientUIActives slot 2.
				widen_client_shutdown_loops();
			}

			// cl_maxLocalClients is not seeded here (not yet 2 at post_unpack); the
			// async probe seeds it once, when it first reads 2.

			// Relocate clientGameStates so Com_ControllerIndex_GetLocalClientNum(2)
			// returns 2, not -1. On PS4 that gates the gumball row, guest menu input
			// and the per-player UI models.
			relocate_signin_field();

			// Third screen, part 2. install_isactive_hook() answers IsActive(lc >= 2)
			// (0 until cl_maxLocalClients covers the client); it waits for
			// relocate_signin_field(). Pane counts and bounds come last.
			install_isactive_hook();
			install_pane_counts_and_bounds();
			// Also after relocate_signin_field(): reads seat record 2.
			widen_gamepad_button_models();
			widen_lobby_max_local_players();

			// Hold the injected clients on both pipelines: async stops during a
			// launch, and a hold registered only there hung the load. The hold only
			// moves values forward, so running it twice is idempotent.
			scheduler::loop(hold_injected_clients, scheduler::pipeline::async, 5ms);
			scheduler::loop(hold_injected_clients, scheduler::pipeline::renderer, 5ms);

			// Fill the guest records once element 1 is a signed-in profile. Fast
			// tick: the window closes when the boot storage pass runs.
			scheduler::loop(fill_guests_when_ready, scheduler::pipeline::async, 5ms);

			// Renderer pipeline: it keeps running through a launch, when client 2
			// still has no scene buffers.
			scheduler::loop(fill_scene_buffers, scheduler::pipeline::renderer, 100ms);
			scheduler::loop(maintain_sun_trans_views, scheduler::pipeline::renderer, 100ms);
			scheduler::loop(cl_init_watch, scheduler::pipeline::renderer, 250ms);
			scheduler::loop(mirror_signin_state, scheduler::pipeline::async, 50ms);
			scheduler::loop(maintain_signin_seats, scheduler::pipeline::async, 50ms);

			// s_targets must be widened before anything pumps controller 2 or 3.
			const auto targets_ok = widen_storage_targets();

			// Before controller 2 does any local-file work.
			widen_local_file_ops();

			hook_if_stock(storage_read_hook, storage_read_rva, storage_read_prologue, storage_read_stub);

			// Settings completion, neutered for guests only. Without this hook file 0
			// must stay out of guest_seated_file_types.
			settings_result_neutered = hook_if_stock(settings_read_result_hook, settings_read_result_rva,
			                                         settings_read_result_prologue,
			                                         settings_read_result_stub);
			shoutcaster_result_neutered = hook_if_stock(shoutcaster_read_result_hook,
			                                            shoutcaster_read_result_rva,
			                                            shoutcaster_read_result_prologue,
			                                            shoutcaster_read_result_stub);

			player_count_detoured = hook_if_stock(splitscreen_player_count_hook, splitscreen_player_count_rva,
			                                      splitscreen_player_count_prologue, splitscreen_player_count_stub);
			// A pad plugged in by a lone player stays player 1 (see assign_player_count).
			install_assign_player_count();
			// Debris cleanup: two view origins fit its stack array (see the function).
			bound_dynent_cleanup_viewers();

			// CL_LocalClient_SetActive: the trigger for CL_Init(2) (see set_active_stub).
			hook_if_stock(set_active_hook, set_active_rva, set_active_prologue, set_active_stub);

			// Hook the plural lobby add early, so a ready controller-2 sign-in joins the
			// original loop.
			hook_if_stock(lobby_add_all_hook, lobby_add_all_rva, lobby_add_all_prologue,
			              lobby_add_all_stub);
			// Deactivating splitscreen must take player 3 out too (guest_signin_stub).
			hook_if_stock(lobbyvm_local_leave_hook, lobbyvm_local_leave_rva,
			              lobbyvm_local_leave_prologue, lobbyvm_local_leave_stub);
			hook_if_stock(guest_signin_hook, guest_signin_rva, guest_signin_prologue,
			              guest_signin_stub);
			hook_if_stock(swap_clients_hook, swap_clients_rva, swap_clients_prologue,
			              swap_clients_stub);

			// Advances the guest joins on the game's own thread.
			hook_if_stock(per_controller_update_hook, per_controller_update_rva,
			              per_controller_update_prologue, per_controller_update_stub);

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
				}
				storage_pump_hook.create(reinterpret_cast<void*>(pump), storage_pump_stub);

				// No async fallback pump: it would race the game's own pumping.
			}
			else
			{
				note("Storage_Pump: not hooked - s_targets not widened or prologue differs");
			}
			return true;
		}
	}

	class component final : public generic_component
	{
	public:
		void post_unpack() override
		{
			if (game::is_server())
			{
				return;
			}

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
