# AstraBot notices

Copyright (c) 2026 AstraBot contributors.

Original AstraBot source files are intended to be licensed under the Mozilla
Public License 2.0.  See `LICENSE`.  No Exhibit B “Incompatible With Secondary
Licenses” notice is applied by this repository.

Phase 0 contained documentation and license metadata only.  P1-01 adds original
portable Core/host contract sources, and P1-02 adds original adapter source.
This repository still does not contain YaPB, SyPB, CS-EBOT, RealBot,
ReGameDLL_CS, Valve HL1 SDK, ReHLDS, Metamod-P, ReAPI, AMX Mod X, SQLite, or
other upstream source code.  P1-02's adapter build references a separately
checked-out, pinned Metamod-P/HLSDK SDK; those headers are not vendored here.
Research documents may name short symbols and facts and link to exact upstream
commits as evidence.  Those references do not grant additional rights to
upstream material.

The engineering policy is:

- GPL, custom-SDK, mixed-provenance, or ambiguous implementations are
  reference-only and must be independently implemented when functionality is
  needed;
- upstream code is not copied or translated into MPL-covered files without a
  separate, file-specific license and attribution decision;
- the official Valve HL1 SDK, Metamod-P SDK, ReAPI, and AMX Mod X headers or
  libraries retain their own licenses and notices;
- an eventual Metamod-P plugin binary must not be described as “MPL-only.”
  Metamod-P supplies GPL-2.0/GPL-2.0-or-later material and its documentation says
  plugins generally need GPL treatment.  Distribution must resolve MPL 2.0
  Larger Work/Secondary License requirements, GPL Corresponding Source and
  notice requirements, and Valve SDK terms before release;
- the optional AMX Mod X/ReAPI bridge should be a separate removable binary and
  must comply with those projects' GPL-family terms and exceptions;
- when SQLite is introduced, use a pinned official amalgamation under SQLite's
  public-domain dedication rather than copying AMX Mod X wrapper code.

The complete Phase 0 evidence and current recommended handling are in
`docs/research/source-manifest.md` and `docs/research/license-matrix.md`.  These
statements are conservative engineering guidance, not legal advice or a legal
determination of derivative-work status.
