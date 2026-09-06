# P3-08 reproducible offline movement evidence

These are original synthetic fixtures, not CS/ReGameDLL NAV assets or live
physics acceptance. `scenarios.json` owns the expected case cross-product and
finite limits. Portable replay uses SDK-free production navigation/motor
components and an independent floor model; adapter replay uses real host
dispatch callbacks against the existing fake engine's synthetic physics.

Configure x86 Debug, tests ON, warnings-as-errors ON and the NAV inspector ON
using AGENTS.md. Ordinary CTest now requires Python 3.9+ (standard library only)
on the build host; Python itself need not be x86. On Windows with an unusable
Store alias, set `-DPython3_EXECUTABLE=<working-python.exe>` at configure time.
The dedicated ASan configuration does not require Python or run movement tests.

Run `ctest --test-dir <build> --output-on-failure`. The movement replay tests
produce `movement-evidence/portable.json` and, in the Metamod test build,
`movement-evidence/adapter.json`, with raw producer JSON and a process log.
Each producer runs every declared row twice and compares ordered event records
before marking replay equality. Measurements describe simulated frame time;
`wallClockSeconds` describes this machine's test duration only.

To regenerate just one producer:

```text
python tools/check-movement-evidence.py run --manifest tests/nav/simulation/scenarios.json --producer portable --executable <build>/astrabot_nav_replay --build-dir <build> --output <build>/movement-evidence/portable.json
```

Use the `.exe` suffix on Windows. For adapter evidence select `--producer
adapter` and the `astrabot_fake_client_tests` executable. Replace `run` with
`validate` to check saved evidence against the current checkout and executable;
the executable and build directory remain required. This is a current-build
gate, not a mechanism for relabelling historical evidence. Preserve old logs
as historical artifacts; regenerate evidence after committing or changing files.

The wrapper binds results to source revision, dirty diff/content hash, fixture
hashes, manifest hash, executable hash, compiler, CMake options and actual x86
PE/ELF header. It refuses relevant source files newer than the executable and
checks that run context did not change during execution. Missing/duplicate or
unexpected rows, unknown schema, malformed trace events, missing actor terminal
records, wrong outcomes, truncation, mismatching repeats and exceeded limits
fail the gate. The 128 MiB input cap and 180-second producer timeout bound
evidence processing. Retained raw JSON/logs diagnose producer failures.

`setupQueries` and `discoveryQueries`, when emitted by the adapter, are separate
from `totalQueries` for motion frames. The per-actor ground motion/guard cap21
must not be applied to whole-map ladder publication. Bounded production history
is checked independently of the larger offline event trace. Actor scaling is
synthetic evidence only. Fixed frame schedules need no random choices; seed308
identifies this fixture contract.

Hosted CI artifacts add run URL/job/SHA when those environment variables are
present. A local run records no hosted success. The pending live cross-product
is defined separately in `tests/live/nav/manifest.json`.
