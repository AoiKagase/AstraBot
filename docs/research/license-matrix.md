# License matrix

This is an engineering provenance decision, not legal advice.  It deliberately
chooses the more restrictive treatment when a root license, file header, origin,
or plugin-linking rule is unclear.  Evidence is frozen in
[source-manifest.md](source-manifest.md).

## Decision

AstraBot-authored files use MPL-2.0 without the Exhibit B incompatibility notice.
No upstream implementation is copied in Phase 0 or P1-01.  GPL, custom-SDK,
mixed-origin, and ambiguous units are **reference-only + independent
implementation**.

The combined-binary policy is now fixed for Phase 1: a future `astrabot_mm`
plugin compiled with Metamod-P/HLSDK must not be advertised as “MPL-only.”
Metamod-P's own header is GPL-2.0-or-later and its FAQ says plugins generally
need to be GPL.  The conservative operational treatment is a
GPL-2.0-or-later-compatible distribution model for that combined runtime,
while preserving MPL notices on AstraBot files and all applicable SDK/upstream
notices.  Before a binary release, the exact compiled dependency inventory,
Larger Work/Secondary License route, Corresponding Source, and notices must be
reviewed and recorded.  This remains an engineering gate, not legal advice.
Keeping the optional AMXX/ReAPI bridge in a separate binary narrows this issue;
it does not waive it.

## Matrix

| Source | Repository license | Relevant file header / exception | Linking or derived-work condition | Planned use | MPL-2.0 concern | Recommended treatment |
|---|---|---|---|---|---|---|
| AstraBot original work | MPL-2.0 | Standard MPL-2.0, no Exhibit B | MPL file-level obligations; a Larger Work may be distributed under a compatible Secondary License subject to MPL 3.3 | Production code and documentation | Runtime aggregation/linking obligations must still be met | MPL-2.0 per authored source file; preserve provenance and SPDX headers |
| YaPB | MIT | Current inspected sources use `SPDX-License-Identifier: MIT` | Preserve copyright and permission notice in copied substantial portions | Host, behavior, lifecycle reference | MIT is permissive, but copying would blur independent architecture and provenance | Reference-only by default; a deliberately selected small adaptation would require attribution and review |
| SyPB | GPL-3.0 | Root GPL-3.0; bundled dependencies have their own headers | Copied/derived code would bring GPL obligations | Versioned AMXX control API concepts | Direct code in an MPL file is inappropriate without a GPL-compatible distribution plan | Reference-only + independently designed API |
| CS-EBOT | MPL-2.0 root | Navigation/base files carry a YaPB-style MIT notice | File-level notice governs copied material; root and history must both be retained | Zombie pursuit, crowd and stuck-recovery behavior reference | Mixed root/file provenance makes casual copying error-prone | Reference-only; independently implement concepts; attribution review before any adaptation |
| Fundynamic/RealBot | No root license found | Core files make a generic GPL claim and disclose HPB-Bot/other-bot portions | Exact GPL version and provenance are not reliably specified at repository root | Learning concepts only | Ambiguous version and mixed heritage are incompatible with clean MPL provenance | Reference-only + independent data model/algorithms; never copy raw persistence code |
| ReGameDLL_CS | MIT root after 2025 transition | `nav_area.h`/`nav_path.h` explicitly retain GPL-2.0-or-later plus HL Engine/MOD exception; `nav_file.cpp` has no notice | Transition says prior contributions are MIT “unless otherwise noted”; retained headers are an unresolved contrary signal | Format and behavior observation; ReGameDLL compatibility target | Direct extraction could create GPL/custom-origin or attribution uncertainty | No extraction. Independently implement the observed format and behavior with separately authored tests |
| Valve HL1 SDK | Custom SDK license | Bot/nav files have authorship comments but no separate grant | Free distribution only; SDK or substantial SDK distribution must include Valve license; commercial use requires Valve contact | ABI/format provenance and permitted SDK integration | Not an OSI license and not automatically subsumed by MPL | Use official SDK only under its terms; avoid copying nav implementation; include Valve license if distribution threshold is met |
| ReHLDS | MIT root after transition | Public API header retains GPL-2.0-or-later plus HL Engine/MOD exception | Root/header mismatch must be resolved for direct header-derived work | Supported server target, optional adapter capability | Header-generated derivative status is not assumed | Runtime/reference use; isolate optional bindings and review before distribution |
| Metamod-P | `GPL.txt` contains GPL-2.0 | `meta_api.h` and related headers: GPL-2.0-or-later plus exception only for HL Engine/MODs | Bundled FAQ says a Metamod plugin generally needs to be GPL | Required host/plugin ABI | Central risk: eventual plugin binary is likely a GPL-governed Larger Work, not MPL-only | Use the pinned Bots-United Metamod-P SDK, isolate adapter, ship full notices/source, and complete legal/distribution review before release |
| ReAPI | GPL-3.0 root | CSSDK API headers retain GPL-2.0-or-later plus HL Engine/MOD exception | Direct linkage/header use brings GPL and exception analysis | Optional ReGameDLL observations/events | Making Core depend on it would couple both license and ABI | Optional separate adapter only; not needed for Phase 1 host skeleton; no Core types |
| AMX Mod X | GPL-3.0-or-later | Explicit HL Engine/MOD exception; modules/plugins/header-built software may choose GPL-2.0-or-later if compatible with GPL-3.0 | AMXX module remains GPL-family work; exception does not make it MPL-only | Optional command/query/event bridge | In-process bridge can expand combined-work obligations | Separate optional bridge binary, narrow versioned ABI, publish bridge source under compatible terms |
| SQLite | Public-domain dedication/blessing in upstream amalgamation | AMX Mod X bundles `sqlite3.c`/`sqlite3.h`, demonstrating static deployment, but AMXX wrapper code is GPL | Use the official SQLite amalgamation, not AMXX wrappers | Candidate Phase 9 persistence backend | No inherent MPL conflict in SQLite itself | Vendor a pinned official amalgamation only when Phase 9 begins; record version/hash; retain blessing |

## Usage classes

| Class | Meaning in AstraBot |
|---|---|
| `reference-only` | Read to understand observable behavior, interfaces, risks, and test cases. No source translation or structural copying. |
| `independent implementation` | Write from AstraBot's contract and format tests, with different structure/names and traceable requirements. |
| `adapted with attribution` | Allowed only after a named file/version/license review and an explicit commit-level decision. Not selected in Phase 0. |
| `copied / derived` | Not selected for any upstream production code. |
| `unclear; requires confirmation` | RealBot provenance and ReGameDLL/Metamod/ReHLDS root-vs-header cases; default action is no copy. |

## Release gate

Before the first distributable binary:

- inventory every compiled header and linked library;
- decide and document the license of the combined Metamod binary, not only the
  license of individual AstraBot files;
- include MPL-2.0, applicable GPL texts, Metamod/HLSDK notices, and source offers
  required by the resolved distribution model;
- confirm whether the Valve SDK license must accompany the package;
- keep AMXX and ReAPI optional and independently removable;
- rerun this matrix against the exact dependency SHAs used by the build.

The Phase 1 toolchain and binary policy is recorded in
[toolchain-policy.md](../toolchain-policy.md).
