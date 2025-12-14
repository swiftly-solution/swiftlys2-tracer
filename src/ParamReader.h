#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstddef>
#include <cstdint>
#include "cor.h"
#include "corprof.h"
#include "Helper.h"

#define PR_NULL_VALUE "<NULL>"
#define PR_UNKNOWN_VALUE "<?>"

template <CorElementType T>
std::string ReadParam(ICorProfilerInfo15 *pInfo, UINT_PTR start);

template <> std::string ReadParam<ELEMENT_TYPE_STRING>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_BOOLEAN>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_CHAR>(ICorProfilerInfo15 *pInfo, UINT_PTR start); 
template <> std::string ReadParam<ELEMENT_TYPE_I1>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_U1>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_I2>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_U2>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_I4>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_U4>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_I8>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_U8>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_R4>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_R8>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_I>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_U>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_PTR>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_OBJECT>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_CLASS>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_SZARRAY>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_ARRAY>(ICorProfilerInfo15 *pInfo, UINT_PTR start);
template <> std::string ReadParam<ELEMENT_TYPE_VOID>(ICorProfilerInfo15 *pInfo, UINT_PTR start);

static inline std::string HexPtr(UINT_PTR p)
{
  std::ostringstream oss;
  oss << "0x" << std::hex << std::uppercase << (std::uint64_t)p;
  return oss.str();
}

static inline std::string TryGetObjectTypeName(ICorProfilerInfo15* pInfo, ObjectID objId)
{
  if (pInfo == nullptr || objId == 0)
    return "";

  ClassID classId = 0;
  if (FAILED(pInfo->GetClassFromObject(objId, &classId)) || classId == 0)
    return "";

  ModuleID moduleId = 0;
  if (FAILED(pInfo->GetClassIDInfo2(classId, &moduleId, nullptr, nullptr, 0, nullptr, nullptr)))
    return "";

  const ULONG bufferLen = 512;
  WCHAR name[bufferLen];
  name[0] = static_cast<WCHAR>(0);
  GetTypeName(pInfo, NULL, classId, moduleId, name, bufferLen);
  return WStrToUtf8(name);
}

static inline size_t ElementSizeBytes(ICorProfilerInfo15* pInfo, CorElementType et, ClassID valueTypeClassId)
{
  switch (et)
  {
    case ELEMENT_TYPE_BOOLEAN: return sizeof(bool);
    case ELEMENT_TYPE_CHAR: return sizeof(WCHAR);
    case ELEMENT_TYPE_I1: return sizeof(char);
    case ELEMENT_TYPE_U1: return sizeof(unsigned char);
    case ELEMENT_TYPE_I2: return sizeof(short);
    case ELEMENT_TYPE_U2: return sizeof(unsigned short);
    case ELEMENT_TYPE_I4: return sizeof(int);
    case ELEMENT_TYPE_U4: return sizeof(unsigned int);
    case ELEMENT_TYPE_I8: return sizeof(LONG64);
    case ELEMENT_TYPE_U8: return sizeof(ULONG64);
    case ELEMENT_TYPE_R4: return sizeof(float);
    case ELEMENT_TYPE_R8: return sizeof(double);
    case ELEMENT_TYPE_I: return sizeof(void*);
    case ELEMENT_TYPE_U: return sizeof(void*);
    // references
    case ELEMENT_TYPE_STRING:
    case ELEMENT_TYPE_CLASS:
    case ELEMENT_TYPE_OBJECT:
    case ELEMENT_TYPE_SZARRAY:
    case ELEMENT_TYPE_ARRAY:
    case ELEMENT_TYPE_PTR:
    case ELEMENT_TYPE_BYREF:
      return sizeof(void*);
    case ELEMENT_TYPE_VALUETYPE:
    {
      if (pInfo == nullptr || valueTypeClassId == 0)
        return 0;
      ULONG fieldCount = 0;
      ULONG classSize = 0;
      if (SUCCEEDED(pInfo->GetClassLayout(valueTypeClassId, nullptr, 0, &fieldCount, &classSize)) && classSize != 0)
        return (size_t)classSize;
      return 0;
    }
    default:
      return 0;
  }
}

static inline std::string ReadParamDynamic(ICorProfilerInfo15* pInfo, CorElementType et, UINT_PTR start, int depth);
static inline std::string ReadArrayParam(ICorProfilerInfo15* pInfo, UINT_PTR start, int depth)
{
  UINT_PTR objRef = *reinterpret_cast<const UINT_PTR*>(reinterpret_cast<const void*>(start));
  if (objRef == 0)
    return PR_NULL_VALUE;
  if (pInfo == nullptr)
    return PR_UNKNOWN_VALUE;

  ObjectID arrayObj = (ObjectID)objRef;
  ClassID arrayClassId = 0;
  if (FAILED(pInfo->GetClassFromObject(arrayObj, &arrayClassId)) || arrayClassId == 0)
    return PR_UNKNOWN_VALUE;

  CorElementType baseEt = ELEMENT_TYPE_END;
  ClassID baseClassId = 0;
  ULONG rank = 0;
  if (FAILED(pInfo->IsArrayClass(arrayClassId, &baseEt, &baseClassId, &rank)) || rank == 0)
    return PR_UNKNOWN_VALUE;

  std::vector<ULONG32> dimSizes(rank);
  std::vector<int> dimLoBounds(rank);
  BYTE* pElements = nullptr;
  if (FAILED(pInfo->GetArrayObjectInfo(arrayObj, rank, dimSizes.data(), dimLoBounds.data(), &pElements)) || pElements == nullptr)
    return PR_UNKNOWN_VALUE;

  std::uint64_t total = 1;
  for (ULONG i = 0; i < rank; i++)
  {
    total *= (std::uint64_t)dimSizes[i];
    if (total > (std::uint64_t)1e9)
    {
      total = (std::uint64_t)1e9;
      break;
    }
  }

  std::ostringstream oss;
  // show type if possible
  auto tn = TryGetObjectTypeName(pInfo, arrayObj);
  if (!tn.empty())
    oss << tn << " ";
  oss << HexPtr((UINT_PTR)arrayObj);

  oss << " [";
  for (ULONG i = 0; i < rank; i++)
  {
    if (i) oss << ", ";
    oss << dimSizes[i];
  }
  oss << "] = {";

  if (total == 0)
  {
    oss << "}";
    return oss.str();
  }

  const std::uint64_t maxElements = 16;
  std::uint64_t count = (total < maxElements) ? total : maxElements;

  size_t elemSize = ElementSizeBytes(pInfo, baseEt, baseClassId);
  if (elemSize == 0)
  {
    elemSize = sizeof(void*);
  }

  BYTE* pCur = pElements;
  for (std::uint64_t i = 0; i < count; i++)
  {
    if (i) oss << ", ";
    oss << ReadParamDynamic(pInfo, baseEt, (UINT_PTR)pCur, depth + 1);
    pCur += elemSize;
  }

  if (total > count)
    oss << ", ...";
  oss << "}";
  return oss.str();
}

static inline std::string ReadParamDynamic(ICorProfilerInfo15* pInfo, CorElementType et, UINT_PTR start, int depth)
{
  if (depth > 2)
    return PR_UNKNOWN_VALUE;

  switch (et)
  {
    case ELEMENT_TYPE_STRING: return ReadParam<ELEMENT_TYPE_STRING>(pInfo, start);
    case ELEMENT_TYPE_BOOLEAN: return ReadParam<ELEMENT_TYPE_BOOLEAN>(pInfo, start);
    case ELEMENT_TYPE_CHAR: return ReadParam<ELEMENT_TYPE_CHAR>(pInfo, start);
    case ELEMENT_TYPE_I1: return ReadParam<ELEMENT_TYPE_I1>(pInfo, start);
    case ELEMENT_TYPE_U1: return ReadParam<ELEMENT_TYPE_U1>(pInfo, start);
    case ELEMENT_TYPE_I2: return ReadParam<ELEMENT_TYPE_I2>(pInfo, start);
    case ELEMENT_TYPE_U2: return ReadParam<ELEMENT_TYPE_U2>(pInfo, start);
    case ELEMENT_TYPE_I4: return ReadParam<ELEMENT_TYPE_I4>(pInfo, start);
    case ELEMENT_TYPE_U4: return ReadParam<ELEMENT_TYPE_U4>(pInfo, start);
    case ELEMENT_TYPE_I8: return ReadParam<ELEMENT_TYPE_I8>(pInfo, start);
    case ELEMENT_TYPE_U8: return ReadParam<ELEMENT_TYPE_U8>(pInfo, start);
    case ELEMENT_TYPE_R4: return ReadParam<ELEMENT_TYPE_R4>(pInfo, start);
    case ELEMENT_TYPE_R8: return ReadParam<ELEMENT_TYPE_R8>(pInfo, start);
    case ELEMENT_TYPE_I: return ReadParam<ELEMENT_TYPE_I>(pInfo, start);
    case ELEMENT_TYPE_U: return ReadParam<ELEMENT_TYPE_U>(pInfo, start);
    case ELEMENT_TYPE_OBJECT: return ReadParam<ELEMENT_TYPE_OBJECT>(pInfo, start);
    case ELEMENT_TYPE_CLASS: return ReadParam<ELEMENT_TYPE_CLASS>(pInfo, start);
    case ELEMENT_TYPE_SZARRAY:
    case ELEMENT_TYPE_ARRAY:
      return ReadArrayParam(pInfo, start, depth);
    default:
      return PR_UNKNOWN_VALUE;
  }
}

static bool TryReadManagedStringUtf8(ICorProfilerInfo15 *pInfo, UINT_PTR managedObjectRef, std::string &out)
{
  out.clear();
  if (pInfo == nullptr)
    return false;

  ULONG lenOffset = 0;
  ULONG bufOffset = 0;
  if (FAILED(pInfo->GetStringLayout2(&lenOffset, &bufOffset)))
    return false;

  auto base = static_cast<const std::byte *>(reinterpret_cast<const void *>(managedObjectRef));
  auto pLen = reinterpret_cast<const ULONG *>(base + lenOffset);
  ULONG len = *pLen;
  if (len == 0)
  {
    out = "";
    return true;
  }

  auto pChars = reinterpret_cast<const WCHAR *>(base + bufOffset);
  std::basic_string<WCHAR> tmp(pChars, pChars + len);
  out = WStrToUtf8(tmp);
  return true;
}

template <>
std::string ReadParam<ELEMENT_TYPE_STRING>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  UINT_PTR objRef = *reinterpret_cast<const UINT_PTR *>(reinterpret_cast<const void *>(start));
  std::string value;
  if (objRef == 0)
  {
    value = PR_NULL_VALUE;
  }
  else if (!TryReadManagedStringUtf8(pInfo, objRef, value))
  {
    value = PR_UNKNOWN_VALUE;
  }
  else
  {
    value = "\"" + value + "\"";
  }

  return value;
}

template <>
std::string ReadParam<ELEMENT_TYPE_BOOLEAN>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  bool *pBool = (bool *)start;
  if (*pBool)
    return "true";
  else
    return "false";
  return PR_UNKNOWN_VALUE;
}

template <>
std::string ReadParam<ELEMENT_TYPE_CHAR>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  WCHAR *pChar = (WCHAR *)start;
  return WStrToUtf8(std::basic_string<WCHAR>(pChar, pChar + 1));
}

template <>
std::string ReadParam<ELEMENT_TYPE_I1>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  char *pI1 = (char *)start;
  return std::to_string(*pI1);
}

template <>
std::string ReadParam<ELEMENT_TYPE_U1>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  unsigned char *pU1 = (unsigned char *)start;
  return std::to_string(*pU1);
}

template <>
std::string ReadParam<ELEMENT_TYPE_I2>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  short *pI2 = (short *)start;
  return std::to_string(*pI2);
}

template <>
std::string ReadParam<ELEMENT_TYPE_U2>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  unsigned short *pU2 = (unsigned short *)start;
  return std::to_string(*pU2);
}

template <>
std::string ReadParam<ELEMENT_TYPE_I4>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  int *pI4 = (int *)start;
  return std::to_string(*pI4);
}

template <>
std::string ReadParam<ELEMENT_TYPE_U4>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  unsigned int *pU4 = (unsigned int *)start;
  return std::to_string(*pU4);
}

template <>
std::string ReadParam<ELEMENT_TYPE_I8>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  LONG64 *pI8 = (LONG64 *)start;
  return std::to_string(*pI8);
}

template <>
std::string ReadParam<ELEMENT_TYPE_U8>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  ULONG64 *pU8 = (ULONG64 *)start;
  return std::to_string(*pU8);
}

template <>
std::string ReadParam<ELEMENT_TYPE_R4>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  float *pR4 = (float *)start;
  return std::to_string(*pR4);
}

template <>
std::string ReadParam<ELEMENT_TYPE_R8>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  double *pR8 = (double *)start;
  return std::to_string(*pR8);
}

template <>
std::string ReadParam<ELEMENT_TYPE_I>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  (void)pInfo;
  intptr_t v = *(const intptr_t*)reinterpret_cast<const void*>(start);
  return std::to_string((long long)v);
}

template <>
std::string ReadParam<ELEMENT_TYPE_U>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  (void)pInfo;
  uintptr_t v = *(const uintptr_t*)reinterpret_cast<const void*>(start);
  return std::to_string((unsigned long long)v);
}

template <>
std::string ReadParam<ELEMENT_TYPE_PTR>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  ULONG64 pPtr = *(ULONG64 *)start;
  return std::to_string(pPtr);
}

template <>
std::string ReadParam<ELEMENT_TYPE_OBJECT>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  if (start == NULL)
    return PR_NULL_VALUE;
  UINT_PTR objRef = *reinterpret_cast<const UINT_PTR *>(reinterpret_cast<const void *>(start));
  if (objRef == 0)
    return PR_NULL_VALUE;

  auto tn = TryGetObjectTypeName(pInfo, (ObjectID)objRef);
  if (!tn.empty())
    return tn + " " + HexPtr(objRef);
  return HexPtr(objRef);
}

template <>
std::string ReadParam<ELEMENT_TYPE_CLASS>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  return ReadParam<ELEMENT_TYPE_OBJECT>(pInfo, start);
}

template <>
std::string ReadParam<ELEMENT_TYPE_SZARRAY>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  return ReadArrayParam(pInfo, start, 0);
}

template <>
std::string ReadParam<ELEMENT_TYPE_ARRAY>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  return ReadArrayParam(pInfo, start, 0);
}

template <>
std::string ReadParam<ELEMENT_TYPE_VOID>(ICorProfilerInfo15 *pInfo, UINT_PTR start)
{
  return "void";
}
