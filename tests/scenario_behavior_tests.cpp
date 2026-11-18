#include "astrabot/compat/scenario_behavior.hpp"
#include "astrabot/compat/bomb_guard.hpp"
#include <cstdio>

namespace
{
bool check(bool value, const char *message)
{
	if (!value)
	{
		std::fprintf(stderr, "%s\n", message);
	}
	return value;
}

astrabot::compat::ScenarioBehaviorInput input()
{
	astrabot::compat::ScenarioBehaviorInput value = {};
	value.actor = {1U, 1U};
	value.frame = {1U, 1U, 1U};
	value.lifeKnown = true;
	value.alive = true;
	value.team = astrabot::objectives::TeamRole::CounterTerrorist;
	value.scenario = astrabot::compat::PublicScenarioKind::Hostage;
	value.reactionSeconds = 0.2f;
	return value;
}

astrabot::compat::ScenarioTarget target()
{
	astrabot::compat::ScenarioTarget value = {};
	value.knowledge = astrabot::compat::ScenarioKnowledge::Visible;
	value.frame = {1U, 1U, 1U};
	value.entity = {5U, 1U};
	value.position = {100.0f, 0.0f, 0.0f};
	value.pathCost = 100.0f;
	return value;
}
} // namespace

int main()
{
	using namespace astrabot::compat;
	bool ok = true;
	{
		ScenarioBehaviorPlanner planner;
		auto scenario = input();
		ScenarioRadioObservation order{};
		order.sequence = 1;
		order.sender = {2, 1};
		order.command = ScenarioRadioCommand::ReportIn;
		order.senderHuman = true;
		order.team = scenario.team;
		scenario.radio = &order;
		scenario.busy = true;
		scenario.reactionSeconds = 0.25f;
		scenario.timestamp = 1.49f;
		planner.update(scenario);
		ok &= check(!planner.takeReportRequest(scenario.frame), "a busy bot waits for the radio reaction delay");
		scenario.timestamp = 1.5f;
		++scenario.frame.tick;
		planner.update(scenario);
		ok &= check(planner.takeReportRequest(scenario.frame) && !planner.takeReportRequest(scenario.frame),
			"a busy bot answers ReportIn once at the reaction boundary");
	}
	{
		ScenarioBehaviorPlanner defender;
		auto spot = target();
		spot.inUseRange = false;
		defender.holdPosition(spot, 30.0f, 3.0f);
		ok &= check(!defender.delayDefenseInvestigation(4.0f), "an approaching defender can investigate immediately");
		defender.observeDefenderFeet(spot.position.x, spot.position.y, spot.position.z);
		ok &= check(defender.delayDefenseInvestigation(5.0f) && defender.delayDefenseInvestigation(8.0f) &&
			!defender.delayDefenseInvestigation(8.125f), "ambush delay starts at the first qualifying sound at the spot");
		defender.holdPosition(target(), 30.0f);
		ok &= check(!defender.finishDefense(30.0f, false), "Hide lasts through the exact duration");
		ok &= check(!defender.finishDefense(31.0f, true), "an active reload delays Hide expiry");
		defender.holdPosition(target(), 60.0f);
		ok &= check(!defender.finishDefense(31.0f, false), "a fresh defense receives its own duration");
		ok &= check(defender.finishDefense(60.125f, false) && !defender.finishDefense(61.0f, false),
			"an expired defense returns to Idle once");
		defender.holdPosition(target(), 30.0f);
		defender.cancelHoldPosition();
		ok &= check(!defender.finishDefense(31.0f, false),
			"an unreachable replacement releases the previous hold");
	}
	{
		auto scenario = input();
		auto decision = ScenarioBehaviorDecision{};
		decision.state = CompatibilityStateId::MoveTo;
		decision.hasTarget = true;
		scenario.hostage = target();
		decision.target = scenario.hostage;
		ok &= check(nativeScenarioMoveTask(scenario, decision) == CompatibilityTaskId::CollectHostages,
			"hostage approach stamps COLLECT_HOSTAGES before preserved Help entry");
		scenario.escortKnown = true;
		scenario.escortCount = 1;
		ok &= check(nativeScenarioMoveTask(scenario, decision) == CompatibilityTaskId::RescueHostages,
			"observed escort stamps native busy RESCUE_HOSTAGES");
		scenario = input();
		scenario.scenario = PublicScenarioKind::Bomb;
		scenario.bombPlantedKnown = true;
		scenario.bombPlanted = true;
		scenario.bomb = target();
		decision.target = scenario.bomb;
		ok &= check(nativeScenarioMoveTask(scenario, decision) == CompatibilityTaskId::DefuseBomb,
			"known planted bomb approach stamps DEFUSE_BOMB");
		scenario.bomb = {};
		scenario.bombSearch = target();
		decision.target = scenario.bombSearch;
		ok &= check(nativeScenarioMoveTask(scenario, decision) == CompatibilityTaskId::FindTickingBomb,
			"unlocated planted bomb search stamps FIND_TICKING_BOMB");
		scenario.team = astrabot::objectives::TeamRole::Terrorist;
		ok &= check(nativeScenarioMoveTask(scenario, decision, true) == CompatibilityTaskId::PlantBomb,
			"C4 approach stamps PLANT_BOMB before Help busy evaluation");
		decision.state = CompatibilityStateId::Hunt;
		ok &= check(nativeScenarioMoveTask(scenario, decision, true) == CompatibilityTaskId::None,
			"other normal state retains its own task entry");
	}
	ScenarioBehaviorPlanner hostage;
	auto value = input();
	ok &= check(hostage.update(value).unavailable, "unknown hostage never yields hidden target");
	value.frame.tick++;
	value.hostage = target();
	value.hostage.inUseRange = true;
	auto result = hostage.update(value);
	ok &= check(result.action == ScenarioAction::Use && result.awaitingReceipt,
		"visible reachable hostage requests use without claiming escort");
	value.frame.tick++;
	result = hostage.update(value);
	ok &= check(result.action == ScenarioAction::Move && result.awaitingReceipt && !result.escortInferred,
		"use keeps moving while escort receipt remains unknown");
	value.frame.tick++;
	value.escortKnown = true;
	value.escortCount = 1U;
	value.rescueZone = target();
	result = hostage.update(value);
	ok &= check(
		result.action == ScenarioAction::Move && result.hasTarget, "observed escort receipt enables rescue route");
	value.frame = {1U, 2U, 1U};
	value.escortKnown = false;
	value.hostage = {};
	value.rescueZone = {};
	ok &= check(!hostage.update(value).hasTarget, "round reset removes hostage and use knowledge");
	ScenarioBehaviorPlanner inferred;
	value = input();
	value.hostage = target();
	value.hostage.inUseRange = true;
	value.rescueZone = target();
	value.rescueZone.entity = {8U, 1U};
	value.rescueZone.position = {1000.0f, 0.0f, 0.0f};
	value.selfPositionKnown = true;
	value.selfPosition = {0.0f, 0.0f, 0.0f};
	ok &= check(inferred.update(value).action == ScenarioAction::Use, "initial hostage interaction requested");
	value.frame.tick++;
	value.timestamp = 0.3f;
	value.selfPosition = {200.0f, 0.0f, 0.0f};
	value.hostage.position = {140.0f, 0.0f, 0.0f};
	result = inferred.update(value);
	ok &= check(result.action == ScenarioAction::Move && result.target.entity == value.rescueZone.entity &&
			result.awaitingReceipt && result.escortInferred && !value.escortKnown && value.escortCount == 0U,
		"following motion supports inferred rescue movement without inventing escort receipt");
	value.frame.tick++;
	value.timestamp = 0.4f;
	value.hostageCompetitorNearby = true;
	result = inferred.update(value);
	ok &= check(!result.escortInferred && result.awaitingReceipt,
		"competing CT vetoes attribution of hostage motion to this bot");
	ScenarioBehaviorPlanner contested;
	value = input();
	value.hostage = target();
	value.hostage.inUseRange = true;
	value.hostageCompetitorNearby = true;
	ok &= check(contested.update(value).action == ScenarioAction::Use,
		"nearby CT vetoes inference without preventing an initial hostage use request");
	ScenarioBehaviorPlanner retries;
	value = input();
	value.hostage = target();
	value.hostage.inUseRange = true;
	value.rescueZone = target();
	value.rescueZone.entity = {8U, 1U};
	for (unsigned int attempt = 0; attempt < 3U; ++attempt)
	{
		value.frame.tick++;
		value.timestamp = static_cast<float>(attempt) * 2.3f;
		result = retries.update(value);
		ok &= check(result.action == ScenarioAction::Use, "unconfirmed hostage permits bounded interaction retry");
		value.frame.tick++;
		value.timestamp += 2.1f;
		result = retries.update(value);
		ok &= check(result.action == ScenarioAction::Move && result.awaitingReceipt && !result.escortInferred,
			"stationary hostage triggers reapproach instead of permanent Stop");
	}
	value.frame.tick++;
	value.timestamp += 0.1f;
	result = retries.update(value);
	ok &= check(result.action == ScenarioAction::Hunt && result.unavailable && !result.awaitingReceipt,
		"three unconfirmed use attempts yield to Hunt without claiming rescue");
	value.frame = {1U, 2U, 1U};
	value.timestamp = 0.0f;
	value.hostage.frame = value.rescueZone.frame = value.frame;
	ok &= check(retries.update(value).action == ScenarioAction::Use, "new round clears bounded retry history");
	ScenarioBehaviorPlanner blind;
	value = input();
	value.blind = true;
	value.hostage = target();
	value.hostage.inUseRange = true;
	ok &= check(blind.update(value).action != ScenarioAction::Use, "blind observer cannot acquire visible hostage");
	ScenarioBehaviorPlanner radio;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	ScenarioRadioObservation order = {};
	order.sequence = 1U;
	order.command = ScenarioRadioCommand::FollowMe;
	order.sender = {2U, 1U};
	order.team = astrabot::objectives::TeamRole::Terrorist;
	order.senderHuman = true;
	order.position = target();
	value.radio = &order;
	ok &= check(radio.update(value).action != ScenarioAction::Follow, "opposing team radio is ignored");
	order.sequence++;
	order.team = value.team;
	value.frame.tick++;
	value.leader = {{2U, 1U}, value.team, true, true, true, target()};
	value.timestamp = 0.1f;
	order.receivedTime = 0.1f;
	ok &= check(radio.update(value).action != ScenarioAction::Follow, "same-team radio waits profile response delay");
	value.frame.tick++;
	value.timestamp = 1.6f;
	ok &= check(
		radio.update(value).action == ScenarioAction::Follow, "eligible same-team human follow order is accepted");
	value.frame.tick++;
	value.leader.alive = false;
	ok &= check(radio.update(value).action == ScenarioAction::Stop, "observed dead leader terminates following");
	ScenarioBehaviorPlanner vip;
	value = input();
	value.scenario = PublicScenarioKind::Vip;
	value.isVipKnown = true;
	value.isVip = true;
	value.vipZone = target();
	ok &= check(vip.update(value).action == ScenarioAction::Escape, "public VIP role uses known VIP escape zone");
	ScenarioBehaviorPlanner bomb;
	value = input();
	value.scenario = PublicScenarioKind::Bomb;
	value.bombPlantedKnown = true;
	value.bombPlanted = true;
	value.bomb = target();
	value.bomb.inUseRange = true;
	ok &= check(bomb.update(value).action == ScenarioAction::Move,
		"Idle assigns bomb approach; only MoveTo path completion may start defuse");
	value.frame.tick++;
	value.humanDefusingKnown = true;
	value.humanDefusing = true;
	ok &= check(bomb.update(value).action == ScenarioAction::Guard,
		"human defuse observation yields guarding instead of duplicate use");
	ScenarioBehaviorPlanner searchBomb;
	value = input();
	value.scenario = PublicScenarioKind::Bomb;
	value.bombPlantedKnown = true;
	value.bombPlanted = true;
	value.bombSearch = target();
	value.bombSearch.entity = {99U, 1U};
	result = searchBomb.update(value);
	ok &= check(result.action == ScenarioAction::Move && result.target.entity == value.bombSearch.entity,
		"CT with public plant notice but unknown bomb searches a known site without receiving hidden bomb position");
	value.plantedBombsite = target();
	value.plantedBombsite.entity = {100U, 1U};
	value.plantedBombsite.inUseRange = true;
	++value.frame.tick;
	result = searchBomb.update(value);
	ok &= check(result.action == ScenarioAction::Move && result.target.entity == value.plantedBombsite.entity &&
		nativeScenarioMoveTask(value, result) == CompatibilityTaskId::DefuseBomb,
		"A reported site directs the CT to its center without using the site as a bomb");
	ScenarioBehaviorPlanner lateBomb;
	value = input();
	value.scenario = PublicScenarioKind::Bomb;
	value.bombPlantedKnown = true;
	value.bombPlanted = true;
	value.bomb = target();
	value.escapeZone = target();
	value.escapeZone.entity = {88U, 1U};
	value.timerEstimated = true;
	value.bombSecondsRemaining = 2.0f;
	value.travelTimeEstimated = true;
	value.travelSecondsToBomb = 1.0f;
	value.defuseKitKnown = true;
	value.defuseKit = true;
	result = lateBomb.update(value);
	ok &= check(result.action == ScenarioAction::Escape && result.state == CompatibilityStateId::EscapeFromBomb &&
			!value.timerKnown && !value.travelTimeKnown,
		"explicit public timer and NAV travel estimates enable late bomb escape without claiming exact timer access");
	value = input();
	value.scenario = PublicScenarioKind::Bomb;
	value.team = astrabot::objectives::TeamRole::Terrorist;
	value.bombPlantedKnown = true;
	value.bombPlanted = true;
	value.bomb = target();
	value.escapeZone = target();
	value.timerEstimated = true;
	value.bombSecondsRemaining = 6.9f;
	value.aggression = 0.5f;
	ok &= check(ScenarioBehaviorPlanner::autoEscapeRequested(value),
		"T automatically escapes below reference aggression threshold");
	ScenarioBehaviorPlanner terroristEscape;
	ok &= check(terroristEscape.update(value).action == ScenarioAction::Escape,
		"both teams receive automatic EscapeFromBomb without needing a CT defuse travel estimate");
	value.attacking = true;
	ok &= check(
		!ScenarioBehaviorPlanner::autoEscapeRequested(value), "active firefight suppresses a new automatic escape");
	ScenarioBehaviorPlanner busyT;
	ok &= check(busyT.update(value).action == ScenarioAction::Guard,
		"attacking T retains bomb guard action instead of starting escape");
	value.attacking = false;
	value.currentDefusing = true;
	ok &= check(!ScenarioBehaviorPlanner::autoEscapeRequested(value), "current defuse suppresses a new escape");
	value.currentDefusing = false;
	value.team = astrabot::objectives::TeamRole::CounterTerrorist;
	value.aggression = 1.0f;
	value.defuseKitKnown = true;
	value.defuseKit = true;
	value.bombSecondsRemaining = 4.0f;
	ok &= check(
		!ScenarioBehaviorPlanner::autoEscapeRequested(value), "kit multiplies six-second aggressive threshold by 0.66");
	value.bombSecondsRemaining = 3.95f;
	ok &= check(ScenarioBehaviorPlanner::autoEscapeRequested(value), "kit-equipped CT escapes below 3.96 seconds");
	value.defuseKit = false;
	value.bombSecondsRemaining = 6.0f;
	ok &=
		check(!ScenarioBehaviorPlanner::autoEscapeRequested(value), "automatic escape threshold comparison is strict");
	value.bombSecondsRemaining = 5.99f;
	ok &= check(
		ScenarioBehaviorPlanner::autoEscapeRequested(value), "known no-kit CT uses unmodified reference threshold");
	value.defuseKitKnown = false;
	ok &= check(!ScenarioBehaviorPlanner::autoEscapeRequested(value),
		"unknown CT kit does not get converted into definite absence");
	value.bombSecondsRemaining = 3.95f;
	ok &= check(ScenarioBehaviorPlanner::autoEscapeRequested(value) && !value.defuseKitKnown,
		"unknown kit still permits escape below both possible thresholds without changing observation");
	value.alreadyEscapingFromBomb = true;
	value.attacking = true;
	ok &= check(
		!ScenarioBehaviorPlanner::autoEscapeRequested(value), "existing escape is not restarted as a new trigger");
	ScenarioBehaviorPlanner continuingEscape;
	ok &= check(continuingEscape.update(value).action == ScenarioAction::Escape,
		"existing escape movement continues without re-entering escape state");
	ScenarioBehaviorPlanner automatic;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	value.automaticLeader = {{3U, 1U}, value.team, true, true, true, target()};
	value.automaticFollowRequested = true;
	value.autoFollowPermissionInferred = true;
	result = automatic.update(value);
	ok &= check(result.action == ScenarioAction::Follow && result.followPermissionInferred && automatic.isFollowing(),
		"automatic visible human follow starts with explicit permission inference");
	value.frame.tick++;
	value.timestamp = 1.0f;
	value.automaticFollowRequested = false;
	result = automatic.update(value);
	ok &= check(result.action == ScenarioAction::Follow && automatic.followLeader() == value.automaticLeader.actor,
		"automatic leader identity persists independently of radio sender");
	value.frame.tick++;
	value.timestamp = 2.0f;
	value.stopFollowingRequested = true;
	result = automatic.update(value);
	ok &= check(!automatic.isFollowing() && result.action != ScenarioAction::Follow,
		"teamwork boredom stop releases automatic follow without fabricating leader death");
	ScenarioBehaviorPlanner radioPriority;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	value.timestamp = 2.0f;
	order = {1U, ScenarioRadioCommand::FollowMe, {2U, 1U}, value.team, true, 0.0f, target()};
	value.radio = &order;
	value.leader = {{2U, 1U}, value.team, true, true, true, target()};
	value.automaticLeader = {{3U, 1U}, value.team, true, true, true, target()};
	value.automaticFollowRequested = true;
	value.autoFollowPermissionInferred = true;
	result = radioPriority.update(value);
	ok &= check(result.action == ScenarioAction::Follow && radioPriority.followLeader() == order.sender &&
			!result.followPermissionInferred,
		"accepted explicit radio follow takes precedence over automatic candidate");
	ScenarioBehaviorPlanner followProjection;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	value.automaticLeader = {{3U, 1U}, value.team, true, true, true, target()};
	CompatibilityStateMachine followingMachine(value.actor);
	StateUpdateContext followingContext{};
	followingContext.actor = value.actor;
	followingContext.frame = value.frame;
	followingContext.timestamp = 0.0f;
	followingContext.fullUpdateSequence = 1U;
	followingContext.fullUpdate = true;
	followingContext.lifeAvailability = ObservationAvailability::Available;
	followingContext.alive = true;
	followingContext.roundAvailability = ObservationAvailability::Available;
	followingContext.roundActive = true;
	followingContext.transitionRequest = ObservationAvailability::Available;
	followingContext.attackObservation = ObservationAvailability::Available;
	followingContext.rngAvailability = ObservationAvailability::Available;
	ok &= check(followingMachine.initialize(followingContext) == StateResult::Transitioned,
		"follow state regression initializes real compatibility machine");
	StateBehaviorInput followBehavior{};
	followingContext.behavior = &followBehavior;
	for (unsigned int tick = 2U; tick <= 4U; ++tick)
	{
		value.frame.tick = tick;
		value.timestamp = static_cast<float>(tick) * 0.1f;
		value.automaticFollowRequested = tick == 2U;
		value.autoFollowPermissionInferred = true;
		result = followProjection.update(value);
		followBehavior.leaderAvailable = followProjection.hasFollowingLeader(value);
		followBehavior.goalAvailable = result.hasTarget;
		followingContext.frame = value.frame;
		followingContext.timestamp = value.timestamp;
		followingContext.fullUpdateSequence = tick;
		followingContext.requestTransition = followingMachine.state() != result.state;
		followingContext.requestedState = result.state;
		followingContext.transitionReason = StateTransitionReason::ObjectiveChanged;
		followingContext.transitionId = StateTransitionId::ExplicitStateChange;
		followingMachine.update(followingContext);
		ok &= check(followBehavior.leaderAvailable && followingMachine.state() == CompatibilityStateId::Follow,
			"automatic leader remains available through complete state-machine full updates instead of reverting to Idle");
	}
	ScenarioBehaviorPlanner teammateProjection;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	teammateProjection.update(value);
	const astrabot::world::ActorKey sameSerial{2U, 7U};
	teammateProjection.followTeammate(sameSerial, 1.0f);
	teammateProjection.followTeammate(sameSerial, 1.1f);
	ok &= check(teammateProjection.followSince() == 1.0f, "repeating a teammate leader preserves the follow timestamp");
	value.frame.tick++;
	value.timestamp = 1.2f;
	value.teammateLeader = {sameSerial, value.team, false, true, true, target()};
	value.teammateLeader.location.frame = value.frame;
	ok &= check(
		teammateProjection.update(value).action == ScenarioAction::Follow, "qualified managed leader is followed");
	value.frame.tick++;
	value.timestamp = 1.3f;
	value.teammateLeader = {};
	value.automaticLeader = {sameSerial, value.team, true, true, true, target()};
	value.leader = value.automaticLeader;
	value.automaticLeader.location.frame = value.frame;
	value.leader.location.frame = value.frame;
	ok &= check(teammateProjection.update(value).action == ScenarioAction::Stop && !teammateProjection.isFollowing(),
		"lost managed leader cannot match a human automatic or radio observation with the same identity key");
	ScenarioBehaviorPlanner staleRadio;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	value.timestamp = 0.1f;
	order = {1U, ScenarioRadioCommand::HoldPosition, {2U, 1U}, value.team, true, 0.1f, target()};
	value.radio = &order;
	value.radioSenderValidation = {order.sender, true, true};
	staleRadio.update(value);
	ok &= check(staleRadio.pendingRadioSender() == order.sender, "radio order waits for reaction delay");
	value.frame.tick++;
	value.timestamp = 2.0f;
	value.radio = nullptr;
	value.pendingRadioSenderValidation = {order.sender, true, false};
	result = staleRadio.update(value);
	ok &= check(result.action != ScenarioAction::Guard && !staleRadio.pendingRadioSender().isValid(),
		"pending radio is discarded when sender generation disconnects or changes before reaction");
	ScenarioBehaviorPlanner reusedSender;
	value = input();
	value.scenario = PublicScenarioKind::Unknown;
	value.timestamp = 2.0f;
	order = {1U, ScenarioRadioCommand::HoldPosition, {2U, 1U}, value.team, true, 0.0f, target()};
	value.radio = &order;
	value.radioSenderValidation = {order.sender, true, false};
	result = reusedSender.update(value);
	ok &=
		check(result.action != ScenarioAction::Guard, "stale current radio cannot issue hold to a reused sender slot");
	const auto oldSender = order.sender;
	order.sequence = 2U;
	order.sender = {2U, 2U};
	value.frame.tick++;
	value.timestamp = 2.1f;
	value.radioSenderValidation = {order.sender, true, true};
	value.pendingRadioSenderValidation = {oldSender, true, false};
	result = reusedSender.update(value);
	ok &= check(result.action == ScenarioAction::Guard,
		"invalid old sender validation does not discard a new valid generation's radio order");
	{
		using namespace astrabot;
		nav::NavDocument document;
		document.setSourceIdentity({5U, 100U, 200U});
		nav::NavArea area{};
		area.id = 1U;
		area.extent = {{0, 0, 0}, {300, 64, 0}};
		area.hidingSpots = {{1U, {100, 32, 0}, 1U}, {2U, {200, 32, 0}, 1U}};
		document.addArea(area);
		area.id = 2U;
		area.extent = {{400, 0, 0}, {500, 64, 0}};
		area.hidingSpots = {{3U, {450, 32, 0}, 1U}}; // Nearby but disconnected.
		document.addArea(area);
		nav::NavSnapshotPublisher publisher;
		ok &=
			check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published, "guard NAV fixture publishes");
		const RandomActor actor{1U, 1U};
		const RandomTimingContext timing{0U, 0U, 1U};
		ScriptedRandomSource random({RandomTapeEntry::longEntry("Hide.Spot", actor, timing, 0, 1, 0),
			RandomTapeEntry::floatEntry("Hide.Duration", actor, timing, 30, 60, 45),
			RandomTapeEntry::longEntry("Hide.Spot", actor, timing, 0, 0, 0),
			RandomTapeEntry::floatEntry("Hide.Duration", actor, timing, 30, 60, 45),
			RandomTapeEntry::longEntry("Hide.Spot", actor, timing, 0, 1, 0),
			RandomTapeEntry::floatEntry("Hide.Duration", actor, timing, 30, 60, 45)});
		BombGuardPlanner planner;
		nav::NavVector guardPoint{};
		const auto choose = [&](world::FrameIdentity frame, float now, const std::vector<nav::NavVector> &occupied)
		{
			return planner.update(publisher.snapshot(), frame, {7U, 1U}, {32, 32, 0}, {16, 32, 0}, false, occupied, now,
				random, actor, timing, &guardPoint);
		};
		ok &= check(choose({1, 1, 1}, 0, {}) && guardPoint.x == 100,
			"guard picks reachable NAV cover instead of C4 coordinates");
		ok &=
			check(!planner.needsSelection(publisher.snapshot(), {1, 1, 2}, {7, 1}, {32, 32, 0}, {16, 32, 0}, {}, 1.0f),
				"retained guard exposes a cheap selection gate for lazy path costs");
		ok &= check(choose({1, 1, 2}, 1, {}) && guardPoint.x == 100 && random.position() == 2U,
			"guard retains its chosen spot without redrawing every update");
		ok &= check(planner.workStats().selectionCalls == 1U && planner.workStats().pathSearches == 1U &&
				planner.workStats().nearestQueries == 2U,
			"retained guard performs no neighborhood, nearest-area, or path search work");
		ok &= check(planner.needsSelection(
						publisher.snapshot(), {1, 1, 3}, {7, 1}, {32, 32, 0}, {16, 32, 0}, {{100, 32, 0}}, 2.0f),
			"visible occupancy invalidates lazy guard selection gate");
		ok &= check(choose({1, 1, 3}, 2, {{100, 32, 0}}) && guardPoint.x == 200,
			"visible occupant forces a different camping position");
		ok &= check(choose({1, 2, 1}, 3, {}) && guardPoint.x == 100 && random.verifyComplete(),
			"new round discards the previous camping choice");
		planner.reset();
		nav::NavDocument noCover;
		noCover.setSourceIdentity({5U, 100U, 201U});
		area = {};
		area.id = 1U;
		area.extent = {{0, 0, 0}, {300, 64, 0}};
		noCover.addArea(area);
		publisher.publish(&noCover, 1U);
		ScriptedRandomSource fallback({RandomTapeEntry::floatEntry("Hide.Duration", actor, timing, 30, 60, 45)});
		ok &= check(planner.update(publisher.snapshot(), {1, 2, 2}, {7, 1}, {32, 32, 0}, {250, 32, 0}, false, {}, 4,
						fallback, actor, timing, &guardPoint) &&
				guardPoint.x == 250 && fallback.verifyComplete(),
			"absent cover holds current position instead of converging on the bomb");
	}
	for (unsigned int areaCount : {737U, 1800U})
	{
		using namespace astrabot;
		// A map-sized connected bomb neighborhood and a disconnected bot area.
		// Retrying an unreachable random cover every frame used to repeat the
		// complete neighborhood walk and path search on all 1,800 areas.
		nav::NavDocument document;
		document.setSourceIdentity({5U, 100U, 202U});
		for (unsigned int id = 1U; id <= areaCount; ++id)
		{
			nav::NavArea area{};
			area.id = id;
			const float x = static_cast<float>((id - 1U) % 45U) * 16.0f;
			const float y = static_cast<float>((id - 1U) / 45U) * 16.0f;
			area.extent = {{x, y, 0}, {x + 16, y + 16, 0}};
			if (id < areaCount - 1U)
				area.connections[0].push_back(id + 1U);
			if (id == 1U)
				area.hidingSpots = {{1U, {8, 8, 0}, 1U}};
			if (id == areaCount)
				area.extent = {{1000, 1000, 0}, {1016, 1016, 0}};
			document.addArea(area);
		}
		nav::NavSnapshotPublisher publisher;
		ok &= check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published,
			"map-sized unreachable guard fixture publishes");
		const RandomActor actor{1U, 1U};
		const RandomTimingContext timing{0U, 0U, 1U};
		std::vector<RandomTapeEntry> tape(24U, RandomTapeEntry::longEntry("Hide.Spot", actor, timing, 0, 0, 0));
		ScriptedRandomSource random(tape);
		BombGuardPlanner planner;
		nav::NavVector point{};
		const auto choose = [&](unsigned int tick, float now, const nav::NavPathCostContext &cost = {})
		{
			return planner.update(publisher.snapshot(), {1, 1, tick}, {7, 1}, {8, 8, 0}, {1008, 1008, 0}, false, {},
				now, random, actor, timing, &point, cost);
		};
		ok &= check(!choose(1U, 0.0f) && random.position() == 1U,
			"unreachable guard cover fails after the original single random selection");
		ok &= check(!choose(2U, 0.05f) && random.position() == 2U,
			"failed guard choice preserves the original random selection cadence");
		ok &= check(planner.workStats().pathSearches == 1U,
			"repeated unreachable guard choice reuses definitive topology failure");
		ok &= check(!choose(3U, 1.1f) && random.position() == 3U,
			"failed guard choice keeps drawing without repeating its unreachable route");
		ok &= check(planner.workStats().indexEntries == areaCount,
			"map-sized guard lookups reuse one index of the immutable NAV document");
		nav::NavPathCostContext changedCosts[8]{};
		changedCosts[0].health = 99;
		changedCosts[1].aggression = 0.5f;
		changedCosts[2].hostageEscortCount = 1U;
		changedCosts[3].attacking = true;
		changedCosts[4].zombie = true;
		changedCosts[5].areas = {{1U, 0.5f, 0U}};
		changedCosts[6].areas = {{1U, 0.5f, 1U}};
		changedCosts[7].areas = {{2U, 0.5f, 1U}};
		for (unsigned int i = 0U; i < 8U; ++i)
		{
			choose(4U + 2U * i, 1.2f + static_cast<float>(i) * 0.1f);
			ok &= check(!choose(5U + 2U * i, 1.25f + static_cast<float>(i) * 0.1f, changedCosts[i]) &&
					planner.workStats().pathSearches == 2U + 2U * i,
				"any changed path-cost input invalidates cached guard failure");
		}
		choose(20U, 2.0f, changedCosts[5]);
		ok &= check(!choose(21U, 2.05f, changedCosts[6]) && planner.workStats().pathSearches == 18U,
			"changed teammate count invalidates guard failure with identical area IDs");
		changedCosts[6].areas[0].danger = 0.75f;
		ok &= check(!choose(22U, 2.1f, changedCosts[6]) && planner.workStats().pathSearches == 19U,
			"changed danger invalidates guard failure with identical teammates");
		changedCosts[6].areas[0].area = 3U;
		ok &= check(!choose(23U, 2.15f, changedCosts[6]) && planner.workStats().pathSearches == 20U,
			"changed area identity invalidates guard failure with identical costs");
		planner.reset();
		ok &= check(!choose(24U, 2.2f) && random.verifyComplete() && planner.workStats().pathSearches == 1U,
			"explicit guard reset immediately permits another search");
	}
	{
		using namespace astrabot;
		nav::NavDocument document;
		document.setSourceIdentity({5U, 100U, 203U});
		nav::NavArea low{};
		low.id = 1U;
		low.extent = {{0, 0, 0}, {16, 16, 0}};
		low.hidingSpots = {{1U, {8, 8, 0}, 1U}};
		document.addArea(low);
		nav::NavArea high{};
		high.id = 2U;
		high.extent = {{0, 0, 400}, {16, 16, 400}};
		high.northEastZ = 400;
		high.southWestZ = 400;
		high.connections[0] = {1U};
		document.addArea(high);
		nav::NavSnapshotPublisher publisher;
		ok &= check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published,
			"guard health-dependent one-way drop fixture publishes");
		const RandomActor actor{1U, 1U};
		const RandomTimingContext timing{0U, 0U, 1U};
		ScriptedRandomSource random({RandomTapeEntry::longEntry("Hide.Spot", actor, timing, 0, 0, 0),
			RandomTapeEntry::longEntry("Hide.Spot", actor, timing, 0, 0, 0),
			RandomTapeEntry::floatEntry("Hide.Duration", actor, timing, 30, 60, 45)});
		BombGuardPlanner planner;
		nav::NavVector point{};
		nav::NavPathCostContext cost{};
		cost.health = 10;
		ok &= check(!planner.update(publisher.snapshot(), {1, 1, 1}, {7, 1}, {8, 8, 0}, {8, 8, 400}, false, {}, 0,
						random, actor, timing, &point, cost),
			"low health rejects lethal drop to guard cover");
		cost.health = 100;
		ok &= check(planner.update(publisher.snapshot(), {1, 1, 2}, {7, 1}, {8, 8, 0}, {8, 8, 400}, false, {}, 0.1f,
						random, actor, timing, &point, cost) &&
				point.z == 0 && random.verifyComplete() && planner.workStats().pathSearches == 2U,
			"changed health invalidates failed guard route and permits a now-survivable drop");
	}
	return ok ? 0 : 1;
}
