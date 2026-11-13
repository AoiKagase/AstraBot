# Runtime responsibility repair

Approved scope: five audit findings, based on ac68225. Preserve public ABI,
commands/CVars, FullUpdate/Upkeep ordering and plant timer semantics. Keep
unrelated working-tree changes. Implement sequentially on the existing feature
branch; do not create another checkout or reuse stale binaries as evidence.

1. Centralize observed player teams in Observation. TeamInfo (including zero)
   takes precedence over valid raw entity teams; requested admission teams are
   not observed membership. Remove team metadata from objective caches.
2. Separate perception collection, read-only snapshot access and Behavior-owned
   threat recognition. Preserve first-use collection and once-per-update reaction
   timing. Transfer recognition lifecycle resets to the new owner.
3. Move delivered-input plant attempts to SDK-free Core. Preserve >=5 second
   attempt timeout / 1 second retry and the distinct >5 second compatibility
   state timeout. Keep command translation and public entity geometry in adapters.
4. Rename overall bot updates/resets to describe execution, keeping exact reset
   fields and callback order.
5. Separate behavior priority arbitration from buying. Keep purchase conditions
   with buying, and noise investigation with priority arbitration.

Verification: behavior regression tests, ownership/port contracts and checker
tests; complete Windows x86 Debug build before full CTest; Linux x86 gate and
separate PE gate; report live HLDS acceptance separately. Historical trace
fixtures must not be regenerated to hide failures. Review each task's diff and
stage explicit paths only.

Environment baseline: prior full build failed C1902 (MSVC PDB manager mismatch).
Recover the developer environment without changing cached toolchains or debug
flags. Session evidence / recovery state: C:/Temp/astrabot-responsibility-state.md.

## Completion evidence (2026-11-13)

All five repairs implemented. Windows x86 full build and CTest: 88/88 PASS.
Linux x86 full build: PASS; CTest: 86/87 PASS. The unchanged combat_policy
threshold test also fails in an independently archived/rebuilt ac68225 Linux
checkout, so it is recorded as a pre-existing platform issue, not waived as PASS.
PE/ELF x86 artifact checks: seven exact exports PASS on each platform.
Live HLDS/ReHLDS acceptance was not performed or inferred from offline checks.

The frozen historical trace is retained. An independent ac68225 Windows rebuild
exactly matches its previous actor-generation-corrected trace (131028 records).
Canonical team resolution adds 18127 indexOfEdict read callbacks and changes no
other recorded operation, value or order. observed-team-correction.json pins
every insertion offset and both platform hashes; both actual platform traces
match the strict ordered projection (149155 records). No callbacks are filtered
and no baseline is replaced. Ten correction-checker tests pass.

Independent read-only review approved the runtime changes and separately checked
the strict trace projection against actual artifacts. No actionable findings.
GCC build recovery is limited to explicit empty Snapshot initialization, SDK
Vector scalar writes with unchanged values, the parent's existing SDK warning
exception for derived fixtures, and a signed test-literal comparison.
