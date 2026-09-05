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
# P2-05 geometry/query fixtures

geometry_tests.cpp uses original literal rectangles, a z=x slope and a bilinear
saddle. spatial_index_tests.cpp serializes original v1 area-only records through
NavMeshLoader. Stacked floors, 64 varied patches and a fixed LCG seed 12345 provide
repeatable query coverage. The oracle evaluates four bilinear weights directly
and linearly scans fixture descriptions; it does not call production projection,
index traversal or production fixture generators. No upstream code/assets are used.

# P2-06 route fixtures

route_fixture.hpp independently serializes tiny v1 NAV records: magic/version/area
count, ID, zero attributes, eight extent floats, four cardinal connection lists,
zero hiding and approach bytes, and a zero uint32 encounter count. Integer fields
use local little-endian shifts and floats use memcpy of their bits. The checked
loader validates these original area IDs/extents/targets with explicit small
limits. No upstream source or assets are used. graph_tests.cpp supplies unsorted
IDs and target lists, distinct cardinal edges, a singleton, and precision/extreme
coordinate cases. Empty means null graph input or no outgoing edges, not a valid
zero-area NAV file; the loader continues to reject zero areas.
