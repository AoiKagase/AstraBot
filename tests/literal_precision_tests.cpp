#include "astrabot/compat/combat_behavior.hpp"

#include <cmath>
#include <cstdio>

namespace
{
// CSBot compares its stored profile skill and weapon accuracy to float constants.
bool aimThresholdsUseStoredFloatPrecision()
{
	struct ThresholdCase
	{
		float skill;
		float accuracy;
		bool chest;
	};
	constexpr float skillThreshold = .8f;
	constexpr float recoilThreshold = .4f;
	const ThresholdCase cases[] = {
		{1.0f, std::nextafter(recoilThreshold, 0.0f), false},
		{1.0f, recoilThreshold, false},
		{1.0f, std::nextafter(recoilThreshold, 1.0f), true},
		{std::nextafter(skillThreshold, 0.0f), 0.0f, true},
		{skillThreshold, 0.0f, false},
		{std::nextafter(skillThreshold, 1.0f), 0.0f, false}};
	constexpr int kRifleWeaponId = 28;
	constexpr float kHeadHeight = 100.0f;
	constexpr int kFeetOffset = 36;
	constexpr float kChestFeetScale = .25f;
	for (const ThresholdCase &entry : cases)
	{
		const float expectedHeight = entry.chest ? kHeadHeight - kFeetOffset * kChestFeetScale : kHeadHeight;
		const auto actual = astrabot::compat::combatAimPoint({100, 0, kHeadHeight}, {0, 0, 0},
			astrabot::world::VisibleHead | astrabot::world::VisibleChest, kRifleWeaponId, entry.skill, kFeetOffset,
			true, entry.accuracy);
		if (actual.z != expectedHeight)
		{
			std::fprintf(stderr, "aim threshold mismatch: skill=%.9g accuracy=%.9g z=%g expected=%g\n",
				static_cast<double>(entry.skill), static_cast<double>(entry.accuracy), static_cast<double>(actual.z),
				static_cast<double>(expectedHeight));
			return false;
		}
	}
	return true;
}

bool profilePercentageAtThresholdAimsAtHead()
{
	volatile int profileSkill = 80;
	const auto actual = astrabot::compat::combatAimPoint({100, 0, 100}, {0, 0, 0},
		astrabot::world::VisibleHead | astrabot::world::VisibleChest, 28,
		static_cast<float>(profileSkill) / 100.0f, 36, false, 0.0f);
	if (actual.z == 100.0f)
		return true;
	std::fprintf(stderr, "calculated profile skill 80%% aimed at z=%g instead of 100\n", static_cast<double>(actual.z));
	return false;
}
}

int main()
{
	return aimThresholdsUseStoredFloatPrecision() && profilePercentageAtThresholdAimsAtHead() ? 0 : 1;
}
