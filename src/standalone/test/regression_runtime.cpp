#include "regression_check.hpp"
#include <cstddef>
#include <cstdint>
// The Windows build's void* -> function-pointer static_cast is an MSVC
// extension. Adapt only casts in this declaration header for native GCC;
// runtime.cpp and every tested runtime function body remain unmodified.
#define static_cast reinterpret_cast
#include "standalone/compat/utils/hook.hpp"
#undef static_cast
#include "standalone/runtime.cpp"
#include <array>
#include <string>
void original_renderer() { }
int renderer_calls = 0;
void counting_renderer() { ++renderer_calls; }
void reset_writes()
{
    winmock::protect_calls = winmock::protect_failure = winmock::flush_calls = 0;
    winmock::flush_ok = true;
}
void hooks()
{
    unsigned char memory[] = {1, 2, 3}, replacement[] = {4, 5, 6};
    reset_writes(); winmock::protect_failure = 1;
    throws([&] { utils::hook::write(memory, replacement, 3); });
    CHECK(memory[0] == 1 && winmock::flush_calls == 0);
    reset_writes(); winmock::protect_failure = 2;
    throws([&] { utils::hook::write(memory, replacement, 3); });
    CHECK(memory[0] == 4 && winmock::protect_calls == 2 && winmock::flush_calls == 1);
    reset_writes(); winmock::flush_ok = false;
    throws([&] { utils::hook::write(memory, replacement, 3); });
    CHECK(winmock::protect_calls == 2 && winmock::flush_calls == 1);
    reset_writes(); utils::hook::write(memory, replacement, 3);
    CHECK(winmock::protect_calls == 2 && winmock::flush_calls == 1);
    utils::hook::detour hook;
    mhmock::create = MH_ERROR_UNKNOWN;
    throws([&] { hook.create(memory, replacement); });
    CHECK(hook.get_place() == nullptr && hook.get_original() == nullptr && mhmock::remove_calls == 0);
    mhmock::create = MH_OK; mhmock::enable = MH_ERROR_UNKNOWN;
    throws([&] { hook.create(memory, replacement); });
    CHECK(hook.get_place() == nullptr && hook.get_original() == nullptr && mhmock::remove_calls == 1);
    mhmock::enable = MH_OK; mhmock::original = reinterpret_cast<void*>(&original_renderer);
    hook.create(memory, replacement);
    CHECK(hook.get_place() == memory && hook.get_original() == mhmock::original);
    hook.clear(); CHECK(mhmock::remove_calls == 2 && hook.get_original() == nullptr);
    hook.clear(); CHECK(mhmock::remove_calls == 2);
    CHECK(mhmock::init_calls == 1);
}
void initialization_failure()
{
    mhmock::init = MH_ERROR_UNKNOWN;
    utils::hook::detour hook;
    throws([&] { hook.create(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2)); });
    CHECK(mhmock::create_calls == 0 && hook.get_place() == nullptr);
}
void scheduling()
{
    int bad = 0, good = 0, loops = 0, nested = 0;
    scheduler::once([&] { ++bad; throw std::runtime_error("callback"); }, scheduler::renderer);
    scheduler::once([&] { ++good; scheduler::once([&] { ++nested; }, scheduler::renderer); }, scheduler::renderer);
    scheduler::schedule([&] { return ++loops == 2 ? scheduler::cond_end : scheduler::cond_continue; }, scheduler::renderer);
    scheduler::pipelines()[scheduler::renderer].execute();
    CHECK(bad == 1 && good == 1 && loops == 1 && nested == 0);
    scheduler::pipelines()[scheduler::renderer].execute();
    scheduler::pipelines()[scheduler::renderer].execute();
    CHECK(bad == 1 && good == 1 && loops == 2 && nested == 1);
    mhmock::original = reinterpret_cast<void*>(&counting_renderer);
    scheduler::r_end_frame_hook().create(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2));
    scheduler::once([] { throw std::runtime_error("renderer callback"); }, scheduler::renderer);
    scheduler::r_end_frame_stub(); CHECK(renderer_calls == 1);
    scheduler::r_end_frame_stub(); CHECK(renderer_calls == 2);
    std::vector<int> order;
    struct component : generic_component
    {
        std::vector<int>& order;
        int id;
        component(std::vector<int>& o, int i) : order(o), id(i) { order.push_back(id); }
        void post_load() override { order.push_back(id + 10); }
        void post_unpack() override { order.push_back(id + 20); }
    };
    component_loader::register_component([&] { return std::make_unique<component>(order, 1); }, component_type::client);
    component_loader::register_component([&] { return std::make_unique<component>(order, 2); }, component_type::any);
    component_loader::register_component([&] { return std::make_unique<component>(order, 3); }, component_type::server);
    component_loader::activate_client_components();
    CHECK(order == std::vector<int>({1, 2, 11, 12, 21, 22}));
}
void startup(bool unsupported_only)
{
    alignas(8) std::array<unsigned char, 512> image{};
    winmock::module = image.data(); winmock::module_size = image.size();
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE; dos->e_lfanew = sizeof(*dos);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data() + dos->e_lfanew);
    nt->Signature = IMAGE_NT_SIGNATURE; nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    CHECK(!runtime::start() && mhmock::create_calls == 0); // unsupported checksum remains retryable
    if (unsupported_only) return;
    nt->OptionalHeader.CheckSum = 0x06531394;
    mhmock::enable = MH_ERROR_UNKNOWN; // startup fails before spawning detached scheduler
    CHECK(runtime::start());
    CHECK(mhmock::create_calls == 1 && mhmock::remove_calls == 1);
    CHECK(game::get_base() == reinterpret_cast<size_t>(image.data()));
    mhmock::enable = MH_OK;
    CHECK(runtime::start() && mhmock::create_calls == 1); // consumed, never retry partial patches
}
int main(int argc, char** argv)
{
    CHECK(argc == 2); const std::string scenario = argv[1];
    if (scenario == "hooks") hooks();
    else if (scenario == "init-fail") initialization_failure();
    else if (scenario == "scheduler") scheduling();
    else if (scenario == "startup") startup(false);
    else if (scenario == "unsupported") startup(true);
    else CHECK(false);
    CHECK(release_policy::get_environment_variable("LOCALAPPDATA", nullptr, 0) == 0);
    CHECK(winmock::last_error == ERROR_ENVVAR_NOT_FOUND);
    CHECK(release_policy::create_file("anything", 0, 0, nullptr, 0, 0, nullptr) == INVALID_HANDLE_VALUE);
    std::printf("PASS actual runtime.cpp (mock Windows/MinHook): %s\n", argv[1]);
}