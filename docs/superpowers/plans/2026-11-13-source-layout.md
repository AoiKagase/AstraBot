# Source and workspace layout cleanup

Requested scope: source/include plus repository root. Base: c700a68.
Keep public include/astrabot paths, all behavior and public ABI unchanged.

1. Group the flat Metamod implementation into plugin, runtime (behavior,
   observation, navigation, command, shared), bridge, lifecycle, config,
   navigation and diagnostics. Update private include directives, CMake,
   source-contract paths and current source documentation together.
2. Relocate the six original untracked root artifacts by explicit filename
   to artifacts/local, artifacts/imports and config/local. Preserve bytes and
   a move manifest; do not stage local binaries/configuration/evidence.
3. Keep existing absolute-path CMake build-* caches and historical documentation
   in place. Add README navigation and recommend build/<platform> for new builds.
4. Verify source layout, ownership/dependency gates, unchanged non-include C++
   tokens, Windows/Linux complete builds and full CTest, strict runtime traces,
   and separate PE/ELF export gates. Known unchanged Linux combat_policy failure
   was independently reproduced at ac68225 in the preceding task.

No file deletion, SDK editing, behavior changes, public-header relocation,
toolchain/cache overrides, deployment, push or merge. Preserve unrelated edits
to .gitignore, AGENTS.md and .planning/PROJECT.md.

Recovery/evidence: C:/Temp/astrabot-source-layout-state.json and related logs.

## Completion evidence

- 43 private adapter files relocated; all non-include source bytes unchanged.
  Public headers and SDK untouched. Six local files relocated with identical
  SHA256 values; no files deleted and local artifacts/config remain untracked.
- Windows x86 complete build and all 89 CTest cases PASS.
- Linux x86 complete build PASS; 87/88 CTest cases PASS. The unchanged
  combat_policy threshold failure remains the independently reproduced baseline
  issue documented in the preceding responsibility repair.
- Strict runtime characterization PASS on both platforms without changing
  historical baselines or reviewed corrections. PE/ELF x86 seven exact exports PASS.
- Read-only review found an RNG source-path reference and a narrowed dependency
  guard; both were reproduced by failing checks, fixed, and verified GREEN.
  Architecture checker: 49 tests PASS. Current documentation path fixes included.
- Existing build caches, user edits, historical records and .serena remain in
  place. Live HLDS/ReHLDS acceptance was not performed.
- Integration tests recreated a root BotProfile.db; it was preserved separately
  in artifacts/local/test-fixtures with its own verification_moves hash record.
