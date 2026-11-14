# Knife Enemy Fire Run Plan

> Use superpowers:executing-plans inline.

Spec: docs/superpowers/specs/2026-11-15-csbot-knife-enemy-fire-run-design.md
Base: 3452978. Existing checkout/caches; preserve unrelated files. No push/deploy.

## Review Focus
Direct event delivery must precede hearing dedup/range/NAV gates. Current-enemy serial, observer generation, clock rollback and exact deadline. Timer survives weapon switch/enemy loss, cancels lifecycle. Walking override must affect cached and full commands without forcing movement or defeating crouch/freeze. Unknown/reload samples must not create firing receipts.

1. [x] Core timer, independent failing tests then implementation; CMake/manifest.
2. [x] Retained public enemy identity and direct observation port; real runtime tests RED then GREEN; all reset paths/ownership fixture; source serial carried by existing threat lookup.
3. [x] Fresh final read-only review;2Important fixed in one RED→GREEN pass; Windows110/110, Linux108/109 known baseline; serialized full suites, independent artifacts and source identity confirmed. Explicit-path commit/local FF and delivered byte identity required below.

Review findings: cached walking must yield to an enemy event on the very next command; extra recognition lookup must not alter frozen call order. Both fixed; no deferred minors. Test40ms interval explicitly verifies30Hz command dispatch without a10Hz full update. Old early-walk-cap control reproduced RED, fixed source restored exactly.

Rulings: retain shared checkout/caches and generated evidence (mixing risk checked by exact paths and hashes); only proven fire/dryfire receipts (native reload/knife/grenade/friend-death behavior remains unavailable); live acceptance remains open (runtime discrepancy possible); parent executes full/artifact gates after source review; known Linux combat_policy remains unresolved and overall goal active.
