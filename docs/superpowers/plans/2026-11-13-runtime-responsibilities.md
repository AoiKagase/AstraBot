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
