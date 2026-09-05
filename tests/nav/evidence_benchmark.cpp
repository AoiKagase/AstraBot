// SPDX-License-Identifier: MPL-2.0
#include "evidence/scene.hpp"
#include "nav/query/spatial_index.hpp"
#include "nav/query/route_search.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
using namespace evidence;
namespace q = astrabot::nav::query;
template<class Operation>
void measure(std::uint32_t areas, const char* name, unsigned batch, Operation operation) {
    for (unsigned i = 0; i < 5; ++i) operation();
    std::vector<double> samples;
    for (unsigned i = 0; i < 31; ++i) {
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned j = 0; j < batch; ++j) operation();
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::micro>(end-begin).count() / batch);
    }
    std::sort(samples.begin(), samples.end());
    // nearest-rank p95: ceil(.95 * 31) - 1 == 29.
    std::printf("%u,%s,31,%u,%.3f,%.3f\n", areas, name, batch, samples[15], samples[29]);
}
int main(int argc, char** argv) {
    configureErrors();
    if (argc > 2) return 2;
    const std::filesystem::path dir = argc == 2 ? argv[1] : "benchmark-fixtures";
    std::filesystem::create_directories(dir);
    std::puts("areas,operation,samples,batch,median_us,p95_us");
    for (std::uint32_t count : {128U, 1024U}) {
        const auto original = scene(count);
        const auto f = encode(original);
        std::ofstream out(dir / ("scene-" + std::to_string(count) + ".nav"), std::ios::binary);
        out.write(reinterpret_cast<const char*>(f.bytes.data()), static_cast<std::streamsize>(f.bytes.size()));
        out.close(); check(bool(out), "benchmark fixture output");
        auto caps = limits();
        caps.maxInputBytes = 1048576; caps.header.maxAreas = count; caps.areas.maxAreas = count;
        const auto loaded = load(f.bytes, caps);
        check(bool(loaded), "benchmark load");
        const auto index = q::NavSpatialIndex::build(*loaded.value, {count, count*2U-1, 4194304});
        const auto graph = q::NavGraph::build(*loaded.value, {count, 4096, 4194304});
        check(bool(index) && bool(graph), "benchmark build");
        measure(count, "load", 1, [&] { check(bool(load(f.bytes, caps)), "timed load"); });
        unsigned queryNumber = 0;
        measure(count, "nearest", 100, [&] {
            const auto& patch = original.patches[(queryNumber++) % count];
            const auto r = (*index.value)->nearestGeometry({patch.x+1, patch.y+1, patch.z00}, {1000, 1000});
            check(bool(r) && r.value->has_value(), "timed nearest");
        });
        measure(count, "route", 1, [&] {
            const auto r = q::NavRouteSearch::search(**graph.value, {{1}, {count}, {count*4U, 4194304}, false});
            check(bool(r) && r.value->status == q::NavRouteStatus::Complete, "timed route");
        });
    }
}
