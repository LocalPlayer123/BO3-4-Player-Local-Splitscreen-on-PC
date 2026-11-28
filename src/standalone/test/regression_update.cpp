#include "regression_check.hpp"
#include "standalone/compat/utils/finally.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
using DWORD = uint32_t;
constexpr int WINHTTP_ACCESS_TYPE_DEFAULT_PROXY = 0, INTERNET_DEFAULT_HTTPS_PORT = 443;
constexpr int WINHTTP_FLAG_SECURE = 1, WINHTTP_QUERY_STATUS_CODE = 2, WINHTTP_QUERY_FLAG_NUMBER = 4;
constexpr auto WINHTTP_NO_PROXY_NAME = nullptr, WINHTTP_NO_PROXY_BYPASS = nullptr;
constexpr auto WINHTTP_NO_REFERER = nullptr, WINHTTP_DEFAULT_ACCEPT_TYPES = nullptr;
constexpr auto WINHTTP_NO_REQUEST_DATA = nullptr, WINHTTP_HEADER_NAME_BY_INDEX = nullptr;
constexpr auto WINHTTP_NO_HEADER_INDEX = nullptr;
int failure = 0, stage = 0, closes = 0, reads = 0;
bool throw_open = false, read_error = false;
std::string response = "{\"tag_name\":\"v9.0.0\"}";
const char* command_line = "game";
bool open_entered = false;
bool step() { return ++stage != failure; }
void* WinHttpOpen(const wchar_t*, int, std::nullptr_t, std::nullptr_t, int)
{
    if (throw_open)
    {
        open_entered = true;
        throw std::runtime_error("mock network/allocator exception");
    }
    return step() ? reinterpret_cast<void*>(1) : nullptr;
}
bool WinHttpCloseHandle(void*) { ++closes; return true; }
bool WinHttpSetTimeouts(void*, int, int, int, int) { return true; }
void* WinHttpConnect(void*, const wchar_t*, int, int) { return step() ? reinterpret_cast<void*>(2) : nullptr; }
void* WinHttpOpenRequest(void*, const wchar_t*, const wchar_t*, std::nullptr_t, std::nullptr_t, std::nullptr_t, int)
{ return step() ? reinterpret_cast<void*>(3) : nullptr; }
bool WinHttpSendRequest(void*, const wchar_t*, DWORD, std::nullptr_t, int, int, int) { return step(); }
bool WinHttpReceiveResponse(void*, std::nullptr_t) { return step(); }
bool WinHttpQueryHeaders(void*, int, std::nullptr_t, DWORD* status, DWORD*, std::nullptr_t)
{ *status = 200; return step(); }
bool WinHttpReadData(void*, char* chunk, size_t capacity, DWORD* read)
{
    ++reads;
    if (reads == 2 && read_error) return false;
    const auto offset = static_cast<size_t>(reads - 1) * capacity;
    *read = offset >= response.size() ? 0 : static_cast<DWORD>(std::min(capacity, response.size() - offset));
    if (*read) std::memcpy(chunk, response.data() + offset, *read);
    return true;
}
const char* GetCommandLineA() { return command_line; }
void note(const char*, ...) {}
#include "production_update.hpp"
void reset()
{
    failure = stage = closes = reads = 0;
    read_error = throw_open = false;
    response = "{\"tag_name\":\"v9.0.0\"}";
    update_result.store(update_pending);
}
int main()
{
    for (int f = 1; f <= 6; ++f)
    {
        reset(); failure = f; std::string body = "stale";
        CHECK(!fetch_latest_release(body) && body.empty());
        CHECK(closes == (f <= 3 ? f - 1 : 3));
    }
    reset(); read_error = true; std::string body;
    CHECK(!fetch_latest_release(body));
    CHECK(body == response && closes == 3); // valid-looking prefix must NOT mean success
    reset(); read_error = true; run_update_check(); CHECK(update_result.load() == update_none);
    reset(); CHECK(fetch_latest_release(body) && body == response && closes == 3);
    reset(); run_update_check(); CHECK(update_result.load() == update_newer);
    CHECK(std::strcmp(update_latest, "9.0.0") == 0);
    reset(); response.clear(); CHECK(!fetch_latest_release(body) && closes == 3);
    reset(); response.assign(update_body_max + 1, 'x');
    CHECK(!fetch_latest_release(body) && body.size() <= update_body_max && closes == 3);
    reset(); response.assign(update_body_max, 'x'); CHECK(fetch_latest_release(body));
    int parsed[3];
    CHECK(parse_version("v2.6", parsed) && parsed[0] == 2 && parsed[1] == 6 && parsed[2] == 0);
    for (const char* invalid : {"", "v", "1.", "1..2", "1000", "1.2.3.4", "1.2-beta"}) CHECK(!parse_version(invalid, parsed));
    reset(); throw_open = true;
    // Invoke the actual worker on a real detached std::thread. A second task
    // signals completion after the worker returns; no arbitrary sleeps/polling.
        // Shared ownership keeps the CV alive even if main wakes before notify_one
        // returns and the detached thread releases its capture.
        struct completion_state { std::mutex mutex; std::condition_variable cv; bool done = false; };
        auto completion = std::make_shared<completion_state>();
        std::thread([completion] {
        run_update_check();
                { std::lock_guard lock(completion->mutex); completion->done = true; }
                completion->cv.notify_one();
    }).detach();
        { std::unique_lock lock(completion->mutex);
            CHECK(completion->cv.wait_for(lock, std::chrono::seconds(5), [&] { return completion->done; })); }
    CHECK(open_entered && update_result.load(std::memory_order_acquire) == update_none);
    reset(); command_line = "game -splitscreen_noupdate";
    start_update_check(); CHECK(update_result.load() == update_none && stage == 0);
    std::puts("PASS update: actual fetch read failure/limits/cleanup and detached worker exception containment");
}