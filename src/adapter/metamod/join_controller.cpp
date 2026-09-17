#include "astrabot/metamod/join_controller.hpp"

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
		  team_(compat::CommandTeam::Any), deadline_(0U), nextFrame_(0U),
		  attempts_(0U), teamMenuReceived_(false), classMenuReceived_(false),
		  teamConfirmed_(false)
	{
	}

	JoinAction JoinController::begin(
		const runtime::ActorId &actor,
		compat::CommandTeam team,
		std::uint32_t startFrame) noexcept
	{
		if (actor.slot == 0U || actor.actorGeneration == 0U ||
				(team != compat::CommandTeam::Any && team != compat::CommandTeam::Terrorist &&
				 team != compat::CommandTeam::CounterTerrorist))
		{
			return fail(JoinError::InvalidRequest);
		}
		if (active())
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
		teamMenuReceived_ = false;
		classMenuReceived_ = false;
		teamConfirmed_ = false;
		return JoinAction::noOp();
	}

	JoinAction JoinController::onMenu(bool classMenu, std::uint32_t frame) noexcept
	{
		if (!active())
		{
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::WaitingTeamMenu && !classMenu)
		{
			teamMenuReceived_ = true;
			phase_ = JoinPhase::TeamCommandPending;
			++attempts_;
			nextFrame_ = frame;
			return JoinAction::send(JoinCommandKind::MenuSelect, teamSelection());
		}
		if ((phase_ == JoinPhase::TeamCommandPending || phase_ == JoinPhase::WaitingClassMenu) &&
				classMenu)
		{
			classMenuReceived_ = true;
			nextFrame_ = frame;
			return JoinAction::noOp();
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
			teamConfirmed_ = true;
			return JoinAction::noOp();
		}
		if (oppositeTeamName(teamName))
		{
			return fail(JoinError::WrongTeam);
		}
		return JoinAction::noOp();
	}

	JoinAction JoinController::onFrame(edict_t *entity, std::uint32_t frame) noexcept
	{
		if (!active())
		{
			return JoinAction::noOp();
		}
		if (frame >= deadline_)
		{
			return fail(JoinError::Timeout);
		}
		if (phase_ == JoinPhase::WaitingConfirmation && readyEntity(entity))
		{
			phase_ = JoinPhase::Joined;
			return JoinAction::joined();
		}
		if (frame < nextFrame_)
		{
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::WaitingTeamMenu)
		{
			phase_ = JoinPhase::TeamCommandPending;
			++attempts_;
			return JoinAction::send(JoinCommandKind::JoinTeam, teamSelection());
		}
		if (phase_ == JoinPhase::WaitingClassMenu)
		{
			phase_ = JoinPhase::ClassCommandPending;
			return JoinAction::send(
					classMenuReceived_ ? JoinCommandKind::MenuSelect : JoinCommandKind::JoinClass,
					1U);
		}
		if (phase_ == JoinPhase::WaitingConfirmation && attempts_ < kMaximumAttempts)
		{
			return retryTeam(frame);
		}
		return JoinAction::noOp();
	}

	JoinAction JoinController::commandCompleted(std::uint32_t frame) noexcept
	{
		if (phase_ == JoinPhase::TeamCommandPending)
		{
			phase_ = JoinPhase::WaitingClassMenu;
			nextFrame_ = addFrames(frame, classMenuReceived_ ? 1U : kInitialFallbackFrames);
			return JoinAction::noOp();
		}
		if (phase_ == JoinPhase::ClassCommandPending)
		{
			phase_ = JoinPhase::WaitingConfirmation;
			nextFrame_ = addFrames(frame, kRetryFrames);
			teamMenuReceived_ = false;
			classMenuReceived_ = false;
			return JoinAction::noOp();
		}
		return JoinAction::noOp();
	}

	JoinAction JoinController::commandFailed(JoinError error) noexcept
	{
		return fail(error);
	}

	JoinAction JoinController::cancel(JoinError error) noexcept
	{
		phase_ = JoinPhase::Cancelled;
		error_ = error;
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
	teamMenuReceived_ = false;
	classMenuReceived_ = false;
		teamConfirmed_ = false;
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

	JoinAction JoinController::fail(JoinError error) noexcept
	{
		phase_ = JoinPhase::Failed;
		error_ = error;
		return JoinAction::failed(error);
	}

	JoinAction JoinController::retryTeam(std::uint32_t frame) noexcept
	{
		++attempts_;
		phase_ = JoinPhase::TeamCommandPending;
		nextFrame_ = addFrames(frame, kRetryFrames);
		teamMenuReceived_ = false;
		classMenuReceived_ = false;
		return JoinAction::send(JoinCommandKind::JoinTeam, teamSelection());
	}

	std::uint8_t JoinController::teamSelection() const noexcept
	{
		if (team_ == compat::CommandTeam::Any)
		{
			return 5U;
		}
		return team_ == compat::CommandTeam::CounterTerrorist ? 2U : 1U;
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

	bool JoinController::readyEntity(edict_t *entity) const noexcept
	{
		return entity != nullptr && (entity->v.flags & FL_SPECTATOR) == 0 &&
				entity->v.deadflag == DEAD_NO;
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
