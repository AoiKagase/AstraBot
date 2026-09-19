# Phase 8 live acceptance

## Scope

This document records real-server evidence for AstraBot as a Metamod-P plugin beside an unmodified ReGameDLL-CS host. It keeps live acceptance separate from offline CTest, artifact checks, CRG/FocalSpan navigation evidence, and synthetic differential replay.

Each platform report must identify the HLDS/ReHLDS binary, ReGameDLL-CS and Metamod-P builds, AstraBot artifact, BSP, read-only `.nav`, configuration, Bot count, duration, correlated map/round/tick/actor-generation diagnostics, and remaining limitations. Secrets and unrelated installation data are excluded.

## Required runs

1. Verify plugin load beside unmodified ReGameDLL-CS.
2. Run one Bot through creation, team/class entry, control/removal, Nav load, locomotion, perception, combat/objective intent, radio/chatter, restart, death/respawn, map change, disconnect, reconnect, and slot reuse.
3. Repeat with the configured multi-Bot two-team scenario.
4. Check actor isolation, stale-command rejection, native-Bot suppression, crash/hang absence, and bounded CPU/log behavior across transitions.
5. Classify every criterion as `passed`, `failed`, `blocked`, or `not-run`.

## Current checkpoint

Status: `partial` on 2026-09-18. Windows direct team/class join and human-attack/death evidence were captured; autonomous post-join action failed, while Linux live evidence and the remaining gameplay/lifecycle criteria remain pending.

- Windows x86 evidence is recorded in `docs/evidence/phase8-live/windows-x86.md`.
- Windows confirms public fake-client initialization, direct standard `jointeam`/`joinclass` entry independent of menu mode, and two active Bot slots after restart.
- Windows also records a human CT damaging and killing the TERRORIST Bot `Bert` with a USP. This is partial live damage/death evidence for `PAR-03`, not full CSBot combat parity.
- Windows locomotion remains failed: `RunPlayerMove` is dispatched, but the managed Bot remains outside a containing legacy Nav area and does not progress.
- The four managed Bots were eventually removed by `Game_idle_kick`, confirming that team entry did not produce autonomous post-join action in this run.
- A fresh post-fix log-only run recorded `teamConfirmed=1` and `ready=1`, but later produced four `Game_idle_kick` lines. The strict verifier found no post-spawn sustained horizontal movement, no Bot-to-Bot attack, and no C4 plant/defuse event. Nav diagnostics reported `roam_no_intent`/`Stuck`.
- Debian Linux x86 live evidence is still blocked/not-run.
- Natural combat respawn, map change, complete disconnect/reconnect/slot-reuse matrix, full gameplay surfaces, and bounded CPU/log measurements remain open.

`PAR-06`, `TEST-03`, and `TEST-04` remain pending until both platform reports contain complete evidence. No requirement is promoted from CTest, fixtures, CRG, or FocalSpan alone.
