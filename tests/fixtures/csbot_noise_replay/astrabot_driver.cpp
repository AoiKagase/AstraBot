#include "astrabot/compat/noise_investigation.hpp"
#include "astrabot/compat/noise_look.hpp"
#include "astrabot/compat/noise_navigation.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

using namespace astrabot;

struct ReplayRandom final : compat::ICompatibilityRandomSource
{
	float decisionDraw = 0;
	compat::RandomFloatResult nextFloat(const compat::RandomRequest &request) override
	{
		const bool location = std::strcmp(request.semanticId, "noise.position.x") == 0 ||
			std::strcmp(request.semanticId, "noise.position.y") == 0;
		const float value = location ? 0.0f : decisionDraw;
		std::printf("rng %s %.9g %.9g %.9g\n", request.semanticId, request.floatLower, request.floatUpper, value);
		return {compat::RandomStatus::Ok, value};
	}
	compat::RandomLongResult nextLong(const compat::RandomRequest &) override
	{
		std::abort();
	}
};

int main()
{
	nav::NavDocument document;
	document.setSourceIdentity({1, 0, 0});
	nav::NavArea area{};
	area.id = 1;
	area.extent.lo = {-2000, -2000, 0};
	area.extent.hi = {2000, 2000, 0};
	document.addArea(area);
	nav::NavSnapshotPublisher publisher;
	if (publisher.publish(&document, 1) != nav::NavSnapshotResult::Published)
		return 2;
	const auto navigation = publisher.snapshot();
	const world::ActorKey actor{1, 1};
	perception::PerceptionAssembler perception;
	compat::NoiseInvestigationPlanner planner;
	compat::NoiseLookPlanner look;
	ReplayRandom random;
	float time, x;
	int event, priority;
	unsigned tick = 0;
	for (std::string row; std::getline(std::cin, row);)
	{
		if (row.empty() || row[0] == '#')
			continue;
		if (row[0] == 'L')
		{
			int visible;
			if (!(std::istringstream(row.substr(1)) >> time >> x >> visible >> random.decisionDraw))
				return 2;
			const world::FrameIdentity frame{1, 1, ++tick};
			std::printf("tick %u %.9g\n", tick, time);
			std::printf("look-input %.9g %d\n", x, visible);
			compat::NoiseLookInput input{};
			input.actor = actor;
			input.frame = frame;
			input.now = input.noiseTime = time;
			input.reactionSeconds = .125f;
			input.hasNoise = input.highPriority = true;
			input.distanceSquared = x * x;
			input.clearLine = visible != 0;
			look.synchronize(actor, frame, time);
			const auto kind = compat::noiseLookKind(input, look.inhibitedUntil());
			const bool accepted = look.request(input, kind, {x, 0, 36}, random, {tick, 0, tick});
			std::printf("look %d %.9g\n", accepted, look.inhibitedUntil());
			continue;
		}
		if (!(std::istringstream(row) >> time >> event >> x >> priority >> random.decisionDraw))
			return 2;
		const world::FrameIdentity frame{1, 1, ++tick};
		std::printf("tick %u %.9g\n", tick, time);
		const compat::RandomTimingContext timing{tick, 0, tick};
		compat::NavNoisePositionResolver resolver(navigation, random, {actor.slot, actor.generation}, timing);
		perception::PerceptionInput input({frame, actor, 0U});
		if (input.setHearingTime(time, .125f) != perception::PerceptionInputResult::Accepted)
			return 2;
		input.setNoisePositionResolver(&resolver);
		if (event)
		{
			world::AudibleEvent sound{};
			sound.id = tick;
			sound.kind = world::SoundKind::WeaponFire;
			sound.origin = {x, 0, 0};
			sound.distance = x;
			sound.hearingRange = 2000;
			sound.loudness = 1;
			sound.confidence = {1, 0};
			sound.timestampSeconds = time;
			sound.timestampTick = tick;
			sound.priority = static_cast<world::NoisePriority>(priority - 1);
			std::printf("event %.9g %d\n", x, priority);
			if (input.addSound(sound) != perception::PerceptionInputResult::Accepted)
				return 2;
		}
		world::WorldSnapshot belief;
		if (perception.publish(input, &belief) != perception::PerceptionResult::Published)
			return 2;
		const auto &noise = belief.noise();
		std::printf("memory %d %d %.9g %.9g %d\n", noise.active, noise.ready,
			noise.active ? noise.timestampSeconds : 0, noise.active ? noise.position.x : 0,
			noise.active ? static_cast<int>(noise.priority) + 1 : 0);
		compat::NoiseInvestigationInput decision{};
		decision.actor = actor;
		decision.frame = frame;
		decision.timing = timing;
		decision.now = decision.elapsedRound = time;
		decision.reactionSeconds = .125f;
		decision.navRevision = navigation.revision();
		decision.noise = noise;
		decision.allowed = decision.alive = true;
		const bool attend = planner.evaluate(decision, random) == compat::NoiseInvestigationResult::Attend;
		std::printf("decision %d\n", attend);
		if (attend)
		{
			const bool consumed = perception.consumeNoise(noise);
			std::printf("consume %d\n", consumed && !perception.snapshot().noise().active);
			planner.cancel();
		}
	}
}
