// SPDX-License-Identifier: MPL-2.0
#include "evidence/fuzz.hpp"
#include <cassert>
int main(int argc, char** argv) {
    evidence::configureErrors();
    evidence::Random random{0xA208};
    assert(random.next() == 2624892200U);
    assert(random.next() == 870617536U);
    assert(random.next() == 2588206318U);
    try {
        std::size_t count = 10000, first = 0;
        if (argc == 3 && std::string(argv[1]) == "--cases") count = std::stoul(argv[2]);
        else if (argc == 3 && std::string(argv[1]) == "--replay") {
            first = std::stoul(argv[2]); count = 1;
        } else if (argc != 1) return 2;
        evidence::check(count > 0 && count <= 100000 && first < 100000, "CLI range");
        evidence::runFuzz(count, {}, first);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fuzz failed: %s; see fuzz-artifacts/current.case and replay.txt\n", e.what());
        return 1;
    }
}
