#pragma once

#include <string>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
#include "cor.h"
#include "corprof.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

inline std::string WStrToUtf8(const WCHAR *wstr)
{
  if (wstr == nullptr)
    return "";
#if defined(_WIN32)
  int n = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1)
    return "";
  std::string out((size_t)n - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), n, nullptr, nullptr);
  return out;
#else
  static_assert(sizeof(WCHAR) == sizeof(char16_t));
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
  return conv.to_bytes(reinterpret_cast<const char16_t *>(wstr));
#endif
}

inline std::string WStrToUtf8(const std::basic_string<WCHAR> &wstr)
{
  return WStrToUtf8(wstr.c_str());
}

inline void FixGenericSyntax(WCHAR *name)
{
  ULONG currentCharPos = 0;
  while (name[currentCharPos] != static_cast<WCHAR>(0))
  {
    if (name[currentCharPos] == static_cast<WCHAR>(u'`'))
    {
      name[currentCharPos] = static_cast<WCHAR>(0);
      return;
    }
    currentCharPos++;
  }
}

inline void CopyWTrunc(WCHAR *dst, ULONG dstLen, const std::basic_string<WCHAR> &src)
{
  if (dst == nullptr || dstLen == 0)
    return;
  size_t n = (std::min)(src.size(), (size_t)dstLen - 1);
  if (n)
    std::char_traits<WCHAR>::copy(dst, src.data(), n);
  dst[n] = static_cast<WCHAR>(0);
}

inline void GetTypeName2(ICorProfilerInfo15 *pInfo, IMetaDataImport2 *pMetaDataImport, mdToken mdType, ULONG numGenericTypeArgs, ClassID *genericTypeArgs, WCHAR *pszName, ULONG bufferLen);

inline void GetTypeName(ICorProfilerInfo15 *pInfo, IMetaDataImport2 *pMetaDataImport, ClassID classId, ModuleID moduleId, WCHAR *pszName, ULONG bufferLen)
{
  mdTypeDef mdType;
  ClassID parentClassId;
  ULONG32 numGenericTypeArgs = 0;

  pInfo->GetClassIDInfo2(classId, NULL, &mdType, &parentClassId, 0, &numGenericTypeArgs, NULL);

  std::vector<ClassID> genericTypeArgs;
  if (numGenericTypeArgs)
    genericTypeArgs.resize(numGenericTypeArgs);
  if (numGenericTypeArgs)
    pInfo->GetClassIDInfo2(classId, NULL, &mdType, &parentClassId, numGenericTypeArgs, &numGenericTypeArgs, genericTypeArgs.data());

  IMetaDataImport2 *metadataImport = pMetaDataImport;
  bool releaseMeta = false;
  if (metadataImport == NULL)
    releaseMeta = SUCCEEDED(pInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport2, reinterpret_cast<IUnknown **>(&metadataImport))) && metadataImport != nullptr;

  GetTypeName2(pInfo, metadataImport, mdType, numGenericTypeArgs, genericTypeArgs.empty() ? nullptr : genericTypeArgs.data(), pszName, bufferLen);
  if (releaseMeta)
    metadataImport->Release();
}

inline void GetTypeName2(ICorProfilerInfo15 *pInfo, IMetaDataImport2 *pMetaDataImport, mdToken mdType, ULONG numGenericTypeArgs, ClassID *genericTypeArgs, WCHAR *pszName, ULONG bufferLen)
{
  ULONG length = bufferLen;
  DWORD flags;
  mdTypeDef mdBaseType;
  pszName[0] = static_cast<WCHAR>(0);

  auto hr = pMetaDataImport->GetTypeDefProps(mdType, pszName, length, &length, &flags, &mdBaseType);

  if (!IsTdNested(flags) && numGenericTypeArgs == 0)
  {
    return;
  }

  std::basic_string<WCHAR> out;
  if (IsTdNested(flags))
  {
    mdToken mdEnclosingClass;
    pMetaDataImport->GetNestedClassProps(mdType, &mdEnclosingClass);

    std::vector<WCHAR> enclosing(bufferLen);
    enclosing[0] = static_cast<WCHAR>(0);
    GetTypeName2(pInfo, pMetaDataImport, mdEnclosingClass, numGenericTypeArgs, genericTypeArgs, enclosing.data(), bufferLen);
    out += enclosing.data();
    out += static_cast<WCHAR>(u'+');
  }
  if (numGenericTypeArgs > 0)
  {
    FixGenericSyntax(pszName);

    out += pszName;
    out += static_cast<WCHAR>(u'<');

    for (size_t currentGenericArg = 0; currentGenericArg < numGenericTypeArgs; currentGenericArg++)
    {
      ClassID argClassId = genericTypeArgs[currentGenericArg];
      ModuleID argModuleId;
      pInfo->GetClassIDInfo2(argClassId, &argModuleId, NULL, 0, NULL, NULL, NULL);
      WCHAR argTypeName[260];
      GetTypeName(pInfo, pMetaDataImport, argClassId, argModuleId, argTypeName, 260);
      out += argTypeName;

      if (currentGenericArg < numGenericTypeArgs - 1)
      {
        out += static_cast<WCHAR>(u',');
        out += static_cast<WCHAR>(u' ');
      }
    }

    out += static_cast<WCHAR>(u'>');
  }
  else
  {
    out += pszName;
  }

  CopyWTrunc(pszName, bufferLen, out);
}

inline std::string GetTypeNameFromTypeToken(IMetaDataImport2 *pMetaDataImport, mdToken tkType);
inline std::string ParseSigType(ICorProfilerInfo15 *pInfo, IMetaDataImport2 *pMetaDataImport, ModuleID moduleId, const ClassID *typeArgs, ULONG typeArgsCount, PCCOR_SIGNATURE &pSig);

inline std::string GetTypeNameFromTypeToken(IMetaDataImport2 *pMetaDataImport, mdToken tkType)
{
  if (TypeFromToken(tkType) == mdtTypeDef)
  {
    const ULONG bufferLen = 512;
    WCHAR name[bufferLen];
    name[0] = static_cast<WCHAR>(0);
    DWORD flags = 0;
    mdTypeDef mdBaseType = 0;
    pMetaDataImport->GetTypeDefProps(tkType, name, bufferLen, nullptr, &flags, &mdBaseType);
    GetTypeName2(nullptr, pMetaDataImport, tkType, 0, nullptr, name, bufferLen);
    FixGenericSyntax(name);
    return WStrToUtf8(name);
  }

  if (TypeFromToken(tkType) == mdtTypeRef)
  {
    const ULONG bufferLen = 512;
    WCHAR name[bufferLen];
    name[0] = static_cast<WCHAR>(0);
    mdToken scope = 0;
    pMetaDataImport->GetTypeRefProps(tkType, &scope, name, bufferLen, nullptr);
    FixGenericSyntax(name);
    return WStrToUtf8(name);
  }

  if (TypeFromToken(tkType) == mdtTypeSpec)
  {
    PCCOR_SIGNATURE pSpecSig = nullptr;
    ULONG cbSpecSig = 0;
    if (SUCCEEDED(pMetaDataImport->GetTypeSpecFromToken(tkType, &pSpecSig, &cbSpecSig)) && pSpecSig != nullptr)
    {
      PCCOR_SIGNATURE p = pSpecSig;
      return ParseSigType(nullptr, pMetaDataImport, 0, nullptr, 0, p);
    }
  }

  return "";
}

inline std::string ElementTypeName(CorElementType et)
{
  switch (et)
  {
  case ELEMENT_TYPE_VOID: return "void";
  case ELEMENT_TYPE_BOOLEAN: return "bool";
  case ELEMENT_TYPE_CHAR: return "char";
  case ELEMENT_TYPE_I1: return "sbyte";
  case ELEMENT_TYPE_U1: return "byte";
  case ELEMENT_TYPE_I2: return "short";
  case ELEMENT_TYPE_U2: return "ushort";
  case ELEMENT_TYPE_I4: return "int";
  case ELEMENT_TYPE_U4: return "uint";
  case ELEMENT_TYPE_I8: return "long";
  case ELEMENT_TYPE_U8: return "ulong";
  case ELEMENT_TYPE_R4: return "float";
  case ELEMENT_TYPE_R8: return "double";
  case ELEMENT_TYPE_STRING: return "string";
  case ELEMENT_TYPE_OBJECT: return "object";
  case ELEMENT_TYPE_I: return "nint";
  case ELEMENT_TYPE_U: return "nuint";
  default: return "";
  }
}

inline std::string GetTypeNameFromClassID(ICorProfilerInfo15 *pInfo, IMetaDataImport2 *pMetaDataImport, ClassID classId, ModuleID moduleId)
{
  const ULONG bufferLen = 512;
  WCHAR name[bufferLen];
  name[0] = static_cast<WCHAR>(0);
  if (pInfo != nullptr)
  {
    GetTypeName(pInfo, pMetaDataImport, classId, moduleId, name, bufferLen);
    return WStrToUtf8(name);
  }
  return "";
}

inline std::string ParseSigType(ICorProfilerInfo15 *pInfo, IMetaDataImport2 *pMetaDataImport, ModuleID moduleId, const ClassID *typeArgs, ULONG typeArgsCount, PCCOR_SIGNATURE &pSig)
{
  CorElementType et = (CorElementType)*pSig++;
  auto prim = ElementTypeName(et);
  if (!prim.empty())
    return prim;

  switch (et)
  {
  case ELEMENT_TYPE_CLASS:
  case ELEMENT_TYPE_VALUETYPE:
  {
    mdToken tk = 0;
    pSig += CorSigUncompressToken(pSig, &tk);
    return GetTypeNameFromTypeToken(pMetaDataImport, tk);
  }
  case ELEMENT_TYPE_SZARRAY:
  {
    auto elem = ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs, typeArgsCount, pSig);
    return elem + "[]";
  }
  case ELEMENT_TYPE_ARRAY:
  {
    auto elem = ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs, typeArgsCount, pSig);
    ULONG rank = 0;
    pSig += CorSigUncompressData(pSig, &rank);
    ULONG numSizes = 0;
    pSig += CorSigUncompressData(pSig, &numSizes);
    for (ULONG i = 0; i < numSizes; i++)
    {
      ULONG tmp = 0;
      pSig += CorSigUncompressData(pSig, &tmp);
    }
    ULONG numLoBounds = 0;
    pSig += CorSigUncompressData(pSig, &numLoBounds);
    for (ULONG i = 0; i < numLoBounds; i++)
    {
      int tmp = 0;
      pSig += CorSigUncompressSignedInt(pSig, &tmp);
    }
    std::string dims = "[";
    for (ULONG i = 1; i < rank; i++)
      dims += ",";
    dims += "]";
    return elem + dims;
  }
  case ELEMENT_TYPE_BYREF:
  {
    auto inner = ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs, typeArgsCount, pSig);
    return inner + "&";
  }
  case ELEMENT_TYPE_PTR:
  {
    auto inner = ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs, typeArgsCount, pSig);
    return inner + "*";
  }
  case ELEMENT_TYPE_GENERICINST:
  {
    CorElementType kind = (CorElementType)*pSig++;
    mdToken tk = 0;
    pSig += CorSigUncompressToken(pSig, &tk);
    ULONG n = 0;
    pSig += CorSigUncompressData(pSig, &n);
    std::string name = GetTypeNameFromTypeToken(pMetaDataImport, tk);
    name += "<";
    for (ULONG i = 0; i < n; i++)
    {
      if (i)
        name += ", ";
      name += ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs, typeArgsCount, pSig);
    }
    name += ">";
    return name;
  }
  case ELEMENT_TYPE_VAR:
  case ELEMENT_TYPE_MVAR:
  {
    ULONG idx = 0;
    pSig += CorSigUncompressData(pSig, &idx);
    if (typeArgs != nullptr && idx < typeArgsCount && pInfo != nullptr)
    {
      ClassID cid = typeArgs[idx];
      ModuleID mid = 0;
      pInfo->GetClassIDInfo2(cid, &mid, NULL, 0, NULL, NULL, NULL);
      return GetTypeNameFromClassID(pInfo, pMetaDataImport, cid, mid);
    }
    return (et == ELEMENT_TYPE_MVAR ? "!!" : "!") + std::to_string(idx);
  }
  case ELEMENT_TYPE_TYPEDBYREF: return "typedref";
  case ELEMENT_TYPE_FNPTR: return "fnptr";
  case ELEMENT_TYPE_PINNED: return ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs, typeArgsCount, pSig);
  default: return "";
  }
}

inline std::string GetMethodSignature(ICorProfilerInfo15 *pInfo, FunctionID functionId, COR_PRF_ELT_INFO eltInfo, const std::string &declaringTypeName)
{
  if (pInfo == nullptr)
    return "";

  COR_PRF_FRAME_INFO frameInfo = NULL;
  ULONG cbArgumentInfo = 0;
  pInfo->GetFunctionEnter3Info(functionId, eltInfo, &frameInfo, &cbArgumentInfo, nullptr);

  ClassID classId = 0;
  ModuleID moduleId = 0;
  mdToken tkMethod = 0;

  ULONG32 typeArgsCount = 0;
  pInfo->GetFunctionInfo2(functionId, frameInfo, &classId, &moduleId, &tkMethod, 0, &typeArgsCount, nullptr);

  std::vector<ClassID> typeArgs;
  if (typeArgsCount)
  {
    typeArgs.resize(typeArgsCount);
    pInfo->GetFunctionInfo2(functionId, frameInfo, &classId, &moduleId, &tkMethod, typeArgsCount, &typeArgsCount, typeArgs.data());
  }

  IMetaDataImport2 *pMetaDataImport = nullptr;
  if (FAILED(pInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport2, reinterpret_cast<IUnknown **>(&pMetaDataImport))) || pMetaDataImport == nullptr)
    return "";

  const ULONG nameBufLen = 512;
  WCHAR wszMethodName[nameBufLen];
  wszMethodName[0] = static_cast<WCHAR>(0);
  PCCOR_SIGNATURE pSig = nullptr;
  ULONG cbSig = 0;
  mdTypeDef mdClass = 0;

  auto hr = pMetaDataImport->GetMethodProps((mdMethodDef)tkMethod, &mdClass, wszMethodName, nameBufLen, nullptr, nullptr, &pSig, &cbSig, nullptr, nullptr);
  if (FAILED(hr) || pSig == nullptr)
  {
    pMetaDataImport->Release();
    return "";
  }

  ULONG callConv = 0;
  pSig += CorSigUncompressData(pSig, &callConv);

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

  PCCOR_SIGNATURE pSigWalk = pSig;
  auto retType = ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs.empty() ? nullptr : typeArgs.data(), typeArgsCount, pSigWalk);

  std::string sig = retType;
  sig += " ";
  sig += declaringTypeName;
  sig += ".";
  sig += WStrToUtf8(wszMethodName);
  sig += "(";

  for (ULONG i = 0; i < paramCount; i++)
  {
    if (i)
      sig += ", ";
    auto t = ParseSigType(pInfo, pMetaDataImport, moduleId, typeArgs.empty() ? nullptr : typeArgs.data(), typeArgsCount, pSigWalk);
    sig += t;
    if (!paramNames[i].empty())
    {
      sig += " ";
      sig += paramNames[i];
    }
  }
  sig += ")";

  pMetaDataImport->Release();
  return sig;
}