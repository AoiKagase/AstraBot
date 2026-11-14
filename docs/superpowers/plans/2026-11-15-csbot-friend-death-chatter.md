# Friend-death chatter implementation plan

Use superpowers:executing-plans inline; spec docs/superpowers/specs/2026-11-15-csbot-friend-death-chatter-design.md. Basea146109b19a54eef5ce77ea1ae7fe3ad69548e42.

## Review Focus
- Native retaliation early return suppresses both speech paths, even rejected Attack; eligible speech precedes Hunt/reposition state mutation.
- Career human friendly victim vs human friendly non-self killer gates; world/friendly kills and no victim visibility/nearby constraints; career callback and delay RNG revalidate original receipt identities.
- Delay.5..1 always drawn before OFF/MINIMAL/duplicate admission; CommanderDown20 and KilledFriend2 lifetimes expire only strictly after deadline.
- CommanderDown and NiceShot share intention redundancy for pending/current team speech, KilledFriend distinct; both slots preserve ordering and no-friends checks only when selected.
- Default catalog vs profile Important/speakables, actual host voice/radio delivery, source reuse/death/round/clock cancellation, no event-time profile I/O or duplicate event RNG/frozen churn.

## Tasks
1. [x] Core RED→GREEN: append phrase IDs/names; SDK-free friend-death speech eligibility/delay/qualified two-topic memory. Shared intention admission/suppression with existing KillPraise. Tests branch/RNG/admission/lifetime/order/context/lifecycle.
2. [x] Runtime RED→GREEN: existing validated friend death event integration before tail; maintain/arbitrate new speech topics in existing radio owner, actual registered DeathMsg/voice/radio tests incl early return/rejected Attack/callback reuse. Update CMake/manifest/ownership/resets.
3. [x] One fresh final reviewer (0Critical/3Important/0Minor;3Important fixed RED→GREEN); one Important/Critical RED→GREEN fix pass, ledger rulings/minors. Full x86 builds/suites serialized, separate PE/ELF7exports, architecture/frozen/manifest/identity; docs + explicit-path commit/local FF. Full goal active.

Final gates: Windows115/115 PASS10.29s; Linux113/114 PASS12.69s (unchanged combat_policy failure). Full-suite dead/spectator callback regression fixed against unchanged frozen characterization (4/4 focused then full suites refreshed). Independent PE/ELF x86 exact7exports and manifest286/279 PASS. Source289/artifact2/user4 identities retained. Local commit/FF receipt is session RESULT.json; no push/live acceptance; broader user goal active.
