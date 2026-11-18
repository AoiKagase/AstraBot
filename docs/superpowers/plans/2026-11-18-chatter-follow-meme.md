# Original08 implementation plan
Spec: docs/superpowers/specs/2026-11-18-chatter-follow-meme-design.md
Base: d67cb7e375034630c5866978b73d44e400891a3d

## Task1: ordered Core response
- [x] Semantic RED/GREEN for shared Rogue→Busy→SAFEST range<=1000→Follow→CoveringFriend; no proximity/Place/extra RNG, invalid input and callback reset guards.

## Task2: production constructor and natural completion
- [x] Typed Follow payload/catalog phrases; exact GoingToPlantBomb critical constructor, shared plan timer, MoveTo state-entry/age/friend/cover precedence, reset/admission rules. Resolve actual remaining-path predicate.
- [x] Completion versus cancellation/defined silent phrase and immediate CoveringFriend metadata.

## Task3: actual runtime ongoing follow
- [x] Qualified teammate delivery reused from07; explicit current leader identity in shared ScenarioBehaviorPlanner and Follow state, timestamp/reentry behavior, public position/serial/actor/connection validation.
- [x] Actual two-managed-bot production phrase/natural completion, moving leader target and RunPlayerMove proof, leader serial invalidation and human-key collision test. Shared 07 delivery guards plus dedicated staged response failures cover cancellation and unavailable callbacks.

## Task4: verification and local integration
- [x] Review and fixes, complete builds, serialized CTest, PE/ELF checks, manifest, and input fingerprints. The known unit22 Linux test remains open.
- Local integration and post-merge fingerprints are recorded in the feature session receipt.

Resolved native source question: cs_bot_pathfind.cpp1735–1751 sums centers after the current path index, excluding distance from the bot position; no path returns-1 (therefore <50). Implement the existing path-progress view rather than Euclidean target distance. Native FollowState full locomotion/RNG gaps must remain explicit in23 rather than being silently called complete.
