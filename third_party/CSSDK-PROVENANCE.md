# Counter-Strike SDK snapshot

Copied from ReGameDLL_CS/regamedll/extra/cssdk on 2026-09-09 at the user's
request. Source checkout HEAD: b0889847fe6d03898be88acc9e366660efb40ab5.
The local source tree, rather than a downloaded release, is the source of this
snapshot. All 179 files, including LICENSE and original notices, are retained.

The SDK is third-party code and is not relicensed as MPL-2.0. Its LICENSE file
contains GPL-3.0; individual headers such as dlls/weapontype.h state
GPL-2.0-or-later with an HL Engine exception. Preserve these original terms.
Distribution of binaries incorporating this SDK must account for those terms.

Only adapter targets receive the third_party include root. Use explicit
cssdk/dlls/... paths for CS declarations; do not add CSSDK's engine/common
directories ahead of the pinned Metamod-P SDK. Core and portable NAV remain
SDK-independent. No GameDLL private object layout or implementation is linked.

adapter/cstrike/weapon_protocol.hpp consumes WeaponIdType and the prediction
flags directly, and centralizes AstraBot weapon classification and command
mapping. Classification is bot policy, not a GameDLL API. The prediction
buffer size and no-clip sentinel remain named ABI constants because weapons.h
pulls private GameDLL classes into the translation unit.

JoinState's cstrike::Team is a two-value local request type, not a CS team
number. Do not cast it to the SDK TeamName enum.
