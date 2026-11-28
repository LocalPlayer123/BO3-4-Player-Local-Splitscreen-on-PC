#pragma once
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (false)
template <typename F> void throws(F&& f)
{
    bool caught = false;
    try { f(); } catch (const std::runtime_error&) { caught = true; }
    CHECK(caught);
}