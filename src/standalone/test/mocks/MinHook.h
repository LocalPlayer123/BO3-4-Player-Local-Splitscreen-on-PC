#pragma once
enum MH_STATUS { MH_OK, MH_ERROR_ALREADY_INITIALIZED, MH_ERROR_UNKNOWN };
namespace mhmock
{
    inline MH_STATUS init = MH_OK, create = MH_OK, enable = MH_OK;
    inline int init_calls = 0, create_calls = 0, enable_calls = 0, remove_calls = 0;
    inline void* original = nullptr;
}
inline MH_STATUS MH_Initialize() { ++mhmock::init_calls; return mhmock::init; }
inline MH_STATUS MH_CreateHook(void*, void*, void** original)
{ ++mhmock::create_calls; *original = mhmock::original; return mhmock::create; }
inline MH_STATUS MH_EnableHook(void*) { ++mhmock::enable_calls; return mhmock::enable; }
inline MH_STATUS MH_RemoveHook(void*) { ++mhmock::remove_calls; return MH_OK; }