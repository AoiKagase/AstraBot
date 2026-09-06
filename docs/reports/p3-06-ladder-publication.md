# P3-06 fingerprint-bound ladder publication

NAV load now attempts ladder enrichment using the running engine's map name and
game directory: `<game>/maps/<map>.bsp`. It validates the bounded map-name/path
components, opens the BSP read-only and hashes its bytes with SHA-256. The
portable hash uses fixed working memory, a caller-selected cap no greater than
512 MiB and typed input/size errors. If the NAV header has a BSP size, a mismatch
rejects enrichment. Hashing identifies the supplied byte stream; it does not
authenticate NAV provenance or prove that the file matches the engine's already
loaded in-memory BSP. Real-file compatibility remains partial.

## Whole-map transaction

`discoverLadderLinks` enumerates the bounded candidate set and each of its four
cardinal faces/two exit variants once. Each variant retains the preceding
12-query inspector contract. Hard map-level bounds are 8192 entity slots,
128 candidates, 12288 traces, 1024 passages and 2048 directed links. These
load-time discovery bounds are separate from per-actor motion query budgets.
There are no automatic retries after a budget failure.

Successful passages create separate Up and Down links. Source ID is the fixed
ladder service ID; generation is monotonically assigned for discovery. Link ID
encodes slot/face/exit/direction. Endpoints use supported NAV floor coordinates;
the retained immutable passage packet keeps hull-origin/contact/clearance and
entity/model/bounds evidence for subsequent movement revalidation.

Before return, the full candidate set and each accepted entity/model identity
are checked again. Unlinked candidates, stale results, unavailable APIs, malformed
traces, cap failures or allocation failures produce no batch. The graph composer
validates the computed fingerprint and existing area/link contracts before
publication. Existing routes and old enrichment are retired before replacement;
failure leaves a native graph, or fully invalidated navigation after a deferred
map/lifecycle event. The host prints explicit failure diagnostics or Ready with
candidate/passage/link/query counts, generation, BSP byte count and SHA-256.

No new command/export, NAV serialization, private SDK state access or upstream
implementation copy is added. Existing `astrabot_nav_load <path>` performs this
step automatically. A missing current-map BSP or unavailable discovery support
leaves native navigation available and reports the discovery failure.

## Verification

- Windows x86 NMake Debug, warnings as errors: all 42 tests passed, including
  automatic command loading and the extended host publication tests.
- WSL Debian GCC -m32 Debug: all 36 portable tests passed; the final mid-stream
  error regression rebuilt and passed in the fingerprint target.
- Windows x86 Release adapter rebuilt; exactly six required undecorated exports.
- SHA-256 standard empty/abc/multiblock/million-a vectors, exact-size and exceeded
  caps, initial/mid-stream I/O failure, unchanged source bytes.
- Link identity/direction, fingerprint mismatch, immutable retained data,
  unlinked/empty discovery and global budgets. Existing model/map/geometry
  mutation tests remain; host publication additionally covers deferred invalidation
  during a trace, replacement generation, failed replacement and native fallback.
- Actual NAV command loading uses synthetic BSP/NAV fixtures and verifies
  unchanged bytes plus BSP-size mismatch rejection. No real map files were changed.

The P3-06 discovery/enrichment implementation slice has offline evidence.
First-class up/down motion, contact observation, bounded re-acquisition and
dispatch integration remain open. A* may now select ladder edges; until that
motion slice is connected, unsupported ladder execution fails explicitly.
Live ladder validation remains post-Finish. Phase 3 and project Finish are open.
