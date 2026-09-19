# P13 — Enhanced Intelligence Foundation (Only After Baseline Acceptance)

## Goal

Resume the actual long-term AstraBot mission: become more human-like and tactically capable than CSBot without destroying the compatibility baseline.

This plan must not begin until P12 is explicitly accepted by the project owner.

## Architecture rule

Treat the frozen compatibility implementation as a proven **execution/baseline policy**. Enhanced reasoning should override decisions through narrow interfaces, not fork every low-level system.

A useful conceptual split is:

```text
observations -> belief/memory -> enhanced reasoning -> intent
                                         |             |
                                         +----fallback-+
                                               |
                                    compatibility action layer
                                               |
                                            command
```

## First enhanced capabilities

Prioritize capabilities that improve decision quality without granting impossible information:

1. **Belief-based memory**
   - known vs suspected vs unknown;
   - uncertainty decay;
   - last-seen/heard evidence;
   - no omniscient enemy positions.
2. **Opponent profiling**
   - aggression/rush/camp/weapon/route tendencies;
   - confidence and sample count;
   - optionally map-scoped persistence after explicit design decision.
3. **Adaptive routing**
   - learn danger/success tendencies;
   - preserve exploration and imperfect choice;
   - avoid deterministic exploitation that looks robotic.
4. **Tactical reasoning**
   - risk/reward, time, economy, teammate status, objective state;
   - allow “no override” so compatibility baseline remains fallback.
5. **Team coordination**
   - complementary roles, trading, spacing, crossfire and utility cooperation;
   - communicate using information the bots could plausibly share.
6. **Human-like imperfection**
   - reaction and confidence limits;
   - uncertainty-driven hesitation;
   - profile-consistent mistakes rather than artificial random sabotage.

## Required enhanced-mode policy

Every enhanced override should be inspectable as:

- baseline intent;
- enhanced evidence;
- enhanced confidence;
- chosen override or `none`;
- final intent.

This makes “smarter” measurable and debuggable.

## Evaluation

Run A/B simulations:

- compatibility vs compatibility to prove harness stability;
- compatibility vs enhanced under same map/profile/team distributions;
- measure not only K/D but objective success, trades, teammate survival, route diversity, utility impact, survival after information loss and decision calibration.

Do not optimize only for win rate if the mission is human-like intelligence.

## Regression contract

P13 and all later work must keep the entire compatibility test suite runnable. A new intelligence feature that breaks compatibility mode isolation is a regression even if enhanced mode improves.

## Acceptance criteria

- enhanced reasoning is a separate, inspectable layer;
- compatibility baseline remains unchanged/passing;
- each enhanced feature can be enabled/disabled independently enough to A/B test;
- enhanced decisions never use forbidden ground-truth information;
- at least one enhancement demonstrates measurable improvement without compatibility regressions.

## Commit

Do not use one huge “AI rewrite” commit. Split later enhanced features into their own plans/commits after this foundation is approved.
