#include "astrabot/nav/nav_snapshot.hpp"

#include <cstdint>
#include <cstdio>

namespace
{
bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}

	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

astrabot::nav::NavDocument makeDocument(std::uint32_t areaId)
{
	astrabot::nav::NavDocument document;
	const astrabot::nav::NavSourceIdentity identity = {5U, 4096U, 0x12345678U};
	astrabot::nav::NavArea area = {};
	area.id = areaId;
	area.extent.lo = {0.0f, 0.0f, 0.0f};
	area.extent.hi = {64.0f, 64.0f, 0.0f};
	area.northEastZ = 0.0f;
	area.southWestZ = 0.0f;
	document.setSourceIdentity(identity);
	document.addArea(area);
	return document;
}
}

int main()
{
	using astrabot::nav::NavDocument;
	using astrabot::nav::NavModelResult;
	using astrabot::nav::NavSnapshotPublisher;
	using astrabot::nav::NavSnapshotResult;

	NavSnapshotPublisher publisher;
	if (!check(publisher.snapshot().document() == nullptr, "publisher starts empty"))
	{
		return 1;
	}

	NavDocument document = makeDocument(1U);
	if (!check(publisher.publish(&document, 11U) == NavSnapshotResult::Published,
			"valid document is published"))
	{
		return 1;
	}

	const auto first = publisher.snapshot();
	if (!check(first.document() != nullptr && first.revision() == 1U &&
			first.mapGeneration() == 11U, "snapshot metadata is published"))
	{
		return 1;
	}
	if (!check(publisher.isCurrent(first), "published snapshot is current"))
	{
		return 1;
	}

	if (!check(document.addArea(makeDocument(2U).areas()[0]) == NavModelResult::Accepted,
			"source document remains mutable"))
	{
		return 1;
	}
	if (!check(first.document()->areaCount() == 1U,
			"snapshot owns an immutable document copy"))
	{
		return 1;
	}

	NavDocument invalidDocument;
	if (!check(publisher.publish(&invalidDocument, 11U) == NavSnapshotResult::InvalidDocument,
			"invalid replacement is rejected"))
	{
		return 1;
	}
	if (!check(publisher.isCurrent(first) && publisher.revision() == 1U,
			"failed replacement preserves prior snapshot"))
	{
		return 1;
	}
	if (!check(publisher.publish(nullptr, 11U) == NavSnapshotResult::InvalidArgument,
			"null replacement is rejected"))
	{
		return 1;
	}

	if (!check(publisher.invalidate(12U) == NavSnapshotResult::Invalidated,
			"map invalidation succeeds"))
	{
		return 1;
	}
	if (!check(publisher.snapshot().document() == nullptr && publisher.revision() == 2U,
			"invalidation clears current snapshot and advances revision"))
	{
		return 1;
	}
	if (!check(!publisher.isCurrent(first), "old map snapshot is stale"))
	{
		return 1;
	}

	if (!check(publisher.publish(&document, 12U) == NavSnapshotResult::Published,
			"new map snapshot is published"))
	{
		return 1;
	}
	const auto second = publisher.snapshot();
	return check(second.revision() == 3U && second.mapGeneration() == 12U &&
			publisher.isCurrent(second), "new map receives a new current revision") ? 0 : 1;
}
