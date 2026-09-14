// SPDX-License-Identifier: MPL-2.0
#include "nav/local/motion_envelope.hpp"
#include <cassert>
using namespace astrabot;
int main()
{
	nav::runtime::MovementSnapshot s;
	s.agent = {1};
	s.actor = {1, {1}};
	s.map = {1};
	s.tick = {2};
	s.kind = nav::runtime::ActorKind::ManagedBot;
	s.connected = s.alive = s.joined = true;
	s.position = nav::model::NavVector3{5, 2, 36};
	s.hull = nav::runtime::HullDimensions{{-16, -16, -36}, {16, 16, 36}};
	nav::local::MotionEnvelope envelope;
	envelope.binding = {s.agent, s.actor, s.map, 1, 0};
	envelope.tick = {1};
	envelope.origin = {0, 0, 36};
	envelope.target = {40, 0, 36};
	envelope.hull = *s.hull;
	envelope.maximumDisplacement = 64;
	envelope.validForUs = 120000;
	// Identity/freshness permit correctable lateral movement; physical
	// clearance is independently rechecked at dispatch, not inferred here.
	assert(envelope.matches(s, 10000));
	s.actor.generation = {2};
	assert(!envelope.matches(s, 10000));
	s.actor.generation = {1};
	assert(!envelope.matches(s, 120001));
	s.position = nav::model::NavVector3{65, 0, 36};
	assert(!envelope.matches(s, 10000));
	s.position = nav::model::NavVector3{5, 2, 36};
	s.alive = false;
	assert(!envelope.matches(s, 10000));
}
