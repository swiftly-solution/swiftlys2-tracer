#pragma once

#include "Logger.h"

#include <memory>
#include <mutex>
#include <array>
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

struct StackFrame
{
  FunctionID functionId;
  const FunctionInfo* functionInfo = nullptr;
  std::vector<std::string> argumentInfo;
  
  void DebugPrint()
  {
    // printf("FunctionId = %lld\n", functionId);
    if (functionInfo != nullptr)
    {
      // functionInfo->DebugPrint();
    }
    for (const auto& s : argumentInfo)
    {
      printf("%s\n", s.c_str());
    }
  }

};

class StackManager
{
private:
  std::unordered_map<FunctionID, std::unique_ptr<FunctionInfo>> m_functionInfos;
  mutable std::shared_mutex m_functionInfosMutex;
  ICorProfilerInfo15 *m_corProfilerInfo;

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

  FunctionInfo BuildFunctionInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  const FunctionInfo* GetOrBuildFunctionInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void GetArgumentInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo, std::vector<std::string>& argumentInfo);

public:
  void FunctionEnter(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void FunctionLeave(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void FunctionTailcall(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void SetCorProfilerInfo(ICorProfilerInfo15* corProfilerInfo);

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

StackManager* GlobalStackManager();