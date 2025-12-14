// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "Guid.h"
#include "ClassFactory.h"
#include "StackManager.h"
#include "Logger.h"
#include "Exports.h"

// {a2648b53-a560-486c-9e56-c3922a330182}
const IID CLSID_CorProfiler = {0xa2648b53, 0xa560, 0x486c, {0x9e, 0x56, 0xc3, 0x92, 0x2a, 0x33, 0x01, 0x82}};

BOOL STDMETHODCALLTYPE DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    if (ppv == nullptr || rclsid != CLSID_CorProfiler)
    {
        return E_FAIL;
    }

    auto factory = new ClassFactory;
    if (factory == nullptr)
    {
        return E_FAIL;
    }

    LOG("Tracer Started");


    return factory->QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return S_OK;
}