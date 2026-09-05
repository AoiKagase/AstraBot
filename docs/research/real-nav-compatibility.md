# Real NAV compatibility prerequisite

Date: 2026-09-05. Audited source: `b49f4da6e20beec43aa1c47678725eb1865aacf6`.
Result: **Not yet validated**. This is a read-only integration protocol, not a
claim that synthetic fixtures establish real-map compatibility.

## Available evidence and limits

The repository has no tracked real NAV. The local NAV files found under this
checkout/build worktrees are generated evidence, benchmark and fuzz files.
`tests/nav/fixtures/README.md` documents their original synthetic provenance.
No lawful real NAV plus independently recorded expected structure was supplied.
No installed game directories were searched and no asset was copied or added.

`src/nav/io/nav_reader.cpp`, `area_reader.cpp`, `mesh_loader.cpp` and
`model/mesh_validator.cpp` implement bounded v1-v5 decoding, reference validation,
transactional immutable publication and exact end-of-file checking. The wire
observations are pinned in [nav-extraction.md](nav-extraction.md) to ReGameDLL
`b0889847fe6d03898be88acc9e366660efb40ab5`. That source is a format reference;
this audit did not run or freshly compare that upstream checkout.

Synthetic encoders and the parser can share an incorrect format interpretation.
The independent geometry/Dijkstra oracles address queries, not this risk.
Real compatibility therefore remains a **manual integration prerequisite**.

## P3-01 first commit: local offline inspection tool

Plan an original SDK-free `tools/nav-inspect.cpp`, linked only to `astrabot_nav`,
with a conditional CMake executable and `tests/nav/inspection_tests.cpp`.
These are proposed files; the tool does not exist in this planning commit.
It accepts an explicit local NAV path, optional BSP path and limits profile;
opens inputs read-only; never copies, repairs or reserializes the input.
An operator may redirect its report to a chosen local output file.

Use `NavMeshLoader::load` on owned bounded bytes, then `NavSpatialIndex::build`
and `NavGraph::build`. Optional explicit start/goal positions use
`containing`/`nearestGeometry`, followed by `NavRouteSearch::search`. Label
nearest results geometry-only, never grounded or physically reachable.
Use explicit nonzero limits: input 64 MiB, 100000 areas, 65535 Places,
65535 bytes per Place and 8 MiB total Place bytes; per-direction connections
4096, hiding/approach/encounter spots 255, encounters per area 65536;
each aggregate nested category 1000000; snapshot/index/graph/query logical
memory each capped at 256 MiB and route expansions at 100000. Check arithmetic
and input file size before allocation. Record every limit in the report;
limit failure is not format incompatibility and requires a reviewed profile
change, never an automatic unbounded retry. Verify target API limit fields
against the current headers before implementing the profile.

## Lawful local fixture procedure

1. Record the operator's right to use the map/NAV locally, origin, creation
   method/date, generator version/commit and 32-bit writer architecture. Local
   use permission does not imply redistribution permission. Keep bytes out of Git.
2. Record file length and SHA-256 before inspection (`Get-FileHash` on Windows).
   When available, record map identity and BSP length/hash separately. A NAV
   BSP-size field alone is not a cryptographic binding to the map.
3. Obtain independent expected fields from the lawful generator's existing
   report or an independently authored read-only byte inspector plus a recorded
   manual offset sample. Do not use AstraBot's encoder to manufacture expected
   values. Do not start HLDS/ReHLDS to obtain evidence before Finish.
4. Run the proposed inspector and compare every applicable row below. Store
   generator evidence and actual report paths/hashes, AstraBot HEAD, limits,
   compiler, command and exit code in an operator-controlled local record.
5. Record explicit start/goal IDs/positions and route direction. At least one
   known connected pair and its reverse/unreachable expectation are required;
   inspect selected edges, not just a nonempty area list. Do not require reverse
   reachability when the independently expected graph is directed.
6. Re-hash inputs and confirm unchanged length/hash. Commit only the permission
   summary and non-sensitive results, never local asset bytes or private paths.
   A mismatch records byte offset/record/field, expected/actual and reproduction;
   it blocks compatibility approval. Do not relax the parser in this audit.

## Required comparisons

| Item | Expected observation / comparison |
|---|---|
| Magic/version | `0xFEEDFACE`, explicit little endian, v1-v5 only; pin observed version, never extrapolate one v5 pass to every version |
| BSP size | Present only v4/v5; compare actual local BSP length when available; missing BSP means binding unverified |
| Place dictionary | v5 count, lengths including NUL, opaque bytes and per-area uint16 references; no text recoding |
| Areas | Exact header/decoded count, sampled IDs/extents/ne_z/sw_z, finite ordered geometry and unique IDs |
| Connections | Exact directed totals and per-side samples using uint32 wire counts; `size_t` in upstream writer assumes historical 32-bit build |
| Hiding spots | v1 position-only; v2+ IDs/positions/flags; compare counts and sampled values |
| Approach | here/previous/next IDs and raw traversal bytes; these are not motion commands |
| Encounters | v1/v2 legacy bytes are consumed/discarded; v3+ IDs/directions/spots retained; independent inspector supplies legacy wire counts |
| End of file | Exact consumed length, no trailing records silently accepted |
| Traversal metadata | Attributes and approach bytes preserved; static cardinal edges default to Walk, which does not prove safe motion |
| Ladders | No serialized v1-v5 ladder records; file validation must not invent them; P3-06 owns host enrichment |
| Queries | Geometry nearest/containing then route status, ordered selected edges, costs and limit metrics; no locomotion claim |

Current reader preserves attribute/how bytes as opaque values. Earlier research
wording about rejecting all unknown attribute bits is not the implemented
contract. Motion consumers must reject unsupported semantics rather than
converting them silently to Walk. Empty meshes, strict dangling-reference rules
and trailing-data rejection are additional real-file compatibility probes.

## Result vocabulary

- **Validated:** named lawful fixture/version, all applicable comparisons and
  queries pass with independent expected evidence and unchanged inputs. Scope
  the result to that fixture and generator, not universal NAV interoperability.
- **Partially validated:** real bytes load but independent expectations, BSP
  binding or requested query comparisons are incomplete; enumerate missing rows.
- **Not yet validated:** no lawful real fixture inspected (current state).

Missing real bytes do not prevent portable contract/unit development. They block
the P3-01 compatibility sub-gate and any claim of real-map readiness. Runtime
ladder/ground/door/movement acceptance remains a separate post-Finish gate.
