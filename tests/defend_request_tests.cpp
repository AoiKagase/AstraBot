#include "astrabot/compat/defend_request.hpp"
#include "astrabot/compat/defend_hiding_spots.hpp"
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace astrabot;
using namespace astrabot::compat;

namespace
{
constexpr nav::NavVector requestPosition{350.0f, -12.0f, 20.0f};
constexpr nav::NavVector selectedSpot{505.0f, 45.0f, 25.0f};

bool samePosition(const nav::NavVector &left, const nav::NavVector &right)
{
	return left.x == right.x && left.y == right.y && left.z == right.z;
}

struct DefendEnvironment : IDefendRequestEnvironment
{
	bool valid = true;
	bool isRogue = false;
	bool isBusy = false;
	bool spotFound = true;
	std::uint16_t place = 12;
	std::uint16_t searchedPlace = 0;
	nav::NavVector placeQuery{}, hidePosition{};
	std::string invalidateAt, unavailableAt;
	std::vector<std::string> calls;

	bool current() const override
	{
		return valid;
	}
	bool stage(const char *name)
	{
		calls.emplace_back(name);
		if (invalidateAt == name)
			valid = false;
		return unavailableAt != name;
	}
	bool rogue(bool *value) override
	{
		*value = isRogue;
		return stage("rogue");
	}
	bool busy() override
	{
		stage("busy");
		return isBusy;
	}
	bool placeAt(const nav::NavVector &position, std::uint16_t *value) override
	{
		placeQuery = position;
		*value = place;
		return stage("place");
	}
	bool randomHidingSpot(std::uint16_t value, nav::NavVector *position, bool *found) override
	{
		searchedPlace = value;
		*position = selectedSpot;
		*found = spotFound;
		return stage("spot");
	}
	bool hideAtSpot(const nav::NavVector &position) override
	{
		hidePosition = position;
		return stage("hide at spot");
	}
	bool hideNearPosition(const nav::NavVector &position) override
	{
		hidePosition = position;
		return stage("hide nearby");
	}
	void affirmative() override
	{
		stage("affirmative");
	}
};

nav::NavArea area(nav::AreaId id, float lo, float hi, std::uint16_t place = 0)
{
	nav::NavArea result{};
	result.id = id;
	result.extent = {{lo, -20, 0}, {hi, 20, 0}};
	result.placeId = place;
	return result;
}

void testHidingSpots(const std::function<void(bool, const char *)> &check)
{
	constexpr std::uint8_t cover = 0x01, good = 0x02, ideal = 0x04;
	const nav::NavVector firstCover{5, 0, 0}, goodSpot{205, 0, 0}, firstIdeal{405, 0, 0};
	const nav::NavVector secondCover{905, 0, 0}, secondIdeal{805, 0, 0};
	nav::NavDocument document;
	document.setSourceIdentity({5, 100, 200});
	document.addPlace({12, "Site"});
	document.addPlace({13, "Other"});
	auto first = area(42, 0, 500, 12);
	first.hidingSpots = {{1, firstCover, cover}, {2, goodSpot, good}, {3, firstIdeal, ideal}};
	auto second = area(7, 700, 1000, 12);
	second.hidingSpots = {{4, secondCover, cover}, {5, secondIdeal, ideal}};
	auto other = area(12, 1250, 1375, 13);
	other.hidingSpots = {{6, {1300, 0, 0}, ideal}};
	check(document.addArea(first) == nav::NavModelResult::Accepted && document.addArea(second) == nav::NavModelResult::Accepted &&
			document.addArea(other) == nav::NavModelResult::Accepted,
		"Place fixture areas and hiding spots are valid");
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 2);
	HidingSpotOccupancy occupancy;
	const auto select = [&](bool sniper, int upper, int draw, const nav::NavVector &expected)
	{
		ScriptedRandomSource random({RandomTapeEntry::longEntry("Hide.Spot", {}, {}, 0, upper, draw)});
		const HidingSpotSearch search{publisher.snapshot(), occupancy, random, {}, {}, {}, sniper};
		const auto result = search.atPlace(12);
		check(result.available && result.found && samePosition(result.position, expected) && random.verifyComplete(),
			"Place selection uses the first available tier, loaded order and one bounded RNG call");
	};
	select(false, 1, 0, firstCover);
	select(false, 1, 1, secondCover);
	select(true, 1, 1, secondIdeal);
	occupancy.players = {{firstIdeal, false}};
	select(true, 0, 0, secondIdeal);
	occupancy.hostages = {secondIdeal};
	select(true, 0, 0, goodSpot);
	occupancy.players.push_back({goodSpot, false});
	select(true, 1, 0, firstCover);
	{
		ScriptedRandomSource noDraw({});
		const HidingSpotSearch search{publisher.snapshot(), occupancy, noDraw, {}, {}, {}, false};
		const auto none = search.atPlace(13);
		check(none.available && !none.found && noDraw.verifyComplete(), "No eligible cover consumes no random draw");
	}
	{
		const nav::NavVector point{};
		check(!HidingSpotOccupancy{{{{20, 0, 0}, true}, {{30, 0, 0}, false}}, {}}.taken(point),
			"The closest player being the receiver leaves the spot available");
		check(HidingSpotOccupancy{{{{20, 0, 0}, false}, {{-20, 0, 0}, true}}, {}}.taken(point),
			"Equal distance keeps the first client slot as the closest player");
		check(!HidingSpotOccupancy{{{{75, 0, 0}, false}}, {{0, 0, 75}}}.taken(point),
			"Occupancy uses a strict 75-unit 3D range for players and hostages");
		check(HidingSpotOccupancy{{{{0, 0, 0}, true}}, {{0, 0, 74}}}.taken(point),
			"A nearby hostage occupies a spot even when the closest player is the receiver");
	}
	{
		nav::NavDocument nearby;
		nearby.setSourceIdentity({5, 100, 201});
		auto anchor = area(1, -20, 20);
		anchor.connections[0] = {2, 3};
		anchor.hidingSpots = {{7, {5, 0, 0}, cover}};
		auto floor = area(2, 740, 760);
		floor.hidingSpots = {{8, {750, 0, 0}, cover}, {9, {751, 0, 0}, cover}};
		auto excluded = area(3, 750, 760);
		excluded.hidingSpots = {{10, {750, 0, 0}, cover}};
		auto ladderFront = area(4, 100, 200);
		ladderFront.hidingSpots = {{11, {150, 0, 0}, cover}};
		auto ladderBehind = area(5, 200, 300);
		ladderBehind.hidingSpots = {{12, {250, 0, 0}, cover}};
		for (const auto &item : {anchor, floor, excluded, ladderFront, ladderBehind})
			check(nearby.addArea(item) == nav::NavModelResult::Accepted, "Nearby fixture area is valid");
		nav::NavSnapshotPublisher nearbyPublisher;
		nearbyPublisher.publish(&nearby, 2);
		const auto snapshot = nearbyPublisher.snapshot();
		nav::NavLadder ladder;
		ladder.bottomArea = 1;
		ladder.topAreas = {4, 0, 0, 5};
		const nav::NavLadderTopology ladders{snapshot.revision(), 2, {ladder}};
		const HidingSpotOccupancy empty;
		for (int draw = 0; draw < 3; ++draw)
		{
			ScriptedRandomSource random({RandomTapeEntry::longEntry("Hide.Spot", {}, {}, 0, 2, draw)});
			const HidingSpotSearch search{snapshot, empty, random, {}, {}, {}, false};
			const auto result = search.nearArea(1, {}, ladders);
			const float expected[] = {5, 750, 150};
			check(result.available && result.found && result.position.x == expected[draw] && random.verifyComplete(),
				"Nearby search orders start, floor, then upward ladder and keeps exact 750-unit spots");
		}
	}
}
}

int main()
{
	int failures = 0;
	const auto check = [&](bool value, const char *message)
	{
		if (!value)
		{
			std::fprintf(stderr, "%s\n", message);
			++failures;
		}
	};
	const std::vector<std::string> direct{"rogue", "busy", "place", "spot", "hide at spot"};
	const std::vector<std::string> nearby{"rogue", "busy", "place", "spot", "hide nearby", "affirmative"};
	{
		DefendEnvironment environment;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::HideAtSpot,
			"A defined Place with a suitable spot starts the direct Hide branch");
		check(environment.calls == direct && environment.searchedPlace == environment.place,
			"A direct Hide uses Rogue, Busy, Place and the random hiding spot without any acknowledgement");
		check(samePosition(environment.placeQuery, requestPosition) && samePosition(environment.hidePosition, selectedSpot),
			"NAV Place uses the captured request coordinates and Hide uses the selected coordinates");
	}
	{
		DefendEnvironment environment;
		environment.spotFound = false;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::HideNearby,
			"A defined Place without a spot tries the nearby Hide branch");
		check(environment.calls == nearby && samePosition(environment.hidePosition, requestPosition),
			"Nearby Hide uses the captured coordinates and acknowledges after entering the branch");
	}
	{
		DefendEnvironment environment;
		environment.place = 0;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::HideNearby &&
				environment.calls == std::vector<std::string>{"rogue", "busy", "place", "hide nearby", "affirmative"},
			"Undefined Place goes straight to nearby Hide and Affirmative");
	}
	{
		DefendEnvironment environment;
		environment.isRogue = true;
		environment.isBusy = true;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::Ignored &&
				environment.calls == std::vector<std::string>{"rogue"},
			"Rogue rejects before querying Busy or NAV");
	}
	{
		DefendEnvironment environment;
		environment.isBusy = true;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::Ignored &&
				environment.calls == std::vector<std::string>{"rogue", "busy"},
			"Busy rejects before Place lookup");
	}
	for (const auto &branch : {direct, nearby})
	{
		for (std::size_t stop = 0; stop < branch.size(); ++stop)
		{
			DefendEnvironment environment;
			environment.spotFound = branch == direct;
			environment.invalidateAt = branch[stop];
			check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::Unavailable,
				"An invalidated recipient or sender cannot continue the response");
			check(environment.calls == std::vector<std::string>(branch.begin(), branch.begin() + stop + 1U),
				"Invalidation stops all later queries, state changes and acknowledgements");
		}
	}
	for (const char *stage : {"rogue", "place", "spot", "hide at spot", "hide nearby"})
	{
		DefendEnvironment environment;
		environment.spotFound = std::string(stage) != "hide nearby";
		environment.unavailableAt = stage;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::Unavailable &&
				!environment.calls.empty() && environment.calls.back() == stage,
			"An unavailable query or action prevents all later stages");
	}
	{
		DefendEnvironment environment;
		environment.valid = false;
		check(respondToDefendRequest(requestPosition, environment) == DefendRequestResult::Unavailable &&
				environment.calls.empty(),
			"Already stale ownership performs no query");
	}
	testHidingSpots(check);
	return failures ? 1 : 0;
}
