#ifndef ASTRABOT_NAV_NAV_SNAPSHOT_HPP
#define ASTRABOT_NAV_NAV_SNAPSHOT_HPP

#include "astrabot/nav/nav_model.hpp"

#include <cstdint>
#include <memory>

namespace astrabot
{
namespace nav
{
enum class NavSnapshotResult
{
	Published,
	InvalidArgument,
	InvalidDocument,
	Invalidated,
	ResourceLimit,
	RevisionExhausted
};

class NavSnapshot
{
public:
	NavSnapshot();

	bool isValid() const;
	std::uint64_t revision() const;
	std::uint32_t mapGeneration() const;
	const NavDocument *document() const;

private:
	friend class NavSnapshotPublisher;

	NavSnapshot(const std::shared_ptr<const NavDocument> &document,
		std::uint64_t revision, std::uint32_t mapGeneration);

	std::shared_ptr<const NavDocument> document_;
	std::uint64_t revision_;
	std::uint32_t mapGeneration_;
};

class NavSnapshotPublisher
{
public:
	NavSnapshotPublisher();

	NavSnapshotResult publish(const NavDocument *document,
		std::uint32_t mapGeneration);
	NavSnapshotResult invalidate(std::uint32_t mapGeneration);

	NavSnapshot snapshot() const;
	bool isCurrent(const NavSnapshot &snapshot) const;
	std::uint64_t revision() const;

private:
	static bool canAdvanceRevision(std::uint64_t revision);

	std::shared_ptr<const NavDocument> document_;
	std::uint64_t revision_;
	std::uint32_t mapGeneration_;
};
}
}

#endif
