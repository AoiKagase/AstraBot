#include "astrabot/compat/help_request.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
namespace
{
using namespace astrabot;
using namespace astrabot::compat;
int failures = 0;
void check(bool value, const char *message)
{
	if (!value)
	{
		++failures;
		std::fprintf(stderr, "FAIL: %s\n", message);
	}
}
struct Environment : IHelpRequestEnvironment
{
	std::vector<std::string> calls;
	std::function<void(const char *)> callback;
	bool valid = true, rogueValue = false, busyValue = false, spotFound = false, moveAccepted = true;
	float travel = 3000;
	std::uint16_t place = 0;
	nav::NavVector self{0, 0, 0}, sender{750, 0, 0}, spot{20, 30, 40}, moved{};
	nav::NavVector senderAtTravel{};
	void stage(const char *name)
	{
		calls.emplace_back(name);
		if (callback)
			callback(name);
	}
	bool current() const override
	{
		return valid;
	}
	bool rogue(bool *value) override
	{
		stage("rogue");
		*value = rogueValue;
		return true;
	}
	bool busy() override
	{
		stage("busy");
		return busyValue;
	}
	bool travelDistanceToSender(float *distance) override
	{
		stage("travel");
		senderAtTravel = sender;
		*distance = travel;
		return true;
	}
	bool squaredDistanceToSender(float *distance) override
	{
		stage("near");
		const auto dx = self.x - sender.x, dy = self.y - sender.y, dz = self.z - sender.z;
		*distance = dx * dx + dy * dy + dz * dz;
		return true;
	}
	bool senderPosition(nav::NavVector *position) override
	{
		stage("sender");
		*position = sender;
		return true;
	}
	std::uint16_t ownPlace() override
	{
		stage("place");
		return place;
	}
	bool randomSpotAtPlace(std::uint16_t, nav::NavVector *position, bool *found) override
	{
		stage("spot");
		*position = spot;
		*found = spotFound;
		return true;
	}
	bool moveTo(const nav::NavVector &position) override
	{
		stage("move");
		moved = position;
		return moveAccepted;
	}
	void onMyWay() override
	{
		stage("ack");
	}
};
void testOrderedResponse()
{
	Environment ordinary;
	auto result = respondToHelpRequest({}, ordinary);
	check(result.available && result.accepted && result.moved && result.target.x == 750 && ordinary.moved.x == 750 &&
			ordinary.calls == std::vector<std::string>{"rogue", "busy", "travel", "near", "sender", "move", "ack"},
		"ordinary Help uses Rogue,busy,SAFEST range,near,MoveTo,immediate OnMyWay order");
	Environment rogue;
	rogue.rogueValue = true;
	rogue.busyValue = true;
	result = respondToHelpRequest({}, rogue);
	check(result.available && !result.accepted && rogue.calls == std::vector<std::string>{"rogue"},
		"Rogue takes priority over busy and performs no path/position work");
	Environment busy;
	busy.busyValue = true;
	result = respondToHelpRequest({}, busy);
	check(result.available && !result.accepted && busy.calls == std::vector<std::string>{"rogue", "busy"},
		"busy recipient declines before public path probes");
	for (float distance : {-1.0f, 3000.01f})
	{
		Environment distant;
		distant.travel = distance;
		distant.place = 77;
		distant.sender.x = 1;
		result = respondToHelpRequest({77, 3000}, distant);
		check(result.available && !result.accepted &&
				distant.calls == std::vector<std::string>{"rogue", "busy", "travel"},
			"unreachable or over3000 path declines even when physically close and already in requested Place");
	}
	for (float range : {0.0f, -1.0f})
	{
		Environment unlimited;
		unlimited.travel = -1;
		result = respondToHelpRequest({0, range}, unlimited);
		check(result.available && result.moved &&
				unlimited.calls == std::vector<std::string>{"rogue", "busy", "near", "sender", "move", "ack"},
			"native nonpositive maximum range skips the path-range check");
	}
	Environment near;
	near.sender = {0, 0, 749.99f};
	result = respondToHelpRequest({}, near);
	check(result.available && result.accepted && !result.moved && near.calls.back() == "near",
		"strict three-dimensional proximity below750 accepts without movement or acknowledgement");
	Environment boundary;
	boundary.sender = {0, 0, 750};
	result = respondToHelpRequest({}, boundary);
	check(result.moved && boundary.moved.z == 750, "exact vertical750 boundary must move");
	Environment samePlace;
	samePlace.place = 77;
	result = respondToHelpRequest({77, 3000}, samePlace);
	check(result.accepted && !result.moved &&
			samePlace.calls == std::vector<std::string>{"rogue", "busy", "travel", "place"},
		"same Place accepts after range without random spot or OnMyWay");
	Environment spot;
	spot.spotFound = true;
	result = respondToHelpRequest({77, 3000}, spot);
	check(result.moved && result.target.x == 20 && result.target.z == 40 &&
			spot.calls == std::vector<std::string>{"rogue", "busy", "travel", "place", "spot", "move", "ack"},
		"selected Place spot becomes a copied goal without sender fallback lookup");
	Environment moving;
	moving.callback = [&](const char *stage)
	{
		if (std::string(stage) == "rogue")
			moving.sender = {900, 0, 0};
		if (std::string(stage) == "spot")
			moving.sender = {1200, 250, 80};
	};
	result = respondToHelpRequest({77, 3000}, moving);
	check(result.moved && moving.senderAtTravel.x == 900 && result.target.x == 1200 && result.target.y == 250,
		"sender NAV is sampled after Rogue and missing Place fallback samples sender after Place RNG");
	moving.sender = {9999, 0, 0};
	check(result.target.x == 1200, "accepted movement target does not follow subsequent sender motion");
	for (const char *interrupt : {"rogue", "busy", "travel", "near", "sender", "place", "spot", "move"})
	{
		Environment stale;
		stale.callback = [&](const char *stage)
		{
			if (std::string(stage) == interrupt)
				stale.valid = false;
		};
		const bool placed = std::string(interrupt) == "place" || std::string(interrupt) == "spot";
		result = respondToHelpRequest({static_cast<std::uint16_t>(placed ? 77 : 0), 3000}, stale);
		check(!result.available && !stale.calls.empty() && stale.calls.back() == interrupt,
			"lost identity at each decision stage stops before later side effects");
	}
	Environment rejected;
	rejected.moveAccepted = false;
	result = respondToHelpRequest({}, rejected);
	check(!result.accepted && !rejected.calls.empty() && rejected.calls.back() == "move",
		"failed movement publication has no stale acknowledgement");
}
void testNativeRandomPlaceQuirk()
{
	nav::NavDocument doc;
	doc.setSourceIdentity({5, 100, 200});
	nav::NavPlace place{};
	place.id = 77;
	place.name[0] = 'A';
	place.name[1] = 0;
	doc.addPlace(place);
	for (unsigned id : {42U, 9U, 81U})
	{
		nav::NavArea area{};
		area.id = id;
		area.placeId = 77;
		area.extent = {{static_cast<float>(id), 0, 0}, {static_cast<float>(id + 10), 10, 20}};
		area.northEastZ = 400;
		area.southWestZ = 400;
		doc.addArea(area);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&doc, 3);
	for (int draw : {0, 1, 2})
	{
		ScriptedRandomSource random({RandomTapeEntry::longEntry("RNG-CSBOT-HELP-PLACE", {}, {}, 0, 2, draw)});
		const auto selected = chooseHelpPlaceSpot(publisher.snapshot(), 77, random, {}, {});
		check(selected.available && selected.found == (draw == 0) && random.verifyComplete(),
			"native missing-decrement Place iterator only finds first matching area on random zero");
		if (draw == 0)
			check(selected.position.x == 47 && selected.position.z == 10,
				"Place zero draw uses first loaded area extent center, not ID order or saddle floor");
	}
	ScriptedRandomSource empty({});
	const auto missing = chooseHelpPlaceSpot(publisher.snapshot(), 88, empty, {}, {});
	check(
		missing.available && !missing.found && !empty.failed(), "absent Place has no RNG and permits sender fallback");
	class CancelRandom : public ICompatibilityRandomSource
	{
	public:
		bool *valid;
		RandomFloatResult nextFloat(const RandomRequest &) override
		{
			return {RandomStatus::Ok, 0};
		}
		RandomLongResult nextLong(const RandomRequest &) override
		{
			*valid = false;
			return {RandomStatus::Ok, 0};
		}
	} cancel;
	bool valid = true;
	cancel.valid = &valid;
	const auto stale = chooseHelpPlaceSpot(publisher.snapshot(), 77, cancel, {}, {},
		[&]
		{
			return valid;
		});
	check(!stale.available && !stale.found, "Place RNG identity loss cannot publish an old selected center");
}
}
int main()
{
	for (auto task :
		{CompatibilityTaskId::RescueHostages, CompatibilityTaskId::MoveToSniperSpot, CompatibilityTaskId::Sniping})
		check(nativeBotBusy(CompatibilityStateId::MoveTo, task, false),
			"native task-based busy excludes rescue and sniper intentions before Help NAV/RNG");
	check(!nativeBotBusy(CompatibilityStateId::Hide, CompatibilityTaskId::SeekAndDestroy, false),
		"ordinary hiding is not native sniping solely from a weapon or state");
	check(!nativeBotBusy(CompatibilityStateId::MoveTo, CompatibilityTaskId::CollectHostages, false),
		"collecting hostages may respond to Help");
	{
		RetainedHelpTaskInput in{};
		in.bombScenario = true;
		in.counterTerrorist = true;
		in.bombPlantedKnown = true;
		in.bombPlanted = true;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Idle,
			"CT planted bomb cancels unrelated preserved Help task before path movement");
		in.task = CompatibilityTaskId::DefuseBomb;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Continue,
			"native DEFUSE_BOMB keeps assistance path until path result");
		in.otherDefuser = true;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Continue,
			"unseen planted bomb does not preempt task for other defuser");
		in.bombVisible = true;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Idle,
			"visible planted bomb with other defuser abandons preserved defuse path");
		in.task = CompatibilityTaskId::FindTickingBomb;
		in.bombLocated = false;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Continue,
			"FIND_TICKING_BOMB special branch continues before generic CT bomb rule");
		in.bombLocated = true;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Idle,
			"bomb location learned cancels old search task and its copied assistance path");
		in = {};
		in.task = CompatibilityTaskId::MoveToLastKnownEnemyPosition;
		in.taskLifeKnown = true;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Idle,
			"dead or missing pursuit task entity stops Help with preserved pursuit task");
		in.task = CompatibilityTaskId::CollectHostages;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Idle,
			"missing or dead collected hostage stops preserved task");
		in.taskAlive = true;
		in.hostageFollowingKnown = true;
		in.hostageFollowing = true;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Idle,
			"hostage already following someone cancels preserved collect task");
		in.hostageFollowing = false;
		in.hostagePositionKnown = true;
		in.hostagePosition = {75, 0, 0};
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Continue,
			"hostage exactly 75 units from assistance goal does not repath");
		in.hostagePosition.x = 75.01f;
		check(updateRetainedHelpTask(in) == RetainedHelpTaskAction::Repath,
			"hostage beyond strict 75 unit tolerance overwrites assistance path with hostage goal");
	}
	testOrderedResponse();
	testNativeRandomPlaceQuirk();
	return failures ? 1 : 0;
}
