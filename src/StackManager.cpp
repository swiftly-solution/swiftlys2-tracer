
#ifndef _WIN32
#include "specstrings_undef.h"
#endif

#include "StackManager.h"
#include "Logger.h"
#include <string>
#include <codecvt>
#include <sstream>
#include <cstdint>
#include <utility>
#include <vector>
#include <shared_mutex>
#include <fstream>
#include <algorithm>
#include "Helper.h"
#include "ParamReader.h"

namespace
{
  struct ParamMeta
  {
    CorElementType elementType = ELEMENT_TYPE_END;
    std::string declarationPrefix;
  };

  struct MethodParamMeta
  {
    bool hasThis = false;
    std::vector<ParamMeta> parameters;
  };

  static std::unordered_map<FunctionID, std::unique_ptr<MethodParamMeta>> g_methodParamMetaCache;
  static std::shared_mutex g_methodParamMetaCacheMutex;

  static std::string DefaultArgName(ULONG idx)
  {
    return "arg" + std::to_string(idx);
  }

  static MethodParamMeta BuildMethodParamMeta(ICorProfilerInfo15 *pInfo, FunctionID functionId, COR_PRF_FRAME_INFO frameInfo)
  {
    MethodParamMeta out;
    if (pInfo == nullptr)
      return out;

    ClassID classId = 0;
    ModuleID moduleId = 0;
    mdToken tkMethod = 0;
    if (FAILED(pInfo->GetFunctionInfo2(functionId, frameInfo, &classId, &moduleId, &tkMethod, 0, NULL, NULL)))
      return out;

    IMetaDataImport2 *pMetaDataImport = NULL;
    if (FAILED(pInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport2, reinterpret_cast<IUnknown **>(&pMetaDataImport))) || pMetaDataImport == nullptr)
      return out;

    mdTypeDef type;
    WCHAR name[260];
    ULONG size;
    ULONG attributes;
    PCCOR_SIGNATURE pSig = nullptr;
    ULONG blobSize;
    ULONG codeRva;
    DWORD flags;
    auto hr = pMetaDataImport->GetMethodProps(
        tkMethod, &type, name, ARRAY_LEN(name) - 1, &size, &attributes, &pSig, &blobSize, &codeRva, &flags);
    if (FAILED(hr) || pSig == nullptr)
    {
      pMetaDataImport->Release();
      return out;
    }

    ULONG callConv = 0;
    pSig += CorSigUncompressData(pSig, &callConv);
    out.hasThis = ((callConv & IMAGE_CEE_CS_CALLCONV_HASTHIS) != 0);

    if (callConv & IMAGE_CEE_CS_CALLCONV_GENERIC)
    {
      ULONG genCount = 0;
      pSig += CorSigUncompressData(pSig, &genCount);
    }

    ULONG paramCount = 0;
    pSig += CorSigUncompressData(pSig, &paramCount);

    std::vector<std::string> paramNames(paramCount);
    HCORENUM hEnum = nullptr;
    mdParamDef paramDefs[32];
    ULONG fetched = 0;
    while (SUCCEEDED(pMetaDataImport->EnumParams(&hEnum, (mdMethodDef)tkMethod, paramDefs, ARRAY_LEN(paramDefs), &fetched)) && fetched)
    {
      for (ULONG i = 0; i < fetched; i++)
      {
        ULONG seq = 0;
        WCHAR wszParam[256];
        wszParam[0] = static_cast<WCHAR>(0);
        if (SUCCEEDED(pMetaDataImport->GetParamProps(paramDefs[i], nullptr, &seq, wszParam, 256, nullptr, nullptr, nullptr, nullptr, nullptr)) && seq >= 1 && seq <= paramCount)
          paramNames[seq - 1] = WStrToUtf8(wszParam);
      }
    }
    if (hEnum)
      pMetaDataImport->CloseEnum(hEnum);

    PCCOR_SIGNATURE pWalk = pSig;
    (void)ParseSigType(pInfo, pMetaDataImport, moduleId, nullptr, 0, pWalk);

    out.parameters.reserve(paramCount);
    for (ULONG i = 0; i < paramCount; i++)
    {
      ParamMeta pm;
      pm.elementType = (CorElementType)*pWalk;
      std::string typeName = ParseSigType(pInfo, pMetaDataImport, moduleId, nullptr, 0, pWalk);
      std::string nameForDisplay = paramNames[i].empty() ? DefaultArgName(i) : paramNames[i];
      pm.declarationPrefix = typeName + " " + nameForDisplay + " = ";
      out.parameters.push_back(std::move(pm));
    }

    pMetaDataImport->Release();
    return out;
  }

  static const MethodParamMeta *GetOrBuildMethodParamMeta(ICorProfilerInfo15 *pInfo, FunctionID functionId, COR_PRF_FRAME_INFO frameInfo)
  {
    {
      std::shared_lock lock(g_methodParamMetaCacheMutex);
      auto it = g_methodParamMetaCache.find(functionId);
      if (it != g_methodParamMetaCache.end())
        return it->second.get();
    }

    MethodParamMeta built = BuildMethodParamMeta(pInfo, functionId, frameInfo);

    {
      std::unique_lock lock(g_methodParamMetaCacheMutex);
      auto it = g_methodParamMetaCache.find(functionId);
      if (it != g_methodParamMetaCache.end())
        return it->second.get();

      auto ptr = std::make_unique<MethodParamMeta>(std::move(built));
      const MethodParamMeta *raw = ptr.get();
      g_methodParamMetaCache.emplace(functionId, std::move(ptr));
      return raw;
    }
  }

  static const MethodParamMeta *TryGetCachedMethodParamMeta(FunctionID functionId)
  {
    std::shared_lock lock(g_methodParamMetaCacheMutex);
    auto it = g_methodParamMetaCache.find(functionId);
    if (it == g_methodParamMetaCache.end())
      return nullptr;
    return it->second.get();
  }

  static std::string ReadArgumentValue(ICorProfilerInfo15 *pInfo, CorElementType elementType, UINT_PTR start)
  {
    if (start == 0)
      return "<?>";

    switch (elementType)
    {
    case ELEMENT_TYPE_STRING:
      return ReadParam<ELEMENT_TYPE_STRING>(pInfo, start);
    case ELEMENT_TYPE_BOOLEAN:
      return ReadParam<ELEMENT_TYPE_BOOLEAN>(pInfo, start);
    case ELEMENT_TYPE_CHAR:
      return ReadParam<ELEMENT_TYPE_CHAR>(pInfo, start);
    case ELEMENT_TYPE_I1:
      return ReadParam<ELEMENT_TYPE_I1>(pInfo, start);
    case ELEMENT_TYPE_U1:
      return ReadParam<ELEMENT_TYPE_U1>(pInfo, start);
    case ELEMENT_TYPE_I2:
      return ReadParam<ELEMENT_TYPE_I2>(pInfo, start);
    case ELEMENT_TYPE_U2:
      return ReadParam<ELEMENT_TYPE_U2>(pInfo, start);
    case ELEMENT_TYPE_I4:
      return ReadParam<ELEMENT_TYPE_I4>(pInfo, start);
    case ELEMENT_TYPE_U4:
      return ReadParam<ELEMENT_TYPE_U4>(pInfo, start);
    case ELEMENT_TYPE_I8:
      return ReadParam<ELEMENT_TYPE_I8>(pInfo, start);
    case ELEMENT_TYPE_U8:
      return ReadParam<ELEMENT_TYPE_U8>(pInfo, start);
    case ELEMENT_TYPE_R4:
      return ReadParam<ELEMENT_TYPE_R4>(pInfo, start);
    case ELEMENT_TYPE_R8:
      return ReadParam<ELEMENT_TYPE_R8>(pInfo, start);
    case ELEMENT_TYPE_I:
      return ReadParam<ELEMENT_TYPE_I>(pInfo, start);
    case ELEMENT_TYPE_U:
      return ReadParam<ELEMENT_TYPE_U>(pInfo, start);
    case ELEMENT_TYPE_CLASS:
      return ReadParam<ELEMENT_TYPE_CLASS>(pInfo, start);
    case ELEMENT_TYPE_SZARRAY:
      return ReadParam<ELEMENT_TYPE_SZARRAY>(pInfo, start);
    case ELEMENT_TYPE_ARRAY:
      return ReadParam<ELEMENT_TYPE_ARRAY>(pInfo, start);
    case ELEMENT_TYPE_PTR:
      return ReadParam<ELEMENT_TYPE_PTR>(pInfo, start);
    case ELEMENT_TYPE_VOID:
      return ReadParam<ELEMENT_TYPE_VOID>(pInfo, start);
    default:
      return "<?>";
    }
  }
}

size_t StackManager::BucketIndex(ThreadID tid) const
{
  return std::hash<ThreadID>{}(tid) % kThreadBuckets;
}

StackManager::ThreadStackState &StackManager::GetOrCreateThreadState(ThreadID tid)
{
  thread_local ThreadID cachedTid = 0;
  thread_local ThreadStackState *cachedState = nullptr;
  if (cachedState != nullptr && cachedTid == tid)
    return *cachedState;

  auto &bucket = m_threadBuckets[BucketIndex(tid)];

  {
    std::shared_lock lock(bucket.mutex);
    auto it = bucket.stacks.find(tid);
    if (it != bucket.stacks.end())
    {
      cachedTid = tid;
      cachedState = it->second.get();
      return *cachedState;
    }
  }

  {
    std::unique_lock lock(bucket.mutex);
    auto it = bucket.stacks.find(tid);
    if (it != bucket.stacks.end())
    {
      cachedTid = tid;
      cachedState = it->second.get();
      return *cachedState;
    }

    auto st = std::make_unique<ThreadStackState>();
    st->EnsureInit();
    auto &ref = *st;
    bucket.stacks.emplace(tid, std::move(st));
    cachedTid = tid;
    cachedState = &ref;
    return ref;
  }
}

StackManager::ThreadStackState *StackManager::GetCurrentThreadStateFast()
{
  if (m_corProfilerInfo == nullptr)
    return nullptr;

  thread_local ThreadStackState *cachedState = nullptr;
  if (cachedState != nullptr)
    return cachedState;

  ThreadID tid = 0;
  if (FAILED(m_corProfilerInfo->GetCurrentThreadID(&tid)) || tid == 0)
    return nullptr;

  cachedState = &GetOrCreateThreadState(tid);
  return cachedState;
}

const FunctionInfo *StackManager::GetOrBuildFunctionInfo(FunctionID id, COR_PRF_FRAME_INFO frameInfo)
{
  {
    std::shared_lock lock(m_functionInfosMutex);
    auto it = m_functionInfos.find(id);
    if (it != m_functionInfos.end())
    {
      return it->second.get();
    }
  }

  FunctionInfo built = BuildFunctionInfo(id, frameInfo);

  {
    std::unique_lock lock(m_functionInfosMutex);
    auto it = m_functionInfos.find(id);
    if (it != m_functionInfos.end())
    {
      return it->second.get();
    }
    auto ptr = std::make_unique<FunctionInfo>(std::move(built));
    const FunctionInfo *raw = ptr.get();
    m_functionInfos.emplace(id, std::move(ptr));
    return raw;
  }
}

void StackManager::GetArgumentInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo, COR_PRF_FRAME_INFO frameInfo, ULONG argumentInfoSize, ArgumentStartsStorage &argumentStarts)
{
  if (GetTracerLevel() != 2 || m_corProfilerInfo == nullptr || argumentInfoSize == 0)
    return;

  thread_local FunctionID cachedMetaFunctionId = 0;
  thread_local const MethodParamMeta *cachedMeta = nullptr;

  const MethodParamMeta *meta = nullptr;
  if (cachedMeta != nullptr && cachedMetaFunctionId == id.functionID)
  {
    meta = cachedMeta;
  }
  else
  {
    meta = GetOrBuildMethodParamMeta(m_corProfilerInfo, id.functionID, frameInfo);
    cachedMetaFunctionId = id.functionID;
    cachedMeta = meta;
  }

  if (meta == nullptr || meta->parameters.empty())
    return;

  thread_local std::vector<std::byte> argBuf;
  if (argBuf.size() < argumentInfoSize)
    argBuf.resize(argumentInfoSize);

  ULONG cbArgumentInfo = argumentInfoSize;
  if (SUCCEEDED(m_corProfilerInfo->GetFunctionEnter3Info(id.functionID, eltInfo, &frameInfo, &cbArgumentInfo, reinterpret_cast<COR_PRF_FUNCTION_ARGUMENT_INFO *>(argBuf.data()))))
  {
    auto *pArgInfo = reinterpret_cast<COR_PRF_FUNCTION_ARGUMENT_INFO *>(argBuf.data());
    ULONG hiddenThisOffset = meta->hasThis ? 1u : 0u;

    if (pArgInfo != NULL && pArgInfo->numRanges > hiddenThisOffset)
    {
      const ULONG paramCount = static_cast<ULONG>(meta->parameters.size());
      const ULONG available = pArgInfo->numRanges - hiddenThisOffset;
      const ULONG captureCount = (std::min)(available, paramCount);

      argumentStarts.Reset(captureCount);
      for (ULONG i = 0; i < captureCount; i++)
      {
        argumentStarts.Set(i, pArgInfo->ranges[i + hiddenThisOffset].startAddress);
      }
    }
  }
}

void StackManager::FunctionEnter(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  ThreadStackState *state = GetCurrentThreadStateFast();
  if (state == nullptr)
    return;

  StackFrame stackFrame;
  stackFrame.functionId = id.functionID;

  COR_PRF_FRAME_INFO frameInfo = NULL;
  ULONG argumentInfoSize = 0;
  const bool captureArguments = (GetTracerLevel() == 2);
  ULONG *argumentInfoSizePtr = captureArguments ? &argumentInfoSize : nullptr;
  m_corProfilerInfo->GetFunctionEnter3Info(id.functionID, eltInfo, &frameInfo, argumentInfoSizePtr, NULL);
  stackFrame.functionInfo = GetOrBuildFunctionInfo(id.functionID, frameInfo);

  if (captureArguments)
    GetArgumentInfo(id, eltInfo, frameInfo, argumentInfoSize, stackFrame.argumentStarts);

  {
    std::lock_guard<std::mutex> guard(state->mutex);
    state->EnsureInit();
    state->frames.push_back(std::move(stackFrame));
  }

  // LOG_F(INFO, "FunctionEnter: %d, duration: %lld us", id.functionID, duration.count());
  // Dump();

  // LOG_F(INFO, "FunctionEnter: %d", id.functionID);
}

FunctionInfo StackManager::BuildFunctionInfo(FunctionID id, COR_PRF_FRAME_INFO frameInfo)
{
  FunctionInfo info;
  ClassID classId;
  ModuleID moduleId;
  mdToken mdtokenFunction;

  m_corProfilerInfo->GetFunctionInfo(id, &classId, &moduleId, &mdtokenFunction);

  LPCBYTE loadAddress;
  ULONG nameLen = 0;
  AssemblyID assemblyId;

  auto hr = m_corProfilerInfo->GetModuleInfo(moduleId, &loadAddress, nameLen, &nameLen, NULL, &assemblyId);
  if (SUCCEEDED(hr))
  {
    WCHAR *pszName = new WCHAR[nameLen]; // count the trailing \0
    m_corProfilerInfo->GetModuleInfo(moduleId, &loadAddress, nameLen, &nameLen, pszName,
                                     &assemblyId);

    info.moduleName = WStrToUtf8(pszName);
    delete[] pszName;
  }

  hr = m_corProfilerInfo->GetAssemblyInfo(assemblyId, 0, &nameLen, NULL, NULL, NULL);
  if (SUCCEEDED(hr))
  {
    WCHAR *pszName = new WCHAR[nameLen]; // count the trailing \0
    hr = m_corProfilerInfo->GetAssemblyInfo(assemblyId, nameLen, &nameLen, pszName, NULL, NULL);
    info.assemblyName = WStrToUtf8(pszName);
    delete[] pszName;
  }

  if (classId == 0)
  {
    hr = m_corProfilerInfo->GetFunctionInfo2(id, frameInfo, &classId, &moduleId, &mdtokenFunction, 0, NULL, NULL);
  }

  const ULONG bufferLen = 1024;
  WCHAR pszName[bufferLen];
  GetTypeName(m_corProfilerInfo, NULL, classId, moduleId, pszName, bufferLen);
  info.typeName = WStrToUtf8(pszName);

  std::string methodSignature = GetMethodSignature(m_corProfilerInfo, id, frameInfo, info.typeName);
  info.methodSignature = methodSignature;
  return info;
}

void StackManager::FunctionLeave(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  (void)eltInfo;

  ThreadStackState *state = GetCurrentThreadStateFast();
  if (state == nullptr)
    return;

  std::lock_guard<std::mutex> guard(state->mutex);

  auto &frames = state->frames;
  if (frames.empty())
    return;

  const FunctionID leaveFunctionId = id.functionID;

  if (frames.back().functionId == leaveFunctionId)
  {
    frames.pop_back();
    return;
  }

  for (size_t i = frames.size(); i > 0; --i)
  {
    if (frames[i - 1].functionId == leaveFunctionId)
    {
      state->desyncFoundNotTop++;
      frames.resize(i - 1);
      if ((state->desyncFoundNotTop & 0x3FFu) == 0)
      {
        LOG("WARNING: Leave desync repaired (count=%u)", state->desyncFoundNotTop);
      }
      return;
    }
  }

  state->desyncNotFound++;
  if ((state->desyncNotFound & 0x3FFu) == 0) // every 1024 times
  {
    LOG("WARNING: Leave desync not found (count=%u)", state->desyncNotFound);
  }
}

void StackManager::FunctionTailcall(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  (void)id;
  (void)eltInfo;

  ThreadStackState *state = GetCurrentThreadStateFast();
  if (state == nullptr)
    return;

  std::lock_guard<std::mutex> guard(state->mutex);

  auto &frames = state->frames;
  if (frames.empty())
    return;

  frames.pop_back();
  state->tailcallPops++;
}

void StackManager::OnUnmanagedToManaged(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
  (void)reason;

  if (m_corProfilerInfo == nullptr)
    return;

  std::shared_lock lock(m_functionInfosMutex);

  const FunctionInfo *info = nullptr;
  auto it = m_functionInfos.find(functionId);
  if (it != m_functionInfos.end())
  {
    info = it->second.get();
  }
  else
  {
    return;
  }
  lock.unlock();

  auto now = std::chrono::steady_clock::now();

  std::unique_lock lock2(m_unmanagedToManagedTransitionsMutex);
  auto &record = m_unmanagedToManagedTransitions[functionId];
  record.functionInfo = info;
  record.lastTimestamp = now;
}

void StackManager::SetCorProfilerInfo(ICorProfilerInfo15 *corProfilerInfo)
{
  m_corProfilerInfo = corProfilerInfo;
}

ICorProfilerInfo15 *StackManager::GetCorProfilerInfo()
{
  return m_corProfilerInfo;
}

void StackManager::SetTracerLevel(int level)
{
  if (level < 0)
    level = 0;
  m_tracerLevel.store(level, std::memory_order_relaxed);
}

int StackManager::GetTracerLevel() const
{
  return m_tracerLevel.load(std::memory_order_relaxed);
}

void StackManager::OnThreadCreated(ThreadID threadId)
{
  auto &state = GetOrCreateThreadState(threadId);
  std::lock_guard<std::mutex> guard(state.mutex);
  state.EnsureInit();
}

void StackManager::OnThreadDestroyed(ThreadID threadId)
{
  auto &bucket = m_threadBuckets[BucketIndex(threadId)];
  std::unique_lock lock(bucket.mutex);
  bucket.stacks.erase(threadId);
}

void StackManager::OnThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
{
  auto &state = GetOrCreateThreadState(managedThreadId);
  std::lock_guard<std::mutex> guard(state.mutex);
  state.osThreadId = osThreadId;
}

void StackManager::OnThreadNameChanged(ThreadID threadId, const WCHAR *name)
{
  auto &state = GetOrCreateThreadState(threadId);
  std::lock_guard<std::mutex> guard(state.mutex);
  state.name = WStrToUtf8(name);
}

std::vector<StackManager::ThreadStackSnapshot> StackManager::SnapshotAllStacks() const
{
  std::vector<ThreadStackSnapshot> out;

  for (const auto &bucket : m_threadBuckets)
  {
    std::shared_lock bucketLock(bucket.mutex);
    for (const auto &kv : bucket.stacks)
    {
      const ThreadID tid = kv.first;
      const ThreadStackState *st = kv.second.get();
      if (st == nullptr)
        continue;

      ThreadStackSnapshot snap;
      snap.threadId = tid;

      {
        std::lock_guard<std::mutex> guard(st->mutex);
        snap.osThreadId = st->osThreadId;
        snap.nameUtf8 = st->name;
        snap.desyncNotFound = st->desyncNotFound;
        snap.desyncFoundNotTop = st->desyncFoundNotTop;
        snap.tailcallPops = st->tailcallPops;
        snap.frames = st->frames;
      }

      out.push_back(std::move(snap));
    }
  }

  return out;
}

void StackManager::Dump(std::string path) const
{
  std::ofstream outFile(path);
  if (!outFile.is_open())
    return;

  const auto snapshots = SnapshotAllStacks();
  for (const auto &snapshot : snapshots)
  {
    outFile << "Thread " << snapshot.threadId << ": " << snapshot.nameUtf8 << std::endl;
    for (auto it = snapshot.frames.rbegin(); it != snapshot.frames.rend(); ++it)
    {
      const auto &frame = *it;
      if (frame.functionInfo == nullptr)
        continue;

      outFile << "    " << frame.functionInfo->methodSignature << std::endl;
      const MethodParamMeta *meta = TryGetCachedMethodParamMeta(frame.functionId);
      if (meta != nullptr && !frame.argumentStarts.Empty())
      {
        const size_t count = (std::min)(meta->parameters.size(), frame.argumentStarts.Size());
        for (size_t i = 0; i < count; i++)
        {
          UINT_PTR start = frame.argumentStarts.Get(i);
          if (start == 0)
            continue;

          const auto &pm = meta->parameters[i];
          std::string value = ReadArgumentValue(m_corProfilerInfo, pm.elementType, start);
          outFile << "    " << pm.declarationPrefix << value << std::endl;
        }
      }
      outFile << "        Assembly: " << frame.functionInfo->assemblyName << std::endl;
      outFile << "        Module  : " << frame.functionInfo->moduleName << std::endl;
      outFile << std::endl;
    }
    if (snapshot.frames.empty())
    {
      outFile << "    No frames" << std::endl;
      outFile << std::endl;
    }
  }

  outFile << "Recent 50 unmanaged to managed transitions (last seen):" << std::endl;
  auto now = std::chrono::steady_clock::now();
  std::vector<std::pair<FunctionID, TransitionRecord>> sortedTransitions;
  {
    std::shared_lock lock(m_unmanagedToManagedTransitionsMutex);
    sortedTransitions.assign(m_unmanagedToManagedTransitions.begin(), m_unmanagedToManagedTransitions.end());
  }

  if (sortedTransitions.empty())
  {
    outFile << "    (none)" << std::endl;
    return;
  }

  std::sort(sortedTransitions.begin(), sortedTransitions.end(), [](const auto &a, const auto &b)
            { return a.second.lastTimestamp > b.second.lastTimestamp; });
  sortedTransitions.resize(std::min<size_t>(sortedTransitions.size(), 50));
  for (const auto &kv : sortedTransitions)
  {
    const auto &record = kv.second;
    if (record.functionInfo == nullptr || record.lastTimestamp.time_since_epoch().count() == 0)
      continue;

    auto age = std::chrono::duration_cast<std::chrono::nanoseconds>(now - record.lastTimestamp).count();
    outFile << "    " << record.functionInfo->methodSignature << std::endl;
    outFile << "        Assembly: " << record.functionInfo->assemblyName << std::endl;
    outFile << "        Module  : " << record.functionInfo->moduleName << std::endl;
    outFile << "        Age (ns): " << age << std::endl;
    outFile << std::endl;
  }
}

StackManager g_StackManager;

StackManager *GlobalStackManager()
{
  return &g_StackManager;
}
