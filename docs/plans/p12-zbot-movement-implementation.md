# P12 ZBot movement implementation record

The first implementation slice establishes the ZBot baseline as the required acceptance target and adds a bounded runtime terrain extension. The immutable NAV mesh remains authoritative; runtime shortcuts are an external per-route overlay and do not rewrite the NAV file.

Implemented behavior:

- Active route-step traffic reservations, current-goal retention, time-based courtesy cost, and actor-local transient failure handling.
- Distance-safe portal steering, stair lift probing when the floor sample is still on the lower tread, and `NAV_PRECISE` precision steering.
- Consistent effective Walk/Jump/Drop classification, external Walk/Jump/Drop endpoints, and a shared local execution path for runtime shortcuts.
- Nearby shortcut discovery from the current area, bounded to eight world queries per simulation tick and eight candidate links per route. Candidates require target support and a swept-hull check for ordinary Walk; Jump and high Drop candidates are revalidated by the existing execution probes.
- Drop risk prediction using the ReGameDLL fall-damage constants. Damage is accepted only within the configured light-damage and post-landing-health bounds.
- Regression sources for graph overlay ownership and precise Jump classification. They remain unbuilt until the explicit live PASS gate is satisfied.

Verification on 2026-09-11:

- x86 Release `astrabot_mm` build succeeded with tests disabled.
- The six required exports were verified.
- Built and deployed DLL SHA-256: `A17F2A738F6070BC964D790C6F23F88D58E3430CDBB43706E731710AC396EEDE`.
- `git diff --check` passed.

Open acceptance:

- Restart/changelevel HLDS and compare ZBot/AstraBot on `de_dust`, including the three target transitions and 1v1, 2v2, and configured-BOT-count runs.
- Confirm live movement, Jump/Drop, combat regression, round recovery, map transition, and shortcut cases in `qconsole.log`.
- After explicit live PASS, build/run the focused regression tests and canonical gate.
