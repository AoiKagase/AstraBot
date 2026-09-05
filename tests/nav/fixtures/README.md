# P2-02/P2-03 minimal `.nav` fixtures

These fixtures are generated directly by
`tests/nav/nav_reader_tests.cpp` and
`tests/nav/area_reader_tests.cpp`. They are intentionally small,
independently authored byte sequences and do not contain Valve, ReGameDLL, or
other upstream map assets or source code.

The field order follows the interoperability observations in
`docs/research/nav-extraction.md`: magic, version, optional v4 BSP size,
optional v5 Place dictionary, and area count. P2-03 extends the independent
payloads with area ID/attributes/extents, north/east/south/west connection
lists, versioned hiding spots, approaches, legacy v1/v2 encounter byte
consumption, v3+ encounter spots, and the v5 per-area Place entry.

All field values, offsets, truncation cases, and sentinel bytes are authored
in the tests from the documented wire layout. No real Valve/ReGameDLL file,
map bytes, upstream source, or generated upstream asset is included.
# P2-04 loader fixtures

The additional fixture builders in mesh_loader_tests.cpp independently encode
the documented field sequence from docs/research/nav-extraction.md. No upstream
code or map assets are included. Minimal headers are 12 bytes (v1-v3), 16 (v4),
and 22 (v5 with one one-byte opaque Place). Minimal area records are 59 bytes,
plus the v5 two-byte Place. Tactical fixtures use independently chosen area 7,
hiding ID 101 and owner references; connection fixtures use areas 7 and 8.
Mutation offsets are hand-counted, not computed by the production reader.
Legacy v1/v2 encounter endpoints are intentionally unresolved because those
discarded records are outside snapshot semantic validation.
