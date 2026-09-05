# P2-04 implementation plan

Approved design: ../specs/2026-09-05-p204-nav-snapshot-design.md.

1. Establish portable Debug baseline. Add a move-only result regression and
   observe compile failure; fix ReadResult success to move the payload.
2. Add loader tests for v1-v5 ownership and exact invalid-field diagnostics;
   observe missing loader failure. Add query-local read context, budget checks,
   tracked reader paths, internal validator/builder and immutable snapshot.
3. Extend independent fixtures for tactical/connection records, conflicting
   diagnostics, limits and allocation failpoints. Verify red/green for changes.
4. Run portable Debug and /analyze, review diff and FocalSpan contract, explicitly
   stage source/tests/docs, cached diff check and commit. Leave feature branch
   unmerged and preserve local FocalSpan/Serena state.

All steps enforce the approved contract. Use exact field positions captured by
the shared decoder, not reconstructed offsets or a second wire parser.

## Completion evidence

- Move-only ReadResult test failed to compile with the original copying factory,
  then passed with move transfer.
- Loader tests first failed on the absent loader header. Independent v1-v5
  fixtures now test ownership, full minimal-file truncation coverage, exact
  semantic offsets, reference ordering, directed duplicate policy and Place.
- Tactical and long-Place failpoint sweeps fail each allocation in turn until
  successful publication; every failure returns AllocationFailure and no value.
- Legacy encounter semantic validation and full-file Place total-limit classification
  each had a failing regression before their fixes.
- Portable x64 Debug NMake /W4 /WX CTest: 8/8 passed.
- MSVC /analyze x64 Debug NMake /W4 /WX CTest: 8/8 passed, no warnings.
  Use a separate build directory and preserve /EHsc when overriding flags:
  -DCMAKE_CXX_FLAGS="/DWIN32 /D_WINDOWS /GR /EHsc /analyze".
- FocalSpan update and query locate load, validateMesh and DecodeContext at the
  implemented boundaries. No SDK headers or libraries were added to Nav.
- Integration remains pending: no main merge, push, Linux or live validation.
