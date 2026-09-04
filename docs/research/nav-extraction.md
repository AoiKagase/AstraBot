# Navigation extraction decision

## Executive decision

Select **B: format-compatible independent implementation**.  AstraBot will read
the observed Counter-Strike `.nav` versions 1–5 with original parser code and
tests.  It will not extract or translate Valve/ReGameDLL navigation code.

The most important correction to design v0.2 is that a version-5 `.nav` file
does **not** serialize ladders.  ReGameDLL calls `BuildLadders()` after the area
file has loaded（[`nav_file.cpp`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L775-L908)）、and that function enumerates live `func_ladder` entities and performs traces
（[`nav_area.cpp`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_area.cpp#L1520-L1680)）。Therefore static file data, host/BSP enrichment, and learned Experience are three
different layers.

## Provenance and license boundary

Valve's current HL1 SDK snapshot contains `game_shared/bot/nav_*`, including
[`nav_file.cpp`](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/game_shared/bot/nav_file.cpp#L1-L31) and
[`nav_area.h`](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/game_shared/bot/nav_area.h#L1-L20), under the custom HL1 SDK license.  ReGameDLL's current
implementation is derived from that structure, but its rewritten headers such as
[`nav_area.h`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_area.h#L1-L27) explicitly say GPL-2.0-or-later plus the HL Engine/MOD exception, while
`nav_file.cpp` has no individual notice and the repository root transitioned to
MIT.  This is not a sufficiently clear basis for copying into MPL files.

The format facts below are interoperability observations.  Names, control flow,
containers, search state, error handling, and tests in AstraBot must be authored
independently.  No upstream nav source is vendored.

## Observed `.nav` format

The source defines magic `0xFEEDFACE` and maximum supported version 5
（[`nav.h`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav.h#L32-L44)）。The version history is explicit:

| Version | Change |
|---|---|
| 1 | Hiding spots are position vectors. |
| 2 | Hiding spots become objects with ID/flags. |
| 3 | Encounter paths refer to hiding-spot IDs instead of repeating vectors. |
| 4 | Header adds source BSP file size. |
| 5 | Header/areas add a Place dictionary and per-area Place entry. |

### Record order

All numeric observations are native 32-bit GoldSrc-era binary values; the
independent reader will decode explicit little-endian fixed widths, never C++
`sizeof`-dependent structs.

| Scope | Serialized fields in order |
|---|---|
| File header | `uint32 magic`, `uint32 version`; when version ≥4, `uint32 bsp_size`; when version ≥5, Place directory (`uint16 count`, then each `uint16 byte_length` and NUL-terminated bytes); `uint32 area_count`. Evidence: [`SaveNavigationMap`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L567-L625) and [`PlaceDirectory::Save`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L79-L96). |
| Area base | `uint32 id`, `uint8 attributes`, six `float32` extent coordinates, `float32 ne_z`, `float32 sw_z`. |
| Connections | For N/E/S/W: a connection count followed by `uint32 area_id` values. Current save uses `size_t` for the count but load reads `uint32`（[`nav_file.cpp`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L145-L175), [load](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L296-L336)）。Compatibility therefore assumes the historical 32-bit writer. |
| Hiding spots | `uint8 count`; v1 stores three floats each; v2+ stores `uint32 id`, three floats and `uint8 flags`. |
| Approach | `uint8 count`; each record stores here/previous area IDs and traversal byte, then next area ID and traversal byte. |
| Encounter | `uint32 path_count`; v1–2 contains endpoints/vectors that current loader discards; v3+ stores from/to area IDs and direction bytes, then `uint8 spot_count` with `(uint32 hiding_spot_id, uint8 t)` entries. Evidence: [`CNavArea::Load`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L296-L465). |
| Place | v5 stores one `uint16` dictionary entry per area. |
| Not present | ladder entity bounds, ladder/area links, dynamic obstacles, doors, learned danger/traffic, runtime visibility results. |

Attributes include crouch, jump, precise movement, and no-jump; the latter two
are movement hints, not proof that a direct center-to-center segment is clear.
Connections are directed by cardinal side.  Hiding, approach, and encounter data
are static analysis results; they are not learned match experience.

### Defensive reader contract

The offline parser must:

- reject wrong magic, version 0/version >5, truncated reads, impossible counts,
  duplicate/zero area IDs where invalid, non-finite geometry, inverted extents,
  invalid direction/attribute bits, dangling connection/approach/encounter/spot
  references, oversized strings, and trailing-data policy violations;
- cap every allocation before multiplying/counting and report byte offset +
  record context without leaking partial state;
- preserve unknown-but-valid Place text as data and validate NUL termination;
- expose BSP size as a fingerprint component, not call an engine filesystem;
- parse transactionally into an immutable `NavMeshSnapshot` only after all
  references validate;
- keep a fixture for each version and mutation/fuzz corpus for corrupt files.

## Unit classification

The classification is about responsibility, not permission to copy code.

| Unit / concept | Class | Reason / AstraBot placement |
|---|---|---|
| v1–v5 byte decoding; area geometry; IDs; connection, hiding, approach, encounter, Place records | **Pure/portable** | Deterministic bytes-to-values transform in `nav` library; no engine call. |
| Geometry-only `containingArea` and nearest-point/nearest-area | **Pure/portable** | Use snapshot geometry and deterministic tie-breaking. No ground trace. |
| Connectivity and A* | **Pure/portable** | Per-query search records keyed by `NavAreaId`; no mutable state in area objects. Route result includes edge/cost breakdown. |
| BSP fingerprint check | **Adapter-bound** | Core accepts caller-supplied map fingerprint/file size; host resolves actual BSP. |
| Grounded nearest-area, LOS, hull clearance | **Adapter-bound** | Current `GetNearestNavArea` calls `GetGroundHeight` and `UTIL_TraceLine`（[`nav_area.cpp`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_area.cpp#L4823-L4870)）。Expose a trace/query port, not engine types. |
| Ladder discovery/linking | **Adapter-bound** | Host or future BSP reader produces immutable `TraversalEnrichment`; synthetic enrichment is offline-testable. |
| Door/breakable/dynamic obstacle state and local steering | **Adapter-bound** | Per-frame observation overlays static corridor; never serialize into base mesh. |
| ReGameDLL `BuildLadders`, nav generation/analysis, phrase directory resolution | **GameDLL-bound** | Uses `CBaseEntity`, `gpGlobals`, `UTIL_*`, Bot phrases and global nav lists. Reference only. |
| ReGameDLL `NavAreaBuildPath` implementation | **GameDLL-bound source; portable concept** | The template uses global/mutable open/closed/parent state on `CNavArea`（[`nav_area.h`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_area.h#L622-L735)）。Reimplement A* with query-local state. |

## Three implementation options

| Axis | A. Extract ReGameDLL code | B. Independent compatible reader | C. Hybrid adaptation |
|---|---|---|---|
| Initial code volume | Low-looking, but requires a large dependency excision | Medium and bounded to required format/query surface | Medium-high; adapter seams plus retained code |
| Correctness risk | Hidden globals/32-bit assumptions and partial extraction failures | Format mistakes are explicit and fixture-testable | Highest seam risk: behavior diverges across copied and new halves |
| `.nav` compatibility | High only while built in original environment | High after golden v1–v5 fixtures and differential observation | High initially, uncertain after upstream changes |
| Maintenance | Coupled to ReGameDLL internals | Contract-owned, changes are versioned reader additions | Must track both upstream implementation and local seams |
| Offline testing | Difficult until engine/global dependencies are removed | Native design goal | Partial; adapter mocks tend to mirror upstream globals |
| License impact | High/unclear due retained GPL headers and Valve provenance | Lowest; interoperability facts and independent code | Still inherits copied-unit obligations/ambiguity |
| Future ReGameDLL changes | Merge/cherry-pick pressure | Add support only when a format version changes | Repeated reclassification and merge work |
| Portability | Weak | Strong: Windows/Linux and no server process | Moderate |
| Fatal constraint | License/provenance and pervasive GameDLL state | Requires careful fixture provenance and parser hardening | Does not actually remove the license/seam problem |

**Recommendation: B.**  It costs more than copying a loader, but it is the only
option that simultaneously supports MPL-authored Core files, deterministic
offline tests, safe malformed-file handling, and a clean GameDLL adapter.

## Layered model

```text
SerializedNavSnapshot (immutable, from .nav)
        +
TraversalEnrichment (immutable per map generation; ladders/host facts)
        +
DynamicTraversalOverlay (frame/short-lived door, blocker, obstacle facts)
        +
ExperienceSnapshot (versioned learned values, keyed by map fingerprint + area ID)
        =
RouteQuery input -> RouteResult{corridor, total cost, component costs, reason}
```

Experience never mutates `NavArea`.  A map fingerprint includes at least map
name, observed BSP size and a stronger content hash when available.  A mismatch
quarantines experience rather than applying it to coincidentally reused area IDs.

## Revised Phase 2/3 gate

Phase 2 loads v1–v5 fixtures, validates area/connection/hiding/approach/encounter/
Place, performs pure nearest-area and A*, and proves corrupt-file rejection.
Because ladders are absent from `.nav`, its ladder test feeds a synthetic
`TraversalEnrichment` fixture and proves it participates in connectivity/A*.

Phase 3 is the first live gate for discovering `func_ladder`, producing the
enrichment, and traversing it.  This prevents an impossible acceptance criterion
from being attributed to the file reader.
