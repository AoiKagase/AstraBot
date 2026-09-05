// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstddef>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace astrabot::nav::query::detail {
// Records expose f, h and heapPosition. Vertex indices are in ascending area-ID
// order in NavGraph, so comparing indices is exactly the public ID tie-break.
// Reserve one slot per vertex before insertion; each vertex has at most one slot.
template <typename Record> class IndexedHeap final {
  public:
    static constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
    explicit IndexedHeap(std::vector<Record> &records) noexcept : records_(records) {}
    std::size_t max_size() const noexcept { return vertices_.max_size(); }
    void reserve(std::size_t count) { vertices_.reserve(count); }
    bool empty() const noexcept { return vertices_.empty(); }
    std::size_t size() const noexcept { return vertices_.size(); }
    std::size_t top() const noexcept { return vertices_.front(); }

    void improve(std::size_t vertex) {
        auto position = records_[vertex].heapPosition;
        if (position == absent) {
            position = vertices_.size();
            vertices_.push_back(vertex);
            records_[vertex].heapPosition = position;
        }
        while (position != 0) {
            const auto parent = (position - 1) / 2;
            if (!less(vertices_[position], vertices_[parent]))
                break;
            exchange(position, parent);
            position = parent;
        }
    }

    std::size_t pop() noexcept {
        const auto vertex = top();
        exchange(0, vertices_.size() - 1);
        vertices_.pop_back();
        records_[vertex].heapPosition = absent;
        std::size_t position = 0;
        // This condition also bounds 2*position+1 without overflowing size_t.
        while (position < vertices_.size() / 2) {
            auto child = position * 2 + 1;
            if (child + 1 < vertices_.size() && less(vertices_[child + 1], vertices_[child]))
                ++child;
            if (!less(vertices_[child], vertices_[position]))
                break;
            exchange(position, child);
            position = child;
        }
        return vertex;
    }

  private:
    bool less(std::size_t a, std::size_t b) const noexcept {
        return std::tie(records_[a].f, records_[a].h, a) <
               std::tie(records_[b].f, records_[b].h, b);
    }
    void exchange(std::size_t a, std::size_t b) noexcept {
        std::swap(vertices_[a], vertices_[b]);
        records_[vertices_[a]].heapPosition = a;
        records_[vertices_[b]].heapPosition = b;
    }
    std::vector<Record> &records_;
    std::vector<std::size_t> vertices_;
};
} // namespace astrabot::nav::query::detail
