#include "regression_check.hpp"
#include "component/splitscreen_safety.hpp"
#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>
namespace safety = splitscreen::safety;
namespace utils::hook { struct detour {}; }
void note(const char*, ...) {}
uintptr_t base() { return 0; } // RVAs below are mock function/data addresses
int decoded = 0, decodes = 0, errors = 0;
std::vector<int> flushed;
int decode(const char* msg, uint8_t*, int capacity)
{ CHECK(std::strcmp(msg, "payload") == 0); CHECK(capacity == 64); ++decodes; return decoded; }
void error(const char*, int, int, const char*, ...) { ++errors; }
void internal(int, int, const uint8_t*, int size, int) { flushed.push_back(size); }
int register_hwm(const char*) { return 1; }
void set_hwm(int, uint64_t) {}
uint32_t guard = 0;
int32_t id = 0;
constexpr size_t statscache_stride = 0x4404;
const uintptr_t com_decode_yenc_rva = reinterpret_cast<uintptr_t>(&decode);
const uintptr_t com_error_rva = reinterpret_cast<uintptr_t>(&error);
const uintptr_t set_stat_changed_internal_rva = reinterpret_cast<uintptr_t>(&internal);
const uintptr_t bb_register_hwm_rva = reinterpret_cast<uintptr_t>(&register_hwm);
const uintptr_t bb_set_hwm_rva = reinterpret_cast<uintptr_t>(&set_hwm);
const uintptr_t statscache_hwm_guard_rva = reinterpret_cast<uintptr_t>(&guard);
const uintptr_t statscache_hwm_id_rva = reinterpret_cast<uintptr_t>(&id);
const uintptr_t str_empty_rva = reinterpret_cast<uintptr_t>("");
const uintptr_t str_statsoverflow_rva = reinterpret_cast<uintptr_t>("overflow");
const uintptr_t str_statscache2_rva = reinterpret_cast<uintptr_t>("cache");
alignas(8) unsigned char engine_cache[statscache_stride * 2]{};
const uintptr_t statscache_rva = reinterpret_cast<uintptr_t>(engine_cache);
#include "production_stats.hpp"
int main()
{
    stats_cache cache{};
    for (int size : {-1, 65, INT32_MAX})
    {
        decoded = size; const int old = decodes;
        set_stat_changed(cache, 0, 2, "!payload", 0, false);
        CHECK(decodes == old + 1 && cache.count == 0 && flushed.empty());
    }
    for (int count : {-1, INT32_MIN, 257, INT32_MAX})
    {
        cache.count = count; const int old = decodes;
        set_stat_changed(cache, 0, 2, "!payload", 0, false);
        CHECK(cache.count == 0 && decodes == old && flushed.empty());
    }
    for (const char* message : {static_cast<const char*>(nullptr), ""})
    {
        cache.count = 1; set_stat_changed(cache, 0, 2, message, 0, false); CHECK(cache.count == 0);
    }
    decoded = 64;
    set_stat_changed(cache, 0, 2, "!payload", 1, true);
    CHECK(cache.count == 1 && flushed.empty());
    decoded = -1;
    set_stat_changed(cache, 0, 2, "!payload", 0, false);
    CHECK(cache.count == 1 && flushed.empty()); // preserve previously queued valid entry
    cache.changes[1].size = -1; cache.changes[2].size = 65; cache.count = 3;
    decoded = 0;
    set_stat_changed(cache, 0, 2, "!payload", 0, true);
    CHECK(cache.count == 0 && flushed == std::vector<int>({64, 0}) && guard == 1);
    cache.count = 256; const int old = decodes;
    set_stat_changed(cache, 0, 2, "!payload", 0, false);
    CHECK(errors == 1 && decodes == old && cache.count == 256); // ERR_DROP mock returns
    CHECK(stats_cache_for(-1) == nullptr && stats_cache_for(4) == nullptr);
    CHECK(stats_cache_for(2) == &statscache_extra[0] && stats_cache_for(3) == &statscache_extra[1]);
    statscache_extra[0].count = statscache_extra[1].count = 5;
    CHECK(stats_cache_reset_stub(engine_cache, 0, sizeof(engine_cache)) == engine_cache);
    CHECK(statscache_extra[0].count == 0 && statscache_extra[1].count == 0);
    std::puts("PASS stats: actual decoder guards/count/cache flush/reset/returning ERR_DROP");
}