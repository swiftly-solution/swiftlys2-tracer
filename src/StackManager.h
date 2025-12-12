#pragma once


#include "loguru.hpp"

#include <memory>
#include <mutex>
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

  FunctionInfo BuildFunctionInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  const FunctionInfo* GetOrBuildFunctionInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void GetArgumentInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo, std::vector<std::string>& argumentInfo);

public:
  void FunctionEnter(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void FunctionLeave(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void FunctionTailcall(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo);
  void SetCorProfilerInfo(ICorProfilerInfo15* corProfilerInfo);
};

StackManager* GlobalStackManager();