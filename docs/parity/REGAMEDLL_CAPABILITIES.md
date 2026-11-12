# Optional ReGameDLL observations

The optional provider negotiates the official bundled `IReGameApi`, interface
`VRE_GAMEDLL_API_VERSION001`, major 5 and minor at least 3. It uses the SDK class
definition; it does not reconstruct vtables, inspect private player data, or
load another GameDLL. Windows probes already loaded `mp.dll`/`cs.dll`; Linux
enumerates loaded modules and opens their exact paths with `RTLD_NOLOAD`.

## Capability assessment

| Official API surface | Applicability to current observations |
| --- | --- |
| `GetWeaponInfo`, `GetWeaponSlot`, `GetItemInfo`, `GetAmmoInfo` | Weapon/ammo metadata, not the actor's selected weapon or current ammo |
| `GetGameRules` | Returns a GameDLL object; no private object member reads are permitted |
| `GetPlayerMove` | Shared movement context, not a stable per-actor weapon observation |
| `GetFuncs` | Entity creation, damage, hash and command helpers; no per-actor weapon getters |
| `GetHookchains` | Event interception is possible, but is not a getter for current private timers or accuracy |

Consequently **the implemented optional provider supplements zero fields**.
It reports `MetadataOnly` on successful negotiation and all observation
capabilities remain false. Public Vanilla messages continue to supply their
own capabilities. No metadata-only negotiation is presented as timer, reload,
accuracy, economy or weapon-state support. Observations remain byte-for-byte
unchanged, including unavailable fields and their provenance.

The bundled API is the compiled ABI source. The parent agent checked the
official `IReGameApi` getter surface at pinned reference
`b0889847fe6d03898be88acc9e366660efb40ab5`: version, functions, hookchains,
GameRules, weapon info, player movement, weapon slots, item and ammo metadata.
There is no per-actor weapon timer or accuracy getter in that surface.

## Integration and limits

At GameDLL attach / server activation, call `probeLoadedGameDll()`, report
`diagnostic()` with `majorVersion()` and `minorVersion()`, and install the
provider after the public message provider. Reset it on detach. A missing
factory/interface or version mismatch is a normal Vanilla fallback. No API
pointer is retained, so subsequent supplementation cannot dereference an
unloaded GameDLL. Linux must link the platform dynamic loader library.

`regamedll_observation_provider_tests` covers missing factory/interface,
major/minor mismatches, compatible versions, reset, loaded-module absence and
preservation of both known public fields and unknown supplemental fields.
Real-server API negotiation on Windows/Linux remains a separate live gate.

Additional hookchain-derived observations are **not implemented**, rather than
declared impossible. Private weapon timers and accuracy remain unavailable
through the inspected getter surface under the no-private-layout requirement.
