# P2-08 reproducible corruption corpus

The corpus is source-generated, not an imported collection of map assets.
corruption_tests.cpp applies the fixed mutations to all ten independently
encoded fixtures in evidence/fixture.hpp. The field spans are recorded by the
encoder, not recovered from Nav readers. evidence-manifest.json freezes the
valid source bytes and records the mutation rules.

Every prefix (1788 total) is rejected at the interrupted field's start, including
inside scalar and Place text bytes. Every float is replaced with NaN/+Inf;
counts, IDs, references, directions, Place termination and trailing data have
exact kind/record/field/offset expectations. Boundary cases and combined
decode/semantic/trailing errors supplement this deterministic corpus.

The fuzz extension uses xorshift32 (13,17,5), uint32 wraparound and seed 0xA208.
Case i has seed 0xA208 xor ((i+1)*0x9E3779B9 modulo 2^32), then chooses version
1..5, minimal/full and 1..8 operations. Operation codes are 0 bit flip,
1 boundary overwrite, 2 byte insertion, 3 byte deletion, 4 truncation.
replay.txt records version/profile and each operation:position:value.
Each case can be reconstructed independently with:

```powershell
# Run from the chosen build directory; outputs stay in its fuzz-artifacts.
rtk proxy ./astrabot_nav_fuzz_tests.exe --replay 9999
```

Before calling Nav, open-once streams flush current.case and replay.txt.
current.case has a four-byte little-endian payload length followed by exact
input bytes; ignore an older suffix. replay.txt ends its current record with
END (ignore anything after it). This avoids reopening files 200000 times while
retaining evidence even if ASan terminates the process. A successful run leaves
the final successful test candidate, which need not be valid NAV.

Use tools/extract-nav-fuzz-case.ps1 with explicit -Journal and -OutputFile to
extract raw bytes; it refuses invalid lengths and existing output files.
No fuzz failure was promoted to a new production regression during P2-08.
