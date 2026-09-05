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
