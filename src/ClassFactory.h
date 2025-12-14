// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef _WIN32
#include "specstrings_undef.h"
#endif
#include <atomic>
#include "corhlpr.h"


class ClassFactory : public IClassFactory
{
private:
    std::atomic<int> refCount;
public:
    ClassFactory();
    virtual ~ClassFactory();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
    ULONG   STDMETHODCALLTYPE AddRef(void) override;
    ULONG   STDMETHODCALLTYPE Release(void) override;
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) override;
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override;
};