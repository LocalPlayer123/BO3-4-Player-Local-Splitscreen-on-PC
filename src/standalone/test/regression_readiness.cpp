#include "regression_check.hpp"
#include <initializer_list>

bool raise_local_client_count = false;
bool local_entities_relocated = true, entcoll_relocated = true, cf_relocated = true;
bool percg_relocated = true, lui_roots_relocated = true, uielem_relocated = true;
bool scrplace_relocated = true, perclient54_relocated = true, aaglob_relocated = true;
bool lightobj_fix_installed = true;
int installed = 0;
void install_active_count_fix() { ++installed; }
void apply_local_client_count_patches() { ++installed; }
void widen_client_ui_walker_bounds() { ++installed; }
void widen_client_shutdown_loops() { ++installed; }
void widen_lobby_max_local_players() { ++installed; }
void note(const char*) {}
#include "production_readiness.hpp"

int main()
{
    // Every prerequisite independently blocks all count/loop/capacity changes.
    critical_tables_ready = true;
    for (auto* flag : {&critical_tables_ready, &local_entities_relocated, &entcoll_relocated,
                      &cf_relocated, &percg_relocated, &lui_roots_relocated, &uielem_relocated,
                      &scrplace_relocated, &perclient54_relocated, &aaglob_relocated,
                      &lightobj_fix_installed})
    {
        *flag = false;
        CHECK(!enable_extra_clients_when_ready());
        CHECK(!raise_local_client_count && installed == 0);
        *flag = true;
    }
    // Simulate the safe local-entities retry between matches.
    local_entities_relocated = false;
    CHECK(!enable_extra_clients_when_ready());
    local_entities_relocated = true;
    CHECK(enable_extra_clients_when_ready());
    CHECK(raise_local_client_count && installed == 5);
    CHECK(enable_extra_clients_when_ready());
    CHECK(installed == 5); // never apply startup patches twice
    std::puts("PASS readiness: every prerequisite, deferred success, idempotent enable");
}