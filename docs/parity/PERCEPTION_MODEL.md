# P06 Compatibility Perception Model

Reference: ReGameDLL-CS commit b0889847fe6d03898be88acc9e366660efb40ab5.

## Boundary

The P06 data flow is:

    public adapter observation
        -> CSBot-compatible perception
        -> per-Bot short-term belief
        -> existing Compatibility consumer

World/entity enumeration is not itself knowledge. A contact is published as
observed only after the public FOV and LOS/body-probe boundary accepts it.
Hidden contacts may retain actor identity for lifecycle bookkeeping, but their
position, velocity, and view data are scrubbed before Core publication.

## Reference semantics inventory

- FOV uses CBaseMonster::FInViewCone: horizontal 2D dot product, strict
  dot > 0.5, and no vertical-angle restriction. The private FOV member is
  initialized to 0.5; public player pev->fov is not substituted for it.
- IsVisible checks blind state, smoke, then a trace from GetEyePosition. The
  player overload probes chest, head (+25), feet (-34 standing or -14 ducking),
  left edge (+13 perpendicular), and right edge (-13), in that order.
- FindMostDangerousThreat scans alive hostile players, ignores self and
  non-target/drawn-out contacts, requires visible body parts, and orders
  visible threats by distance. UpdateReactionQueue then applies the private
  reaction queue.
- OnEvent accepts hostile audible events only. A recent noise is replaced only
  by a higher priority event or an equally prioritized nearer event. Noise
  position is randomized inside its NAV area by reference RNG and is forgotten
  after 20 seconds.
- IsNoiseHeard has a profile reaction delay. The Core model keeps an explicit
  ready flag so presence of a noise is not confused with usability.
- Player death, spawn/respawn, round, bomb, radio, and other GameEventType
  paths are distinct from direct entity observation. P06 models the mutation
  boundary without claiming a live public GameEvent feed.

## AstraBot ownership

- perception::PerceptionInput is the adapter-to-Core input boundary.
- perception::PerceptionAssembler owns bounded per-observer memory and noise.
- world::WorldSnapshot exposes current observed contacts, scrubbed absent
  contacts, generation-scoped last-known memory, current sounds, and selected
  noise memory.
- PluginRuntime owns one PerceptionAssembler per managed actor. Scanning is
  tied to the existing Full Update sequence; command execution reuses the
  current belief rather than rescanning omniscient entity state.
- Enhanced profiles, TeamDirector state, long-term memory, and opponent
  profiles are not read by the Compatibility perception path.

## Knowledge and lifetime

Unknown means no usable contact knowledge. Observed is a current visible
observation. Believed is retained short-term knowledge, such as a remembered
last-seen contact after LOS/FOV loss or an event mutation. Actor slot and
generation are part of the key.

Memory age and noise age are explicit deterministic bounds in the Core config.
Map/round generation changes clear the bounded memory. Player respawn events
clear that actor's prior life memory. Player death downgrades current contact
knowledge to Believed while retaining the last-known position; this preserves
the distinction between death knowledge and direct observation.

## Public trace limits

The public adapter calls pfnTraceLine with the observer skipped and accepts a
probe only when the returned fraction is exactly 1.0. The reference
ignore_glass/smoke behavior and ReGameDLL private blind state are not available
through this boundary. Those rows remain IMPLEMENTED_UNVERIFIED.

Optional Core traces contain vision, knowledge, noise, and event records. The
sink is null by default and no trace writes files or consumes RNG.

## Ground-truth leak audit

| Path | Classification | Evidence |
|---|---|---|
| public observer/target edict reads in ObservationAdapter | SAFE boundary | translated into typed adapter observations |
| hidden hostile position in PluginRuntime | FIXED | published as ObservedAbsent with zero position/velocity |
| visible hostile position in WorldSnapshot | SAFE | only after FOV/LOS body-probe acceptance |
| opponent profile / TeamDirector / enhanced memory | NOT_USED | no P06 Compatibility read path |
| objective entity scan | INFERRED public objective path | retained for P05/P06 objective regression, not enemy perception |

## Acceptance

Unit semantics are verified by deterministic Core and adapter fixtures.
Engine trace parity, smoke, private reaction queue timing, live GameEvent
delivery, and ReGameDLL differential traces remain unverified. P06 is therefore
PARTIAL, not MATCH.
