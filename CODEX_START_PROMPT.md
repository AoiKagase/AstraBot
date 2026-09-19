# Copy/paste prompt for Codex

Use this when starting the roadmap in a fresh Codex session:

```text
Work on AstraBot using the parity roadmap included in this repository.

Read only:
- README_FIRST.md
- ROADMAP.md
- docs/STATUS.md if it already exists
- plans/P00_FREEZE_REFERENCE_AND_AUDIT.md

Then execute P00 only.

Important constraints:
- The target is an original implementation with behavioral compatibility to the pinned ReGameDLL_CS CSBot reference. Do not copy ReGameDLL implementation code into AstraBot.
- Inspect the current AstraBot implementation before making changes. Preserve anything already correct.
- Existing Astra-only advanced features must not be deleted merely because they are non-CSBot behavior; document them and later isolate them behind enhanced mode.
- Do not perform unrelated refactors.
- Run only tests needed for the active plan plus the normal build/smoke gate.
- Update persistent parity/status documents so another fresh session can continue without relying on chat memory.
- Commit P00 as one coherent phase commit and stop. Do not start P01 and do not push unless I explicitly request it.
```

For subsequent phases replace `plans/P00_FREEZE_REFERENCE_AND_AUDIT.md` with the next plan and say `execute Pxx only`.
