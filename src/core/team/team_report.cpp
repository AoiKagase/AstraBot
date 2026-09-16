#include "astrabot/team/team_report.hpp"

#include <cmath>

namespace astrabot
{
namespace team
{
namespace
{
bool isValidTeam(TeamRole team)
{
	return team == TeamRole::Terrorist ||
		team == TeamRole::CounterTerrorist;
}

bool isValidKind(ReportKind kind)
{
	return kind != ReportKind::Unknown;
}

bool isValidState(ReportState state)
{
	return state == ReportState::Unknown || state == ReportState::Observed ||
		state == ReportState::Unverified || state == ReportState::Unavailable;
}

bool isBoundedPosition(const world::WorldPosition &position)
{
	return std::isfinite(position.x) && std::isfinite(position.y) &&
		std::isfinite(position.z) &&
		std::fabs(position.x) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(position.y) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(position.z) <= world::WorldLimits::kMaximumCoordinate;
}
}

bool ReportSource::isValid() const
{
	return actor.isValid() && frame.isValid() && isValidTeam(team);
}

bool TeamReport::isValid() const
{
	return id != 0U && source.isValid() && isValidKind(kind) &&
		isValidState(state) && subject.isValid() && isBoundedPosition(position) &&
		std::isfinite(confidence) && confidence >= 0.0f && confidence <= 1.0f &&
		ageTicks <= TeamReportLimits::kMaximumAgeTicks &&
		expiresAtTick > source.frame.tick &&
		expiresAtTick - source.frame.tick <= TeamReportLimits::kMaximumAgeTicks;
}

bool TeamReport::isUsable(const world::FrameIdentity &frame) const
{
	if (!isValid() || state != ReportState::Observed ||
		confidence <= 0.0f || !frame.isValid() ||
		frame.mapGeneration != source.frame.mapGeneration ||
		frame.roundGeneration != source.frame.roundGeneration ||
		frame.tick < source.frame.tick || frame.tick >= expiresAtTick)
	{
		return false;
	}

	return static_cast<std::uint64_t>(ageTicks) +
		frame.tick - source.frame.tick <= TeamReportLimits::kMaximumAgeTicks;
}

bool TeamReport::isConfirmedFact() const
{
	return false;
}

TeamReportBoard::TeamReportBoard()
	: owner_(),
	  team_(TeamRole::Unknown),
	  reports_(),
	  reportCount_(0U),
	  lastFrame_(),
	  reporterGenerations_()
{
}

TeamReportBoard::TeamReportBoard(
	const world::ActorKey &owner,
	TeamRole team)
	: owner_(owner),
	  team_(team),
	  reports_(),
	  reportCount_(0U),
	  lastFrame_(),
	  reporterGenerations_()
{
}

ReportResult TeamReportBoard::add(const TeamReport &report)
{
	if (!owner_.isValid() || !isValidTeam(team_))
	{
		return ReportResult::InvalidArgument;
	}
	if (!report.isValid())
	{
		return ReportResult::InvalidReport;
	}
	if (report.source.team != team_)
	{
		return ReportResult::WrongTeam;
	}
	const std::size_t reporterIndex = static_cast<std::size_t>(
		report.source.actor.slot - 1U);
	if (reporterGenerations_[reporterIndex] != 0U &&
			report.source.actor.generation <
			reporterGenerations_[reporterIndex])
	{
		return ReportResult::StaleGeneration;
	}
	if (lastFrame_.isValid() &&
			(!sameRound(report.source.frame, lastFrame_) ||
			 report.source.frame.tick < lastFrame_.tick))
	{
		return ReportResult::StaleFrame;
	}
	for (std::size_t index = 0U; index < reportCount_; ++index)
	{
		if (sameReport(reports_[index], report))
		{
			return ReportResult::DuplicateReport;
		}
	}
	if (reportCount_ >= reports_.size())
	{
		return ReportResult::ResourceLimit;
	}
	reports_[reportCount_] = report;
	++reportCount_;
	reporterGenerations_[reporterIndex] = report.source.actor.generation;
	if (!lastFrame_.isValid() || report.source.frame.tick > lastFrame_.tick)
	{
		lastFrame_ = report.source.frame;
	}
	return ReportResult::Accepted;
}

ReportResult TeamReportBoard::select(
	const world::FrameIdentity &frame,
	TeamReport *report) const
{
	if (report == nullptr || !frame.isValid())
	{
		return ReportResult::InvalidArgument;
	}
	bool found = false;
	TeamReport selected = {};
	for (std::size_t index = 0U; index < reportCount_; ++index)
	{
		if (!reports_[index].isUsable(frame))
		{
			continue;
		}
		const std::size_t reporterIndex = static_cast<std::size_t>(
			reports_[index].source.actor.slot - 1U);
		if (reporterGenerations_[reporterIndex] !=
				reports_[index].source.actor.generation)
		{
			continue;
		}
		if (!found || isBetter(reports_[index], selected))
		{
			selected = reports_[index];
			found = true;
		}
	}
	if (!found)
	{
		return ReportResult::NoReport;
	}
	*report = selected;
	return ReportResult::Selected;
}

void TeamReportBoard::clear()
{
	reportCount_ = 0U;
	lastFrame_ = {};
	for (std::uint32_t &generation : reporterGenerations_)
	{
		generation = 0U;
	}
}

std::size_t TeamReportBoard::size() const
{
	return reportCount_;
}

const world::ActorKey &TeamReportBoard::owner() const
{
	return owner_;
}

TeamRole TeamReportBoard::team() const
{
	return team_;
}

bool TeamReportBoard::sameRound(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left.mapGeneration == right.mapGeneration &&
		left.roundGeneration == right.roundGeneration;
}

bool TeamReportBoard::sameReport(
	const TeamReport &left,
	const TeamReport &right)
{
	return left.id == right.id && left.source.actor == right.source.actor &&
		left.source.frame == right.source.frame;
}

bool TeamReportBoard::isBetter(
	const TeamReport &candidate,
	const TeamReport &current)
{
	if (candidate.confidence != current.confidence)
	{
		return candidate.confidence > current.confidence;
	}
	if (candidate.expiresAtTick != current.expiresAtTick)
	{
		return candidate.expiresAtTick < current.expiresAtTick;
	}
	return candidate.source.actor.slot < current.source.actor.slot;
}
}
}
