#ifndef ASTRABOT_COMPAT_NOISE_NAVIGATION_HPP
#define ASTRABOT_COMPAT_NOISE_NAVIGATION_HPP
#include "astrabot/perception/perception.hpp"
#include "astrabot/nav/nav_query.hpp"
#include "astrabot/compat/random_source.hpp"
namespace astrabot
{
namespace compat
{
class INoiseNavProbe
{
public:
	virtual ~INoiseNavProbe() = default;
	virtual bool available() const { return true; }
	virtual bool groundHeight(const nav::NavVector &source, float *z) = 0;
	virtual bool clearLine(const nav::NavVector &from, const nav::NavVector &to) = 0;
};
class NavNoisePositionResolver : public perception::INoisePositionResolver
{
public:
	NavNoisePositionResolver(const nav::NavSnapshot &snapshot, ICompatibilityRandomSource &random, RandomActor actor,
		RandomTimingContext timing, INoiseNavProbe *probe = nullptr)
		: snapshot_(snapshot), random_(random), actor_(actor), timing_(timing), probe_(probe)
	{
	}
	bool resolve(const world::AudibleEvent &sound, world::WorldPosition *position, std::uint32_t *area,
		std::uint64_t *revision) override;

private:
	nav::NavSnapshot snapshot_;
	ICompatibilityRandomSource &random_;
	RandomActor actor_;
	RandomTimingContext timing_;
	INoiseNavProbe *probe_;
};
float noiseTravelDistance(const nav::NavSnapshot &snapshot, nav::AreaId from, nav::AreaId to,
	const nav::NavPathCostContext &cost, const nav::NavLadderTopology &ladders);
}
}
#endif
