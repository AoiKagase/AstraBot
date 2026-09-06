# P4-09 reproducible perception evidence

Original synthetic fixtures only; no game assets or live compatibility claim.
`scenarios.json` owns the input timeline, settings and matrix. Portable18 rows:
1/8/16 observers x8/16/100ms x NAV/absent. Fake-engine15 rows:9 hook/load rows
and6 arrival/recovery rows. Movement rows use one moving Bot; existing P3 host
matrix continues to cover multi-actor movement. This is not a 16-Bot server FPS benchmark.

Configure x86 Debug, tests ON, inspector ON, warnings-as-errors ON per AGENTS.md.
Ordinary CTest runs `astrabot.perception.replay` and (Metamod build)
`astrabot.adapter.perception_replay`. Python3.9+ standard library only.

```text
python tools/check-perception-evidence.py run --manifest tests/perception/scenarios.json --producer portable --executable <build>/astrabot_perception_replay.exe --build-dir <build> --output <build>/perception-evidence/portable.json
```

On Linux omit `.exe`. For adapter use producer `adapter` and
`astrabot_fake_client_tests.exe`. Replace `run` with `validate` to recheck a
saved result against the same current checkout/build. Committing or editing
source changes its provenance; retain old evidence as historical and regenerate.

The wrapper reuses P3's build-context reader: revision, dirty content hash,
manifest/fixture/executable hashes, compiler/options, x86 PE/ELF and source
freshness. It runs the producer twice in independent processes, compares full
ordered JSON snapshots/diagnostics and binds the result to the context checked
again after execution. Both raw runs and stdout/stderr logs remain beside the
final JSON. No output is reused after process failure. CI uploads portable
evidence on success or failure; hosted execution requires a separately authorized push.

Each row records actual source-specific confidence/time, identities, canonical
distribution masses, per-frame work and fixed object sizes. The checker owns
expected positions, lifetimes, isolation, checkpoints, FIFO drain, complete actor
matrix and movement terminals. Ten deliberate mutations must be rejected per
run (missing/duplicate row, missing checkpoint, confidence/position/trace/source
corruption, non-finite value, wrong revision, oversized model). Input JSON is
limited to128MiB, each producer to180s and each CTest wrapper to420s.
Output is reproducibility evidence for ordinary builds, not binary attestation.

Core timeline: direct sight x100 (observer2 initially has no direct sight),
occlusion, anonymous sound alone, then explicit report, half confidence at2.5s, expiry
at5s, fresh x210 sight, round change, slot reuse, map change,64 round/generation
retirement cycles. Dense999-edge NAV expansion checks every eligible observer
under the256-edge/frame budget; last complete distributions retain truncation
mass in unknownMass. No-NAV rows publish no candidates.

Fake-engine: actual hooks queue266 distinct footsteps (256 retained,10 rejected),
smoke plus receiver flashes suppress fresh sight, StartFrame drains32 events/frame
for every listener, hidden current x4000 never replaces x100, effects expire and
x210 becomes visible again. Round/disconnect/reuse and map deactivation retire
knowledge. Existing effect/world/report reentry tests run in the same producer.
Arrival/recovery runs alternate visibility while emitting sound every frame;
transient700ms stalls enter recovery, arrive and dispatch no further motion
during10 stopped frames. Full CTest also retains P3 multi-Bot arrival/cancellation/
recovery evidence and the separate P4 contract suites.

Snapshots are borrowed canonical views and copied into evidence before the next
model change. No new production engine query, command or export is introduced.
