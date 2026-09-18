#include "astrabot/metamod/join_controller.hpp"

#include <cstddef>
#include <limits>

namespace astrabot
{
namespace metamod
{
namespace
{
std::uint32_t addFrames(std::uint32_t frame, std::uint32_t amount) noexcept
{
	const std::uint32_t maximum = (std::numeric_limits<std::uint32_t>::max)();
	return frame > maximum - amount ? maximum : frame + amount;
}
}

JoinAction JoinAction::noOp() noexcept
{
	return {JoinActionKind::None, JoinError::None, JoinCommandKind::MenuSelect, 0U};
}

JoinAction JoinAction::send(std::uint8_t selection) noexcept
{
	return send(JoinCommandKind::MenuSelect, selection);
}

JoinAction JoinAction::send(JoinCommandKind command, std::uint8_t selection) noexcept
{
	return {JoinActionKind::SendMenuSelect, JoinError::None, command, selection};
}

JoinAction JoinAction::joined() noexcept
{
	return {JoinActionKind::Joined, JoinError::None, JoinCommandKind::MenuSelect, 0U};
}

JoinAction JoinAction::failed(JoinError error) noexcept
{
	return {JoinActionKind::Failed, error, JoinCommandKind::MenuSelect, 0U};
}

JoinAction JoinAction::cancelled(JoinError error) noexcept
{
	return {JoinActionKind::Cancelled, error, JoinCommandKind::MenuSelect, 0U};
}

JoinController::JoinController() noexcept
	: phase_(JoinPhase::Idle), error_(JoinError::None), actor_(),
	  team_(compat::CommandTeam::Any), deadline_(0U), nextFrame_(0U), attempts_(0U),
	  teamConfirmed_(false), teamInfoReceived_(false), classSelectionCompleted_(false),
	  postClassFrameAdvanced_(false), teamMenuReceived_(false), classMenuReceived_(false),
	  teamMenuSource_(JoinMenuSource::Unknown), classMenuSource_(JoinMenuSource::Unknown),
	  pendingSelection_(false), repeatedPrompt_(false),
	  pendingValue_(0U), pendingFrame_(0U), promptGraceFrames_(0U)
{
}

JoinAction JoinController::begin(
	const runtime::ActorId &actor,
	compat::CommandTeam team,
	std::uint32_t startFrame) noexcept
{
	if (active())
	{
		return JoinAction::failed(JoinError::AlreadyJoining);
	}
	if (actor.slot == 0U || actor.actorGeneration == 0U ||
		(team != compat::CommandTeam::Any && team != compat::CommandTeam::Terrorist &&
		 team != compat::CommandTeam::CounterTerrorist))
	{
		return fail(JoinError::InvalidRequest);
	}

	actor_ = actor;
	team_ = team;
	phase_ = JoinPhase::WaitingTeamMenu;
	error_ = JoinError::None;
	deadline_ = addFrames(startFrame, kTimeoutFrames);
	nextFrame_ = addFrames(startFrame, kInitialFallbackFrames);
	attempts_ = 0U;
	teamConfirmed_ = false;
	teamInfoReceived_ = false;
	classSelectionCompleted_ = false;
	postClassFrameAdvanced_ = false;
	teamMenuReceived_ = false;
	classMenuReceived_ = false;
	teamMenuSource_ = JoinMenuSource::Unknown;
	classMenuSource_ = JoinMenuSource::Unknown;
	pendingSelection_ = false;
	repeatedPrompt_ = false;
	pendingValue_ = 0U;
	pendingFrame_ = 0U;
	promptGraceFrames_ = 0U;
	return JoinAction::noOp();
}

JoinAction JoinController::onMenu(
	JoinMenuKind menu,
	std::uint16_t validSlots,
	std::uint32_t frame,
	JoinMenuSource source) noexcept
{
	if (!active())
	{
		return JoinAction::noOp();
	}

	if (menu == JoinMenuKind::TerroristClass ||
			menu == JoinMenuKind::CounterTerroristClass)
	{
		if ((team_ == compat::CommandTeam::Terrorist &&
				menu != JoinMenuKind::TerroristClass) ||
			(team_ == compat::CommandTeam::CounterTerrorist &&
				menu != JoinMenuKind::CounterTerroristClass))
		{
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::WaitingTeamMenu && !teamConfirmed_)
		{
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::TeamCommandPending)
		{
			if (!selectionAvailable(validSlots, kClassSelection))
			{
				return JoinAction::noOp();
			}
			classMenuReceived_ = true;
			classMenuSource_ = source;
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::WaitingTeamMenu || phase_ == JoinPhase::WaitingClassMenu)
		{
			if (!selectionAvailable(validSlots, kClassSelection))
			{
				return fail(JoinError::MenuOptionUnavailable);
			}
			classMenuReceived_ = true;
			classMenuSource_ = source;
			phase_ = JoinPhase::WaitingClassMenu;
			pendingSelection_ = true;
			pendingValue_ = kClassSelection;
			pendingFrame_ = frame;
			nextFrame_ = frame;
			promptGraceFrames_ = 0U;
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::ClassCommandPending || phase_ == JoinPhase::WaitingConfirmation)
		{
			if (selectionAvailable(validSlots, kClassSelection))
			{
				repeatedPrompt_ = true;
			}
		}
		return JoinAction::noOp();
	}

	if (menu == JoinMenuKind::Team && phase_ == JoinPhase::WaitingTeamMenu)
	{
		teamMenuReceived_ = true;
		teamMenuSource_ = source;
		if (teamConfirmed_)
		{
			phase_ = JoinPhase::WaitingClassMenu;
			pendingSelection_ = false;
			nextFrame_ = frame;
			return JoinAction::noOp();
		}
		if (!selectionAvailable(validSlots, teamSelection()))
		{
			return fail(JoinError::MenuOptionUnavailable);
		}
		pendingSelection_ = true;
		pendingValue_ = teamSelection();
		pendingFrame_ = frame;
		nextFrame_ = frame;
		promptGraceFrames_ = 0U;
		return JoinAction::noOp();
	}
	if (menu == JoinMenuKind::Team && phase_ == JoinPhase::TeamCommandPending &&
		selectionAvailable(validSlots, teamSelection()))
	{
		repeatedPrompt_ = true;
	}
	return JoinAction::noOp();
}

JoinAction JoinController::onTeamInfo(const char *teamName) noexcept
{
	if (!active() || teamName == nullptr)
	{
		return JoinAction::noOp();
	}
	if (expectedTeamName(teamName))
	{
		const bool changed = !teamConfirmed_;
		teamConfirmed_ = true;
		teamInfoReceived_ = true;
		(void)changed;
		return JoinAction::noOp();
	}
	if (oppositeTeamName(teamName))
	{
		return fail(JoinError::WrongTeam);
	}
	return JoinAction::noOp();
}

JoinAction JoinController::onFrame(std::uint32_t frame) noexcept
	{
		return onFrame(nullptr, frame);
	}

JoinAction JoinController::onFrame(const edict_t *entity, std::uint32_t frame) noexcept
{
	if (!active())
	{
		return JoinAction::noOp();
	}
	if (phase_ == JoinPhase::WaitingConfirmation)
	{
		if (!classSelectionCompleted_ || !teamConfirmed_)
		{
			return JoinAction::noOp();
		}
		if (readyEntity(entity))
		{
			phase_ = JoinPhase::Joined;
			return JoinAction::joined();
		}
		return JoinAction::noOp();
	}

	if (frame >= deadline_)
	{
		return fail(JoinError::Timeout);
	}

	if (phase_ == JoinPhase::WaitingClassMenu && !teamConfirmed_)
	{
		return JoinAction::noOp();
	}

	if ((phase_ == JoinPhase::WaitingTeamMenu || phase_ == JoinPhase::WaitingClassMenu) &&
		!pendingSelection_ && frame >= nextFrame_)
	{
		pendingSelection_ = true;
		pendingFrame_ = frame;
		pendingValue_ = phase_ == JoinPhase::WaitingTeamMenu ? teamSelection() : kClassSelection;
	}
	if (!pendingSelection_)
	{
		return JoinAction::noOp();
	}
	return sendPendingSelection();
}

JoinAction JoinController::commandCompleted(std::uint32_t frame) noexcept
{
	if (phase_ != JoinPhase::TeamCommandPending && phase_ != JoinPhase::ClassCommandPending)
	{
		return JoinAction::noOp();
	}
	if (repeatedPrompt_)
	{
		pendingSelection_ = true;
		pendingValue_ = phase_ == JoinPhase::TeamCommandPending ? teamSelection() : kClassSelection;
		pendingFrame_ = frame;
		nextFrame_ = frame;
		phase_ = phase_ == JoinPhase::TeamCommandPending ? JoinPhase::WaitingTeamMenu
														 : JoinPhase::WaitingClassMenu;
		repeatedPrompt_ = false;
		return JoinAction::noOp();
	}

	if (phase_ == JoinPhase::TeamCommandPending)
	{
		phase_ = JoinPhase::WaitingClassMenu;
		nextFrame_ = addFrames(frame, kInitialFallbackFrames);
		promptGraceFrames_ = 0U;
		return JoinAction::noOp();
	}

	classSelectionCompleted_ = true;
	postClassFrameAdvanced_ = false;
	phase_ = JoinPhase::WaitingConfirmation;
	nextFrame_ = addFrames(frame, 1U);
	promptGraceFrames_ = 0U;
	return JoinAction::noOp();
}

JoinAction JoinController::commandFailed(JoinError error) noexcept
{
	return fail(error);
}

JoinAction JoinController::cancel(JoinError error) noexcept
{
	if (!active())
	{
		return JoinAction::noOp();
	}
	phase_ = JoinPhase::Cancelled;
	error_ = error;
	pendingSelection_ = false;
	repeatedPrompt_ = false;
	return JoinAction::cancelled(error);
}

void JoinController::reset() noexcept
{
	phase_ = JoinPhase::Idle;
	error_ = JoinError::None;
	actor_ = {};
	team_ = compat::CommandTeam::Any;
	deadline_ = 0U;
	nextFrame_ = 0U;
	attempts_ = 0U;
	teamConfirmed_ = false;
	teamInfoReceived_ = false;
	classSelectionCompleted_ = false;
	postClassFrameAdvanced_ = false;
	teamMenuReceived_ = false;
	classMenuReceived_ = false;
	teamMenuSource_ = JoinMenuSource::Unknown;
	classMenuSource_ = JoinMenuSource::Unknown;
	pendingSelection_ = false;
	repeatedPrompt_ = false;
	pendingValue_ = 0U;
	pendingFrame_ = 0U;
	promptGraceFrames_ = 0U;
}

bool JoinController::active() const noexcept
{
	return phase_ != JoinPhase::Idle && phase_ != JoinPhase::Joined &&
			phase_ != JoinPhase::Failed && phase_ != JoinPhase::Cancelled;
}

bool JoinController::isCurrent(const runtime::ActorId &actor) const noexcept
{
	return actor_.slot == actor.slot && actor_.actorGeneration == actor.actorGeneration;
}

JoinPhase JoinController::phase() const noexcept
{
	return phase_;
}

JoinError JoinController::error() const noexcept
{
	return error_;
}

std::uint8_t JoinController::attempts() const noexcept
{
	return attempts_;
}

bool JoinController::teamConfirmed() const noexcept
{
	return teamConfirmed_ && teamInfoReceived_;
}

bool JoinController::classSelectionCompleted() const noexcept
{
	return classSelectionCompleted_;
}

compat::CommandTeam JoinController::requestedTeam() const noexcept
{
	return team_;
}

JoinAction JoinController::fail(JoinError error) noexcept
{
	phase_ = JoinPhase::Failed;
	error_ = error;
	pendingSelection_ = false;
	repeatedPrompt_ = false;
	return JoinAction::failed(error);
}

JoinAction JoinController::sendPendingSelection() noexcept
{
	if (attempts_ >= kMaximumAttempts)
	{
		return fail(JoinError::CommandAttemptsExhausted);
	}
	const bool teamSelectionPending = phase_ == JoinPhase::WaitingTeamMenu;
	const bool classSelectionPending = phase_ == JoinPhase::WaitingClassMenu;
	if (!teamSelectionPending && !classSelectionPending)
	{
		return JoinAction::noOp();
	}
	const std::uint8_t selection = pendingValue_;
	const JoinCommandKind command = pendingCommand();
	pendingSelection_ = false;
	repeatedPrompt_ = false;
	++attempts_;
	phase_ = teamSelectionPending ? JoinPhase::TeamCommandPending : JoinPhase::ClassCommandPending;
	return JoinAction::send(command, selection);
}

JoinCommandKind JoinController::pendingCommand() const noexcept
{
	if (phase_ == JoinPhase::WaitingTeamMenu)
	{
		return teamMenuReceived_ && teamMenuSource_ == JoinMenuSource::LegacyShowMenu
				? JoinCommandKind::MenuSelect
				: JoinCommandKind::JoinTeam;
	}
	if (phase_ == JoinPhase::WaitingClassMenu)
	{
		return classMenuReceived_ && classMenuSource_ == JoinMenuSource::LegacyShowMenu
				? JoinCommandKind::MenuSelect
				: JoinCommandKind::JoinClass;
	}
	return JoinCommandKind::MenuSelect;
}

std::uint8_t JoinController::teamSelection() const noexcept
{
	if (team_ == compat::CommandTeam::Any)
	{
		return 5U;
	}
	return team_ == compat::CommandTeam::CounterTerrorist ? 2U : 1U;
}

bool JoinController::selectionAvailable(std::uint16_t validSlots, std::uint8_t selection) const noexcept
{
	if (selection == 0U || selection > 16U)
	{
		return false;
	}
	if (validSlots == 0U)
	{
		return true;
	}
	return (validSlots & static_cast<std::uint16_t>(1U << (selection - 1U))) != 0U;
}

bool JoinController::readyEntity(const edict_t *entity) const noexcept
{
	if (entity == nullptr || (entity->v.flags & FL_SPECTATOR) != 0 ||
		entity->v.deadflag != DEAD_NO || entity->v.health <= 0.0f ||
		entity->v.solid != SOLID_SLIDEBOX ||
		entity->v.movetype != MOVETYPE_WALK)
	{
		return false;
	}
	return true;
}

bool JoinController::expectedTeamName(const char *teamName) const noexcept
{
	if (team_ == compat::CommandTeam::Any)
	{
		return textEqualsIgnoreCase(teamName, "TERRORIST") ||
				textEqualsIgnoreCase(teamName, "CT");
	}
	return team_ == compat::CommandTeam::CounterTerrorist
			? textEqualsIgnoreCase(teamName, "CT")
			: textEqualsIgnoreCase(teamName, "TERRORIST");
}

bool JoinController::oppositeTeamName(const char *teamName) const noexcept
{
	if (team_ == compat::CommandTeam::Any)
	{
		return false;
	}
	return team_ == compat::CommandTeam::CounterTerrorist
			? textEqualsIgnoreCase(teamName, "TERRORIST")
			: textEqualsIgnoreCase(teamName, "CT");
}

bool JoinController::textEqualsIgnoreCase(const char *left, const char *right) noexcept
{
	if (left == nullptr || right == nullptr)
	{
		return false;
	}
	std::size_t index = 0U;
	while (left[index] != '\0' && right[index] != '\0')
	{
		char leftValue = left[index];
		char rightValue = right[index];
		if (leftValue >= 'a' && leftValue <= 'z')
		{
			leftValue = static_cast<char>(leftValue - ('a' - 'A'));
		}
		if (rightValue >= 'a' && rightValue <= 'z')
		{
			rightValue = static_cast<char>(rightValue - ('a' - 'A'));
		}
		if (leftValue != rightValue)
		{
			return false;
		}
		++index;
	}
	return left[index] == '\0' && right[index] == '\0';
}
}
}
