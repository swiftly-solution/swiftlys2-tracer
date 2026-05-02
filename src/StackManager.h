#pragma once

#ifndef _WIN32
#include "specstrings_undef.h"
#endif

#include "Logger.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <array>
#include <chrono>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "cor.h"
#include "corprof.h"

struct FunctionInfo
{
  std::string moduleName;
  std::string assemblyName;
  std::string typeName;
  std::string methodSignature;
  void DebugPrint() const
  {
    printf("\n");
    printf("ModuleName = %s\n", moduleName.c_str());
    printf("AssemblyName = %s\n", assemblyName.c_str());
    printf("TypeName = %s\n", typeName.c_str());
    printf("MethodSignature = %s\n", methodSignature.c_str());
    printf("\n");
  }
};

struct ArgumentStartsStorage
{
  static constexpr size_t kInlineCapacity = 8;
  size_t count = 0;
  std::array<UINT_PTR, kInlineCapacity> inlineStarts{};
  std::vector<UINT_PTR> overflowStarts;

  void Reset(size_t newCount)
  {
    count = newCount;
    if (newCount > kInlineCapacity)
      overflowStarts.resize(newCount);
    else
      overflowStarts.clear();
  }

  void Set(size_t idx, UINT_PTR value)
  {
    if (idx < kInlineCapacity)
      inlineStarts[idx] = value;
    else
      overflowStarts[idx] = value;
  }

  UINT_PTR Get(size_t idx) const
  {
    if (idx < kInlineCapacity)
      return inlineStarts[idx];
    return overflowStarts[idx];
  }

  size_t Size() const
  {
    return count;
  }

  bool Empty() const
  {
    return count == 0;
  }
};

struct StackFrame
{
  FunctionID functionId;
  const FunctionInfo *functionInfo = nullptr;
  ArgumentStartsStorage argumentStarts;

  void DebugPrint()
  {
    printf("FunctionId = %lld\n", functionId);
    if (functionInfo != nullptr)
    {
      functionInfo->DebugPrint();
    }
    for (size_t i = 0; i < argumentStarts.Size(); i++)
    {
      printf("arg@0x%p\n", reinterpret_cast<void *>(argumentStarts.Get(i)));
    }
  }
};

class StackManager
{
private:
  std::unordered_map<FunctionID, std::unique_ptr<FunctionInfo>> m_functionInfos;
  mutable std::shared_mutex m_functionInfosMutex;
  ICorProfilerInfo15 *m_corProfilerInfo;
  std::atomic<int> m_tracerLevel{1};
  struct TransitionRecord
  {
    const FunctionInfo *functionInfo = nullptr;
    std::chrono::steady_clock::time_point lastTimestamp{};
  };
  std::unordered_map<FunctionID, TransitionRecord> m_unmanagedToManagedTransitions;
  mutable std::shared_mutex m_unmanagedToManagedTransitionsMutex;

  struct ThreadStackState
  {
    mutable std::mutex mutex;
    std::vector<StackFrame> frames;
    uint32_t desyncNotFound = 0;
    uint32_t desyncFoundNotTop = 0;
    uint32_t tailcallPops = 0;
    DWORD osThreadId = 0;
    std::string name;

    void EnsureInit()
    {
      if (frames.capacity() == 0)
      {
        frames.reserve(256);
      }
    }
  };

  struct ThreadBucket
  {
    mutable std::shared_mutex mutex;
    std::unordered_map<ThreadID, std::unique_ptr<ThreadStackState>> stacks;
  };

  static constexpr size_t kThreadBuckets = 64;
  std::array<ThreadBucket, kThreadBuckets> m_threadBuckets;

  size_t BucketIndex(ThreadID tid) const;
  ThreadStackState &GetOrCreateThreadState(ThreadID tid);
  ThreadStackState *GetCurrentThreadStateFast();

  void GetArgumentInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo, COR_PRF_FRAME_INFO frameInfo, ULONG argumentInfoSize, ArgumentStartsStorage &argumentStarts);

public:
  FunctionInfo BuildFunctionInfo(FunctionID id, COR_PRF_FRAME_INFO frameInfo);
  const FunctionInfo *GetOrBuildFunctionInfo(FunctionID id, COR_PRF_FRAME_INFO frameInfo);
  void FunctionEnter(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void FunctionLeave(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void FunctionTailcall(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void OnUnmanagedToManaged(FunctionID functionId, COR_PRF_TRANSITION_REASON reason);
  void SetCorProfilerInfo(ICorProfilerInfo15 *corProfilerInfo);
  ICorProfilerInfo15 *GetCorProfilerInfo();
  void SetTracerLevel(int level);
  int GetTracerLevel() const;

  void OnThreadCreated(ThreadID threadId);
  void OnThreadDestroyed(ThreadID threadId);
  void OnThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId);
  void OnThreadNameChanged(ThreadID threadId, const WCHAR *name);

  struct ThreadStackSnapshot
  {
    ThreadID threadId = 0;
    DWORD osThreadId = 0;
    std::string nameUtf8;
    uint32_t desyncNotFound = 0;
    uint32_t desyncFoundNotTop = 0;
    uint32_t tailcallPops = 0;
    std::vector<StackFrame> frames;
  };

  std::vector<ThreadStackSnapshot> SnapshotAllStacks() const;
  void Dump(std::string path) const;
};

StackManager *GlobalStackManager();