# Pre-A* traversal connection contract

Approved scope: minimal traversal representation before P2-06, based on
3194f899ff30edb89e79b5e1e0fea05250d53778 (P2-05). Existing task numbers remain.

## Decision and source semantics

Minimal traversal extension added. Previously NavAreaRecord retained four
cardinal vectors of destination IDs. Each element is now NavConnection with a
target ID and NavTraversalKind, defaulting to Walk. The owning area and N/E/S/W
bucket supply source and direction. Preserve wire order and directedness;
duplicate targets in one direction remain invalid, while the same target in
different directions remains valid. Source reader signatures are unchanged,
but C++ consumers of connection elements must use .target.

NavTraversalKind contains only Walk, Crouch, Jump, Ladder and Drop. Its numbers
are neither NAV wire values nor a fixed ABI/persistence contract. The
isKnownTraversalKind helper rejects every other underlying byte value.

The v1-v5 decoder still reads only cardinal target IDs. Walk is the runtime
default for that adjacency, not a serialized fact or proof of walkability.
Area attributes (including crouch/jump hints), approach how bytes, hiding flags
and retained encounter records preserve their existing raw semantics. Legacy
v1/v2 encounters remain consumed but unpublished. No source bytes are reinterpreted
as the runtime traversal enum. No ladder, jump, or drop is inferred by the reader.

The transactional loader retains its reference/geometry/EOF validation and
immutable publication. Its logical budget charges sizeof(NavConnection) per
connection. File bytes cannot introduce unknown runtime kinds because the
reader always constructs the default; future external enrichment must validate
kinds before graph publication and reject unknown values rather than use Walk.
There is currently no external-link ingestion API or NAV writer.

## A*, local navigation and experience

P2-06 must enumerate directed connection values through its graph query boundary.
Its cost policy receives the selected edge and area context; its query-local
parent record and result retain the edge, source, direction and traversal, not
only an area ID sequence. Source and direction distinguish currently legal
connections sharing endpoints. Future enrichment adds its own link identity
and provenance at composition time, without changing serialized source facts.
Do not add unused identity/generation/geometry fields to every connection now.

The result can later express an area corridor with traversal instructions for
Phase 3. Motion primitives translate those instructions into per-frame actions;
they are not part of NavConnection. Attributes can require local probes and
combined actions; the enum is not an exhaustive movement command.

Distance, traversal penalty, danger and experience belong in a future pure cost
policy with component evidence. This change implements none of those policies.
Static traversal facts and learned runtime knowledge stay separate: experience
must not mutate NavMeshSnapshot. A future experience key must include map
identity and selected-link identity, not just an ambiguous endpoint pair.

P2-07 still owns synthetic ladder enrichment/composition. GapJump, EdgeTraverse,
NarrowPassage, LongJump, Boost, Motion Primitive, Human traversal observation,
learned traversal discovery and Traversal Experience remain deferred.

## Verification

Model tests fix default/explicit kinds and reject all unknown byte values.
Independent v1-v5 loader fixtures cover all four directions, retained target IDs,
Walk defaults despite raw attributes, no inferred reverse edge, exact logical
budget and one-byte-under rejection, and allocation failure until successful
publication. Existing reader tests retain destination ordering and raw tactical
semantics. Existing corruption and P2-05 geometry/spatial tests remain required.

Windows x64 Debug NMake, /W4 /WX, tests enabled and Metamod disabled is the
acceptance build. Linux and live validation remain post-Finish; no project-wide
Finish is declared by this work. The original P2-05 branch/worktree and main are
preserved; implementation commits belong only to codex/pre-astar-traversal.

## Completion evidence

- The model test first failed with C1083 for the absent connection.hpp.
- After implementation, portable x64 Debug /W4 /WX build succeeded and CTest
  passed 10/10, including P2-05 geometry/spatial tests.
- Commands executed inside VS 2026 Community VsDevCmd.bat with
  `-arch=x64 -host_arch=x64`, from the isolated worktree:

```text
cmake -S . -B build-portable-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON
cmake --build build-portable-test
ctest --test-dir build-portable-test --output-on-failure
```

P2-06 can now build on P2-05 using traversal-bearing directed edges. Route search,
enrichment ingestion and live movement remain their respective later tasks.
