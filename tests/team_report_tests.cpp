#include "astrabot/team/team_report.hpp"

#include <cmath>
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

	astrabot::world::FrameIdentity frame(
		std::uint32_t roundGeneration,
		std::uint32_t tick)
	{
		return {3U, roundGeneration, tick};
	}

	astrabot::team::TeamReport report(
		std::uint32_t roundGeneration,
		std::uint32_t tick)
	{
		astrabot::team::TeamReport value = {};
		value.id = tick;
		value.source.actor = {2U, 6U};
		value.source.team = astrabot::team::TeamRole::CounterTerrorist;
		value.source.frame = frame(roundGeneration, tick);
		value.kind = astrabot::team::ReportKind::EnemySpotted;
		value.subject = {3U, 7U};
		value.state = astrabot::team::ReportState::Observed;
		value.position = {128.0f, 64.0f, 16.0f};
		value.confidence = 0.8f;
		value.ageTicks = 0U;
		value.expiresAtTick = tick + 32U;
		return value;
	}
}

bool testScopedReportSelectionPreservesUncertainty()
{
	using astrabot::team::ReportResult;
	using astrabot::team::TeamReport;
	using astrabot::team::TeamReportBoard;

	TeamReportBoard board({1U, 5U}, astrabot::team::TeamRole::CounterTerrorist);
	const TeamReport candidate = report(4U, 10U);
	if (!check(board.add(candidate) == ReportResult::Accepted,
			"team report from same team is accepted"))
	{
		return false;
	}

	TeamReport selected = {};
	const ReportResult selection = board.select(frame(4U, 11U), &selected);
	if (!check(selection == ReportResult::Selected &&
			selected.subject.generation == 7U &&
			std::fabs(selected.confidence - 0.8f) < 0.001f &&
			!selected.isConfirmedFact(),
			"report selection preserves confidence without confirmed shared fact"))
	{
		return false;
	}

	TeamReport newerGeneration = candidate;
	newerGeneration.id = 11U;
	newerGeneration.source.actor.generation = 7U;
	newerGeneration.source.frame = frame(4U, 11U);
	if (!check(board.add(newerGeneration) == ReportResult::Accepted,
			"new reporter generation is accepted"))
	{
		return false;
	}
	if (!check(board.add(candidate) == ReportResult::StaleGeneration,
			"old reporter generation is rejected"))
	{
		return false;
	}

	TeamReport unknown = candidate;
	unknown.source.actor = {4U, 8U};
	unknown.source.frame = frame(4U, 11U);
	unknown.id = 12U;
	unknown.state = astrabot::team::ReportState::Unknown;
	unknown.confidence = 1.0f;
	if (!check(board.add(unknown) == ReportResult::Accepted,
			"unknown report is retained as uncertainty"))
	{
		return false;
	}

	TeamReport wrongTeam = candidate;
	wrongTeam.source.actor = {5U, 9U};
	wrongTeam.source.team = astrabot::team::TeamRole::Terrorist;
	return check(board.add(wrongTeam) == ReportResult::WrongTeam,
		"cross-team report cannot enter actor team board");
}

bool testReportGenerationExpiryAndBounds()
{
	using astrabot::team::ReportResult;
	using astrabot::team::TeamReport;
	using astrabot::team::TeamReportBoard;

	TeamReportBoard board({1U, 5U}, astrabot::team::TeamRole::CounterTerrorist);
	if (!check(board.add(report(4U, 10U)) == ReportResult::Accepted,
			"baseline report establishes current round"))
	{
		return false;
	}
	TeamReport stale = report(3U, 10U);
	if (!check(board.add(stale) == ReportResult::StaleFrame,
			"report from stale round is rejected"))
	{
		return false;
	}

	TeamReport invalid = report(4U, 10U);
	invalid.position.x = 100000.0f;
	if (!check(board.add(invalid) == ReportResult::InvalidReport,
			"out-of-bounds report is rejected"))
	{
		return false;
	}

	TeamReport candidate = report(4U, 10U);
	for (std::size_t index = 0U;
			index + 1U < astrabot::team::TeamReportLimits::kMaximumReports;
			++index)
	{
		candidate.source.actor.slot = static_cast<std::uint32_t>(index + 3U);
		candidate.source.actor.generation = static_cast<std::uint32_t>(index + 6U);
		candidate.subject.slot = static_cast<std::uint32_t>(index + 4U);
		candidate.subject.generation = static_cast<std::uint32_t>(index + 7U);
		if (!check(board.add(candidate) == ReportResult::Accepted,
				"report remains within fixed capacity"))
		{
			return false;
		}
	}

	candidate.source.actor.slot += 1U;
	return check(board.add(candidate) == ReportResult::ResourceLimit,
		"report over-capacity is rejected");
}

int main()
{
	if (!testScopedReportSelectionPreservesUncertainty() ||
			!testReportGenerationExpiryAndBounds())
	{
		return 1;
	}

	return 0;
}
