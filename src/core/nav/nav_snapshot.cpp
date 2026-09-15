#include "astrabot/nav/nav_snapshot.hpp"

#include <limits>
#include <new>

namespace astrabot
{
namespace nav
{
NavSnapshot::NavSnapshot() : document_(), revision_(0U), mapGeneration_(0U)
{
}

NavSnapshot::NavSnapshot(const std::shared_ptr<const NavDocument> &document,
	std::uint64_t revision, std::uint32_t mapGeneration) :
	document_(document), revision_(revision), mapGeneration_(mapGeneration)
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

NavSnapshotPublisher::NavSnapshotPublisher() :
	document_(), revision_(0U), mapGeneration_(0U)
{
}

NavSnapshotResult NavSnapshotPublisher::publish(const NavDocument *document,
	std::uint32_t mapGeneration)
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

	++revision_;
	document_ = candidate;
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
	++revision_;
	mapGeneration_ = mapGeneration;
	return NavSnapshotResult::Invalidated;
}

NavSnapshot NavSnapshotPublisher::snapshot() const
{
	return NavSnapshot(document_, revision_, mapGeneration_);
}

bool NavSnapshotPublisher::isCurrent(const NavSnapshot &snapshot) const
{
	return snapshot.isValid() && document_ == snapshot.document_ &&
		revision_ == snapshot.revision_ &&
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
