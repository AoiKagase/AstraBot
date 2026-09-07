# Phase 14 — Final Source Cleanup and Release Preparation

After functionality is complete, Phase 14 organizes readability,
documentation, licensing, and distribution artifacts. Cleanup is kept separate
from functional changes.

## Goal

Prepare the completed project for maintenance and release without changing
runtime behavior as part of formatting or packaging work.

## P14-01 — Source Readability Cleanup

Perform the first project-wide formatting pass here.

Conventions:

- one statement per line;
- actual tab indentation;
- tab display width of four;
- `{` and `}` on separate lines;
- long conditions split for readability;
- no multiple side effects packed into one line;
- comments for important boundaries and intent; and
- comments should explain why rather than only what.

Keep this in a separate commit from functional changes.

## P14-02 — Test Policy Cleanup

Apply CTest labels such as:

```text
fast
integration
replay
fuzz
live
```

Daily workflow:

```text
focused
→ fast
```

Phase gate and CI workflow:

```text
full
replay
fuzz
```

Avoid duplicate full-test runs for the same Git tree.

## P14-03 — Documentation

Ensure the release documentation covers at least:

```text
README
architecture
navigation
perception
combat
action planner
tactical planner
experience
AMXX API
build
configuration
troubleshooting
```

## P14-04 — License / Provenance Audit

Verify:

- MPL-2.0 headers;
- `NOTICE.md`;
- upstream references;
- the license matrix;
- fixture provenance; and
- absence of accidentally copied GPL code.

## P14-05 — Release Packaging

Prepare Windows and Linux packages containing:

- plugin binaries;
- configuration files;
- required runtime directories;
- the AMXX bridge, when enabled; and
- installation instructions.

## P14-06 — Final Gate

The final decision requires:

```text
Build PASS
Tests PASS
Live acceptance PASS
Performance PASS
Stability PASS
License audit PASS
Documentation PASS
```

Only after all criteria pass may the project declare:

```text
Project-wide Finish
```

