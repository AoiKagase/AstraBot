#ifndef ASTRABOT_COMPAT_RUNTIME_MODE_POLICY_HPP
#define ASTRABOT_COMPAT_RUNTIME_MODE_POLICY_HPP

#include "astrabot/compat/cvar_state.hpp"

namespace astrabot
{
namespace compat
{
class RuntimeModePolicy
{
public:
	explicit RuntimeModePolicy(RuntimeMode mode);

	RuntimeMode mode() const;
	bool allowsEnhancedDecisionOverrides() const;
	bool allowsAdaptiveRouteWeighting() const;
	bool allowsOpponentProfileDecisionChanges() const;
	bool allowsTacticalTeamOverrides() const;
	bool allowsLearningSideEffects() const;

private:
	RuntimeMode mode_;
};
}
}

#endif
