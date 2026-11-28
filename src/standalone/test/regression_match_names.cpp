#include "regression_check.hpp"
#include "component/splitscreen_names.hpp"
#include <sys/mman.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace splitscreen;
constexpr uint32_t signin_new_base = 0x1A828500, signin_stride = 0x24;
constexpr uint32_t cl_max_local_clients_rva = 0x05323720;
constexpr size_t cgs_flags = 0, cgs_local_client_num = 4, cgs_controller_index = 8;
constexpr size_t image_size = 0x1A830000;
auto* image = static_cast<uint8_t*>(mmap(nullptr, image_size, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
std::vector<uint8_t> cg(4 * 0x342720);
char rendered[40];
uintptr_t base() { return reinterpret_cast<uintptr_t>(image); }
bool signin_relocated = true, offline = true, frontend = false;
uintptr_t denied = 0;
bool readable(const void* p, size_t n)
{
    const auto at = reinterpret_cast<uintptr_t>(p);
    if (!at || at == denied) return false;
    for (const auto region : {std::pair<uintptr_t, size_t>{base(), image_size},
        {reinterpret_cast<uintptr_t>(cg.data()), cg.size()},
        {reinterpret_cast<uintptr_t>(rendered), sizeof(rendered)}})
        if (at >= region.first && at - region.first <= region.second
            && n <= region.second - (at - region.first)) return true;
    // Formatter output and its local plain-name scratch are stack buffers.
    const auto stack = reinterpret_cast<uintptr_t>(&at);
    return at > stack - 1024 * 1024 && at < stack + 1024 * 1024 && n <= 1024;
}
bool names_offline() { return offline; }
namespace game { bool Com_IsRunningUILevel() { return frontend; } }
void note(const char*, ...) {}
std::atomic<const names::slots*> names_published{nullptr};
int stock_result = 1, stock_calls = 0;
bool plain_failure = false, unterminated = false, malformed_clan = false;
int stock(int, int, char* out, int capacity, bool clan)
{
    ++stock_calls;
    if (stock_result == 0 || (plain_failure && !clan)) return 0;
    if (!out || capacity <= 0) return stock_result;
    if (unterminated) { std::memset(out, 'x', capacity); return stock_result; }
    const std::string name = clan ? (malformed_clan ? "[TAG]other" : "[TAG]original(2)") : "original(2)";
    const auto size = std::min<size_t>(name.size(), capacity - 1);
    std::memcpy(out, name.data(), size); out[size] = 0;
    return stock_result;
}
bool allocation_ok = true, allocation_throws = false, hook_throws = false;
int allocations = 0, writes = 0, fail_write = 0, creates = 0, clears = 0;
void* allocate_near_module(size_t n)
{
    CHECK(n == 17); ++allocations;
    if (allocation_throws) throw std::runtime_error("allocation");
    return allocation_ok ? image + 0x1000000 : nullptr;
}
bool write_bytes(void* at, const void* data, size_t n)
{
    if (++writes == fail_write) return false;
    std::memcpy(at, data, n); return true;
}
namespace utils::hook
{
    struct detour
    {
        template<typename T, typename... Args> T invoke(Args... args)
        { return reinterpret_cast<T (*)(Args...)>(&stock)(args...); }
        void create(size_t, void*)
        { ++creates; if (hook_throws) throw std::runtime_error("hook"); }
        void clear() { ++clears; }
    };
}
#include "production_match_byte_check.hpp"
#include "component/splitscreen/19_match_names.inl"

template<typename T> void put(uintptr_t at, T value) { std::memcpy(reinterpret_cast<void*>(at), &value, sizeof(value)); }
template<size_t N> void seed(uint32_t rva, const uint8_t (&bytes)[N]) { std::memcpy(image + rva, bytes, N); }
names::slots custom = {"Host", "Guest", "Third", "Fourth"};
void reset()
{
    denied = 0; allocations = writes = fail_write = creates = clears = stock_calls = 0;
    allocation_ok = signin_relocated = offline = true;
    allocation_throws = hook_throws = frontend = plain_failure = unterminated = malformed_clan = false;
    stock_result = 1; names_match_ready = false; names_draw_context = {};
    names_published = &custom;
    seed(names_match_formatter_rva, names_match_formatter_bytes);
    seed(0x0069D641, names_cg_layout_bytes); seed(0x0069D7E7, names_cg_client_bytes);
    seed(names_cache_call_rva - 9, names_cache_call_bytes);
    seed(names_cache_update_rva, names_cache_update_bytes);
    // Use the real relocated-LEA policy: all three must point at the same buffer.
    for (const auto rva : {0x00677B94u, 0x0069D795u, 0x006A7AEBu})
    {
        image[rva] = 0x48; image[rva + 1] = 0x8D;
        image[rva + 2] = rva == 0x00677B94 ? 0x0D : 0x05;
        put<int32_t>(base() + rva + 3, 0x1100000 - rva - 7);
    }
    put<uint32_t>(base() + cl_max_local_clients_rva, 4);
    for (int i = 0; i < 4; ++i)
    {
        const auto seat = base() + signin_new_base + i * signin_stride;
        put<uint32_t>(seat, 1); put<int>(seat + 4, i); put<int>(seat + 8, i);
        put<int>(reinterpret_cast<uintptr_t>(cg.data()) + i * names_cg_stride, i);
        put<uintptr_t>(reinterpret_cast<uintptr_t>(cg.data()) + i * names_cg_stride + 0x30, 0x123456);
    }
}
constexpr uint32_t host_relay_rva = 0x1500000, host_code_rva = 0x1600000;
void seed_host_formatter()
{
    auto* entry = image + names_match_formatter_rva;
    entry[0] = 0xE9;
    put<int32_t>(base() + names_match_formatter_rva + 1, host_relay_rva - names_match_formatter_rva - 5);
    auto* trampoline = image + host_relay_rva - 19;
    std::memcpy(trampoline, names_match_formatter_bytes, 5);
    const uint8_t jump[] = {0xFF,0x25,0,0,0,0};
    std::memcpy(trampoline + 5, jump, sizeof(jump));
    put<uintptr_t>(reinterpret_cast<uintptr_t>(trampoline + 11), base() + names_match_formatter_rva + 5);
    std::memcpy(trampoline + 19, jump, sizeof(jump));
    put<uintptr_t>(reinterpret_cast<uintptr_t>(trampoline + 25), base() + host_code_rva);
    seed(host_code_rva, names_formatter_host_bytes);
}
int target_client = 1, target_viewer = 0;
bool want_clan = false, throw_update = false, nested = false;
void update(int viewer, int client)
{
    CHECK(viewer == target_viewer && client == target_client);
    CHECK(names_draw_context.viewer == viewer);
    if (throw_update) throw std::runtime_error("stock updater");
    if (nested)
    {
        const auto outer = names_draw_context;
        nested = false;
        names_cache_stub(viewer, client, outer.cg);
        CHECK(names_draw_context.viewer == outer.viewer && names_draw_context.cg == outer.cg);
    }
    std::memset(rendered, 0x5A, sizeof(rendered));
    CHECK(names_match_stub(viewer, client, rendered, want_clan ? 40 : 32, want_clan) == stock_result);
}
void draw(int viewer, int client, bool clan)
{
    target_viewer = viewer; target_client = client; want_clan = clan;
    names_cache_update = &update;
    names_cache_stub(viewer, client, reinterpret_cast<uintptr_t>(cg.data()) + viewer * names_cg_stride);
    CHECK(names_draw_context.viewer == -1 && names_draw_context.cg == 0);
}

int main()
{
    CHECK(image != MAP_FAILED);
    // The unchanged production controller mapper and formatter run for all viewers,
    // shuffled match client numbers and controllers (including players 3 and 4).
    const int clients[] = {7, 11, 4, 2}, controllers[] = {2, 0, 3, 1};
    reset();
    for (int i = 0; i < 4; ++i)
    {
        put<int>(base() + signin_new_base + i * signin_stride + 8, controllers[i]);
        put<int>(reinterpret_cast<uintptr_t>(cg.data()) + i * names_cg_stride, clients[i]);
    }
    for (int viewer = 0; viewer < 4; ++viewer) for (int i = 0; i < 4; ++i) for (bool clan : {false, true})
    {
        draw(viewer, clients[i], clan);
        if (std::string(rendered) != (clan ? "[TAG]" : "") + custom[controllers[i]])
        {
            names_draw_context = {viewer, reinterpret_cast<uintptr_t>(cg.data()) + viewer * names_cg_stride};
            std::cerr << "viewer=" << viewer << " client=" << clients[i] << " clan=" << clan
                << " mapping=" << names_match_controller(viewer, clients[i]) << " got=" << rendered << '\n';
            names_draw_context = {};
        }
        CHECK(std::string(rendered) == (clan ? "[TAG]" : "") + custom[controllers[i]]);
        CHECK(rendered[clan ? 39 : 31] == char(0x5A)); // only output text written
    }
    // No valid draw scope means no override, even when all seats are present.
    reset(); char out[40]; names_match_stub(0, 1, out, 32, false);
    CHECK(std::string(out) == "original(2)");
    for (int failure = 0; failure < 18; ++failure)
    {
        reset();
        const auto seat = base() + signin_new_base + signin_stride;
        if (failure == 0) offline = false;
        if (failure == 1) frontend = true;
        if (failure == 2) signin_relocated = false;
        if (failure == 3) names_published = nullptr;
        if (failure == 4) put<int>(seat + 8, 4);
        if (failure == 5) put<int>(seat + 4, 0);
        if (failure == 6) put<uint32_t>(seat, 0);
        if (failure == 7) put<int>(reinterpret_cast<uintptr_t>(cg.data()) + names_cg_stride, 64);
        if (failure == 8) put<uintptr_t>(reinterpret_cast<uintptr_t>(cg.data()) + names_cg_stride + 0x30, 0);
        if (failure == 9) put<int>(reinterpret_cast<uintptr_t>(cg.data()) + 2 * names_cg_stride, 1);
        if (failure == 10) put<int>(base() + signin_new_base + 2 * signin_stride + 8, 1);
        if (failure == 11) put<uint32_t>(base() + cl_max_local_clients_rva, 0);
        if (failure == 12) put<uint32_t>(base() + cl_max_local_clients_rva, 5);
        if (failure == 13) denied = base() + cl_max_local_clients_rva;
        if (failure == 14) denied = seat;
        if (failure == 15) denied = reinterpret_cast<uintptr_t>(cg.data()) + names_cg_stride;
        if (failure == 16) plain_failure = true;
        if (failure == 17) malformed_clan = true;
        draw(0, 1, true); CHECK(std::string(rendered) == (failure == 17 ? "[TAG]other" : "[TAG]original(2)"));
    }
    reset(); draw(0, 38, false); CHECK(std::string(rendered) == "original(2)");
    const names::slots empty{}; names_published = &empty;
    draw(0, 1, false); CHECK(std::string(rendered) == "original(2)");
    for (uintptr_t invalid : {uintptr_t(0), uintptr_t(1), UINTPTR_MAX})
    {
        reset(); names_draw_context = {1, invalid}; CHECK(names_match_controller(1, 1) == -1);
    }
    reset(); names_draw_context = {0, reinterpret_cast<uintptr_t>(cg.data())};
    CHECK(names_match_controller(1, 1) == -1);
    for (int capacity : {0, 1, 1025})
    {
        stock_result = 0; CHECK(names_match_stub(0, 1, out, capacity, false) == 0);
    }
    stock_result = 7; CHECK(names_match_stub(0, 1, nullptr, 32, false) == 7);
    unterminated = true; names_match_stub(0, 1, out, 32, false);
    CHECK(std::string(out, 32) == std::string(32, 'x'));
    reset(); nested = true; draw(0, 1, false); CHECK(std::string(rendered) == "Guest");
    throw_update = true;
    try { draw(0, 1, false); CHECK(false); } catch (const std::runtime_error&) {}
    CHECK(names_draw_context.viewer == -1 && names_draw_context.cg == 0); throw_update = false;
    reset(); names_draw_context = {0, reinterpret_cast<uintptr_t>(cg.data())};
    std::thread other([] { CHECK(names_draw_context.viewer == -1 && names_draw_context.cg == 0); }); other.join();
    names_draw_context = {};

    // Every guarded byte fails closed; only the synchronized relocation operands
    // are allowed to differ from stock. No code/site writes occur on mismatches.
    const std::pair<uint32_t, size_t> guards[] = {{names_match_formatter_rva, sizeof(names_match_formatter_bytes)},
        {0x0069D641, sizeof(names_cg_layout_bytes)}, {0x0069D7E7, sizeof(names_cg_client_bytes)},
        {names_cache_call_rva - 9, sizeof(names_cache_call_bytes)}, {names_cache_update_rva, sizeof(names_cache_update_bytes)}};
    for (const auto guard : guards) for (size_t i = 0; i < guard.second; ++i)
    {
        reset(); image[guard.first + i] ^= 1;
        install_match_names(); CHECK(!names_match_ready && allocations == 0 && writes == 0);
    }
    for (int failure = 0; failure < 6; ++failure)
    {
        reset();
        if (failure == 0) allocation_ok = false;
        if (failure == 1) allocation_throws = true;
        if (failure == 2) fail_write = 1;
        if (failure == 3) hook_throws = true;
        if (failure == 4) fail_write = 2;
        if (failure == 5) denied = base() + 0x1100000;
        install_match_names(); CHECK(!names_match_ready);
        CHECK(std::memcmp(image + names_cache_call_rva, names_cache_call_bytes + 9, 5) == 0);
        CHECK(clears == (failure == 4 ? 1 : 0));
    }
    reset(); install_match_names(); CHECK(names_match_ready && creates == 1 && writes == 2);
    CHECK(image[names_cache_call_rva] == 0xE8);
    int32_t distance{}; std::memcpy(&distance, image + names_cache_call_rva + 1, 4);
    CHECK(base() + names_cache_call_rva + 5 + distance == base() + 0x1000000);
    const uint8_t prefix[] = {0x49,0x89,0xD8,0xFF,0x25,0,0,0,0};
    CHECK(std::memcmp(image + 0x1000000, prefix, sizeof(prefix)) == 0);
    uintptr_t destination{}; std::memcpy(&destination, image + 0x1000000 + 9, 8);
    CHECK(destination == reinterpret_cast<uintptr_t>(&names_cache_stub));
    install_match_names(); CHECK(allocations == 1 && writes == 2 && creates == 1);
    // Real deployment shape: ezz's formatter is already hooked before this
    // plugin starts. The valid host form must install, not silently stand down.
    reset(); seed_host_formatter(); CHECK(names_formatter_bytes_match());
    install_match_names(); CHECK(names_match_ready && creates == 1 && writes == 2);
    for (const auto guard : {std::pair<uint32_t, size_t>{host_relay_rva - 19, 33},
        {host_code_rva, sizeof(names_formatter_host_bytes)},
        {names_match_formatter_rva + 5, sizeof(names_match_formatter_bytes) - 5}})
        for (size_t i = 0; i < guard.second; ++i)
        {
            reset(); seed_host_formatter(); image[guard.first + i] ^= 1;
            install_match_names(); CHECK(!names_match_ready && allocations == 0 && writes == 0);
        }
    for (const auto bad : {uintptr_t(0), uintptr_t(1), reinterpret_cast<uintptr_t>(&names_match_stub)})
    {
        reset(); seed_host_formatter(); put<uintptr_t>(base() + host_relay_rva + 6, bad);
        install_match_names(); CHECK(!names_match_ready && allocations == 0);
    }
    reset(); seed_host_formatter(); denied = base() + host_relay_rva - 19;
    install_match_names(); CHECK(!names_match_ready && allocations == 0);
    reset(); seed_host_formatter(); denied = base() + host_code_rva;
    install_match_names(); CHECK(!names_match_ready && allocations == 0);
    munmap(image, image_size);
    std::cout << "PASS production scoped overhead mapping/formatter, stock fallbacks, nested/thread-local lifetime, stock/ezz guards and checked relay install\n";
}