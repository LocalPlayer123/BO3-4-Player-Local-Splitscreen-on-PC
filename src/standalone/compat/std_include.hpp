#pragma once

// Precompiled-header stand-in for the standalone (mod) build of the component.
//
// component\splitscreen.cpp is written against the BOIII client's headers. This
// directory provides the handful of them it uses, so the SAME source file
// compiles into the drop-in DLL without a single line changed. Only the
// component's translation unit includes this header.

#pragma warning(push)
#pragma warning(disable: 4100 4127 4244 4458 4702 4996 5054 5056)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <climits>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#pragma warning(pop)
#pragma warning(disable: 4100)

using namespace std::literals;

// Release policy. The component reads its development switches from the
// environment and writes a UI trace file for the developers. A released mod
// must behave the same on every machine and must not create files, so in this
// build both calls are answered by release_policy (runtime.cpp) instead of
// Windows: exactly the tested switch set is on, every file open is refused.
#include "release_policy.hpp"
#define GetEnvironmentVariableA release_policy::get_environment_variable
#define CreateFileA release_policy::create_file
