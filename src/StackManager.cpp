#include "StackManager.h"
#include "loguru.hpp"
#include <string>
#include <codecvt>
#include <sstream>
#include <cstdint>
#include <utility>
#include <vector>
#include "Helper.h"
#include "ParamReader.h"

namespace
{
  struct ThreadStackState
  {
    std::vector<StackFrame> frames;
    uint32_t desyncNotFound = 0;
    uint32_t desyncFoundNotTop = 0;
    uint32_t tailcallPops = 0;

    void EnsureInit()
    {
      if (frames.capacity() == 0)
      {
        frames.reserve(256);
      }
    }
  };

  thread_local ThreadStackState g_tlsStack;

  struct ParamMeta
  {
    CorElementType elementType = ELEMENT_TYPE_END;
    std::string typeName;
    std::string name;
  };

  struct MethodParamMeta
  {
    bool hasThis = false;
    std::vector<ParamMeta> parameters;
  };

  static MethodParamMeta GetMethodParamMeta(ICorProfilerInfo15* pInfo, FunctionID functionId, COR_PRF_ELT_INFO eltInfo)
  {
    MethodParamMeta out;
    if (pInfo == nullptr)
      return out;

    COR_PRF_FRAME_INFO frameInfo = NULL;
    ULONG cbArgumentInfo = 0;
    pInfo->GetFunctionEnter3Info(functionId, eltInfo, &frameInfo, &cbArgumentInfo, NULL);

    ClassID classId = 0;
    ModuleID moduleId = 0;
    mdToken tkMethod = 0;
    pInfo->GetFunctionInfo2(functionId, frameInfo, &classId, &moduleId, &tkMethod, 0, NULL, NULL);

    IMetaDataImport2 *pMetaDataImport = NULL;
    if (FAILED(pInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport2, reinterpret_cast<IUnknown**>(&pMetaDataImport))) || pMetaDataImport == nullptr)
      return out;

    mdTypeDef type;
    WCHAR name[260];
    ULONG size;
    ULONG attributes;
    PCCOR_SIGNATURE pSig;
    ULONG blobSize;
    ULONG codeRva;
    DWORD flags;
    auto hr = pMetaDataImport->GetMethodProps(
        tkMethod, &type, name, ARRAY_LEN(name) - 1, &size, &attributes, &pSig, &blobSize, &codeRva, &flags);

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
      pm.typeName = ParseSigType(pInfo, pMetaDataImport, moduleId, nullptr, 0, pWalk);
      pm.name = std::move(paramNames[i]);
      out.parameters.push_back(std::move(pm));
    }

    pMetaDataImport->Release();
    return out;
  }

  static std::string DefaultArgName(ULONG idx)
  {
    return "arg" + std::to_string(idx);
  }
}


const FunctionInfo *StackManager::GetOrBuildFunctionInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  {
    std::shared_lock lock(m_functionInfosMutex);
    auto it = m_functionInfos.find(id.functionID);
    if (it != m_functionInfos.end())
    {
      return it->second.get();
    }
  }

  FunctionInfo built = BuildFunctionInfo(id, eltInfo);

  {
    std::unique_lock lock(m_functionInfosMutex);
    auto it = m_functionInfos.find(id.functionID);
    if (it != m_functionInfos.end())
    {
      return it->second.get();
    }
    auto ptr = std::make_unique<FunctionInfo>(std::move(built));
    const FunctionInfo* raw = ptr.get();
    m_functionInfos.emplace(id.functionID, std::move(ptr));
    return raw;
  }
}

void StackManager::GetArgumentInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo, std::vector<std::string> &argumentInfo)
{
  COR_PRF_FRAME_INFO frameInfo = NULL;
  ULONG argumentInfoSize = 0;
  m_corProfilerInfo->GetFunctionEnter3Info(id.functionID, eltInfo, &frameInfo, &argumentInfoSize, NULL);
  if (argumentInfoSize > 0)
  {
    std::vector<std::byte> argBuf(argumentInfoSize);
    if (SUCCEEDED(m_corProfilerInfo->GetFunctionEnter3Info(id.functionID, eltInfo, &frameInfo, &argumentInfoSize, reinterpret_cast<COR_PRF_FUNCTION_ARGUMENT_INFO *>(argBuf.data()))))
    {
      auto *pArgInfo = reinterpret_cast<COR_PRF_FUNCTION_ARGUMENT_INFO *>(argBuf.data());
      MethodParamMeta meta = GetMethodParamMeta(m_corProfilerInfo, id.functionID, eltInfo);
      ULONG hiddenThisOffset = meta.hasThis ? 1u : 0u;

      if (pArgInfo != NULL && pArgInfo->numRanges >= meta.parameters.size() + hiddenThisOffset)
      {
        for (ULONG i = 0; i < (ULONG)meta.parameters.size(); i++)
        {
          const auto &pm = meta.parameters[i];

          UINT_PTR start = pArgInfo->ranges[i + hiddenThisOffset].startAddress;
          if (start == 0)
            continue;

          std::string s = "<?>";
          switch (pm.elementType)
          {
            case ELEMENT_TYPE_STRING:
              s = ReadParam<ELEMENT_TYPE_STRING>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_BOOLEAN:
              s = ReadParam<ELEMENT_TYPE_BOOLEAN>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_CHAR:
              s = ReadParam<ELEMENT_TYPE_CHAR>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_I1:
              s = ReadParam<ELEMENT_TYPE_I1>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_U1:
              s = ReadParam<ELEMENT_TYPE_U1>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_I2:
              s = ReadParam<ELEMENT_TYPE_I2>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_U2:
              s = ReadParam<ELEMENT_TYPE_U2>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_I4:
              s = ReadParam<ELEMENT_TYPE_I4>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_U4:
              s = ReadParam<ELEMENT_TYPE_U4>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_I8:
              s = ReadParam<ELEMENT_TYPE_I8>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_U8:
              s = ReadParam<ELEMENT_TYPE_U8>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_R4:
              s = ReadParam<ELEMENT_TYPE_R4>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_R8:
              s = ReadParam<ELEMENT_TYPE_R8>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_I:
              s = ReadParam<ELEMENT_TYPE_I>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_U:
              s = ReadParam<ELEMENT_TYPE_U>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_OBJECT:
              s = ReadParam<ELEMENT_TYPE_OBJECT>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_CLASS:
              s = ReadParam<ELEMENT_TYPE_CLASS>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_SZARRAY:
              s = ReadParam<ELEMENT_TYPE_SZARRAY>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_ARRAY:
              s = ReadParam<ELEMENT_TYPE_ARRAY>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_PTR:
              s = ReadParam<ELEMENT_TYPE_PTR>(m_corProfilerInfo, start);
              break;
            case ELEMENT_TYPE_VOID:
              s = ReadParam<ELEMENT_TYPE_VOID>(m_corProfilerInfo, start);
              break;
            default:
              s = "<?>";
              break;
          }
          const std::string &name = pm.name.empty() ? DefaultArgName(i) : pm.name;
          argumentInfo.push_back(pm.typeName + " " + name + " = " + s);
        }
      }
    }
  }
}

void StackManager::FunctionEnter(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  StackFrame stackFrame;
  stackFrame.functionId = id.functionID;

  stackFrame.functionInfo = GetOrBuildFunctionInfo(id, eltInfo);

  GetArgumentInfo(id, eltInfo, stackFrame.argumentInfo);
  
  stackFrame.DebugPrint();

  g_tlsStack.EnsureInit();
  g_tlsStack.frames.push_back(stackFrame);

  // LOG_F(INFO, "FunctionEnter: %d", id.functionID);
}

FunctionInfo StackManager::BuildFunctionInfo(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  FunctionInfo info;
  ClassID classId;
  ModuleID moduleId;
  mdToken mdtokenFunction;

  m_corProfilerInfo->GetFunctionInfo(id.functionID, &classId, &moduleId, &mdtokenFunction);

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
    COR_PRF_FRAME_INFO frameInfo = NULL;
    ULONG nbArgumentInfo = 0;

    hr = m_corProfilerInfo->GetFunctionEnter3Info(id.functionID, eltInfo, &frameInfo, &nbArgumentInfo, NULL);
    hr = m_corProfilerInfo->GetFunctionInfo2(id.functionID, frameInfo, &classId, &moduleId, &mdtokenFunction, 0, NULL, NULL);
  }

  const ULONG bufferLen = 1024;
  WCHAR pszName[bufferLen];
  GetTypeName(m_corProfilerInfo, NULL, classId, moduleId, pszName, bufferLen);
  info.typeName = WStrToUtf8(pszName);

  std::string methodSignature = GetMethodSignature(m_corProfilerInfo, id.functionID, eltInfo, info.typeName);
  info.methodSignature = methodSignature;
  return info;
}

void StackManager::FunctionLeave(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  (void)eltInfo;

  auto& frames = g_tlsStack.frames;
  if (frames.empty())
    return;

  if (frames.back().functionId == id.functionID)
  {
    frames.pop_back();
    return;
  }

  for (size_t i = frames.size(); i-- > 0;)
  {
    if (frames[i].functionId == id.functionID)
    {
      g_tlsStack.desyncFoundNotTop++;
      frames.resize(i);
      if ((g_tlsStack.desyncFoundNotTop & 0x3FFu) == 0) 
      {
        LOG_F(WARNING, "Leave desync repaired (count=%u)", g_tlsStack.desyncFoundNotTop);
      }
      return;
    }
  }

  g_tlsStack.desyncNotFound++;
  if ((g_tlsStack.desyncNotFound & 0x3FFu) == 0) // every 1024 times
  {
    LOG_F(WARNING, "Leave desync not found (count=%u)", g_tlsStack.desyncNotFound);
  }
}

void StackManager::FunctionTailcall(FunctionIDOrClientID id, COR_PRF_ELT_INFO eltInfo)
{
  (void)id;
  (void)eltInfo;

  auto& frames = g_tlsStack.frames;
  if (frames.empty())
    return;

  frames.pop_back();
  g_tlsStack.tailcallPops++;
}

void StackManager::SetCorProfilerInfo(ICorProfilerInfo15 *corProfilerInfo)
{
  m_corProfilerInfo = corProfilerInfo;
}

StackManager g_StackManager;

StackManager *GlobalStackManager()
{
  return &g_StackManager;
}