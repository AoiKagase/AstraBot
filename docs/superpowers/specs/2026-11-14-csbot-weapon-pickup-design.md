# CSBot contact weapon exchange

Authorization: commit/merge obstacle work and proceed to the next unimplemented item. Pinned behavioral evidence: ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_weapon.cpp OnTouchingWeapon, weapons.cpp CWeaponBox::Touch, player.cpp DropPrimary/GetCSModelName. Reference sources remain read-only; implementation is independent.

## Behavior
On a grounded weaponbox Touch callback (box first), a living managed bot exchanges its existing primary only when a different dropped primary precedes it in the ordered profile preferences and no enemy has been visible for at least 2.5 seconds. Non-primary preferences are ignored. Missing primary, unknown model and unsupported shield/custom contents leave GameDLL pickup alone. No remote weapon scan or movement target is introduced.

## Public boundary
Infer ordinary primary contents from exact standard world models and carried primary from public entvars.weapons. Ambiguous carried inventory skips exchange. Issue synchronous drop with the current primary classname before the original GameDLL Touch continues, even when a pistol is active. Do not synthesize inventory or pickup success. Public VIP ScoreAttrib can reject VIPs; no private offsets or API dependencies. Existing configuration, actor identity and lifecycle guards remain in force.

## Verification
Regression tests exercise ordered preferences and the 2.5-second threshold, actual pre-Touch command arguments, callback direction, unsupported models, airborne boxes, automatic empty-slot pickup, active pistol, recent visible enemies, and subsequent public inventory changes. Complete Windows x86 build/CTest and PE gate; Linux x86 build/CTest and ELF gate. Native map acceptance remains separate.
