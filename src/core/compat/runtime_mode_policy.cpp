#include "astrabot/compat/runtime_mode_policy.hpp"

namespace astrabot
{
namespace compat
{
RuntimeModePolicy::RuntimeModePolicy(RuntimeMode mode) : mode_(mode)
{
}

RuntimeMode RuntimeModePolicy::mode() const
{
	return mode_;
}

bool RuntimeModePolicy::allowsEnhancedDecisionOverrides() const
{
	return mode_ == RuntimeMode::Enhanced;
}

bool RuntimeModePolicy::allowsAdaptiveRouteWeighting() const
{
	return mode_ == RuntimeMode::Enhanced;
}

bool RuntimeModePolicy::allowsOpponentProfileDecisionChanges() const
{
	return mode_ == RuntimeMode::Enhanced;
}

bool RuntimeModePolicy::allowsTacticalTeamOverrides() const
{
	return mode_ == RuntimeMode::Enhanced;
}

bool RuntimeModePolicy::allowsLearningSideEffects() const
{
	return mode_ == RuntimeMode::Enhanced;
}
}
}
