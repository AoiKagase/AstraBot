# P3-04 reactive Walk and player observation

The portable blocker controller from `98c6bf9` now connects to Walk and the
host's world-query port. The host profile keeps 21 total queries and four floor
samples per decision, including reserved host guards. Dynamic facts live for
120 ms; reciprocal lower-PlayerId priority yields for 400 ms; one attempt ends
after 3 s. The time includes skipped decision frames. No trajectory prediction
or static NAV/graph mutation is introduced.

On a blocked forward segment, a stamped Blocker query can produce Geometry,
Player (team unknown), or Other. Player priority requires a current registry
PlayerId in the same map/tick and an engine entity/index round trip. A public
player hit without registry identity remains Other, with no inferred team or
reciprocal priority. Entity serial/index is retained as obstacle identity.
Missing entity lookup capability and malformed/stale observations stop with a
typed blocker reason. These observations do not register clients or prove the
pending multi-client lifecycle contract.

The higher PlayerId initially emits neutral yield. An eligible actor inspects
one side segment using the existing hull/floor/portal checks and fixed side
choice. At most 25 side attempts occur within the same episode. If a side is
blocked, unsupported, too short or its attempt budget is exhausted, the actor
waits within the original deadline. A later complete ground/segment inspection
invalidates the fact on a neutral tick before normal translation resumes. A
verified portal crossing similarly retires the old fact before advancing.
Replacement observations never restart the timeout, and timeout wins over
clearance at the exact deadline. Terminal intents are neutral.

Motion history/status includes blocker identity/class, PlayerId, action and
typed reason. The host currently stops on a terminal DynamicBlocked/Replan
request. Automatic replan consumption with expiring edge facts and a carried
attempt budget is still pending; repeated static A* is not presented as recovery.

Portable simulations use real IntentPump/Motor commands at 8/16/100 ms frames.
They cover stationary, slowly approaching and receding player obstacles,
clearance after removal, a closed corridor, replacement every observation,
unavailable/stale capability, invalid limits, reserved query ordinals, duplicate
ticks, deterministic replay and unchanged static route results. Movement is
checked against independent swept collision geometry. These are one controlled
actor with synthetic blockers, not live two-Bot proof.

Adapter tests exercise actual Blocker traces, registry generation/serial changes,
map/tick mismatch, absent lookup, unregistered players, removal, host avoidance,
passage release and bounded timeout at 8/16/100 ms. The test fixture explicitly
sets the engine entity limit, as real engine globals do.

Verification: Windows x86 NMake Debug adapter/portable 34/34 and WSL Debian
GCC -m32 Debug portable 29/29 pass with warnings-as-errors. Release x86 DLL
build passes; the six contracted exports were confirmed. Diff review/check and
FocalSpan update completed. Hosted CI for this branch remains pending.

P3-04 remains open: its separate per-player entity/join/dispatch mapping commit,
two-client isolation, and bounded replan integration are required. Live server
and multi-Bot acceptance remain post-Finish. Real NAV compatibility is partial;
neither Phase 3 nor project-wide Finish is declared.
