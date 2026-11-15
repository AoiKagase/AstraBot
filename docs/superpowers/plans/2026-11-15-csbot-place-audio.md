# CSBot Place audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans inline; one final fresh source reviewer, one Important/Critical RED→GREEN pass, no re-review.

**Goal:** Complete02 default/profile Place audio catalogs and native shared repeat policy.
**Architecture:** Extend existing SDK-free catalogs with typed Place metadata and variant groups; source IDs/occurrences and profile validation prevent namespace/ID aliasing. Fixed64entry explicit-reset history is shared by03; mutation generations fence callback publication.
**Tech Stack:** C++14, Windows/Linux x86 public plugin.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-place-audio-design.md

## Global Constraints
No private/SDK Core dependencies/new exports/assets/user overwrite/frozen regeneration/push/deployment. Keep shared checkout/caches/evidence, explicit paths/fingerprints and pending03 fixture. Full goal and remaining units active; native shuffle tape21, outer bank/transport03 remain open.

## Review Focus
- Default names/source IDs, same-name Chatter versus Place and duplicate occurrences never authorize the wrong profile samples or metadata flags.
- Selection criteria/cursors/filtering/shuffle work for Places independently of fixed phrases, missing banks and exact default IDs; no bank0 fallback is invented.
- Reentrant loader/filter/RNG callbacks cannot overwrite fresh catalog data or invalidate an active traversal; failed operations preserve published data.
- Shared64entry history retains denied attempts, exact20 boundary, full-table behavior and explicit map/round reset; clock rewind does not fabricate a reset.
- Existing bank initialization and supported fixed phrases retain valid-native behavior; source-correct profile fixture definitions instead of weakening default validation.

### Task1: Place catalog RED→GREEN
Files: chatter_catalog.hpp, chatter_voice.hpp, tests/chatter_places_tests.cpp, CMakeLists.txt/source manifest; runtime behavior voice.cpp profile load passes definitions.
Interfaces: ChatterPlaceDefinition{id,name,choice,important,occurrence}; ChatterRadioCatalog.findPlace(name),place(id),definedPhraseName(name),nameToId(name,beforeId),hasPlaceBefore(beforeId). Typed native criteria IDs use a name-sorted factual fallback lookup only if no Place is visible. ChatterVoiceCatalog.loadText(data,len,definitions=nullptr),hasPlace(id,definitions),selectPlace(id,definitions,count,criteria),selectAtPlace(phrase,count,id),selectPlaceAt(id,definitions,count,criteriaId), mutation generation accessor for stale candidate guards. Existing fixed API remains.
- [x] Existing API RED: load Place Arena arena.wav and Help control; retainAvailable sees Help but not Arena today.
- [x] Named criteria existing-API RED (--criteria): unknown becomes UNDEFINED, forward/default prefix and Chatter/Place shadowing respect native ID binding; fallback factual IDs activate only before any Place definition.
- [x] Add tests/API stubs then implement source-ordered IDs/typed metadata and profile namespace validation; assert default flags cannot be overridden, profile-only Place rejected, duplicate default/profile behavior and atomic failures.
- [x] Place speakables use existing selection criteria/cursor contract, path/capacity validation, asset filtering and Place-first shuffle. Test RNG type/range/order and callback replacement, then minimal implementation GREEN; no samples packaged.

### Task2: Native place repeat policy RED→GREEN
Files: create chatter_place_history.hpp and tests/chatter_place_history_tests.cpp with independent Core test target.
Interfaces: ChatterPlaceHistory.allow(uint32_t metadataId,float now)->bool,reset(). No actor/team key;03 owns one instance and reset boundary.
- [x] Write history tests first: first/exact20/denied-attempt timing,64tracked+65th untracked, explicit reset, clock rewind, invalid time/ID. Stub semantic RED then implement fixed64native policy GREEN.

### Task3: Review, full gates and local integration
- [x] Source-only builder check questions resolved before behavior changes; one fresh final whole-unit source review after GREEN, Important/Critical reproduced/fixed once.
- [x] Correct VS18 Hostx86/x86 and WSL full directory builds, serialized full suites, exact7 PE/ELF, manifest/source/artifact/user identity. Known unchanged Linux combat_policy baseline remains22.
- [x] Update parity/backlog02 complete/03 onward open; explicit owned stage/commit/local FF master, postmerge refs/hashes/RESULT.json; no runtime Place delivery/full-goal acceptance claim.
