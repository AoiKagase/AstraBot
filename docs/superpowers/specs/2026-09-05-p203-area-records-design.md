# P2-03 Area and Tactical Static Records Design

Status: approved A architecture

Date: 2026-09-05

## Goal

Extend the SDK-free NAV reader with a transactional decoder for serialized
area records. The decoder will preserve the static values needed by later
snapshot validation and queries while keeping the P2-04 cross-reference and
publication responsibilities separate.

## Chosen architecture

Add a dedicated `NavAreaReader` rather than extending `NavFileReader` into a
full-file builder. The header reader remains responsible for the file
envelope, including `headerBytes` and `areaCount`. The area reader accepts the
remaining payload as a `ByteView`, the parsed `NavVersion`, and explicit area
limits. It returns an `AreaBlock` containing decoded records and the exact
number of bytes consumed, so callers can apply a later trailing-data policy.

This keeps the P2-03 parser independently testable, avoids coupling it to the
P2-04 snapshot builder, and preserves the SDK-free portable boundary. The
implementation uses the format observations recorded in
`docs/research/nav-extraction.md`; it does not copy upstream source or ship
upstream assets.

## Public data model

Add value types under `src/nav/model/area_records.hpp`:

- `NavAreaRecord`: area ID, raw attribute byte, `NavExtent`, four directional
  connection ID lists, hiding spots, approach records, encounter records, and
  an optional v5 Place dictionary entry.
- `NavHidingSpot`: position plus optional serialized ID and flags. v1 has no
  ID or flags; v2+ retains both values.
- `NavApproachRecord`: here/previous/next area IDs and the two raw traversal
  bytes.
- `NavEncounterRecord`: from/to area IDs, raw direction bytes, and ordered
  `(hidingSpotId, t)` entries for v3+ records.
- `NavCardinalDirection`: stable North/East/South/West array order for the
  four connection lists.

Raw attributes, traversal bytes, direction bytes, hiding flags, and `t` bytes
remain raw values. P2-04 will decide which bit and enum values are valid.
v1/v2 legacy encounter records are fully consumed but not exposed as modern
ID-based encounter paths.

Add `NavAreaReadLimits` under `src/nav/io/area_reader.hpp` with explicit caps
for area count, connections per direction, hiding spots per area, approaches
per area, encounters per area, encounter spots per path, and the corresponding
total counts. The limits are checked before `reserve` or any count-driven
allocation.

Add:

```cpp
struct NavAreaBlock final {
    std::vector<model::NavAreaRecord> areas;
    std::size_t bytesConsumed{0};
};

class NavAreaReader final {
public:
    static diagnostics::ReadResult<NavAreaBlock> read(
        ByteView bytes,
        model::NavVersion version,
        std::uint32_t areaCount,
        const NavAreaReadLimits& limits) noexcept;
};
```

The returned `ReadResult` is either a complete block or `nullopt` with a
typed diagnostic. No partially decoded area vector is published.

## Wire decoding

For every area, consume these fields in order:

1. `uint32 id`, `uint8 attributes`, six extent floats, `float ne_z`, and
   `float sw_z`.
2. Four directional connection sections in North/East/South/West order;
   each has a `uint32` count followed by that many `uint32` area IDs.
3. `uint8 hiding_count`; v1 entries contain one 3-float position, while v2+
   entries contain `uint32 id`, a 3-float position, and `uint8 flags`.
4. `uint8 approach_count`; each entry contains here/previous/next IDs and
   the two traversal bytes.
5. `uint32 encounter_count`; v1/v2 legacy entries consume two IDs, two
   endpoint vectors, a spot count, and each spot's vector plus float. v3+
   entries contain from ID/direction, to ID/direction, a spot count, and
   `(uint32 hiding_spot_id, uint8 t)` entries.
6. For v5 only, consume the `uint16` Place dictionary entry for the area.

The reader accepts trailing bytes after the requested area count and reports
only `bytesConsumed`; it does not enforce a file-level trailing policy.

## Diagnostics and validation boundary

`ByteReader` supplies little-endian decoding, offset-aware truncation, invalid
input, offset overflow, and non-finite-float diagnostics. The area reader
adds typed count-limit and allocation-failure diagnostics with the offset of
the count or section that caused the rejection. It catches allocation failure
and returns no block.

P2-03 does not reject duplicate or dangling IDs, inverted extents, invalid
direction/attribute bits, or out-of-range v5 Place entries. Those checks belong
to P2-04 transactional validation and snapshot publication.

## Tests

Use independently generated minimal fixtures for v1 through v5. Tests cover:

- area base values, all four connection directions, and exact bytes consumed;
- v1 vector hiding, v2 object hiding, and v3 ID-based encounters;
- v4-compatible area payloads and v5 Place entries;
- legacy v1/v2 encounter byte consumption without publishing modern paths;
- zero and small maximum section counts, every count/field truncation, and
  trailing payload preservation;
- each per-section and total allocation limit, typed allocation diagnostics,
  and no partial publication on failure.

The fixture README records the independently authored field provenance and
contains no Valve, ReGameDLL, or other upstream source or map asset.

## Non-goals

P2-03 does not build a `NavMeshSnapshot`, resolve cross references, validate
geometry or semantic enum values, discover ladders, perform queries, or run
Linux/live-server validation.
