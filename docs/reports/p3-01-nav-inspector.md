# P3-01 first slice: read-only NAV inspector

Date: 2026-09-05. Base: `0e8ab96`. Branch: `codex/p301-nav-inspector`.
Status: inspector implementation and applicable Windows offline tests complete.
P3-01 as a whole remains open. Real NAV compatibility is **not validated**:
the supplied dust/dust2 files are rejected by the existing validator.
This is not project-wide Finish or live acceptance.

## Tool and reproduction

`ASTRABOT_BUILD_NAV_INSPECTOR=ON` builds `astrabot_nav_inspect`; default OFF.
It uses C++17, the standard library and `astrabot_nav` only. No SDK, adapter,
server process, generator or parser relaxation was added.

In the x64 VS2026 environment documented in AGENTS.md:

```text
cmake -S . -B build-portable-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_BUILD_NAV_INSPECTOR=ON
cmake --build build-portable-test
ctest --test-dir build-portable-test --output-on-failure
build-portable-test\astrabot_nav_inspect.exe --nav <local.nav> --bsp <local.bsp> --profile compatibility-v1
build-portable-test\astrabot_nav_inspect.exe --nav <local.nav> --start 1 1 0 --goal 4 1 4 --radius 10 --vertical 10
```

NAV path is mandatory; BSP is optional. `compatibility-v1` is the only CLI
profile and is the default. All four query options are required together.
Numbers must be finite; radius and vertical tolerance may be zero but not
negative. Duplicates, unknown options/profiles and malformed values fail.
Paths containing spaces must be quoted. Reports go to stdout and may be
redirected by the operator; the tool never opens inputs for writing.

Exit codes: 0 = inspection completed (including an Unreachable route),
1 = input/load/build/query failure, BSP size mismatch or ExpansionLimit,
2 = invalid command line. Help returns 0. Limits and logical memory budgets
are printed individually. Limit errors never trigger an expanded retry.
The fixed profile is the compatibility protocol's exact profile, with 199999
index nodes and 1000000 graph edges. A* uses `allowPartial=false`.

Report `astrabot.nav-inspection.v1` contains metadata, opaque Place bytes in
hex, area extents/attributes/Place references, per-direction connections,
retained record totals, query matches/projections, owned selected route edges,
component costs and search metrics. Numbers use the classic locale and
round-trip precision. v1/v2 discarded encounter counts are explicitly unknown.
Geometry queries do not establish ground support or physical reachability.
Reported compatibility stays NotYetValidated: this executable cannot provide
independent expected evidence or authorize compatibility approval.
BSP length equality is not a cryptographic map binding.

## Verification

- Clean baseline: portable Debug 16/16 and existing fixture manifest checks passed.
- Final Windows x64 NMake Debug, `/W4 /WX`: 18/18 CTest passed (7.72 s).
  The standard verification script also passed all ten fixture hashes and
  empty/hash/duplicate/missing manifest regression cases.
- New inspection tests exercise v1-v5 metadata and unchanged bytes; literal
  connection/count/cost expectations; Complete, same-area, directed Unreachable,
  geometric fallback/no match and diagnostic-only ExpansionLimit; missing,
  empty, corrupt, truncated and trailing input; exact/rejected input size,
  fixed-profile 64 MiB + 1 rejection and snapshot/index/graph/route byte limits;
  BSP mismatch; exact HidingSpotId failure offset and named diagnostic fields.
- CLI CTest launches the executable, checks exit codes and output, tests strict
  arguments/nonfinite/out-of-range values, checks input SHA-256, and exercises
  directed routes. Its fixtures are separate from the unit test directory.
- Initial RED: missing tool header. Diagnostic RED: numerical record/field
  output failed the named HidingSpot/HidingSpotId assertion; missing CLI failure
  stage failed the executable-level assertion. All now pass.
- No Linux build, hosted CI, adapter change or live server test was run.

## Supplied real files: compatibility blocker

The user supplied local SteamCMD map files and authorized dust/dust2 inspection.
This records local use only; no redistribution permission is inferred. Asset
bytes and private absolute paths are not committed. Generator version/date,
32-bit writer provenance and a generator-produced expected report remain unknown.

Both runs used the fixed profile and returned exit 1 at `stage=load`,
`kind=InvalidValue`, `record=HidingSpot`, `field=HidingSpotId`.

| File | NAV bytes | Header areas | BSP bytes / header BSP size | Error byte offset | Actual u32 ID |
|---|---:|---:|---:|---:|---:|
| de_dust.nav | 432321 | 805 | 1359684 / 1359684 | 156 | 0 |
| de_dust2.nav | 413585 | 737 | 2057288 / 2057288 | 144 | 0 |

An independently authored, read-only .NET BinaryReader prefix check (no
AstraBot encoder/loader calls) found magic FEEDFACE, version 5, zero Places,
the first area beginning at byte 18 with ID 1/attributes 0, and one hiding
spot. Reading the first area geometry and all four connection lists put its
first hiding ID exactly at the diagnostic offsets, with bytes `00 00 00 00`.
The independent connection counts were dust 10/6/2/3 and dust2 9/3/4/2.
Header/BSP length comparison above comes from that prefix check, since the
CLI deliberately does not publish a validated snapshot after load failure.

`src/nav/model/mesh_validator.cpp` explicitly rejects HidingSpotId zero.
The inspector faithfully exposes this existing policy conflict; it does not
establish whether all later records would load. Follow-up must resolve the
zero-ID convention against independently pinned generator/format evidence,
add a synthetic regression and review reference/duplicate-ID semantics before
changing the loader. No parser change belongs to this inspector slice.
Nearest/route real-file checks are blocked at load, and are not marked passed.

Input SHA-256 before and after inspection was identical for all four files:

| File | SHA-256 |
|---|---|
| de_dust.nav | `3a7fc7984876d9aa21f8e0d4d16fbfc51a2c40c13eed1d981ddd6bce24a3d688` |
| de_dust.bsp | `ac94fbda94e4a170b971f05936fe8706cb9aa895e55f7797916dc160171cab11` |
| de_dust2.nav | `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2` |
| de_dust2.bsp | `15945389528d113562ede0a2c80647ebfa799079ed1c05a25379bcf84e4e9286` |

Local reports and the prefix-check script are retained under the worktree's
ignored `build-portable-test` directory. They are not redistributed assets.

## New NAV generation direction

The user's direction is to optimize newly generated navigation data for
AstraBot instead of constraining new generation to zBot's representation.
Keep the existing v1-v5 reader as a compatibility input. A future native
generator/format should be designed around the movement contract, including
portal clearance, support/step constraints and validated traversal endpoints.
The format, extension, versioning and generator implementation are not decided
or implemented by this slice; it does not change the current phase scope.
