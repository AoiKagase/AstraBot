// SPDX-License-Identifier: MPL-2.0
// Include after the fake-engine fixture and NAV evidence helpers.
#pragma once

namespace p12_map_nav_test {
using namespace astrabot;
std::string gameDirectory;
std::string activeMap;
std::size_t directoryReads{};

void directory(char* output) {
    ++directoryReads;
    assert(gameDirectory.size() < 1024);
    std::memcpy(output, gameDirectory.c_str(), gameDirectory.size() + 1);
}
const char* mapString(int index) { return index == 20 ? activeMap.c_str() : ""; }

std::size_t autoLogs() {
    std::size_t count = 0;
    for (const auto& line : gNavOutput)
        if (line.find("astrabot nav_auto ") == 0) ++count;
    return count;
}

void run() {
    using Reason = adapter::metamod::MapNavLoadReason;
    const auto root = std::filesystem::temp_directory_path() /
        ("astrabot-p12-map-nav-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(root));
    assert(std::filesystem::create_directory(root / "maps"));
    gameDirectory = root.generic_string();
    const auto bytes = evidence::fixture(5, false).bytes;
    const auto writeNav = [&](const char* map) {
        std::ofstream file(root / "maps" / (std::string(map) + ".nav"), std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        assert(file.good());
    };
    writeNav("first_map");
    writeNav("next-map");
    {
        std::ofstream file(root / "maps" / "invalid.nav", std::ios::binary);
        file << "not a NAV";
        assert(file.good());
    }

    Fixture fixture;
    fixture.engine.pfnGetGameDir = &directory;
    fixture.engine.pfnSzFromIndex = &mapString;
    fixture.engineGlobals.mapname = 20;
    activeMap = "first_map";
    attach(fixture); // activate() bypasses LifecycleCoordinator::serverActivate.
    auto& owner = adapter::metamod::lifecycleCoordinator();
    DLL_FUNCTIONS hooks{};
    int version = INTERFACE_VERSION;
    assert(GetEntityAPI2(&hooks, &version) != 0);
    const auto activateMap = [&]() { hooks.pfnServerActivate(nullptr, 0, 32); };
    const auto deactivateMap = [&]() { hooks.pfnServerDeactivate(); };
    gNavOutput.clear();
    activateMap();
    const auto first = owner.registry().mapGeneration();
    assert(owner.mapNavLoadStatus().map == first);
    assert(owner.mapNavLoadStatus().reason == Reason::Ready);
    assert(owner.navConsole().distributionTopology());
    assert(std::string(owner.mapNavLoadStatus().path.data()) ==
           gameDirectory + "/maps/first_map.nav");
    assert(autoLogs() == 1);
    assert(fixture.createCalls == 0); // NAV ready before first bot/StartFrame.
    const auto reads = directoryReads;
    activateMap(); // Duplicate engine notification is not a retry.
    assert(directoryReads == reads && autoLogs() == 1);

    deactivateMap();
    assert(!owner.navConsole().distributionTopology());
    activeMap = "next-map";
    activateMap();
    assert(owner.registry().mapGeneration() != first);
    assert(owner.mapNavLoadStatus().reason == Reason::Ready);
    assert(owner.navConsole().distributionTopology());
    assert(autoLogs() == 2);

    deactivateMap();
    activeMap = "missing";
    activateMap();
    assert(owner.mapNavLoadStatus().reason == Reason::LoadFailed);
    assert(!owner.navConsole().distributionTopology());
    const auto failures = autoLogs();
    // StartFrame must not try the missing file repeatedly.
    hooks.pfnStartFrame(); hooks.pfnStartFrame();
    assert(autoLogs() == failures);
    assert(owner.mapNavLoadStatus().reason == Reason::LoadFailed);
    const auto manual = (root / "maps" / "first_map.nav").generic_string();
    runNav({"astrabot_nav_load", manual.c_str()});
    assert(owner.navConsole().distributionTopology());
    hooks.pfnStartFrame();
    assert(autoLogs() == failures);

    deactivateMap();
    activeMap = "invalid";
    activateMap();
    assert(owner.mapNavLoadStatus().reason == Reason::LoadFailed);
    assert(!owner.navConsole().distributionTopology());

    deactivateMap();
    activeMap = "../first_map";
    directoryReads = 0;
    activateMap();
    assert(owner.mapNavLoadStatus().reason == Reason::InvalidMapName);
    assert(directoryReads == 0);
    assert(!owner.navConsole().distributionTopology());

    deactivateMap();
    activeMap.clear();
    activateMap();
    assert(owner.mapNavLoadStatus().reason == Reason::MissingMapName);
    assert(!owner.navConsole().distributionTopology());
    detach();
    std::filesystem::remove_all(root);
}
} // namespace p12_map_nav_test
