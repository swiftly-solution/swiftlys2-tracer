#pragma once

#include "StackManager.h"

#ifndef _WIN32
#define EXPORT_API extern "C" __attribute__((visibility("default")))
#else
#define EXPORT_API extern "C" __declspec(dllexport)
#endif

EXPORT_API void SW2TracerDump(const char* path)
{
  GlobalStackManager()->Dump(path);
}