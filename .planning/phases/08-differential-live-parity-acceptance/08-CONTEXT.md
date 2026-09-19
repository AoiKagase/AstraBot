---
phase: 8
name: Differential live parity acceptance
status: approved
---

# Phase 8 Context: Differential live parity acceptance

## Goal

Close the v1 acceptance boundary for the current AstraBot HEAD by separating
offline x86 artifact and replay evidence from real HLDS/ReHLDS acceptance. The
phase must establish whether the independent Metamod-P plugin can replace the
CSBot runtime beside an unmodified ReGameDLL-CS installation without treating
Core fixtures or graph indexes as live proof.

## Locked decisions

- Use current HEAD `5a62a98` as the evidence baseline unless a reproducible
  defect is found. Do not add speculative behavior while collecting evidence.
- Verify Windows x86 and Debian Linux x86 portable Core and Metamod builds,
  CTest suites, source-manifest/format checks, and the six exact Metamod
  exports independently for each platform.
- Differential traces compare only observable behavior and bounded outcomes.
  They may use the pinned ReGameDLL-CS identity as provenance, but must not
  copy reference source, private symbols, hidden state, or engine success into
  AstraBot fixtures.
- Real-server evidence requires an unmodified ReGameDLL-CS plus Metamod-P
  installation, an HLDS/ReHLDS runtime, a compatible BSP and `.nav`, and
  captured plugin/server configuration. Single-Bot and multi-Bot lifecycle,
  combat/objective/radio behavior, map/round transitions, disconnect/reconnect,
  slot reuse, and stability remain live checks.
- If the real-server installation or operator run is unavailable, record the
  exact missing prerequisite and keep PAR-06, TEST-03, and TEST-04 pending.
  Offline success cannot be promoted to live acceptance.
- Existing `.nav` remains read-only. AstraNav creation, optimized
  `astranav` persistence, Wallbang data, learned traversal, and adaptive AI
  remain deferred to a later milestone.
- `plugin_runtime.hpp` remains under `src/adapter/metamod`; it owns SDK-bound
  runtime state and is not a Core/public behavior contract.

## Evidence layers

1. 08-01: current-HEAD artifact/export and offline matrix evidence.
2. 08-02: source-independent differential trace/replay evidence.
3. 08-03: real-server single/multi-Bot evidence on both supported OS targets.
4. 08-04: requirement audit, documentation, and deferred-boundary review.

The first two layers can be completed without a live server. The phase as a
whole cannot be marked complete until the required live evidence is present.

## Existing boundaries

- Core behavior and navigation contracts remain SDK-free.
- Metamod adapter owns engine observations, FakeClient dispatch, receipts, and
  runtime diagnostics.
- Phase 7 fixtures remain an offline contract gate and are not differential or
  live-server evidence.
- Use `AGENTS.md`, `C_CPP_REFACTOR_RULES.md`, current source, direct build/test
  output, CRG, and FocalSpan for every acceptance decision.

## Open acceptance inputs

- Exact HLDS/ReHLDS server roots and map/runtime configuration for Windows x86
  and Debian Linux x86 are not present in the AstraBot checkout.
- A pinned, captured CSBot reference trace is not yet present; 08-02 must
  either capture one through the approved reference runtime or record TEST-03
  as pending rather than inventing it.
