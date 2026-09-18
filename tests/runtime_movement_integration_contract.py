from pathlib import Path


def test_start_frame_dispatches_nav_movement():
	path = Path(__file__).parents[1] / "src" / "adapter" / "metamod" / "plugin_runtime.cpp"
	source = path.read_text(encoding="utf-8")
	start = source.index("void PluginRuntime::onStartFrame()")
	end = source.index("void PluginRuntime::onStartFramePost()", start)
	body = source[start:end]

	assert "updateManagedBotMovement" not in body

	post_start = source.index("void PluginRuntime::onStartFramePost()")
	post_body = source[post_start:]
	assert "updateManagedBotMovement" in post_body

	controller_start = source.index("void PluginRuntime::updateManagedBotMovement()")
	controller_body = source[controller_start:]
	assert "managedBotMovement_" in controller_body
	assert "NavRoamDecision roamDecision = {};" in controller_body
	assert "v.mins.z" in controller_body
	assert "&roamDecision" in controller_body
	assert "stage=%d currentArea=%u recoveryArea=%u" in controller_body
	assert "target=(%.1f %.1f %.1f)" in controller_body
	assert "intent=(%.2f %.2f %.2f)" in controller_body
	assert "observedVelocity=(%.1f %.1f %.1f)" in controller_body
	assert "corridorAreas=%u corridorIndex=%u" in controller_body
	assert "link=(%u->%u how=%u dir=%u)" in controller_body
	assert "nearestDistanceSquared=%.1f" in controller_body
	assert "inputDispatcher_.dispatchNext" in controller_body
	assert "ActionAdapter::projectMovement" in controller_body
	assert "actionDispatch.stopMovement" in controller_body
	assert "pfnSetClientMaxspeed" in controller_body
	assert "kDefaultManagedBotMaxSpeed" in source
	assert "movementViewAngles" in controller_body
	assert 'logMovementDiagnostic(index, "roam_no_intent", &roamDecision)' in controller_body
	assert "dispatchNeutralMovement" in controller_body
	assert "captureMovementPhysicsState" in controller_body
	assert "recordMovementPhysicsSample" in controller_body
	assert "movementReadyLogged_" in controller_body
	assert "kMovementPhysicsLogLimit" in source
	assert "movement physics actor=%u generation=%u frame=%u sequence=%u dispatched=%d" in source
	assert "ActorState::Joined" in controller_body
	assert "entityObjectiveCenter" in source
	assert "entity->v.absmin" in source
	assert "entity->v.absmax" in source
	assert "isPlantedC4Entity" in source
	assert "w_c4.mdl" in source
	assert "kC4WeaponBit = (1 << 6)" in source


if __name__ == "__main__":
	test_start_frame_dispatches_nav_movement()
