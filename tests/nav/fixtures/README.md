# P2-02 minimal `.nav` fixtures

These fixtures are generated directly by `tests/nav/nav_reader_tests.cpp`.
They are intentionally small, independently authored byte sequences and do
not contain Valve, ReGameDLL, or other upstream map assets or source code.

The field order follows the interoperability observations in
`docs/research/nav-extraction.md`: magic, version, optional v4 BSP size,
optional v5 Place dictionary, and area count. The tests cover one valid header
for each version from 1 through 5 and malformed mutations of each header
field.
