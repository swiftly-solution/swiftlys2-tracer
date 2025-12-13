#pragma once

#include "CorProfiler.h"
#include <atomic>
#include "Logger.h"

#include "StackManager.h"
#include <string>

#include "corhlpr.h"
#include "ProfilerPal.h"

#ifndef _WIN32
#include "specstrings_undef.h"
#endif

PROFILER_STUB EnterStub(FunctionIDOrClientID functionId, COR_PRF_ELT_INFO eltInfo)
{
  GlobalStackManager()->FunctionEnter(functionId, eltInfo);
}

PROFILER_STUB LeaveStub(FunctionIDOrClientID functionId, COR_PRF_ELT_INFO eltInfo)
{
  GlobalStackManager()->FunctionLeave(functionId, eltInfo);
}

PROFILER_STUB TailcallStub(FunctionIDOrClientID functionId, COR_PRF_ELT_INFO eltInfo)
{
  GlobalStackManager()->FunctionTailcall(functionId, eltInfo);
}

// ASM
EXTERN_C void EnterNaked(FunctionIDOrClientID functionIDOrClientID, COR_PRF_ELT_INFO eltInfo);
EXTERN_C void LeaveNaked(FunctionIDOrClientID functionIDOrClientID, COR_PRF_ELT_INFO eltInfo);
EXTERN_C void TailcallNaked(FunctionIDOrClientID functionIDOrClientID, COR_PRF_ELT_INFO eltInfo);

CorProfiler::CorProfiler() : refCount(0), corProfilerInfo(nullptr)
{
}

CorProfiler::~CorProfiler()
{
  if (this->corProfilerInfo != nullptr)
  {
    this->corProfilerInfo->Release();
    this->corProfilerInfo = nullptr;
  }
}

HRESULT STDMETHODCALLTYPE CorProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
{
  HRESULT queryInterfaceResult = pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo15), reinterpret_cast<void **>(&this->corProfilerInfo));

  if (FAILED(queryInterfaceResult))
  {
    return E_FAIL;
  }

  GlobalStackManager()->SetCorProfilerInfo(this->corProfilerInfo);

  DWORD eventMask =
      COR_PRF_MONITOR_ENTERLEAVE |
      COR_PRF_MONITOR_THREADS |
      COR_PRF_ENABLE_FUNCTION_ARGS |
      COR_PRF_ENABLE_FUNCTION_RETVAL |
      COR_PRF_ENABLE_FRAME_INFO;

  auto hr = this->corProfilerInfo->SetEventMask(eventMask);
  if (hr != S_OK)
  {
    LOG("ERROR: Profiler SetEventMask failed (HRESULT: {})", hr);
  }

  hr = this->corProfilerInfo->SetEnterLeaveFunctionHooks3WithInfo(EnterNaked, LeaveNaked, TailcallNaked);

  if (hr != S_OK)
  {
    LOG("ERROR: Profiler SetEnterLeaveFunctionHooks3WithInfo failed (HRESULT: %d)", hr);
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::Shutdown()
{
  if (this->corProfilerInfo != nullptr)
  {
    this->corProfilerInfo->Release();
    this->corProfilerInfo = nullptr;
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadCreated(ThreadID threadId)
{
  GlobalStackManager()->OnThreadCreated(threadId);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadDestroyed(ThreadID threadId)
{
  GlobalStackManager()->OnThreadDestroyed(threadId);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
{
  GlobalStackManager()->OnThreadAssignedToOSThread(managedThreadId, osThreadId);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[])
{
  (void)cchName;
  GlobalStackManager()->OnThreadNameChanged(threadId, name);
  return S_OK;
}