# Phase 1 toolchain and combined-binary policy

Status: P1-01 decision, 2026-09-04.  This is an engineering and distribution
policy, not legal advice.

## Combined-binary decision

The following policy is fixed for Phase 1:

1. AstraBot-authored source files, including the portable Core and host
   contracts, use MPL-2.0 with an SPDX header.  P1-01 contains no upstream
   source and no SDK header, so its library and tests remain within this
   file-level policy.
2. A future `astrabot_mm` plugin compiled with Metamod-P/HLSDK is not an
   “MPL-only” binary.  Metamod-P is GPL-2.0-or-later material and its pinned
   FAQ says that plugins generally need GPL treatment.  The conservative
   operational policy is to distribute that combined runtime under a
   GPL-2.0-or-later-compatible model, while preserving the MPL notices for
   AstraBot files and all applicable SDK/upstream notices.
3. No `astrabot_mm` binary is releasable until the exact compiled headers,
   libraries, SHAs, file headers, corresponding source offer, and notices are
   inventoried.  The release package must contain the resolved GPL/MPL/SDK
   texts and must not describe the result as MPL-only.
4. ReAPI and AMX Mod X remain optional and separately removable.  Their use
   requires a new dependency-specific review; P1-01 does not link either.

This resolves the Phase 1 engineering gate conservatively without claiming a
final legal determination about every possible linkage or distribution form.

## Portable build policy

- CMake 3.20 or newer is required.
- C++17 is required, CMake extensions are disabled, and the standard library
  is the only portable-target dependency.
- The supported baseline is GCC 11 or newer on Linux, Clang 11 or newer on
  Linux/Windows, and MSVC 19.30 or newer on Windows. As of 2026-09-05 active CI
  uses Windows Server 2022 only; Linux execution remains deferred by AGENTS.md
  until project-wide Finish. The Phase 3 plan proposes an explicit policy
  revision for earlier portable/Nav CI; that proposal is not active permission.
- Portable public headers use fixed-width value types and do not include
  GoldSrc, Metamod-P, ReAPI, ReGameDLL, or AMX Mod X headers.  No engine
  pointer or global state crosses the host boundary.
- Portable targets build with high warnings (`-Wall -Wextra -Wpedantic
  -Wconversion -Wsign-conversion`, or `/W4 /permissive-`) and warnings are
  errors by default.  A local build may turn this off explicitly when
  investigating a compiler-specific issue.
- No package manager, generated SDK, compiler extension, unity build, or
  third-party test framework is required for P1-01.  Metamod/HLSDK builds
  will be separate adapter targets with their own pinned SDK policy.

## Metamod-P adapter policy

- The optional adapter is enabled only with
  `ASTRABOT_BUILD_METAMOD=ON` and an external
  `ASTRABOT_METAMOD_SDK_ROOT` checkout.  The checkout must resolve to
  Metamod-P `7ec9b014f8c0a947a724644aebe34eb33706e44b`; configuration fails for
  a missing, non-git, or different checkout.
- Metamod-P and HLSDK headers are system/private includes of `astrabot_mm`.
  They are not copied into this repository and are never included by
  `src/core` or `src/host`.
- The adapter artifact names are `astrabot_mm.dll` on Windows and
  `astrabot_mm.so` on Linux.  Its public export contract is limited to
  `Meta_Query`, `Meta_Attach`, `Meta_Detach`, `GetEntityAPI2`, and
  `GetEngineFunctions`.
- GoldSrc/Metamod-P is an x86 ABI target.  The adapter configure step rejects
  64-bit builds; use a 32-bit Visual Studio/Win32 environment on Windows or
  `-m32` on Linux.  Portable Core builds are not subject to this adapter-only
  restriction.
- P1-02's combined-binary treatment remains the conservative
  GPL-2.0-or-later-compatible policy above; a binary release still requires
  the exact dependency inventory, corresponding source, and notices review.

The current development host was observed with CMake 3.31.4, MSVC 19.50, and
Clang 11.  Those observations establish the local verification environment;
the policy above is the reproducible project baseline.
