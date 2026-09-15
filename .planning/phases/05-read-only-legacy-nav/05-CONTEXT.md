# Phase 5 Context: Read-only Legacy Nav

## Goal

Load existing Counter-Strike navigation files in legacy versions 1 through 5
into a bounded, format-neutral, SDK-free document and publish it as an
immutable runtime snapshot. The source `.nav` bytes must never be edited or
rewritten.

## Locked decisions

- ReGameDLL-CS `nav.h`, `nav_file.cpp`, and `nav_area.cpp` are behavior and
  format references only. Their classes, globals, parser, and ownership model
  are not copied or linked.
- The Core reader consumes bytes and value types only. It does not include
  HLSDK, Metamod, GameDLL, `Vector`, `edict_t`, or ReGameDLL types.
- All integer fields are little-endian and all floating-point fields must be
  finite. Counts, names, connections, IDs, and total input bytes have named
  upper bounds before allocation or iteration.
- Parsing is transactional: a complete candidate document is validated before
  replacing a published document. Any failure leaves the previous snapshot
  unchanged.
- Legacy version differences are normalized: v1 hiding spots receive the
  legacy cover flag, v2+ spot records retain IDs/flags, v3 encounters are
  parsed, v4 BSP size is retained for fingerprint validation, and v5 place
  directory entries are normalized to bounded place records.
- The initial runtime is read-only. Nav generation, editing, learning,
  enrichment, AstraNav serialization, and wallbang geometry remain deferred.

## Reference format facts

The pinned ReGameDLL-CS reference uses magic `0xFEEDFACE` and accepts versions
up to 5. The header contains magic and version; v4 and later add the source BSP
size. Version 5 adds a place directory before the area count. Each area stores
an unsigned ID, attribute byte, six extent floats, two implicit corner-height
floats, four directional connection lists, and an unsigned-byte hiding-spot
count. Version 1 stores only three floats per hiding spot; later versions store
spot ID, position, and flags. Version 3 and later store encounter records.

## Failure policy

Return explicit results for missing input, truncated data, bad magic/version,
invalid counts, duplicate/unknown connection IDs, non-finite or degenerate
geometry, invalid place directory entries, BSP fingerprint mismatch, and
resource limits. Do not fabricate an empty mesh or silently accept an
unverified traversal.

## Verification boundary

Core tests use synthetic byte fixtures for all versions and corruption cases.
Adapter tests use temporary read-only files and fake file/map metadata. Build
and CTest evidence is required on Windows x86 and Debian WSL Linux x86. Live
HLDS/ReHLDS map loading and bot movement remain later acceptance layers.
