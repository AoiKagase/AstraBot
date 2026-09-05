// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This header is adapter-private.  Metamod-P and HLSDK types deliberately do
// not cross into src/core or src/host public headers.
#include <extdll.h>
#include <meta_api.h>

// GoldSrc engine bootstrap ABI; exported undecorated by the Windows .def file.
#if defined(_WIN32)
extern "C" void WINAPI GiveFnptrsToDll(enginefuncs_t*, globalvars_t*);
#else
extern "C" __attribute__((visibility("default"))) void GiveFnptrsToDll(enginefuncs_t*, globalvars_t*);
#endif
