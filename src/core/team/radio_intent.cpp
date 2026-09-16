#include "astrabot/team/radio_intent.hpp"

#include <algorithm>
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
}

bool RadioIntent::isValid() const
{
	return actor.isValid() && isValidTeam(team) && frame.isValid() &&
		category != RadioCategory::Unknown &&
		(priority == RadioPriority::Low || priority == RadioPriority::Normal ||
		priority == RadioPriority::High || priority == RadioPriority::Critical) &&
		messageCode != 0U && isValidTeam(recipientTeam) &&
		recipientCount <= RadioLimits::kMaximumRecipients &&
		std::all_of(
			recipients.begin(),
			recipients.begin() + recipientCount,
			[](const world::ActorKey &recipient)
			{
				return recipient.isValid();
			});
}

bool RadioIntent::isIntent() const
{
	return isValid();
}

bool RadioIntent::isDelivered() const
{
	return false;
}

RadioConfig::RadioConfig()
	: cooldownTicks(3U),
	  maximumReportAgeTicks(128U),
	  minimumReportConfidence(0.25f)
{
}

bool RadioConfig::isValid() const
{
	return cooldownTicks <= RadioLimits::kMaximumCooldownTicks &&
		maximumReportAgeTicks <= TeamReportLimits::kMaximumAgeTicks &&
		std::isfinite(minimumReportConfidence) &&
		minimumReportConfidence >= 0.0f && minimumReportConfidence <= 1.0f;
}

RadioController::RadioController()
	: config_(),
	  actor_(),
	  team_(TeamRole::Unknown),
	  reporterGenerations_(),
	  lastSentTick_(0U),
	  initialized_(false),
	  hasLastSent_(false)
{
}

RadioController::RadioController(
	const world::ActorKey &actor,
	TeamRole team)
	: config_(),
	  actor_(actor),
	  team_(team),
	  reporterGenerations_(),
	  lastSentTick_(0U),
	  initialized_(actor.isValid() && isValidTeam(team)),
	  hasLastSent_(false)
{
}

RadioController::RadioController(
	const world::ActorKey &actor,
	TeamRole team,
	const RadioConfig &config)
	: config_(config),
	  actor_(actor),
	  team_(team),
	  reporterGenerations_(),
	  lastSentTick_(0U),
	  initialized_(actor.isValid() && isValidTeam(team) && config.isValid()),
	  hasLastSent_(false)
{
}

RadioResult RadioController::compose(
	const TeamReport &report,
	const world::FrameIdentity &frame,
	RadioIntent *intent)
{
	if (intent == nullptr || !frame.isValid())
	{
		return RadioResult::InvalidArgument;
	}
	*intent = {};
	if (!config_.isValid() || !initialized_)
	{
		return RadioResult::Unavailable;
	}
	if (!report.isValid())
	{
		return RadioResult::InvalidReport;
	}
	if (report.source.team != team_)
	{
		return RadioResult::WrongTeam;
	}
	const std::size_t reporterIndex = static_cast<std::size_t>(
		report.source.actor.slot - 1U);
	if (reporterGenerations_[reporterIndex] != 0U &&
			report.source.actor.generation < reporterGenerations_[reporterIndex])
	{
		return RadioResult::StaleGeneration;
	}
	reporterGenerations_[reporterIndex] = report.source.actor.generation;
	if (!sameRound(report.source.frame, frame) ||
			frame.tick < report.source.frame.tick ||
			report.source.frame.tick + config_.maximumReportAgeTicks < frame.tick)
	{
		return RadioResult::StaleFrame;
	}
	if (report.state != ReportState::Observed || report.confidence <= 0.0f ||
			report.confidence < config_.minimumReportConfidence)
	{
		return RadioResult::UnknownInformation;
	}
	if (frame.tick >= report.expiresAtTick)
	{
		return RadioResult::StaleFrame;
	}
	if (hasLastSent_ && frame.tick < lastSentTick_ + config_.cooldownTicks)
	{
		return RadioResult::Cooldown;
	}

	intent->actor = actor_;
	intent->team = team_;
	intent->frame = frame;
	intent->category = categoryFor(report.kind);
	intent->priority = priorityFor(report.kind);
	intent->messageCode = static_cast<std::uint16_t>(report.kind);
	intent->recipientTeam = team_;
	intent->recipientCount = 0U;
	if (!intent->isValid())
	{
		return RadioResult::Unavailable;
	}
	lastSentTick_ = frame.tick;
	hasLastSent_ = true;
	return RadioResult::IntentReady;
}

const world::ActorKey &RadioController::actor() const
{
	return actor_;
}

TeamRole RadioController::team() const
{
	return team_;
}

bool RadioController::sameRound(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left.mapGeneration == right.mapGeneration &&
		left.roundGeneration == right.roundGeneration;
}

RadioCategory RadioController::categoryFor(ReportKind kind)
{
	switch (kind)
	{
	case ReportKind::EnemySpotted:
	case ReportKind::EnemyLost:
		return RadioCategory::Contact;
	case ReportKind::NeedBackup:
		return RadioCategory::NeedBackup;
	case ReportKind::BombStatus:
		return RadioCategory::Bomb;
	case ReportKind::HostageStatus:
		return RadioCategory::Hostage;
	case ReportKind::AreaClear:
	case ReportKind::TacticalProposal:
	case ReportKind::Assignment:
		return RadioCategory::Chatter;
	case ReportKind::Unknown:
		break;
	}
	return RadioCategory::Unknown;
}

RadioPriority RadioController::priorityFor(ReportKind kind)
{
	return kind == ReportKind::NeedBackup ? RadioPriority::High :
		RadioPriority::Normal;
}

bool RadioController::isValidCategory(RadioCategory category)
{
	return category != RadioCategory::Unknown;
}

bool RadioController::isValidPriority(RadioPriority priority)
{
	return priority == RadioPriority::Low || priority == RadioPriority::Normal ||
		priority == RadioPriority::High || priority == RadioPriority::Critical;
}
}
}
