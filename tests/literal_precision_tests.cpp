#include "astrabot/compat/combat_behavior.hpp"

#include <cstdio>

namespace
{
// Keep the original literal expressions as an independent oracle. An x87
// compiler may evaluate them more precisely than a stored constexpr float.
#define ASTRABOT_TEST_HEAD_AIM_MINIMUM_SKILL .8f
#define ASTRABOT_TEST_CHEST_AIM_ACCURACY_THRESHOLD .4f
bool aimThresholdsKeepLiteralPrecision()
{
	struct ThresholdCase
	{
		float skill;
		float accuracy;
	};
	const ThresholdCase cases[] = {
		{1.0f, .4f}, {1.0f, .3999f}, {1.0f, .4001f}, {.8f, 0.0f}, {.7999f, 0.0f}, {.8001f, 0.0f}};
	constexpr int kRifleWeaponId = 28;
	constexpr float kHeadHeight = 100.0f;
	constexpr int kFeetOffset = 36;
	constexpr float kChestFeetScale = .25f;
	for (const ThresholdCase &entry : cases)
	{
		const bool chest = entry.skill < ASTRABOT_TEST_HEAD_AIM_MINIMUM_SKILL ||
			entry.accuracy > ASTRABOT_TEST_CHEST_AIM_ACCURACY_THRESHOLD;
		const float expectedHeight = chest ? kHeadHeight - kFeetOffset * kChestFeetScale : kHeadHeight;
		const auto actual = astrabot::compat::combatAimPoint({100, 0, kHeadHeight}, {0, 0, 0},
			astrabot::world::VisibleHead | astrabot::world::VisibleChest, kRifleWeaponId, entry.skill, kFeetOffset,
			true, entry.accuracy);
		if (actual.z != expectedHeight)
		{
			std::fprintf(stderr, "aim threshold changed literal precision: skill=%.9g accuracy=%.9g z=%g expected=%g\n",
				static_cast<double>(entry.skill), static_cast<double>(entry.accuracy), static_cast<double>(actual.z),
				static_cast<double>(expectedHeight));
			return false;
		}
	}
	return true;
}
#undef ASTRABOT_TEST_HEAD_AIM_MINIMUM_SKILL
#undef ASTRABOT_TEST_CHEST_AIM_ACCURACY_THRESHOLD
}

int main()
{
	return aimThresholdsKeepLiteralPrecision() ? 0 : 1;
}
