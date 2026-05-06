#pragma once

#include "StackManager.h"
#include <atomic>

#ifndef _WIN32
#define EXPORT_API extern "C" __attribute__((visibility("default")))
#else
#define EXPORT_API extern "C" __declspec(dllexport)
#endif

struct DumpGuard
{
    std::atomic_flag &flag;
    ~DumpGuard()
    {
        flag.clear(std::memory_order_release);
    }
};

EXPORT_API void SW2TracerDump(const char *path)
{
    if (path == nullptr || path[0] == '\0')
        return;

    static std::atomic_flag dumpInProgress = ATOMIC_FLAG_INIT;
    if (dumpInProgress.test_and_set(std::memory_order_acquire))
        return;

    DumpGuard guard{dumpInProgress};

    GlobalStackManager()->Dump(path);
}