#include "regression_check.hpp"
#include <atomic>
#include <array>
#include <thread>
#include <vector>
#include <string>
namespace pluginmock
{
    std::atomic<int> calls{0}, active{0}, forwards{0};
    bool result = true, startup_throw = false, forward_throw = false;
}
namespace runtime
{
    bool start()
    {
        CHECK(pluginmock::active.fetch_add(1) == 0);
        ++pluginmock::calls;
        --pluginmock::active;
        if (pluginmock::startup_throw) throw std::runtime_error("startup");
        return pluginmock::result;
    }
}
#include "standalone/ezz_plugin.cpp"
BOOL forward()
{
    ++pluginmock::forwards;
    if (pluginmock::forward_throw) throw std::runtime_error("forward");
    return 7;
}
struct image_fixture
{
    alignas(8) std::array<unsigned char, 8192> image{};
    IMAGE_NT_HEADERS64* nt;
    IMAGE_IMPORT_DESCRIPTOR* descriptor;
    IMAGE_THUNK_DATA64* names;
    IMAGE_THUNK_DATA64* slots;
    image_fixture()
    {
        winmock::module = image.data(); winmock::module_size = image.size();
        winmock::protection = PAGE_READWRITE; winmock::state = MEM_COMMIT; winmock::query_ok = true;
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
        dos->e_magic = IMAGE_DOS_SIGNATURE; dos->e_lfanew = sizeof(*dos);
        nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data() + dos->e_lfanew);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        nt->OptionalHeader.SizeOfImage = image.size(); nt->OptionalHeader.SizeOfHeaders = 512;
        nt->OptionalHeader.NumberOfRvaAndSizes = 16;
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = {512, 2 * sizeof(IMAGE_IMPORT_DESCRIPTOR)};
        descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image.data() + 512);
        descriptor->Name = 600; descriptor->OriginalFirstThunk = 640; descriptor->FirstThunk = 704;
        std::strcpy(reinterpret_cast<char*>(image.data() + 600), "USER32.dll");
        names = reinterpret_cast<IMAGE_THUNK_DATA64*>(image.data() + 640);
        slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(image.data() + 704);
        names[0].u1.AddressOfData = 768;
        std::strcpy(reinterpret_cast<char*>(image.data() + 770), "SetProcessDPIAware");
        slots[0].u1.Function = reinterpret_cast<uintptr_t>(&forward);
    }
    void** find() { return find_import_slot(image.data(), "USER32.dll", "SetProcessDPIAware"); }
};
void imports()
{
    CHECK(find_import_slot(nullptr, "USER32.dll", "SetProcessDPIAware") == nullptr);
    { image_fixture f; CHECK(f.find() == reinterpret_cast<void**>(&f.slots[0].u1.Function));
      CHECK(find_import_slot(f.image.data(), "other.dll", "SetProcessDPIAware") == nullptr); }
    { image_fixture f; f.descriptor->OriginalFirstThunk = 0; CHECK(!f.find()); }
    { image_fixture f; f.descriptor->FirstThunk = 8190; CHECK(!f.find()); }
    { image_fixture f; f.names[0].u1.AddressOfData = UINT64_MAX - 16; CHECK(!f.find()); }
    { image_fixture f; f.names[0].u1.Ordinal = uint64_t(1) << 63;
      f.names[1].u1.AddressOfData = 768;
      CHECK(f.find() == reinterpret_cast<void**>(&f.slots[1].u1.Function)); }
    { image_fixture f; f.descriptor->Name = 4096; std::memset(f.image.data() + 4096, 'x', 4096); CHECK(!f.find()); }
    { image_fixture f; f.nt->OptionalHeader.SizeOfHeaders = 1; CHECK(!f.find()); }
    { image_fixture f; f.nt->OptionalHeader.SizeOfImage = 512; CHECK(!f.find()); }
    { image_fixture f; f.nt->OptionalHeader.DataDirectory[1] = {8190, 40}; CHECK(!f.find()); }
    { image_fixture f; f.nt->OptionalHeader.NumberOfRvaAndSizes = 1; CHECK(!f.find()); }
    { image_fixture f; auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(f.image.data());
      dos->e_lfanew = -1; CHECK(!f.find()); dos->e_lfanew = 0x100001; CHECK(!f.find()); }
    { image_fixture f; winmock::protection = PAGE_GUARD | PAGE_READWRITE; CHECK(!f.find()); }
    { image_fixture f; winmock::protection = PAGE_NOACCESS; CHECK(!f.find()); }
    { image_fixture f; winmock::state = MEM_RESERVE; CHECK(!f.find()); }
    { image_fixture f; CHECK(!readable(reinterpret_cast<void*>(UINTPTR_MAX - 1), 4, f.image.data()));
      CHECK(!readable(f.image.data(), 1, reinterpret_cast<void*>(1))); }
    { image_fixture f; winmock::protect_calls = 0; winmock::protect_failure = 1;
      install_trigger(); CHECK(f.slots[0].u1.Function == reinterpret_cast<uintptr_t>(&forward)); }
    { image_fixture f; winmock::protect_calls = winmock::protect_failure = 0;
      install_trigger(); CHECK(f.slots[0].u1.Function == reinterpret_cast<uintptr_t>(&set_process_dpi_aware_stub));
      CHECK(original_set_process_dpi_aware == &forward && winmock::protect_calls == 2); }
    CHECK(std::strcmp(plugin_name(), "BO3 Local Splitscreen") == 0);
}
int main(int argc, char** argv)
{
    CHECK(argc == 2); const std::string scenario = argv[1];
    original_set_process_dpi_aware = &forward;
    if (scenario == "imports") imports();
    else if (scenario == "retry")
    {
        pluginmock::result = false; CHECK(set_process_dpi_aware_stub() == 7);
        pluginmock::result = true; CHECK(set_process_dpi_aware_stub() == 7);
        CHECK(set_process_dpi_aware_stub() == 7 && pluginmock::calls == 2 && pluginmock::forwards == 3);
    }
    else if (scenario == "exception")
    {
        pluginmock::startup_throw = true; CHECK(set_process_dpi_aware_stub() == 7);
        pluginmock::startup_throw = false; CHECK(set_process_dpi_aware_stub() == 7);
        pluginmock::forward_throw = true; CHECK(set_process_dpi_aware_stub() == FALSE);
        original_set_process_dpi_aware = nullptr; CHECK(set_process_dpi_aware_stub() == TRUE);
        CHECK(pluginmock::calls == 2);
    }
    else if (scenario == "concurrent")
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < 32; ++i) threads.emplace_back([] { CHECK(set_process_dpi_aware_stub() == 7); });
        for (auto& t : threads) t.join();
        CHECK(pluginmock::calls == 1 && pluginmock::forwards == 32);
    }
    else CHECK(false);
    std::printf("PASS actual ezz_plugin.cpp (mock Windows/runtime): %s\n", argv[1]);
}