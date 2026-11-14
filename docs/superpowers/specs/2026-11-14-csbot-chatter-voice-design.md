# CSBot chatter voice transport

Authorization: advance the next missing item while matching CSBot. Base aa6abd5; behavioral oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5 is read-only. Implement independently, using host-owned assets without copying or distributing them.

## Scope and approach
Existing outgoing scheduler already supplies eight chatter concepts. Add voice playback for those concepts: normal/minimal choose matching host BotChatter.db speakables, radio uses existing public menu transport, off remains silent. Missing voice/bank/transport falls back to the default bank's explicit radio equivalent. VoiceBank and VoicePitch inherit from BotProfile defaults/templates. Reuse scheduler rather than introducing a parallel statement/event generator. Other CSBot statement concepts/composition remain future work.

Parse BaseDir, Count (Many=4), Place, Radio, Important and End. Select cyclically through round-shuffled variants, honoring visible enemy count and current read-only NAV place. Resolve duration using public GetApproxWavePlayLen (milliseconds), with reference 1-second fallback; team busy duration is audio duration+0.1 seconds. Engine shuffle is explicit approximation of reference CRT random_shuffle consumption, not tape parity.

Send the reference SendAudio payload (speaker byte, filename string, pitch short) to same-team connected clients, including dead teammates. Honor observed public ignore/accept-radio TextMsg receipts; unknown private ignore state on late attachment remains a documented public-data limit. Observers may be included only when public observer entvars identify a same-team target. No enemies or broadcasts for the audio. Optional BotVoice feedback uses active/speaker bytes and ends after duration+1 second, on death/removal/round/map changes. Feedback expiry must work even with stopped/disabled Bot AI.

## Limits and verification
No engine/private ABI additions or SDK dependencies, no private player fields, no per-frame file loads or world scans. Bound database/token/sample/bank sizes. Unknown assets/criteria never invent a matching clip. Public message emission is not proof of audible client playback. Adapter tests pin actual messages, recipients, pitch, time, stop/death/round behavior and fallback. Pure tests cover parsing/filtering/order and malformed data; profile tests cover inherited VoiceBank. Complete both x86 builds and sequential CTests plus separate PE/ELF gates. Keep the pre-existing Linux combat_policy failure separate.
