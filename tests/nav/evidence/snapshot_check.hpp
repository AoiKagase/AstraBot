// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "fixture.hpp"
#include "check.hpp"
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace evidence {
// Canonical observable values, not addresses/padding or production validation helpers.
inline std::string snapshotState(const nav::model::NavMeshSnapshot& mesh) {
    std::ostringstream out;
    out << std::setprecision(9);
    const auto put = [&out](auto v) { out << v << ','; };
    const auto point = [&put](nav::model::NavVector3 p) {
        check(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z), "finite point");
        put(p.x); put(p.y); put(p.z);
    };
    const auto& h = mesh.header();
    const auto& areas = mesh.areas();
    const unsigned version = unsigned(h.version);
    check(version >= 1 && version <= 5, "version");
    check(h.areaCount == areas.size() && !areas.empty() && areas.size() <= 128, "area count");
    check(h.bspSize.has_value() == (version >= 4), "BSP presence");
    put(version); put(h.headerBytes); put(h.areaCount);
    if (h.bspSize) put(*h.bspSize);
    put(h.places.size());
    check(h.places.size() <= 64 && (version == 5 || h.places.empty()), "Place count");
    std::size_t placeBytes = 0;
    std::size_t logical = sizeof(nav::model::NavMeshSnapshot) +
        areas.size() * sizeof(nav::model::NavAreaRecord) +
        h.places.size() * sizeof(std::string);
    for (const auto& s : h.places) {
        check(!s.empty() && s.size() < 256 && s.find('\0') == std::string::npos, "Place bytes");
        placeBytes += s.size() + 1; put(s.size());
        for (unsigned char c : s) put(unsigned(c));
    }
    check(placeBytes <= 16384, "Place aggregate"); logical += placeBytes;
    std::set<std::uint32_t> ids, hidingIds;
    for (const auto& a : areas) {
        check(a.id.value != 0 && ids.insert(a.id.value).second, "unique area");
        for (const auto& s : a.hidingSpots) {
            check(s.id.has_value() == (version >= 2) &&
                  s.flags.has_value() == (version >= 2), "hiding version");
            if (s.id) check(*s.id != 0 && hidingIds.insert(*s.id).second, "unique hiding");
        }
    }
    const auto reference = [&ids](nav::model::NavAreaId id) {
        check(id.value != 0 && ids.count(id.value) == 1, "area reference");
    };
    std::size_t connections = 0, hiding = 0, approaches = 0, encounters = 0, spots = 0;
    for (const auto& a : areas) {
        put(a.id.value); put(unsigned(a.attributes));
        point(a.extent.northWest); point(a.extent.southEast);
        check(std::isfinite(a.extent.northEastZ) && std::isfinite(a.extent.southWestZ), "corner Z");
        put(a.extent.northEastZ); put(a.extent.southWestZ);
        check(a.extent.northWest.x < a.extent.southEast.x &&
              a.extent.northWest.y < a.extent.southEast.y, "rectangle");
        for (const auto& direction : a.connections) {
            check(direction.size() <= 128, "direction count");
            put(direction.size()); connections += direction.size();
            std::set<std::uint32_t> targets;
            for (const auto& c : direction) {
                reference(c.target);
                check(c.target != a.id && targets.insert(c.target.value).second, "connection");
                check(c.traversal == nav::model::NavTraversalKind::Walk, "static walk");
                put(c.target.value); put(unsigned(c.traversal));
            }
        }
        check(a.hidingSpots.size() <= 64 && a.approaches.size() <= 64 &&
              a.encounters.size() <= 64, "nested count");
        hiding += a.hidingSpots.size(); approaches += a.approaches.size();
        encounters += a.encounters.size();
        put(a.hidingSpots.size());
        for (const auto& s : a.hidingSpots) {
            if (s.id) { put(*s.id); put(unsigned(*s.flags)); }
            point(s.position);
        }
        put(a.approaches.size());
        for (const auto& p : a.approaches) {
            reference(p.here); reference(p.previous); reference(p.next);
            put(p.here.value); put(p.previous.value); put(p.next.value);
            put(unsigned(p.previousToHereHow)); put(unsigned(p.hereToNextHow));
        }
        check(version >= 3 || a.encounters.empty(), "legacy encounters discarded");
        put(a.encounters.size());
        for (const auto& e : a.encounters) {
            reference(e.from); reference(e.to);
            check(e.fromDirection <= 3 && e.toDirection <= 3 && e.spots.size() <= 64, "encounter");
            put(e.from.value); put(e.to.value); put(unsigned(e.fromDirection));
            put(unsigned(e.toDirection)); put(e.spots.size()); spots += e.spots.size();
            for (const auto& s : e.spots) {
                check(s.hidingSpotId != 0 && hidingIds.count(s.hidingSpotId) == 1, "spot reference");
                put(s.hidingSpotId); put(unsigned(s.t));
            }
        }
        check(a.place.has_value() == (version == 5), "area Place presence");
        if (a.place) { check(*a.place <= h.places.size(), "area Place"); put(*a.place); }
    }
    check(connections <= 4096 && hiding <= 4096 && approaches <= 4096 &&
          encounters <= 4096 && spots <= 4096, "aggregate counts");
    logical += connections * sizeof(nav::model::NavConnection) +
        hiding * sizeof(nav::model::NavHidingSpot) + approaches * sizeof(nav::model::NavApproachRecord) +
        encounters * sizeof(nav::model::NavEncounterRecord) + spots * sizeof(nav::model::NavEncounterSpot);
    check(logical <= 4194304, "logical snapshot budget");
    return out.str();
}
}
