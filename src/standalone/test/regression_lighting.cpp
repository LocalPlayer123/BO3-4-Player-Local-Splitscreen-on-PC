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
// Logging is outside this harness: exercise the full-table branch without game I/O.
void note(const char*, ...) {}
#include "production_lighting.hpp"
// Reproduce only the context/XMM stack storage, NOT trampoline instructions.
struct alignas(16) frame
{
    float xmm[32]{};
    midhook::context context{};
};
// Aligned, stable backing storage; all hook-dereferenced objects are real buffers.
struct object_array
{
    std::vector<uint64_t> storage;
    explicit object_array(size_t count) : storage(count * lightobj_stride / sizeof(uint64_t), 0)
    {
        static_assert(lightobj_stride % sizeof(uint64_t) == 0);
    }
    uint64_t address(size_t index = 0) const
    {
        return reinterpret_cast<uintptr_t>(storage.data()) + index * lightobj_stride;
    }
    void stock(size_t index, float value, uint8_t flag)
    {
        const auto object = address(index);
        const float client1 = -17.0f;
        std::memcpy(reinterpret_cast<void*>(object + lightobj_value_off), &value, sizeof(value));
        std::memcpy(reinterpret_cast<void*>(object + lightobj_value_off + 4), &client1, sizeof(client1));
        *reinterpret_cast<uint8_t*>(object + lightobj_flags_off) = flag;
        *reinterpret_cast<uint8_t*>(object + lightobj_flags_off + 1) = 0xA5;
    }
};

void noise(midhook::context& c, uint64_t seed)
{
    // These are deliberately not object indices/bases. Each helper overrides only
    // registers required by its displaced instruction (e.g. rbx IS the value-get object).
    c.r10 = seed + 0x12345; c.r11 = seed + 0x67891;
    c.r14 = seed + 1; c.rdi = seed + 0xABCDF; c.rbx = seed + 0xFEDCB;
    c.rsi = seed + 0x13579; c.rdx = seed + 0x24681;
}

void write_value(uint64_t array, size_t index, uint64_t lc, float value, uint64_t seed = 0)
{
    frame f;
    noise(f.context, seed);
    f.context.rcx = array; f.context.r9 = lc;
    // Faithful movss [rcx+rdx*4+0x138],xmm3 operand, not an independent object index.
    // index == 0 also models rcx already pointing at an individual object, rdx == lc.
    f.context.rdx = index * lightobj_stride / 4 + lc;
    *saved_xmm(f.context, 3) = value;
    lightobj_value_set(f.context);
    CHECK(f.context.rip == lightobj_value_set_next_rva);
}

void write_flag(uint64_t object, uint64_t lc, uint8_t flag, uint64_t seed = 0)
{
    frame f;
    noise(f.context, seed);
    f.context.rcx = object; f.context.r10 = lc; f.context.rdx = flag;
    lightobj_flag_set(f.context);
    CHECK(f.context.rip == lightobj_flag_set_next_rva);
}

float read_value(uint64_t object, uint64_t lc, uint64_t seed = 0)
{
    frame f;
    noise(f.context, seed);
    f.context.rax = lc; f.context.rbx = object;
    *saved_xmm(f.context, 0) = -999.0f;
    lightobj_value_get(f.context);
    CHECK(f.context.rip == lightobj_value_get_next_rva);
    return *saved_xmm(f.context, 0);
}

void check_flag(uint64_t object, uint64_t lc, uint8_t flag, uint64_t seed = 0)
{
    frame f;
    noise(f.context, seed);
    f.context.rax = lc; f.context.rsi = object; f.context.r13 = flag;
    lightobj_flag_test(f.context);
    CHECK(f.context.rip == lightobj_flag_test_clear_rva);
    f.context.r13 = static_cast<uint8_t>(flag ^ 1);
    lightobj_flag_test(f.context);
    CHECK(f.context.rip == lightobj_flag_test_set_rva);
    f.context.rcx = object; f.context.rsi = lc;
    lightobj_flag_save(f.context);
    CHECK(f.context.rip == (flag ? lightobj_flag_save_set_rva : lightobj_flag_save_clear_rva));
}

void restore_flag(uint64_t object, uint64_t lc, bool mark, uint64_t seed = 0)
{
    frame f;
    noise(f.context, seed);
    f.context.r14 = lc;
    if (mark)
    {
        f.context.rax = object;
        lightobj_flag_mark(f.context);
        CHECK(f.context.rip == lightobj_flag_mark_next_rva);
    }
    else
    {
        f.context.rcx = object;
        lightobj_flag_clear(f.context);
        CHECK(f.context.rip == lightobj_flag_clear_next_rva);
    }
}

void stock_pass_through(uint64_t object)
{
    using hook = void (*)(midhook::context&);
    const hook hooks[] = {lightobj_value_set, lightobj_value_get, lightobj_flag_set,
                          lightobj_flag_test, lightobj_flag_clear, lightobj_flag_mark,
                          lightobj_flag_save};
    for (uint64_t lc : {0, 1})
        for (auto fn : hooks)
        {
            frame f;
            noise(f.context, 41);
            f.context.rcx = object; f.context.rbx = object;
            f.context.r9 = lc; f.context.r10 = lc; f.context.r14 = lc;
            f.context.rax = lc; f.context.rsi = lc; f.context.rdx = lc;
            f.context.rip = 0xDEADBEEF; f.context.rflags = 0x202;
            *saved_xmm(f.context, 0) = -99.0f;
            *saved_xmm(f.context, 3) = 99.0f;
            const frame before = f;
            fn(f.context);
            CHECK(std::memcmp(&f.context, &before.context, sizeof(f.context)) == 0);
            CHECK(std::memcmp(f.xmm, before.xmm, sizeof(f.xmm)) == 0);
        }
    // Only callback pass-through is tested: copied stock instructions are not executed.
    std::lock_guard lock(lightobj_mutex());
    for (const auto& slot : lightobj_extra) CHECK(slot.object == 0);
}

int main()
{
    constexpr size_t high_index = 12347;
    object_array first(high_index + 1), second(high_index + 1);
    const size_t indices[] = {0, 7, high_index};
    object_array* arrays[] = {&first, &second};
    for (size_t a = 0; a < 2; ++a)
        for (size_t index : indices) arrays[a]->stock(index, 1.25f + a, static_cast<uint8_t>(a));
    const auto first_original = first.storage, second_original = second.storage;
    reset_lightobj_state();
    stock_pass_through(first.address());
    {
        std::lock_guard lock(lightobj_mutex());
        CHECK(!lightobj_slot(0, first.address()) && !lightobj_slot(1, first.address()));
        CHECK(!lightobj_slot(4, first.address()) && !lightobj_slot(2, 0));
    }

    // Absent value/flag reads use client 0, not the deliberately distinct client 1.
    // A write for one client must not initialize the other client's value or flag.
    for (uint64_t lc : {2, 3})
    {
        CHECK(read_value(first.address(), lc) == 1.25f);
        check_flag(first.address(), lc, 0);
        CHECK(read_value(second.address(), lc) == 2.25f);
        check_flag(second.address(), lc, 1);
    }
    write_value(first.address(), 0, 2, 9.5f);
    write_flag(first.address(), 2, 1);
    CHECK(read_value(first.address(), 3) == 1.25f);
    check_flag(first.address(), 3, 0);
    write_flag(first.address(), 3, 0);
    CHECK(read_value(first.address(), 3) == 1.25f);
    CHECK(read_value(first.address(), 2) == 9.5f);
    check_flag(first.address(), 2, 1);

    // Interleave both arrays and clients, including an index well beyond the old 4096
    // bound. Values and flags must persist while unrelated registers change.
    for (size_t a = 0; a < 2; ++a)
        for (size_t index : indices)
            for (uint64_t lc : {2, 3})
            {
                const float value = static_cast<float>(100 * a + index + 10 * lc);
                write_value(arrays[a]->address(), index, lc, value, index + a);
                write_flag(arrays[a]->address(index), lc, static_cast<uint8_t>(lc - 2), index + 77);
            }
    for (size_t a = 0; a < 2; ++a)
        for (size_t index : indices)
            for (uint64_t lc : {2, 3})
            {
                const auto object = arrays[a]->address(index);
                const float value = static_cast<float>(100 * a + index + 10 * lc);
                CHECK(read_value(object, lc, 999) == value);
                check_flag(object, lc, static_cast<uint8_t>(lc - 2), 123);
                // Same object via rcx == object, rdx == lc aliases the array-base write.
                write_value(object, 0, lc, value + 0.5f, 98765);
                CHECK(read_value(object, lc, 456) == value + 0.5f);
                CHECK(read_value(object, 5 - lc) ==
                      static_cast<float>(100 * a + index + 10 * (5 - lc)) + (lc == 3 ? 0.5f : 0.0f));
            }
    restore_flag(first.address(high_index), 2, true, 1001);
    check_flag(first.address(high_index), 2, 1);
    check_flag(first.address(high_index), 3, 1);
    restore_flag(first.address(high_index), 3, false, 2003);
    check_flag(first.address(high_index), 3, 0);
    check_flag(first.address(high_index), 2, 1);
    CHECK(first.storage == first_original && second.storage == second_original);

    reset_lightobj_state(); // Same live addresses represent replacement map objects.
    first.stock(high_index, 6.25f, 1);
    for (uint64_t lc : {2, 3})
    {
        CHECK(read_value(first.address(high_index), lc) == 6.25f);
        check_flag(first.address(high_index), lc, 1);
        CHECK(read_value(second.address(high_index), lc) == 2.25f);
        check_flag(second.address(high_index), lc, 1);
    }

    // Real aligned addresses separated by max*8 have identical initial buckets.
    // Inspect resulting slots only to verify that a collision was actually exercised;
    // all state writes/reads still go through production callbacks.
    reset_lightobj_state();
    std::vector<uint64_t> collision(lightobj_max + lightobj_stride / 8, 0);
    const auto left = reinterpret_cast<uintptr_t>(collision.data());
    const auto right = left + static_cast<uint64_t>(lightobj_max) * 8;
    const auto bucket = ((left >> 3) * 0x9E3779B97F4A7C15ull) & (lightobj_max - 1);
    CHECK(bucket == (((right >> 3) * 0x9E3779B97F4A7C15ull) & (lightobj_max - 1)));
    const auto collision_original = collision;
    for (uint64_t lc : {2, 3})
    {
        write_value(left, 0, lc, static_cast<float>(lc));
        write_flag(left, lc, static_cast<uint8_t>(lc));
        write_value(right, 0, lc, static_cast<float>(lc + 10));
        write_flag(right, lc, static_cast<uint8_t>(lc + 10));
    }
    {
        std::lock_guard lock(lightobj_mutex());
        CHECK(lightobj_extra[bucket].object == left);
        CHECK(lightobj_extra[(bucket + 1) & (lightobj_max - 1)].object == right);
        CHECK(lightobj_slot(2, left) == lightobj_slot(3, left));
        CHECK(lightobj_slot(2, left) != lightobj_slot(2, right));
    }
    for (uint64_t lc : {2, 3})
    {
        CHECK(read_value(left, lc) == static_cast<float>(lc));
        check_flag(left, lc, static_cast<uint8_t>(lc));
        CHECK(read_value(right, lc) == static_cast<float>(lc + 10));
        check_flag(right, lc, static_cast<uint8_t>(lc + 10));
    }
    CHECK(collision == collision_original);

    reset_lightobj_state();
    object_array capacity(lightobj_max + 1);
    const auto overflow = capacity.address(lightobj_max);
    capacity.stock(lightobj_max, 3.25f, 7);
    const auto capacity_original = capacity.storage;
    for (size_t i = 0; i < lightobj_max; ++i)
    {
        write_value(capacity.address(), i, 2, static_cast<float>(i + 1));
        write_value(capacity.address(), i, 3, static_cast<float>(i + 2));
        write_flag(capacity.address(i), 2, 1);
        write_flag(capacity.address(i), 3, 2);
    }
    {
        std::lock_guard lock(lightobj_mutex());
        for (const auto& slot : lightobj_extra) CHECK(slot.object != 0);
        CHECK(!lightobj_capacity_reported);
    }
    for (uint64_t lc : {2, 3})
    {
        write_value(capacity.address(), lightobj_max, lc, 99.0f);
        write_flag(overflow, lc, 99);
        restore_flag(overflow, lc, false);
        restore_flag(overflow, lc, true);
        CHECK(read_value(overflow, lc) == 3.25f);
        check_flag(overflow, lc, 7);
    }
    CHECK(capacity.storage == capacity_original); // Includes flags/gate bytes after stock [2].
    {
        std::lock_guard lock(lightobj_mutex());
        CHECK(lightobj_capacity_reported);
        CHECK(!lightobj_slot(2, overflow));
    }
    // A full table still supports lookup/update of existing objects for either client.
    for (size_t i = 0; i < lightobj_max; ++i)
        for (uint64_t lc : {2, 3})
        {
            CHECK(read_value(capacity.address(i), lc) == static_cast<float>(i + lc - 1));
            check_flag(capacity.address(i), lc, static_cast<uint8_t>(lc - 1));
        }
    write_value(capacity.address(), 0, 3, 42.0f);
    CHECK(read_value(capacity.address(), 2) == 1.0f);
    CHECK(read_value(capacity.address(), 3) == 42.0f);
    reset_lightobj_state();
    {
        std::lock_guard lock(lightobj_mutex());
        CHECK(!lightobj_capacity_reported);
        for (const auto& slot : lightobj_extra) CHECK(slot.object == 0);
    }
    write_value(overflow, 0, 2, 88.0f);
    CHECK(read_value(overflow, 2) == 88.0f && read_value(overflow, 3) == 3.25f);
    CHECK(capacity.storage == capacity_original);

    reset_lightobj_state();
    first.stock(0, 1.25f, 0);
    const auto stress_first_original = first.storage;
    std::vector<std::thread> workers;
    for (unsigned worker = 0; worker < 4; ++worker)
        workers.emplace_back([&, worker] {
            for (int i = 0; i < 5000; ++i)
            {
                if (worker == 0) { reset_lightobj_state(); continue; }
                const auto lc = 2 + worker % 2;
                const auto a = static_cast<size_t>(i % 2);
                const auto object = arrays[a]->address();
                write_value(arrays[a]->address(), 0, lc, 9.5f, i);
                write_flag(object, lc, 1, i);
                const auto value = read_value(object, lc, i + 100);
                CHECK(value == 9.5f || value == 1.25f + a);
                frame local;
                noise(local.context, i);
                local.context.rax = lc; local.context.rsi = object; local.context.r13 = 0;
                lightobj_flag_test(local.context);
                CHECK(local.context.rip == lightobj_flag_test_set_rva ||
                      local.context.rip == lightobj_flag_test_clear_rva);
            }
        });
    for (auto& worker : workers) worker.join();
    CHECK(first.storage == stress_first_original && second.storage == second_original);
    std::puts("PASS lighting: address-keyed actual hooks, independent arrays/clients, high index, reset, collision, capacity, 20,000 concurrent iterations (not a race-detector proof)");
}