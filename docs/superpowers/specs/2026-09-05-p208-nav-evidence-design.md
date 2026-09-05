# P2-08 approved design

Implement offline evidence only, from main 4fcc3fe in codex/p208-nav-evidence.
No subagents, remote operations, main merge, Linux or live validation.
Phase completion is not project-wide Finish.

Independent v1-v5 minimal/full fixtures and field spans drive value checks,
exact corruption diagnostics and every-prefix truncation. A manifest records
SHA-256, provenance, counts and routes. No upstream source/assets are imported.
Real-NAV differential comparison is optional upon lawful fixture/tool evidence
provision; absence must be explicit.

Seeded byte-reader/loader mutation smoke: seed 0xA208, 10000 cases, timeout 60s.
Long Windows ASan run: same seed, 100000 cases, timeout 600s. Fixed PRNG and
mutation sequence support replay; failure bytes go to build artifacts.
Mutations may remain valid: check publication invariants and repeatability.
Input 65536 bytes; areas 128; Places 64, each 256 bytes, total 16384 bytes;
snapshot logical bytes 4194304. Nested per-category totals <=4096, explicit
per-record caps; byte reader <=256 operations. These are not real heap limits.

Independent linear spatial and Dijkstra route oracles check small graphs;
equal-cost selected-edge ordering remains covered by dedicated existing tests.
Benchmark 128/1024 areas, warmup, median/p95 load/query/route and input hashes;
timing is evidence, not a pass threshold.

Keep public APIs, diagnostics and _ITERATOR_DEBUG_LEVEL=0. Tests first before
any necessary production fix. Normal Debug CTest 13+3=16, separate /analyze,
Windows MSVC ASan on Nav and dedicated corpus/fuzz/oracle binaries. ASan uses
/Zi, no /RTC or incremental link; allocator override tests run under normal
Debug. ASan is not proof of all UB absence.

CI becomes Windows x64 Debug/NMake; Linux is deferred until Finish. Separate
ASan job runs dedicated tests and long fuzz. Hosted CI is not launched here.
Completion requires local Debug16/16, /analyze, ASan and gate evidence,
self-review, FocalSpan update/query, explicit stage/check/commit/hash/status.
Preserve all existing worktrees and local FocalSpan/Serena configuration.
