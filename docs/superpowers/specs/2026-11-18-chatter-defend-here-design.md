# Unit09 CSBot DefendHere

The reference is ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`. The base is AstraBot `6a252a61190a4a090a0d6d66245bb496353b267d`.

- Capture the speaker's location by value when the native planting report is created. Keep the statement's audible Place, critical topic, ten-second lifetime, and round-over gate. Check the native MoveTo, Idle, and Follow production paths.
- Route successful statement completion through the established teammate delivery and identity checks. The receiver runs Rogue, Busy, and then a Place query at the captured position. A suitable hiding spot in that Place attempts Hide with HOLD_POSITION and adds no speech. Undefined Place or an empty candidate list attempts nearby Hide, then admits Affirmative through generic Say.
- Select from the NAV spot tiers in native order. Sniper inventory enables ideal and good sniper spots, then cover. Preserve spot order, the 256-spot cap, the closest alive player and hostage occupancy rules, and the random draw bounds. Nearby search uses the reference's 750-unit surrounds, including available ladder neighbors, and falls back to the receiver origin if no spot is available.
- Make the selected position a real Hide route using FASTEST. Match entry and Idle-on-failure behavior, keep qualified state ownership through reset and cancellation, and verify movement commands in the plugin fixture.

Reference locations include `cs_bot_chatter.cpp` 179–207 and 1928–1938, `cs_bot_statemachine.cpp` 117–205, `cs_bot_hide.cpp` 33–113, and `nav_area.cpp` 3121–3296 and 3347–3387.
