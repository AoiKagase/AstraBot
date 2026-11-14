# CSBot obstacle interaction

User authorization: implement doors/breakables to match pinned CSBot b0889847fe6d03898be88acc9e366660efb40ab5.

## Approach
Keep the existing public RunPlayerMove movement path. CSBot assumes func_door and func_door_rotating open on touch (game_shared/bot/nav.h); it does not seek remote buttons. Add contact-driven func_breakable handling rather than global entity scanning or a door-solving AI. Reference code is read-only behavioral evidence, never copied.

## Behavior
Non-player DAMAGE_YES func_breakable contacts, outside the attack overlay, request a high-priority center look and primary attack. When a local path exists, use the reference's conservative endpoint-side box rejection, with local movement goal raised by 36 units. Preserve its deliberate approximate intersection behavior. Other contacts do not create use/fire commands.
Look uses the existing non-attacking aim spring at the 30Hz command boundary; the reference look lasts 0.2 seconds after alignment within 5 degrees. Contact attack is command-local/pending through the current full update, not an enemy target. A held grenade selects an allowed carried primary, then pistol, then knife; no obstacle attack is sent while the active weapon is still a grenade.
Material 7 (unbreakable glass) is learned only from public map KeyValue callbacks, indexed by edict/serial and cleared on map teardown. Missing material provenance is unknown and conservatively skips shooting; no pdata or private layouts.

## Integration and safety
Behavior owns contact/look state; plugin delegates KeyValue/Touch; command execution consumes a narrow behavior modifier before freeze gate evaluation. Preserve movement by reprojecting its world direction when the look changes. Reset contact state on actor/map/round changes, death, removal or entity reuse. No per-frame entity scan or synchronous diagnostic spam. Leave existing player avoidance and objective/combat decisions intact.

## Acceptance
Adapter regressions must cover actual Touch-to-RunPlayerMove fire/aim, both callback orders, side-of-path rejection, no-path acceptance, ordinary/rotating door pass-through without use/fire, unbreakable/trigger-only/unknown material rejection, enemy attack precedence, grenade switching, expired/destroyed/reused targets, lifecycle invalidation and frozen commands. Windows x86 full build/CTest and separate PE validation; Linux x86 build/CTest/ELF where available. Native obstacle-map acceptance remains separate.
