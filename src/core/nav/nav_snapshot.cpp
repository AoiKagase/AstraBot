#include "astrabot/nav/nav_snapshot.hpp"

#include <cmath>
#include <limits>
#include <new>
#include <unordered_map>

namespace astrabot
{
namespace nav
{
class NavAreaGrid
{
public:
	bool build(const NavDocument &document)
	{
		constexpr std::uint64_t maxReferences = 1024U * 1024U;
		std::uint64_t remaining = maxReferences;
		const auto &areas = document.areas();
		for (std::size_t index = 0; index < areas.size(); ++index)
		{
			for (const auto &spot : areas[index].hidingSpots)
				hidingSpots_.emplace(spot.id, &spot);
			const auto &extent = areas[index].extent;
			std::int64_t x0, y0, x1, y1;
			if (!coordinate(extent.lo.x, &x0) || !coordinate(extent.hi.x, &x1) ||
				!coordinate(extent.lo.y, &y0) || !coordinate(extent.hi.y, &y1))
				return false;
			const auto width = static_cast<std::uint64_t>(x1 - x0 + 1);
			const auto height = static_cast<std::uint64_t>(y1 - y0 + 1);
			if (width > remaining / height)
				return false;
			remaining -= width * height;
			for (auto y = y0; y <= y1; ++y)
				for (auto x = x0; x <= x1; ++x)
					cells_[key(x, y)].push_back(index);
		}
		return true;
	}

	const std::vector<std::size_t> *find(const NavVector &position) const
	{
		std::int64_t x, y;
		if (!coordinate(position.x, &x) || !coordinate(position.y, &y))
			return nullptr;
		const auto cell = cells_.find(key(x, y));
		static const std::vector<std::size_t> empty;
		return cell == cells_.end() ? &empty : &cell->second;
	}

	const NavHidingSpot *findHidingSpot(std::uint32_t id) const
	{
		const auto found = hidingSpots_.find(id);
		return found == hidingSpots_.end() ? nullptr : found->second;
	}

private:
	static bool coordinate(float world, std::int64_t *coordinate)
	{
		constexpr double cellSize = 300.0;
		const auto cell = std::floor(static_cast<double>(world) / cellSize);
		if (!std::isfinite(cell) || cell < (std::numeric_limits<std::int32_t>::min)() ||
			cell > (std::numeric_limits<std::int32_t>::max)())
			return false;
		*coordinate = static_cast<std::int64_t>(cell);
		return true;
	}
	static std::uint64_t key(std::int64_t x, std::int64_t y)
	{
		return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
			static_cast<std::uint32_t>(y);
	}
	std::unordered_map<std::uint64_t, std::vector<std::size_t>> cells_;
	std::unordered_map<std::uint32_t, const NavHidingSpot *> hidingSpots_;
};

NavSnapshot::NavSnapshot() : document_(), grid_(), revision_(0U), mapGeneration_(0U)
{
}

NavSnapshot::NavSnapshot(
	const std::shared_ptr<const NavDocument> &document, const std::shared_ptr<const NavAreaGrid> &grid,
	std::uint64_t revision, std::uint32_t mapGeneration)
	: document_(document), grid_(grid), revision_(revision), mapGeneration_(mapGeneration)
{
}

bool NavSnapshot::isValid() const
{
	return document_ != nullptr;
}

std::uint64_t NavSnapshot::revision() const
{
	return revision_;
}

std::uint32_t NavSnapshot::mapGeneration() const
{
	return mapGeneration_;
}

const NavDocument *NavSnapshot::document() const
{
	return document_.get();
}

const NavHidingSpot *NavSnapshot::findHidingSpot(std::uint32_t id) const
{
	if (grid_)
		return grid_->findHidingSpot(id);
	return document_ ? document_->findHidingSpot(id) : nullptr;
}

const std::vector<std::size_t> *NavSnapshot::areaIndicesInCell(const NavVector &position) const
{
	return grid_ ? grid_->find(position) : nullptr;
}

NavSnapshotPublisher::NavSnapshotPublisher() : document_(), grid_(), revision_(0U), mapGeneration_(0U)
{
}

NavSnapshotResult NavSnapshotPublisher::publish(const NavDocument *document, std::uint32_t mapGeneration)
{
	if (document == nullptr)
	{
		return NavSnapshotResult::InvalidArgument;
	}
	if (document->validate() != NavModelResult::Accepted)
	{
		return NavSnapshotResult::InvalidDocument;
	}
	if (!canAdvanceRevision(revision_))
	{
		return NavSnapshotResult::RevisionExhausted;
	}

	std::shared_ptr<const NavDocument> candidate;
	try
	{
		candidate = std::make_shared<const NavDocument>(*document);
	}
	catch (const std::bad_alloc &)
	{
		return NavSnapshotResult::ResourceLimit;
	}
	std::shared_ptr<const NavAreaGrid> grid;
	try
	{
		auto index = std::make_shared<NavAreaGrid>();
		if (index->build(*candidate))
			grid = std::move(index);
	}
	catch (const std::bad_alloc &)
	{
		// The immutable document remains usable by the original linear lookup.
	}

	++revision_;
	document_ = candidate;
	grid_ = std::move(grid);
	mapGeneration_ = mapGeneration;
	return NavSnapshotResult::Published;
}

NavSnapshotResult NavSnapshotPublisher::invalidate(std::uint32_t mapGeneration)
{
	if (!canAdvanceRevision(revision_))
	{
		return NavSnapshotResult::RevisionExhausted;
	}

	document_.reset();
	grid_.reset();
	++revision_;
	mapGeneration_ = mapGeneration;
	return NavSnapshotResult::Invalidated;
}

NavSnapshot NavSnapshotPublisher::snapshot() const
{
	return NavSnapshot(document_, grid_, revision_, mapGeneration_);
}

bool NavSnapshotPublisher::isCurrent(const NavSnapshot &snapshot) const
{
	return snapshot.isValid() && document_ == snapshot.document_ && revision_ == snapshot.revision_ &&
		mapGeneration_ == snapshot.mapGeneration_;
}

std::uint64_t NavSnapshotPublisher::revision() const
{
	return revision_;
}

bool NavSnapshotPublisher::canAdvanceRevision(std::uint64_t revision)
{
	return revision != std::numeric_limits<std::uint64_t>::max();
}
}
}
