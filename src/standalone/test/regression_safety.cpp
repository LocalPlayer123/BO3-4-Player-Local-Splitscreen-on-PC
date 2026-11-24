#include "regression_check.hpp"
#include "component/splitscreen_safety.hpp"
#include <array>
#include <cstring>
#include <vector>
using namespace splitscreen::safety;
int main()
{
    constexpr size_t n = 7; // lighting transaction has seven sites
    for (size_t failure = 0; failure <= n; ++failure)
    {
        // Every rollback refusal subset, including all refusals at once.
        for (unsigned refused = 0; refused < (1u << failure); ++refused)
        {
            std::array<unsigned char, n> memory{}, before{}, after{};
            std::array<patch, n> patches{};
            for (size_t i = 0; i < n; ++i)
            {
                memory[i] = before[i] = static_cast<unsigned char>(i + 10);
                after[i] = static_cast<unsigned char>(i + 100);
                patches[i] = {&memory[i], &before[i], &after[i], 1};
            }
            std::vector<size_t> calls;
            bool rollback = false;
            auto writer = [&](void* target, const void* data, size_t size)
            {
                const auto i = static_cast<unsigned char*>(target) - memory.data();
                calls.push_back(static_cast<size_t>(i));
                if (!rollback && static_cast<size_t>(i) == failure)
                {
                    rollback = true;
                    return false; // writer contract: refused writes do not modify bytes
                }
                if (rollback && (refused & (1u << i))) return false;
                std::memcpy(target, data, size);
                return true;
            };
            CHECK(apply_patches(patches.data(), n, writer) == (failure == n));
            CHECK(calls.size() == (failure == n ? n : failure * 2 + 1));
            for (size_t i = 0; i < n; ++i)
            {
                CHECK(memory[i] == (failure == n || (i < failure && (refused & (1u << i))) ? after[i] : before[i]));
                if (i <= failure && i < n) CHECK(calls[i] == i);
            }
            if (failure < n)
                for (size_t i = 0; i < failure; ++i) CHECK(calls[failure + 1 + i] == failure - 1 - i);
        }
    }
    CHECK(apply_patches(nullptr, 0, [](void*, const void*, size_t) { CHECK(false); return false; }));
    const auto max = UINTPTR_MAX;
    CHECK(range_fits(100, 10, 100, 10));
    CHECK(range_fits(110, 0, 100, 10));
    CHECK(!range_fits(110, 1, 100, 10));
    CHECK(!range_fits(99, 1, 100, 10));
    CHECK(!range_fits(max - 2, 8, max - 8, 8));
    CHECK(!range_fits(max - 2, SIZE_MAX, max - 8, 8));
    CHECK(!range_fits(0, 1, max - 8, 8));
    CHECK(range_fits(max - 2, 2, max - 8, 8));
    CHECK(object_index(0, 0, 8, 4) == 4);
    CHECK(object_index(100, 100, 0, 4) == 4);
    CHECK(object_index(99, 100, 8, 4) == 4);
    CHECK(object_index(101, 100, 8, 4) == 4);
    CHECK(object_index(124, 100, 8, 4) == 3);
    CHECK(object_index(132, 100, 8, 4) == 4);
    CHECK(object_index(UINT64_MAX, 1, 2, 4) == 4);
    CHECK(object_index(100, 100, 8, 0) == 0);
    for (int size : {-1, 0, 64, 65, INT32_MAX})
        CHECK(valid_decode_size(size, 64) == (size >= 0 && size <= 64));
    for (int count : {-1, 0, 256, 257, INT32_MAX})
        CHECK(valid_cache_count(count, 256) == (count >= 0 && count <= 256));
    std::puts("PASS safety: all failure positions/refusal subsets, overflow, alignment/count");
}