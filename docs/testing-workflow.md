# Efficient verification workflow

This repository uses content identity, not Git operation history, to decide
whether a full verification is needed.

## Verification identity

`tools/verification-cache.ps1` fingerprints the contents of source, headers,
tests, tools, CMake files, dependency manifests, and CI workflow inputs. The
fingerprint also includes the selected platform, toolchain, dependency
identity, and build configuration. Documentation, planning state, local
indexes, and generated build output are excluded deliberately.

Each successful record stores the fingerprint, verified `HEAD`, verified
commit tree, input count, command, and environment identities. Records live
outside the repository by default (`%LOCALAPPDATA%\AstraBot\verification-cache`)
and failed runs are never reusable.

## Local workflow

During implementation, run only the smallest focused target needed for the
change. Before task completion, run the canonical Windows x86 verification
once from the repository root:

```powershell
pwsh -NoProfile -File tools/verify-canonical.ps1 -Profile All
```

The command skips a profile only when a previous successful record has the
same build-input fingerprint and environment identity. It records a new
result after each profile passes. `All` consists of the existing canonical
Portable Debug CTest, Metamod-P Debug CTest, and Metamod-P Release/export
checks described in `AGENTS.md`.

A commit, merge, branch switch, branch deletion, or other Git-only operation
does not invalidate a matching record. Rebase/cherry-pick conflict
resolution, source/test changes, CMake or other build configuration changes,
dependency changes, platform/toolchain changes, and changes to the test
runner do invalidate it because their content or identity changes.

Docs-only changes use documentation checks or review only; they do not force a
full build/test when the build-input fingerprint is unchanged.

## CI workflow

The workflow serializes runs for the same commit SHA and stores a per-job
successful marker with an exact SHA key. A later push/PR run for that SHA
restores the marker and skips that job's full suite. There are no broad
restore keys, so a result from another SHA cannot be used accidentally.
