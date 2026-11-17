# Native Help prerequisite characterization control

The frozen gzip traces describe older AstraBot behavior. They are retained unchanged. CSBot source at b0889847fe6d03898be88acc9e366660efb40ab5 is the behavioral reference.

## Independent control

Export commit b3e4ec67a883a403464ecbc32f623853208b8c0a into a fresh directory. Build only astrabot_compat_actor_command with the pinned public Metamod-P SDK. Use MSVC Hostx86/x86 Debug on Windows and GCC -m32 Debug on Linux. Run from that exported source directory with ASTRABOT_CHARACTERIZATION_PATH set to an absolute output path. The unchanged executable must reproduce the first two corrections exactly:

| Platform | Records | SHA256 |
|---|---:|---|
| Windows x86 |149155|6e4db3a5dabdc0abb92cb0488798c60af131ebe6ea2ba33e0b0017d2e1e2a674|
| Linux x86 |149155|069418793367a217bcb377e98a5ef303fa0c3288125acd6abceb25bcf3ccc685|

Apply the nine literal edits from native-help-preconditions-control.json, in order, to this exported source only. Each before string must occur exactly once. Rebuild the same target and repeat the identical fixture. The control contains no Help delivery/controller implementation and changes no test input or random stub.

The native source locations and reasons are in the control JSON. The changes preserve the same-actor Rogue countdown across ResetValues, prevent a second ordinary-state update in navigation completion, enter Buy after spawn, avoid unavailable purchase queries outside the public buy zone, and discover public ladder topology when an objective path crosses areas. Native ladder discovery reuses the existing public LadderCache; it does not synthesize recorded callbacks.

| Platform | Control records | SHA256 |
|---|---:|---|
| Windows x86 |149075|7d42624076ca24ad5c090d382ebc423baf41678c2165003f86cbfc8baf12b36c|
| Linux x86 |149075|82539676e0d28543df6036815474b20a055f59933cbdda7ae4d1ec38f62028a2|

Both independent control traces were byte-identical to the Help implementation on 2026-11-17. The intermediate Linux controls isolated 600 difference blocks to 41 after timer/single-update changes, then to two public ladder queries after Spawn/Buy changes, then to zero. The native correction was generated only from these independently rebuilt before/after controls; the candidate trace was not an oracle input.

## Strict projection

native-help-preconditions-correction.json is applied after actor-generation-correction.json and observed-team-correction.json. It contains 600 pinned edits, 354 shared patterns, both platform hashes and the control JSON hash. Existing offset, exact-context, record-count and whole-trace SHA checks remain mandatory. No callback, random result, movement command or profile line is globally ignored. The changed engine callback classes are two added public func_ladder discovery queries and objective-visibility samples whose positions/counts follow the changed native RNG/decision sequence (37 fewer samples in this fixture). The checker verifies the remaining engine callbacks stay ordered and identical. Every changed visibility sample is still checked byte-for-byte by the full characterization; this auxiliary category check does not filter the production comparison.

This offline characterization is a regression check. Native behavior boundary/runtime tests and real HLDS/ReHLDS acceptance are separate evidence. Live ladder physics, C4/Aim/turning and multiplayer performance remain in the acceptance backlog.
