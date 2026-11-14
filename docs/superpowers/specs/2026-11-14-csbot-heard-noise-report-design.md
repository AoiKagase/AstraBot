# CSBot heard-noise report

Authorization: continue next unimplemented behavior following noise NAV/investigation. Base efb307c. Read-only behavioral oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5: InvestigateNoiseState::AttendCurrentNoise, BotChatterInterface::HeardNoise, CCSBot::HasNotSeenEnemyForLongTime, BotStatement::IsObsolete/IsRedundant and Speak. Independent implementation; host-owned phrases only.

## Approach
Extend the current outgoing scheduler/voice catalog with HeardNoise, retaining its bounded scheduling/transport. A parallel transport would duplicate recipient/cooldown controls; generic full statement composition is outside this slice. This is the builder choice under the user's continuing CSBot instruction. Existing public Full Update cadence remains.

On successful noise attendance after well-past-safe admission, require >30 seconds since last public visible enemy and priority other than High. Round-over blocks generation. Start a 20-second cooldown BEFORE the float(0,100)<33 draw, including failed chance and disabled chatter; unavailable RNG emits nothing. Use a named shared RNG request, reset on actor/map/round/clock changes. Store accepted report creation time and noise NAV place/revision, no hidden actor identity.

Queue through existing outgoing behavior; report expires only when now>created+5. Preserve original creation time while other statements/team speech delay it. Same HeardNoise topic is redundant only when both places undefined or both same defined place; differing/one-undefined places wait. Select host speakable by report place, not speaker place; unknown means ANY. Normal/minimal/radio/off follow existing metadata controls; missing assets/mapping never invent fallback. NAV replacement, death, map/round change or rollback cannot emit stale reports. Public observed Round_End/restart suppress until Round_Start via current tracker; late attach only knows observed notifications.

## Verification and limits
Pure tests pin chance bounds/cooldown/failed RNG/lifecycle and five-second/place-aware scheduler. Runtime tests attend a real admitted sound and inspect SendAudio/BotVoice, no reports for High sounds, cooldown and round suppression; voice criteria distinguish sound versus self place. Windows and Linux x86 full builds and sequential CTests; independent exact PE/ELF export gates. Frozen runtime characterization unchanged. Existing Linux combat_policy failure remains separately recorded. No deployment/live acceptance/full native chatter queue or RNG tape parity claim.
