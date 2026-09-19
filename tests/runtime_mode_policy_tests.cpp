#include "astrabot/compat/runtime_mode_policy.hpp"

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
}

int main()
{
	using astrabot::compat::RuntimeMode;
	using astrabot::compat::RuntimeModePolicy;

	const RuntimeModePolicy compatibility(RuntimeMode::Compatibility);
	if (!check(!compatibility.allowsEnhancedDecisionOverrides(), "compatibility blocks enhanced decisions") ||
		!check(!compatibility.allowsAdaptiveRouteWeighting(), "compatibility blocks adaptive routes") ||
		!check(!compatibility.allowsOpponentProfileDecisionChanges(), "compatibility blocks opponent profile changes") ||
		!check(!compatibility.allowsTacticalTeamOverrides(), "compatibility blocks tactical team overrides") ||
		!check(!compatibility.allowsLearningSideEffects(), "compatibility blocks learning side effects"))
	{
		return 1;
	}

	const RuntimeModePolicy enhanced(RuntimeMode::Enhanced);
	if (!check(enhanced.allowsEnhancedDecisionOverrides(), "enhanced allows enhanced decisions") ||
		!check(enhanced.allowsAdaptiveRouteWeighting(), "enhanced allows adaptive routes") ||
		!check(enhanced.allowsOpponentProfileDecisionChanges(), "enhanced allows opponent profile changes") ||
		!check(enhanced.allowsTacticalTeamOverrides(), "enhanced allows tactical team overrides") ||
		!check(enhanced.allowsLearningSideEffects(), "enhanced allows learning side effects"))
	{
		return 1;
	}

	return 0;
}
