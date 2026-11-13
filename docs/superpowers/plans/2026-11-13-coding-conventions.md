# Project-owned C/C++ convention repair

Requested: repair the confirmed coding-rule violations across src/include/tests
while preserving behavior and compatibility. Base 5f847b8; 228 C++ files.
Authority: C_CPP_REFACTOR_RULES.md. No commits/pushes/merges requested for this task.

1. Add scoped editor and clang-format 11 settings. Preview representative files;
   then format the explicit owned inventory. Preserve includes, literal tokens,
   comments, preprocessing and every non-whitespace token. No SDK/generated edits.
2. Name specification constants at their smallest useful scope, preserving each
   literal's original type, suffix, value and use-site evaluation order. Inspect
   domain meanings; do not rename mathematical identities or individual table/test
   data entries. Do not introduce speculative names or change public layouts.
3. Update lexical source contracts for harmless line wrapping using the existing
   token-aware guards, preserving negative tests and all semantic predicates.
   Validate remaining formatting/mandatory violations across the full inventory.
4. Full Windows x86 and Linux x86 builds, full CTest, strict unchanged runtime
   traces and separate PE/ELF export gates. The pre-existing Linux combat_policy
   threshold failure is independently established, not treated as PASS.

Preserve the three unrelated dirty files (.gitignore, AGENTS.md,
.planning/PROJECT.md), all user local files and existing build caches/evidence.
Use no broad source restores, staging, deletion, toolchain/cache overrides or
unrequested behavior fixes. Independent review focuses on token/literal identity,
constant types/FP behavior and static-gate strength. Live acceptance is separate.

Session recovery/evidence: C:/Temp/astrabot-conventions-fix-state.json.

Completed: all four steps, with one focused literal-precision regression test added.
Final gates: Windows x86 90/90; Linux x86 88/89 with only the independently
reproduced pre-existing combat_policy threshold failure. Both 149,155-record
characterizations and exact seven-export PE/ELF gates pass. The precision regression
was verified RED on constexpr operands and GREEN after 70 scoped literal macros
restored original evaluation precision. No commits or deployment performed.
Evidence: docs/verification/coding-conventions-20261113/VERIFICATION.md.

Commit follow-up: the user subsequently requested a local commit. Include only
this repair and its verification records; preserve unrelated edits and local
artifacts. No deployment or push requested.
