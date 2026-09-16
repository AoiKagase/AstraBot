#ifndef ASTRABOT_TEAM_TEAM_REPORT_HPP
#define ASTRABOT_TEAM_TEAM_REPORT_HPP

#include "astrabot/objectives/objective_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace team
{
using TeamRole = objectives::TeamRole;

struct TeamReportLimits
{
	static constexpr std::size_t kMaximumReports = 16U;
	static constexpr std::uint32_t kMaximumAgeTicks = 4096U;
};

struct ReportSource
{
	world::ActorKey actor;
	TeamRole team;
	world::FrameIdentity frame;

	bool isValid() const;
};

enum class ReportKind
{
	Unknown,
	EnemySpotted,
	EnemyLost,
	BombStatus,
	HostageStatus,
	NeedBackup,
	AreaClear,
	TacticalProposal,
	Assignment
};

enum class ReportState
{
	Unknown,
	Observed,
	Unverified,
	Unavailable
};

enum class ReportResult
{
	Accepted,
	InvalidArgument,
	InvalidReport,
	WrongTeam,
	StaleGeneration,
	StaleFrame,
	DuplicateReport,
	ResourceLimit,
	Selected,
	NoReport,
	UnknownInformation
};

struct TeamReport
{
	std::uint32_t id;
	ReportSource source;
	ReportKind kind;
	world::ActorKey subject;
	ReportState state;
	world::WorldPosition position;
	float confidence;
	std::uint32_t ageTicks;
	std::uint32_t expiresAtTick;

	bool isValid() const;
	bool isUsable(const world::FrameIdentity &frame) const;
	bool isConfirmedFact() const;
};

class TeamReportBoard
{
public:
	TeamReportBoard();
	TeamReportBoard(const world::ActorKey &owner, TeamRole team);

	ReportResult add(const TeamReport &report);
	ReportResult select(
		const world::FrameIdentity &frame,
		TeamReport *report) const;

	void clear();
	std::size_t size() const;
	const world::ActorKey &owner() const;
	TeamRole team() const;

private:
	static bool sameRound(
		const world::FrameIdentity &left,
		const world::FrameIdentity &right);
	static bool sameReport(
		const TeamReport &left,
		const TeamReport &right);
	static bool isBetter(
		const TeamReport &candidate,
		const TeamReport &current);

	world::ActorKey owner_;
	TeamRole team_;
	std::array<TeamReport, TeamReportLimits::kMaximumReports> reports_;
	std::size_t reportCount_;
	world::FrameIdentity lastFrame_;
	std::array<std::uint32_t, world::WorldLimits::kMaximumActors>
		reporterGenerations_;
};
}
}

#endif
