# Agent Instructions

## FocalSpan and completion workflow

FocalSpan is part of the standard workflow for every implementation or source
change in this repository. Do not silently skip it.

Before editing:

1. Confirm that the working directory is the AstraBot repository root.
2. Run `focalspan status --json`.
3. If the index is stale or not ready, run `focalspan update --root .` and
   resolve any error before proceeding.
4. Query the relevant architecture, policy, and existing implementation with
   `focalspan -- "<question>"` before making design or code changes.

After implementation and verification:

1. Run the tests and builds appropriate to the change, and inspect the final
   diff.
2. Run `focalspan update --root .` so the index reflects the implemented
   state. Use a follow-up FocalSpan query when needed to verify the changed
   contract or integration points.
3. Stage only the intended repository changes and run
   `git diff --cached --check`.
4. Commit the implementation and its tests/documentation. A change is not
   complete until the commit succeeds.
5. Verify the commit with `git log -1 --oneline` and `git status --short`, then
   report the commit hash, verification results, and any remaining acceptance
   work.

If FocalSpan is unavailable, cannot update, or cannot provide the required
context, report the blocker and do not claim the implementation is complete
without explicit user approval for an exception. Preserve unrelated user
changes. Do not stage `.focalspan/` or `.focalspan.json` unless the user
explicitly requests committing the local FocalSpan index/configuration.
