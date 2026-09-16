#ifndef ASTRABOT_TEAM_RADIO_INTENT_HPP
#define ASTRABOT_TEAM_RADIO_INTENT_HPP

#include "astrabot/team/team_report.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace team
{
struct RadioLimits
{
	static constexpr std::size_t kMaximumRecipients = 8U;
	static constexpr std::uint32_t kMaximumCooldownTicks = 4096U;
};

enum class RadioCategory
{
	Unknown,
	Contact,
	NeedBackup,
	Bomb,
	Hostage,
	Round,
	Chatter
};

enum class RadioPriority
{
	Low,
	Normal,
	High,
	Critical
};

enum class RadioResult
{
	IntentReady,
	Cooldown,
	UnknownInformation,
	InvalidArgument,
	InvalidReport,
	WrongTeam,
	StaleGeneration,
	StaleFrame,
	Unavailable
};

struct RadioIntent
{
	world::ActorKey actor;
	TeamRole team;
	world::FrameIdentity frame;
	RadioCategory category;
	RadioPriority priority;
	std::uint16_t messageCode;
	TeamRole recipientTeam;
	std::array<world::ActorKey, RadioLimits::kMaximumRecipients>
		recipients;
	std::size_t recipientCount;

	bool isValid() const;
	bool isIntent() const;
	bool isDelivered() const;
};

struct RadioConfig
{
	std::uint32_t cooldownTicks;
	std::uint32_t maximumReportAgeTicks;
	float minimumReportConfidence;

	RadioConfig();
	bool isValid() const;
};

class RadioController
{
public:
	RadioController();
	RadioController(const world::ActorKey &actor, TeamRole team);
	RadioController(
		const world::ActorKey &actor,
		TeamRole team,
		const RadioConfig &config);

	RadioResult compose(
		const TeamReport &report,
		const world::FrameIdentity &frame,
		RadioIntent *intent);

	const world::ActorKey &actor() const;
	TeamRole team() const;

private:
	static bool sameRound(
		const world::FrameIdentity &left,
		const world::FrameIdentity &right);
	static RadioCategory categoryFor(ReportKind kind);
	static RadioPriority priorityFor(ReportKind kind);
	static bool isValidCategory(RadioCategory category);
	static bool isValidPriority(RadioPriority priority);

	RadioConfig config_;
	world::ActorKey actor_;
	TeamRole team_;
	std::array<std::uint32_t, world::WorldLimits::kMaximumActors>
		reporterGenerations_;
	std::uint32_t lastSentTick_;
	bool initialized_;
	bool hasLastSent_;
};
}
}

#endif
