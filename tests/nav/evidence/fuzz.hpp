// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "snapshot_check.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

namespace evidence {
struct Random {
    std::uint32_t state;
    std::uint32_t next() {
        state ^= state << 13U; state ^= state >> 17U; state ^= state << 5U;
        return state;
    }
};
struct Mutation { Bytes bytes; std::string recipe; };
inline Mutation mutate(std::size_t index) {
    Random rng{0xA208U ^ (static_cast<std::uint32_t>(index) + 1U) * 0x9E3779B9U};
    const unsigned version = 1U + rng.next() % 5U;
    const bool full = (rng.next() & 1U) != 0;
    Mutation m{fixture(version, full).bytes, "v" + std::to_string(version) +
        (full ? "-full" : "-minimal")};
    const unsigned steps = 1U + rng.next() % 8U;
    for (unsigned step = 0; step < steps; ++step) {
        const unsigned op = rng.next() % 5U;
        const std::size_t pos = rng.next() % (m.bytes.size() + 1);
        const auto value = rng.next();
        m.recipe += ";" + std::to_string(op) + ":" + std::to_string(pos) +
                    ":" + std::to_string(value);
        if (op == 0 && pos < m.bytes.size())
            m.bytes[pos] ^= static_cast<std::uint8_t>(1U << (value % 8U));
        if (op == 1 && pos < m.bytes.size()) {
            constexpr std::uint32_t boundaries[]{0, 1, 0xFFFFFFFF, 0x7F800000, 0x7FC00000, 129};
            const auto bits = boundaries[value % 6U];
            const auto width = std::min<std::size_t>(4, m.bytes.size() - pos);
            for (std::size_t i = 0; i < width; ++i)
                m.bytes[pos + i] = static_cast<std::uint8_t>(bits >> (8U * i));
        }
        if (op == 2 && m.bytes.size() < 65536)
            m.bytes.insert(m.bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                           static_cast<std::uint8_t>(value));
        if (op == 3 && pos < m.bytes.size())
            m.bytes.erase(m.bytes.begin() + static_cast<std::ptrdiff_t>(pos));
        if (op == 4) m.bytes.resize(pos);
    }
    check(m.bytes.size() <= 65536, "mutation cap");
    return m;
}
inline void byteOracle(const Bytes& b, std::size_t index) {
    nav::io::ByteReader reader({b.data(), b.size()});
    Random rng{0xA208U + static_cast<std::uint32_t>(index)};
    std::size_t offset = 0;
    for (unsigned step = 0; step < 256; ++step) {
        const unsigned op = rng.next() % 5U;
        const std::size_t width = op == 0 ? 1U : op == 1 ? 2U : op < 4 ? 4U : rng.next() % 33U;
        const bool enough = width <= b.size() - offset;
        std::uint32_t bits = 0;
        if (enough && op < 4) {
            std::uint32_t scale = 1;
            for (std::size_t j = 0; j < width; ++j) {
                bits += b[offset + j] * scale; scale *= 256U;
            }
        }
        bool success = false; nav::diagnostics::NavError error;
        const auto scalar = [&](const auto& result, auto expected) {
            success = bool(result); error = result.error;
            if (success) check(*result.value == expected, "independent scalar decode");
            else check(!result.value, "reader partial result");
        };
        float real = 0; std::memcpy(&real, &bits, 4);
        if (op == 0) scalar(reader.readU8(), bits);
        if (op == 1) scalar(reader.readU16LE(), bits);
        if (op == 2) scalar(reader.readU32LE(), bits);
        if (op == 3) scalar(reader.readF32LE(), real);
        if (op == 4) {
            const auto r = reader.readBytes(width);
            success = bool(r); error = r.error;
            if (success) {
                check(r.value->size == width, "byte slice size");
                for (std::size_t j = 0; j < width; ++j)
                    check(r.value->data[j] == b[offset + j], "byte slice value");
            } else check(!r.value, "slice partial result");
        }
        const K expected = !enough ? K::EndOfInput :
            op == 3 && !std::isfinite(real) ? K::NonFiniteFloat : K::None;
        check(success == (expected == K::None), "reader success");
        if (success) { check(error.isNone(), "success error"); offset += width; }
        else check(error == nav::diagnostics::NavError{expected, offset, R::RawInput, F::RawBytes},
                   "reader exact error");
        check(reader.offset() == offset && reader.remaining() == b.size() - offset &&
              reader.atEnd() == (offset == b.size()), "reader position");
    }
}
class CandidateJournal {
    std::ofstream bytes_, recipe_;
  public:
    explicit CandidateJournal(const std::filesystem::path& dir)
        : bytes_(dir / "current.case", std::ios::binary | std::ios::trunc),
          recipe_(dir / "replay.txt", std::ios::trunc) {}
    void save(const Mutation& m, std::size_t index) {
        // Open once; flush before Nav calls. Format: uint32 LE byte length,
        // then exact candidate bytes. Ignore any suffix from an older case.
        bytes_.seekp(0);
        for (unsigned i = 0; i < 4; ++i)
            bytes_.put(static_cast<char>((m.bytes.size() >> (8U * i)) & 255U));
        bytes_.write(reinterpret_cast<const char*>(m.bytes.data()),
                     static_cast<std::streamsize>(m.bytes.size()));
        bytes_.flush(); check(bool(bytes_), "write candidate");
        recipe_.seekp(0);
        recipe_ << "seed=0xA208 case=" << index << "\n" << m.recipe << "\nEND\n";
        recipe_.flush(); check(bool(recipe_), "write recipe");
    }
};
inline void runFuzz(std::size_t count, const std::filesystem::path& requested,
                    std::size_t first = 0) {
    const auto dir = requested.empty() ? std::filesystem::path("fuzz-artifacts") : requested;
    std::filesystem::create_directories(dir);
    CandidateJournal journal(dir);
    auto retained = load(fixture(5, true).bytes);
    check(bool(retained), "retained fixture");
    const auto before = snapshotState(**retained.value);
    std::size_t valid = 0;
    for (std::size_t i = first; i < first + count; ++i) {
        auto m = mutate(i);
        check(m.bytes == mutate(i).bytes && m.recipe == mutate(i).recipe, "mutation replay");
        journal.save(m, i);
        byteOracle(m.bytes, i);
        const auto a = load(m.bytes), b = load(m.bytes);
        check(bool(a) == bool(b) && a.error == b.error, "loader repeatability");
        if (a) {
            check(a.error.isNone() && b.error.isNone() && *a.value && *b.value, "publication");
            const auto state = snapshotState(**a.value);
            check(state == snapshotState(**b.value), "snapshot repeatability");
            m.bytes.clear(); m.bytes.shrink_to_fit();
            check(state == snapshotState(**a.value), "input lifetime");
            ++valid;
        } else {
            check(!a.value && !b.value && !a.error.isNone(), "failure partial publication");
            check(a.error.offset <= m.bytes.size(), "error offset bound");
            check(a.error.kind != K::AllocationFailure && a.error.kind != K::PolicyFailure,
                  "unexpected resource/policy failure in bounded fuzz");
        }
        check(snapshotState(**retained.value) == before, "retained snapshot changed");
        if ((i + 1) % 10000 == 0) {
            std::printf("fuzz seed=0xA208 completed=%zu valid=%zu\n", i + 1, valid);
            std::fflush(stdout);
        }
    }
}
}
