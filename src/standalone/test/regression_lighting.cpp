#include "regression_check.hpp"
#include "component/splitscreen_safety.hpp"
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
namespace safety = splitscreen::safety;
namespace midhook {
#include "production_context.hpp"
}
uintptr_t base() { return 0; }
#include "production_lighting.hpp"
// Reproduce only the context/XMM stack storage, NOT trampoline instructions.
struct alignas(16) frame
{
    float xmm[32]{};
    midhook::context context{};
};
int main()
{
    alignas(8) unsigned char object[lightobj_stride]{};
    const auto address = reinterpret_cast<uintptr_t>(object);
    *reinterpret_cast<float*>(object + lightobj_value_off) = 1.25f;
    object[lightobj_flags_off] = 0;
    frame f;
    f.context.rcx = address; f.context.r9 = 2; f.context.r10 = 0;
    *saved_xmm(f.context, 3) = 9.5f;
    lightobj_value_set(f.context);
    CHECK(f.context.rip == lightobj_value_set_next_rva);
    f.context.rcx = address; f.context.r10 = 2; f.context.r11 = 0; f.context.rdx = 1;
    lightobj_flag_set(f.context);
    {
        std::lock_guard lock(lightobj_mutex());
        auto* s = lightobj_slot(2, address, 0);
        CHECK(lightobj_value_of(*s, address, 0) == 9.5f && lightobj_flag_of(*s, address, 0) == 1);
        CHECK(!lightobj_slot(1, address, 0) && !lightobj_slot(4, address, 0));
        CHECK(!lightobj_slot(2, 0, 0) && !lightobj_slot(2, address, lightobj_max));
    }
    reset_lightobj_state(); // same-address map replacement must not retain values/flags
    {
        std::lock_guard lock(lightobj_mutex());
        for (uint64_t lc : {2, 3})
        {
            auto* s = lightobj_slot(lc, address, 0);
            CHECK(lightobj_value_of(*s, address, 0) == 1.25f && lightobj_flag_of(*s, address, 0) == 0);
        }
    }
    std::vector<unsigned char> view(lightobj_view_list_off + sizeof(uint64_t));
    alignas(8) unsigned char list[lightobj_list_objects_off + sizeof(uint64_t)]{};
    const auto list_address = reinterpret_cast<uintptr_t>(list);
    std::memcpy(view.data() + lightobj_view_list_off, &list_address, sizeof(list_address));
    std::memcpy(list + lightobj_list_objects_off, &address, sizeof(address));
    f.context.rax = 2; f.context.rbx = address; f.context.rsi = reinterpret_cast<uintptr_t>(view.data());
    lightobj_value_get(f.context);
    CHECK(*saved_xmm(f.context, 0) == 1.25f && f.context.rip == lightobj_value_get_next_rva);
    f.context.rax = 2; f.context.rsi = address; f.context.r14 = 1; f.context.r13 = 0;
    lightobj_flag_test(f.context); // unaligned offset -> stock client-0 fallback
    CHECK(f.context.rip == lightobj_flag_test_clear_rva);
    std::vector<std::thread> workers;
    for (unsigned worker = 0; worker < 4; ++worker)
        workers.emplace_back([&, worker] {
            frame local;
            for (int i = 0; i < 5000; ++i)
            {
                if (worker == 0) { reset_lightobj_state(); continue; }
                local.context.rcx = address; local.context.r9 = 2 + worker % 2; local.context.r10 = 0;
                *saved_xmm(local.context, 3) = 9.5f;
                lightobj_value_set(local.context);
                local.context.rcx = address; local.context.r10 = 2 + worker % 2;
                local.context.r11 = 0; local.context.rdx = 1;
                lightobj_flag_set(local.context);
                local.context.rax = 2 + worker % 2; local.context.rbx = address;
                local.context.rsi = reinterpret_cast<uintptr_t>(view.data());
                lightobj_value_get(local.context);
                const auto value = *saved_xmm(local.context, 0);
                CHECK(value == 9.5f || value == 1.25f);
                local.context.rax = 2 + worker % 2; local.context.rsi = address;
                local.context.r14 = 0; local.context.r13 = 0;
                lightobj_flag_test(local.context);
                CHECK(local.context.rip == lightobj_flag_test_set_rva || local.context.rip == lightobj_flag_test_clear_rva);
            }
        });
    for (auto& worker : workers) worker.join();
    CHECK(*reinterpret_cast<float*>(object + lightobj_value_off) == 1.25f && object[lightobj_flags_off] == 0);
    std::puts("PASS lighting: actual hooks, same-address reset, 20,000 concurrent iterations (not a race-detector proof)");
}