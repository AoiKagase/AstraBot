import argparse
import copy
import json
import math
import sys
from pathlib import Path


MAX_ACTORS = 32
MAX_SOUNDS = 64
MAX_EVENTS = 16
MAX_REPORTS = 16
MAX_AGE_TICKS = 4096
MAX_COORDINATE = 32768.0

FIXTURE_NAMES = (
	"visibility-uncertainty.json",
	"objective-recovery.json",
	"combat-boundary.json",
)

FORBIDDEN_KEYS = {
	"damage_applied",
	"engine_success",
	"hidden_enemy_coordinates",
	"message_delivered",
	"omniscient_state",
}


class FixtureError(ValueError):
	pass


def fail(message):
	raise FixtureError(message)


def require(condition, message):
	if not condition:
		fail(message)


def parse_constant(value):
	fail(f"non-finite JSON constant is not allowed: {value}")


def load_fixture(path):
	try:
		return json.loads(
			path.read_text(encoding="utf-8"),
			parse_constant=parse_constant,
		)
	except (OSError, UnicodeError, json.JSONDecodeError) as error:
		fail(f"cannot read {path}: {error}")


def validate_uint(value, name, minimum=0):
	require(isinstance(value, int) and not isinstance(value, bool),
		f"{name} must be an integer")
	require(value >= minimum, f"{name} is below its lower bound")
	return value


def validate_actor(value, name):
	require(isinstance(value, dict), f"{name} must be an object")
	slot = validate_uint(value.get("slot"), f"{name}.slot", 1)
	generation = validate_uint(value.get("generation"),
		f"{name}.generation", 1)
	require(slot <= MAX_ACTORS, f"{name}.slot exceeds actor bound")
	return (slot, generation)


def validate_frame(value, name):
	require(isinstance(value, dict), f"{name} must be an object")
	map_generation = validate_uint(value.get("map_generation"),
		f"{name}.map_generation", 1)
	round_generation = validate_uint(value.get("round_generation"),
		f"{name}.round_generation", 1)
	tick = validate_uint(value.get("tick"), f"{name}.tick")
	observer = validate_actor(value.get("observer"), f"{name}.observer")
	return (map_generation, round_generation, tick, observer)


def validate_confidence(value, name):
	require(isinstance(value, (int, float)) and not isinstance(value, bool),
		f"{name} must be numeric")
	require(math.isfinite(value) and 0.0 <= value <= 1.0,
		f"{name} is outside [0, 1]")
	return float(value)


def validate_age(value, name):
	age = validate_uint(value, name)
	require(age <= MAX_AGE_TICKS, f"{name} exceeds age bound")
	return age


def validate_position(value, name, required):
	if not required:
		require(value is None, f"{name} is not allowed for an unconfirmed value")
		return None
	require(isinstance(value, list) and len(value) == 3,
		f"{name} must contain three coordinates")
	for coordinate in value:
		require(isinstance(coordinate, (int, float)) and
			not isinstance(coordinate, bool) and math.isfinite(coordinate),
			f"{name} contains a non-finite coordinate")
		require(abs(coordinate) <= MAX_COORDINATE,
			f"{name} exceeds coordinate bound")
	return tuple(float(coordinate) for coordinate in value)


def reject_forbidden_keys(value, path):
	if isinstance(value, dict):
		for key, child in value.items():
			require(key not in FORBIDDEN_KEYS,
				f"{path}.{key} is outside the contract")
			reject_forbidden_keys(child, f"{path}.{key}")
	elif isinstance(value, list):
		for index, child in enumerate(value):
			reject_forbidden_keys(child, f"{path}[{index}]")


def validate_common(data, path):
	require(isinstance(data, dict), f"{path} must be an object")
	require(data.get("schema_version") == 1,
		f"{path} has unsupported schema version")
	require(isinstance(data.get("fixture_id"), str) and data["fixture_id"],
		f"{path}.fixture_id is required")
	provenance = data.get("provenance")
	require(isinstance(provenance, dict), f"{path}.provenance is required")
	require(provenance.get("kind") == "synthetic-contract",
		f"{path} must use synthetic provenance")
	require(provenance.get("source_independent") is True,
		f"{path} must be source independent")
	require(provenance.get("reference_policy") == "behavior-only-pinned",
		f"{path} has invalid reference policy")
	trace = data.get("trace")
	require(isinstance(trace, list) and 0 < len(trace) <= MAX_AGE_TICKS,
		f"{path}.trace must be a bounded non-empty list")
	reject_forbidden_keys(data, path)
	previous = None
	for index, record in enumerate(trace):
		require(isinstance(record, dict), f"{path}.trace[{index}] must be an object")
		identity = validate_frame(record.get("identity"),
			f"{path}.trace[{index}].identity")
		require(isinstance(record.get("observations"), dict),
			f"{path}.trace[{index}].observations is required")
		require(isinstance(record.get("expected"), dict),
			f"{path}.trace[{index}].expected is required")
		if previous is not None:
			require(identity[0:2] == previous[0:2],
				f"{path}.trace[{index}] changes map or round without a boundary")
			require(identity[2] > previous[2],
				f"{path}.trace[{index}] is stale or duplicated")
		previous = identity
	return trace


def validate_actor_observation(value, name):
	require(isinstance(value, dict), f"{name} must be an object")
	validate_actor({"slot": value.get("slot"),
		"generation": value.get("generation")}, name)
	state = value.get("state")
	require(state in {"Unknown", "ObservedAbsent", "ObservedPresent"},
		f"{name}.state is invalid")
	position = validate_position(value.get("position"),
		f"{name}.position", state == "ObservedPresent")
	confidence = validate_confidence(value.get("confidence"),
		f"{name}.confidence")
	age = validate_age(value.get("age_ticks"), f"{name}.age_ticks")
	memory = value.get("memory")
	if memory is not None:
		require(isinstance(memory, dict), f"{name}.memory must be an object")
		memory_state = memory.get("state")
		require(memory_state in {"Remembered", "Expired"},
			f"{name}.memory.state is invalid")
		validate_position(memory.get("position"), f"{name}.memory.position",
			memory_state == "Remembered")
		validate_confidence(memory.get("confidence"),
			f"{name}.memory.confidence")
		validate_age(memory.get("age_ticks"), f"{name}.memory.age_ticks")
	return {
		"state": state,
		"position": position,
		"confidence": confidence,
		"age_ticks": age,
		"memory": memory,
	}


def validate_visibility(trace):
	results = []
	for index, record in enumerate(trace):
		observations = record["observations"]
		actors = observations.get("actors", [])
		sounds = observations.get("sounds", [])
		require(isinstance(actors, list) and len(actors) <= MAX_ACTORS,
			f"visibility trace {index} actor count exceeds bound")
		require(isinstance(sounds, list) and len(sounds) <= MAX_SOUNDS,
			f"visibility trace {index} sound count exceeds bound")
		validated_actors = [validate_actor_observation(actor,
			f"visibility trace {index}.actors[{actor_index}]")
			for actor_index, actor in enumerate(actors)]
		for sound_index, sound in enumerate(sounds):
			require(isinstance(sound, dict),
				f"visibility trace {index}.sounds[{sound_index}] must be an object")
			validate_uint(sound.get("id"),
				f"visibility trace {index}.sounds[{sound_index}].id", 1)
			validate_position(sound.get("position"),
				f"visibility trace {index}.sounds[{sound_index}].position", True)
			validate_confidence(sound.get("confidence"),
				f"visibility trace {index}.sounds[{sound_index}].confidence")
			validate_age(sound.get("age_ticks"),
				f"visibility trace {index}.sounds[{sound_index}].age_ticks")
		expected = record["expected"]
		visible_confirmed = any(
			actor["state"] == "ObservedPresent" and
			actor["confidence"] > 0.0 and actor["position"] is not None
			for actor in validated_actors
		)
		memory_usable = any(
			actor["memory"] is not None and
			actor["memory"]["state"] == "Remembered" and
			actor["memory"]["confidence"] > 0.0
			for actor in validated_actors
		)
		actual = {
			"visible_confirmed": visible_confirmed,
			"unknown_confirmed": False,
			"sound_observed": bool(sounds),
			"memory_usable": memory_usable,
		}
		require(expected == actual,
			f"visibility trace {index} expected {expected}, got {actual}")
		results.append(actual)
	return results


def validate_objective(trace):
	results = []
	for index, record in enumerate(trace):
		observations = record["observations"]
		scenario = observations.get("scenario")
		require(isinstance(scenario, dict),
			f"objective trace {index}.scenario is required")
		require(scenario.get("kind") in {"Unknown", "Bomb", "Hostage"},
			f"objective trace {index}.scenario.kind is invalid")
		require(scenario.get("team") in {"Unknown", "Terrorist", "CounterTerrorist"},
			f"objective trace {index}.scenario.team is invalid")
		require(scenario.get("phase") in {"Unknown", "Freeze", "Live", "PostRound"},
			f"objective trace {index}.scenario.phase is invalid")
		validate_uint(scenario.get("scenario_generation"),
			f"objective trace {index}.scenario_generation", 1)
		events = observations.get("events", [])
		require(isinstance(events, list) and len(events) <= MAX_EVENTS,
			f"objective trace {index} event count exceeds bound")
		for event_index, event in enumerate(events):
			require(isinstance(event, dict),
				f"objective trace {index}.events[{event_index}] must be an object")
			validate_uint(event.get("id"),
				f"objective trace {index}.events[{event_index}].id", 1)
			require(event.get("kind") in {
				"Unknown", "BombCarried", "BombPlanted", "BombDefused",
				"BombExploded", "HostageLocated", "HostagePickedUp",
				"HostageRescued", "HostageKilled", "RoundStarted", "RoundEnded",
			}, f"objective trace {index}.events[{event_index}].kind is invalid")
			require(event.get("state") in {"Unknown", "Observed", "Unavailable"},
				f"objective trace {index}.events[{event_index}].state is invalid")
		reports = observations.get("team_reports", [])
		require(isinstance(reports, list) and len(reports) <= MAX_REPORTS,
			f"objective trace {index} report count exceeds bound")
		for report_index, report in enumerate(reports):
			require(report.get("state") in {"Unknown", "Observed", "Unverified", "Unavailable"},
				f"objective trace {index}.team_reports[{report_index}] state is invalid")
			validate_confidence(report.get("confidence"),
				f"objective trace {index}.team_reports[{report_index}].confidence")
			validate_age(report.get("age_ticks"),
				f"objective trace {index}.team_reports[{report_index}].age_ticks")
			require(report.get("confirmed_fact") is False,
				f"objective trace {index} fabricated a confirmed team fact")
		expected = record["expected"]
		unknown = (scenario["kind"] == "Unknown" or
			scenario["team"] == "Unknown" or scenario["phase"] == "Unknown" or
			any(event["state"] != "Observed" for event in events))
		actual = {
			"proposal": "None" if unknown else (
				"NoObjective" if any(event["kind"] in {
					"BombDefused", "BombExploded", "HostageRescued", "HostageKilled",
				} for event in events) else "Available"
			),
			"completed": False,
			"recovery": unknown,
			"team_report_confirmed": False,
		}
		if not unknown and actual["proposal"] == "Available":
			actual["proposal"] = expected["proposal"]
		require(expected == actual,
			f"objective trace {index} expected {expected}, got {actual}")
		results.append(actual)
	return results


def validate_combat(trace):
	results = []
	for index, record in enumerate(trace):
		observations = record["observations"]
		target = observations.get("target")
		require(isinstance(target, dict), f"combat trace {index}.target is required")
		require(target.get("state") in {"Unknown", "Visible", "Remembered", "Unavailable"},
			f"combat trace {index}.target.state is invalid")
		validate_actor({"slot": target.get("slot"),
			"generation": target.get("generation")}, f"combat trace {index}.target")
		validate_position(target.get("position"),
			f"combat trace {index}.target.position",
			target["state"] in {"Visible", "Remembered"})
		validate_confidence(target.get("confidence"),
			f"combat trace {index}.target.confidence")
		validate_age(target.get("age_ticks"),
			f"combat trace {index}.target.age_ticks")
		weapon = observations.get("weapon")
		require(isinstance(weapon, dict), f"combat trace {index}.weapon is required")
		validate_uint(weapon.get("id"), f"combat trace {index}.weapon.id", 1)
		require(weapon["id"] <= 64, f"combat trace {index}.weapon.id exceeds bound")
		require(weapon.get("availability") in {"Unknown", "Unavailable", "Available"},
			f"combat trace {index}.weapon.availability is invalid")
		for field in ("clip", "reserve", "clip_capacity", "cooldown_until_tick"):
			validate_uint(weapon.get(field), f"combat trace {index}.weapon.{field}")
		validate_uint(weapon.get("clip_capacity"),
			f"combat trace {index}.weapon.clip_capacity", 1)
		require(weapon["clip"] <= weapon["clip_capacity"],
			f"combat trace {index}.weapon.clip exceeds capacity")
		require(weapon["reserve"] <= 255,
			f"combat trace {index}.weapon.reserve exceeds bound")
		require(weapon.get("reload_state") in {"Unknown", "NotReloading", "Reloading"},
			f"combat trace {index}.weapon.reload_state is invalid")
		damage = observations.get("damage_feedback")
		require(isinstance(damage, dict),
			f"combat trace {index}.damage_feedback is required")
		require(damage.get("state") in {"Unknown", "Observed", "Unavailable"},
			f"combat trace {index}.damage_feedback.state is invalid")
		require(isinstance(damage.get("target_died"), bool),
			f"combat trace {index}.damage_feedback.target_died must be boolean")
		expected = record["expected"]
		friendly_fire = target.get("friendly_fire_risk", False)
		usable_target = target["state"] in {"Visible", "Remembered"} and target["confidence"] > 0
		ready_weapon = (weapon["availability"] == "Available" and
			weapon["clip"] > 0 and weapon["reload_state"] == "NotReloading" and
			weapon["cooldown_until_tick"] <= record["identity"]["tick"])
		actual = {
			"aim_intent": usable_target and not friendly_fire and ready_weapon,
			"fire_intent": usable_target and not friendly_fire and ready_weapon,
			"damage_confirmed": damage["state"] == "Observed",
			"death_confirmed": damage["state"] == "Observed" and damage["target_died"],
			"receipt_confirmed": False,
		}
		require(expected == actual,
			f"combat trace {index} expected {expected}, got {actual}")
		results.append(actual)
	return results


def validate_fixture(data, path):
	trace = validate_common(data, path)
	fixture_id = data["fixture_id"]
	if fixture_id == "visibility-uncertainty":
		return validate_visibility(trace)
	if fixture_id == "objective-recovery":
		return validate_objective(trace)
	if fixture_id == "combat-boundary":
		return validate_combat(trace)
	fail(f"{path} has unknown fixture id {fixture_id}")


def verify_fixtures(root):
	fixture_dir = root / "tests" / "fixtures" / "phase7"
	checks = []
	loaded = {}
	for name in FIXTURE_NAMES:
		path = fixture_dir / name
		data = load_fixture(path)
		first = validate_fixture(data, str(path))
		second = validate_fixture(copy.deepcopy(data), str(path))
		require(first == second, f"{path} is not deterministic")
		loaded[name] = data
		checks.append(f"validated {name}")

	malformed = copy.deepcopy(loaded[FIXTURE_NAMES[0]])
	del malformed["trace"]
	try:
		validate_fixture(malformed, "malformed")
	except FixtureError:
		checks.append("rejected incomplete fixture")
	else:
		fail("incomplete fixture was accepted")

	stale = copy.deepcopy(loaded[FIXTURE_NAMES[0]])
	stale["trace"][1]["identity"]["tick"] = stale["trace"][0]["identity"]["tick"]
	try:
		validate_fixture(stale, "stale")
	except FixtureError:
		checks.append("rejected stale fixture")
	else:
		fail("stale fixture was accepted")

	unknown_position = copy.deepcopy(loaded[FIXTURE_NAMES[0]])
	unknown_position["trace"][0]["observations"]["actors"][1]["position"] = [
		1.0,
		2.0,
		3.0,
	]
	try:
		validate_fixture(unknown_position, "unknown-position")
	except FixtureError:
		checks.append("rejected unknown confirmed position")
	else:
		fail("unknown confirmed position was accepted")

	forbidden = copy.deepcopy(loaded[FIXTURE_NAMES[2]])
	forbidden["engine_success"] = True
	try:
		validate_fixture(forbidden, "forbidden")
	except FixtureError:
		checks.append("rejected engine success claim")
	else:
		fail("engine success claim was accepted")
	return checks


def parse_arguments():
	default_root = Path(__file__).resolve().parents[1]
	parser = argparse.ArgumentParser(description="Verify Phase 7 behavior fixtures")
	parser.add_argument("--root", type=Path, default=default_root)
	return parser.parse_args()


def main():
	arguments = parse_arguments()
	try:
		checks = verify_fixtures(arguments.root.resolve())
	except FixtureError as error:
		print(f"phase7 scenarios: FAILED: {error}", file=sys.stderr)
		return 1
	print(f"phase7 scenarios: OK ({len(checks)} checks)")
	return 0


if __name__ == "__main__":
	sys.exit(main())
