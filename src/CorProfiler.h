#pragma once

#include <atomic>
#include <guiddef.h>
#include "Helper.h"
#include "Logger.h"
#include "cor.h"
#include "corprof.h"

#ifndef _WIN32
#include "specstrings_undef.h"
#endif

class CorProfiler : ICorProfilerCallback9
{
private:
  std::atomic<int> refCount;
  ICorProfilerInfo15 *corProfilerInfo;

public:
  CorProfiler();
  ~CorProfiler();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
  {
    if (
        riid == __uuidof(ICorProfilerCallback9) ||
        riid == __uuidof(ICorProfilerCallback8) ||
        riid == __uuidof(ICorProfilerCallback7) ||
        riid == __uuidof(ICorProfilerCallback6) ||
        riid == __uuidof(ICorProfilerCallback5) ||
        riid == __uuidof(ICorProfilerCallback4) ||
        riid == __uuidof(ICorProfilerCallback3) ||
        riid == __uuidof(ICorProfilerCallback2) ||
        riid == __uuidof(ICorProfilerCallback) ||
        riid == IID_IUnknown)
    {
      *ppvObject = this;
      this->AddRef();
      return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef(void) override
  {
    return std::atomic_fetch_add(&this->refCount, 1) + 1;
  }

  ULONG STDMETHODCALLTYPE Release(void) override
  {
    int count = std::atomic_fetch_sub(&this->refCount, 1) - 1;

    if (count <= 0)
    {
      delete this;
    }

    return count;
  }

  HRESULT STDMETHODCALLTYPE Initialize(IUnknown *pICorProfilerInfoUnk) override;
  HRESULT STDMETHODCALLTYPE Shutdown(void) override;
  HRESULT STDMETHODCALLTYPE AppDomainCreationStarted(AppDomainID appDomainId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AppDomainShutdownStarted(AppDomainID appDomainId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AssemblyLoadStarted(AssemblyID assemblyId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AssemblyUnloadStarted(AssemblyID assemblyId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ModuleLoadStarted(ModuleID moduleId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ModuleUnloadStarted(ModuleID moduleId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID AssemblyId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ClassLoadStarted(ClassID classId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ClassLoadFinished(ClassID classId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ClassUnloadStarted(ClassID classId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ClassUnloadFinished(ClassID classId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE FunctionUnloadStarted(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock) { return S_OK; };
  HRESULT STDMETHODCALLTYPE JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock) { return S_OK; };
  HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchStarted(FunctionID functionId, BOOL *pbUseCachedFunction) { return S_OK; };
  HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result) { return S_OK; };
  HRESULT STDMETHODCALLTYPE JITFunctionPitched(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE JITInlining(FunctionID callerId, FunctionID calleeId, BOOL *pfShouldInline) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ThreadCreated(ThreadID threadId) override;
  HRESULT STDMETHODCALLTYPE ThreadDestroyed(ThreadID threadId) override;
  HRESULT STDMETHODCALLTYPE ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId) override;
  HRESULT STDMETHODCALLTYPE RemotingClientInvocationStarted(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingClientSendingMessage(GUID *pCookie, BOOL fIsAsync) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingClientReceivingReply(GUID *pCookie, BOOL fIsAsync) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingClientInvocationFinished(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingServerReceivingMessage(GUID *pCookie, BOOL fIsAsync) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingServerInvocationStarted(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingServerInvocationReturned(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RemotingServerSendingReply(GUID *pCookie, BOOL fIsAsync) { return S_OK; };
  HRESULT STDMETHODCALLTYPE UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason) override;
  HRESULT STDMETHODCALLTYPE ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeSuspendFinished(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeSuspendAborted(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeResumeStarted(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeResumeFinished(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeThreadSuspended(ThreadID threadId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RuntimeThreadResumed(ThreadID threadId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], ULONG cObjectIDRangeLength[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ObjectAllocated(ObjectID objectId, ClassID classId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[], ULONG cObjects[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RootReferences(ULONG cRootRefs, ObjectID rootRefIds[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionThrown(ObjectID thrownObjectId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionEnter(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionLeave(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionSearchFilterEnter(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionSearchFilterLeave(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionSearchCatcherFound(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionOSHandlerEnter(UINT_PTR __unused) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionOSHandlerLeave(UINT_PTR __unused) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionEnter(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionLeave(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyEnter(FunctionID functionId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyLeave(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionCatcherLeave(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable, ULONG cSlots) { return S_OK; };
  HRESULT STDMETHODCALLTYPE COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherFound(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherExecute(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[]) override;
  HRESULT STDMETHODCALLTYPE GarbageCollectionStarted(int cGenerations, BOOL generationCollected[], COR_PRF_GC_REASON reason) { return S_OK; };
  HRESULT STDMETHODCALLTYPE SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], ULONG cObjectIDRangeLength[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE GarbageCollectionFinished(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID) { return S_OK; };
  HRESULT STDMETHODCALLTYPE RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[], COR_PRF_GC_ROOT_KIND rootKinds[], COR_PRF_GC_ROOT_FLAGS rootFlags[], UINT_PTR rootIds[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE HandleCreated(GCHandleID handleId, ObjectID initialObjectId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE HandleDestroyed(GCHandleID handleId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE InitializeForAttach(IUnknown *pCorProfilerInfoUnk, void *pvClientData, UINT cbClientData) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ProfilerAttachComplete(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ProfilerDetachSucceeded(void) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ReJITCompilationStarted(FunctionID functionId, ReJITID rejitId, BOOL fIsSafeToBlock) { return S_OK; };
  HRESULT STDMETHODCALLTYPE GetReJITParameters(ModuleID moduleId, mdMethodDef methodId, ICorProfilerFunctionControl *pFunctionControl) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ReJITCompilationFinished(FunctionID functionId, ReJITID rejitId, HRESULT hrStatus, BOOL fIsSafeToBlock) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ReJITError(ModuleID moduleId, mdMethodDef methodId, FunctionID functionId, HRESULT hrStatus) { return S_OK; };
  HRESULT STDMETHODCALLTYPE MovedReferences2(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], SIZE_T cObjectIDRangeLength[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE SurvivingReferences2(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], SIZE_T cObjectIDRangeLength[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ConditionalWeakTableElementReferences(ULONG cRootRefs, ObjectID keyRefIds[], ObjectID valueRefIds[], GCHandleID rootIds[]) { return S_OK; };
  HRESULT STDMETHODCALLTYPE GetAssemblyReferences(const WCHAR *wszAssemblyPath, ICorProfilerAssemblyReferenceProvider *pAsmRefProvider) { return S_OK; };
  HRESULT STDMETHODCALLTYPE ModuleInMemorySymbolsUpdated(ModuleID moduleId) { return S_OK; };
  HRESULT STDMETHODCALLTYPE DynamicMethodJITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock, LPCBYTE pILHeader, ULONG cbILHeader) { return S_OK; };
  HRESULT STDMETHODCALLTYPE DynamicMethodJITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock) { return S_OK; };
  HRESULT STDMETHODCALLTYPE DynamicMethodUnloaded(FunctionID functionId) { return S_OK; };
};