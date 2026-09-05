# P2-03 Area Records Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (\`- [ ]\`) syntax for tracking.

**Goal:** Decode all serialized NAV area and tactical static records for versions 1–5 into a bounded, SDK-free, transactional \`NavAreaReader\` result.

**Architecture:** Keep \`NavFileReader\` responsible for the completed file header. Add focused model value types and a separate \`NavAreaReader\` that consumes the post-header \`ByteView\`, returns \`NavAreaBlock{areas, bytesConsumed}\`, and does not perform P2-04 graph validation. Count-driven allocation is checked against explicit per-section and total limits before allocation.

**Tech Stack:** C++17, CMake/NMake, MSVC 2026 Community, existing \`ByteReader\`, assert-based CTest binaries, FocalSpan.

**Spec:** \`docs/superpowers/specs/2026-09-05-p203-area-records-design.md\`

## Global Constraints

- Preserve the SDK-free portable NAV boundary; do not add GoldSrc, Metamod-P, ReAPI, GameDLL, or upstream source/assets.
- Decode little-endian scalars through \`ByteReader\`; never use native struct reads.
- Preserve raw attributes, traversal bytes, directions, hiding flags, and encounter \`t\` bytes; semantic validation belongs to P2-04.
- Consume v1/v2 legacy encounter records completely but do not publish them as modern ID-based paths.
- Publish either a complete \`NavAreaBlock\` or no value.
- Do not implement snapshot publication, cross-reference resolution, geometry semantic validation, queries, ladders, Linux, or live-server checks.
- Preserve \`.focalspan.json\`, \`.serena/\`, and unrelated changes; never stage \`.focalspan/\` or \`.focalspan.json\`.

---

### Task 1: Add area model types and reader contract

**Files:**
- Create: \`src/nav/model/area_records.hpp\`
- Create: \`src/nav/io/area_reader.hpp\`
- Create: \`src/nav/io/area_reader.cpp\`
- Test: \`tests/nav/area_reader_tests.cpp\`
- Modify: \`CMakeLists.txt\`

**Interfaces:**
- Consumes: \`NavAreaId\`, \`NavVector3\`, \`NavExtent\`, \`NavVersion\`, \`ByteView\`, \`ReadResult\`.
- Produces: \`NavCardinalDirection\`, \`NavHidingSpot\`, \`NavApproachRecord\`, \`NavEncounterSpot\`, \`NavEncounterRecord\`, \`NavAreaRecord\`, \`NavAreaBlock\`, \`NavAreaReadLimits\`, and \`NavAreaReader::read\`.

- [ ] **Step 1: Write the failing contract test**

Add a test-only compile/use section that constructs the public records and calls the wished-for API:

~~~cpp
const NavAreaRecord area{
    NavAreaId{7U}, 0x05U, NavExtent{}, {}, {}, {}, {},
    std::optional<std::uint16_t>{3U},
};
assert(area.id == NavAreaId{7U});
assert(area.place.has_value() && *area.place == 3U);

const auto result = NavAreaReader::read(
    ByteView{nullptr, 0U}, NavVersion::V1, 0U, NavAreaReadLimits{});
assert(!result);
~~~

Register the test as \`astrabot.nav.area_records\`, linked to \`astrabot_nav\`.

- [ ] **Step 2: Run RED**

~~~powershell
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
cmd /c ("call ""$vs"" -arch=x64 -host_arch=x64 && cmake -S . -B build-p203 -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON && cmake --build build-p203 --target astrabot_nav_area_tests")
~~~

Expected: compile failure because the new model/header/API does not exist. Do not add production code before observing this failure.

- [ ] **Step 3: Add the minimal model and reader declarations**

Define these fields in \`area_records.hpp\`:

~~~cpp
enum class NavCardinalDirection : std::uint8_t { North = 0, East, South, West };
struct NavHidingSpot {
    std::optional<std::uint32_t> id{};
    NavVector3 position{};
    std::optional<std::uint8_t> flags{};
};
struct NavApproachRecord {
    NavAreaId here{}, previous{}, next{};
    std::uint8_t previousToHereHow{0}, hereToNextHow{0};
};
struct NavEncounterSpot {
    std::uint32_t hidingSpotId{0};
    std::uint8_t t{0};
};
struct NavEncounterRecord {
    NavAreaId from{}, to{};
    std::uint8_t fromDirection{0}, toDirection{0};
    std::vector<NavEncounterSpot> spots{};
};
struct NavAreaRecord {
    NavAreaId id{};
    std::uint8_t attributes{0};
    NavExtent extent{};
    std::array<std::vector<NavAreaId>, 4> connections{};
    std::vector<NavHidingSpot> hidingSpots{};
    std::vector<NavApproachRecord> approaches{};
    std::vector<NavEncounterRecord> encounters{};
    std::optional<std::uint16_t> place{};
};
~~~

Define \`NavAreaBlock\` with \`std::vector<NavAreaRecord> areas\` and \`std::size_t bytesConsumed\`. Define \`NavAreaReadLimits\` with \`maxAreas\`, \`maxConnectionsPerDirection\`, \`maxHidingSpotsPerArea\`, \`maxApproachesPerArea\`, \`maxEncountersPerArea\`, \`maxEncounterSpotsPerPath\`, \`maxTotalConnections\`, \`maxTotalHidingSpots\`, \`maxTotalApproaches\`, \`maxTotalEncounters\`, and \`maxTotalEncounterSpots\`, all \`std::uint32_t\`.

Declare:

~~~cpp
static diagnostics::ReadResult<model::NavAreaBlock> read(
    ByteView bytes,
    model::NavVersion version,
    std::uint32_t areaCount,
    const NavAreaReadLimits& limits) noexcept;
~~~

- [ ] **Step 4: Run GREEN**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

Expected: the contract test builds and passes; the minimal reader returns
\`InvalidValue\` for the zero-area call.

- [ ] **Step 5: Commit**

~~~powershell
git add -- CMakeLists.txt src/nav/model/area_records.hpp src/nav/io/area_reader.hpp tests/nav/area_reader_tests.cpp
git diff --cached --check
git commit -m "feat: define nav area record reader contract"
~~~

### Task 2: Decode area base fields and directional connections

**Files:**
- Modify: \`src/nav/io/area_reader.cpp\`
- Modify: \`tests/nav/area_reader_tests.cpp\`

- [ ] **Step 1: Write failing tests**

Add an independently generated v1 payload for one area: ID \`7\`, attributes \`0x05\`, eight finite extent scalars, four connection counts, and connection IDs \`{10}\`, \`{20,21}\`, \`{}\`, \`{40}\`. Call \`NavAreaReader::read\` with \`areaCount=1\`, \`maxAreas=1\`, \`maxConnectionsPerDirection=2\`, and \`maxTotalConnections=4\). Assert every decoded value and \`bytesConsumed==payload.size()\`.

Add truncation cases at ID, attributes, each extent scalar, each directional count, and each connection ID. Assert \`EndOfInput\`, exact field/offset, and no returned value.

- [ ] **Step 2: Run RED**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

Expected: base/connection assertions fail because \`NavAreaReader::read\` is not implemented.

- [ ] **Step 3: Implement minimal decoding**

Use \`ByteReader::readU32LE\`, \`readU8\`, and \`readF32LE\` in this order: ID, attributes, \`northWest.x/y/z\`, \`southEast.x/y/z\`, \`northEastZ\`, \`southWestZ\`, then four \`uint32\` connection counts followed by \`uint32\` area IDs. Check \`areaCount\` before \`reserve\`; reject zero with \`InvalidValue\`. Check section counts before reserve and total counters before growth. Store IDs as \`NavAreaId\` without resolving them.

Use \`NavRecord::Area\), \`NavField::AreaId\`, \`Attributes\`, \`NorthWestExtent\`, \`SouthEastExtent\`, \`NorthEastZ\`, \`SouthWestZ\`, \`ConnectionCount\`, and \`ConnectionAreaId\`.

- [ ] **Step 4: Run GREEN**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

- [ ] **Step 5: Commit**

~~~powershell
git add -- src/nav/io/area_reader.cpp tests/nav/area_reader_tests.cpp
git diff --cached --check
git commit -m "feat: decode nav area base and connections"
~~~

### Task 3: Decode versioned hiding spots and approaches

**Files:**
- Modify: \`src/nav/io/area_reader.cpp\`
- Modify: \`tests/nav/area_reader_tests.cpp\`

- [ ] **Step 1: Write failing tests**

Add v1 data with one hiding position and one approach. Assert the hiding spot has no ID/flags and the approach preserves here/previous/next IDs and both raw traversal bytes.

Add v2 data with one object hiding spot (\`id=101\`, finite position, \`flags=0x03\`). Add truncations at hiding count, every v1 position scalar, v2 ID/position/flags, approach count, each approach ID, and each traversal byte.

- [ ] **Step 2: Run RED**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

Expected: version-specific assertions fail while the base/connection tests remain green.

- [ ] **Step 3: Implement minimal decoding**

After connections, read a \`uint8\` hiding count. For v1 read three floats and create \`NavHidingSpot{nullopt, position, nullopt}\). For v2+ read \`uint32 id\`, three floats, and \`uint8 flags\). Then read the \`uint8\` approach count and per record read here ID, previous ID, previous traversal byte, next ID, and next traversal byte. Check per-area and total limits before reserve/growth.

Use \`HidingSpotCount\`, \`HidingSpotId\`, \`HidingSpotFlags\`, \`ApproachCount\`, \`ApproachAreaId\), and \`ApproachTraversal\` with the corresponding record contexts.

- [ ] **Step 4: Run GREEN**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

- [ ] **Step 5: Commit**

~~~powershell
git add -- src/nav/io/area_reader.cpp tests/nav/area_reader_tests.cpp
git diff --cached --check
git commit -m "feat: decode nav hiding and approach records"
~~~

### Task 4: Decode encounter records, legacy consumption, and v5 Place

**Files:**
- Modify: \`src/nav/io/area_reader.cpp\`
- Modify: \`tests/nav/area_reader_tests.cpp\`
- Modify: \`tests/nav/fixtures/README.md\`

- [ ] **Step 1: Write failing tests**

Add a v1 legacy encounter with two IDs, two endpoint vectors, one legacy spot count, and one spot vector plus float. Assert \`encounters.empty()\) and \`bytesConsumed\) reaches the following sentinel.

Add a v3 encounter with from ID/direction, to ID/direction, two \`(hidingSpotId,t)\` entries, and a v5 area with Place entry \`4\). Add v4 data without Place and verify no Place is present. Add truncation tests for every encounter and Place field plus per-area/total limit failures.

- [ ] **Step 2: Run RED**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

Expected: encounter, legacy-consumption, and Place assertions fail while earlier tests remain green.

- [ ] **Step 3: Implement encounter/Place decoding**

Read encounter count as \`uint32\`. For v1/v2 consume per path: from ID, to ID, three floats for each endpoint, \`uint8 spotCount\), and for each spot three floats plus one float. Do not allocate or publish legacy path values. For v3+ read from ID/direction, to ID/direction, \`uint8 spotCount\), and each \`uint32 hidingSpotId\` plus \`uint8 t\`.

For v5 only, read one \`uint16\` Place entry. Use \`EncounterCount\`, \`EncounterAreaId\`, \`EncounterDirection\`, \`EncounterSpotCount\`, \`EncounterSpotId\`, \`EncounterSpotT\), and \`Place\) at field-start offsets.

- [ ] **Step 4: Run GREEN**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

- [ ] **Step 5: Commit**

~~~powershell
git add -- src/nav/io/area_reader.cpp tests/nav/area_reader_tests.cpp tests/nav/fixtures/README.md
git diff --cached --check
git commit -m "feat: decode nav encounter and place records"
~~~

### Task 5: Complete transactional limits and integration

**Files:**
- Modify: \`src/nav/io/area_reader.cpp\`
- Modify: \`tests/nav/area_reader_tests.cpp\`
- Modify: \`CMakeLists.txt\`
- Modify: \`tests/nav/fixtures/README.md\`

- [ ] **Step 1: Add remaining limit tests**

Cover zero area count, area count over \`maxAreas\), each per-section limit, each total counter limit, \`ByteView{nullptr, nonzero}\`, non-finite geometry propagated by \`ByteReader\), and a failure after an earlier area decoded. Every failure asserts no returned block and the expected diagnostic tuple.

- [ ] **Step 2: Run RED**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

Expected: any uncovered limit or transactional assertion fails before the final refactor.

- [ ] **Step 3: Complete the transactional implementation**

Keep all decoded areas in a local \`NavAreaBlock\`, reserve only after checking caps, use \`std::uint64_t\` total counters, catch \`std::bad_alloc\), and construct the success \`ReadResult\` only after assigning \`bytesConsumed = reader.offset()\`. Do not add P2-04 semantic validation.

Update \`tests/nav/fixtures/README.md\) with the fixture generator path, independent field provenance, and the statement that no upstream source/map bytes are included.

- [ ] **Step 4: Run focused GREEN**

~~~powershell
cmake --build build-p203 --target astrabot_nav_area_tests
ctest --test-dir build-p203 -R astrabot.nav.area_records --output-on-failure
~~~

- [ ] **Step 5: Run portable Debug/NMake/CTest**

~~~powershell
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
cmd /c ("call ""$vs"" -arch=x64 -host_arch=x64 && cmake -S . -B build-p203 -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON && cmake --build build-p203 && ctest --test-dir build-p203 --output-on-failure")
~~~

Expected: all existing tests plus \`astrabot.nav.area_records\) pass; total 7 tests.

- [ ] **Step 6: Run MSVC /analyze**

~~~powershell
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
cmd /c ("call ""$vs"" -arch=x64 -host_arch=x64 && cmake -S . -B build-p203-analyze -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON ""-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /analyze"" && cmake --build build-p203-analyze && ctest --test-dir build-p203-analyze --output-on-failure")
~~~

Expected: build and CTest succeed with \`/analyze\), \`/EHsc\), \`/W4\), and \`/WX\).

- [ ] **Step 7: Refresh FocalSpan and query**

Run sequentially:

~~~powershell
focalspan update --root .
focalspan status --json
focalspan -- "What is the P2-03 NavAreaReader wire order, version-specific hiding/encounter behavior, allocation limit contract, and bytesConsumed boundary?"
~~~

Expected: \`ready=true\), \`index_fresh=true\), and the query returns the new model/reader symbols.

- [ ] **Step 8: Review, stage, check, and commit implementation**

~~~powershell
git diff --check
git status --short
git add -- CMakeLists.txt src/nav/io/area_reader.cpp src/nav/io/area_reader.hpp src/nav/model/area_records.hpp tests/nav/area_reader_tests.cpp tests/nav/fixtures/README.md
git diff --cached --check
git commit -m "feat: decode nav area and tactical records"
git log -1 --oneline
git status --short
~~~

Do not stage \`.focalspan/\`, \`.focalspan.json\`, \`.serena/\`, build outputs, or unrelated changes.
