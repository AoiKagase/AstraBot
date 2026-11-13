#include "astrabot/objectives/plant_attempt.hpp"

#include <cstdio>
#include <limits>

namespace
{
bool check(bool value, const char *message)
{
	if (!value)
	{
		std::fprintf(stderr, "plant attempt failed: %s\n", message);
	}
	return value;
}
}

int main()
{
	using astrabot::objectives::PlantAttemptController;
	using astrabot::objectives::PlantAttemptResult;
	PlantAttemptController attempt;
	bool passed = check(attempt.evaluate(false, false, 10.0f) == PlantAttemptResult::OutsideSite,
		"an actor outside the site cannot start planting");
	passed = check(PlantAttemptController::canBeginObjective(true, true, true, true) &&
					 !PlantAttemptController::canBeginObjective(true, true, true, false),
				 "a carrying Terrorist requires an assigned available target") &&
		passed;
	passed = check(!PlantAttemptController::expired(4.99f) && PlantAttemptController::expired(5.0f) &&
					 !PlantAttemptController::expired(std::numeric_limits<float>::quiet_NaN()),
				 "elapsed-time boundaries retain the prior finite-time contract") &&
		passed;
	passed = check(attempt.evaluate(true, false, 10.0f) == PlantAttemptResult::Ready && !attempt.active(),
				 "a proposal does not start the delivered-input timer") &&
		passed;
	passed = check(attempt.evaluate(true, false, 100.0f) == PlantAttemptResult::Ready,
				 "a rejected or unsent proposal can wait without timing out") &&
		passed;
	attempt.recordDispatched(100.0f);
	attempt.recordDispatched(104.0f);
	passed = check(attempt.evaluate(true, false, 104.99f) == PlantAttemptResult::Continuing,
				 "repeated delivered input does not restart an active attempt") &&
		passed;
	passed = check(attempt.evaluate(true, false, 105.0f) == PlantAttemptResult::TimedOut && !attempt.active(),
				 "the delivered-input timeout includes exactly five seconds") &&
		passed;
	attempt.recordDispatched(105.5f);
	passed = check(!attempt.active() && attempt.evaluate(true, false, 105.5f) == PlantAttemptResult::RetryPending,
				 "input cannot restart the attempt during the retry interval") &&
		passed;
	passed = check(attempt.evaluate(true, false, 106.0f) == PlantAttemptResult::Ready,
				 "the one-second retry deadline is inclusive") &&
		passed;
	attempt.recordDispatched(106.0f);
	passed = check(attempt.evaluate(false, true, 106.1f) == PlantAttemptResult::Confirmed && !attempt.active(),
				 "public plant confirmation clears the attempt") &&
		passed;
	attempt.recordDispatched(107.0f);
	passed = check(attempt.evaluate(false, false, 107.1f) == PlantAttemptResult::OutsideSite && !attempt.active(),
				 "leaving the site cancels the attempt") &&
		passed;
	passed =
		check(attempt.evaluate(true, false, std::numeric_limits<float>::quiet_NaN()) == PlantAttemptResult::InvalidTime,
			"invalid time cannot authorize an attempt") &&
		passed;
	attempt.recordDispatched(108.0f);
	passed = check(attempt.evaluate(true, false, 107.0f) == PlantAttemptResult::Ready && !attempt.active(),
				 "a backwards clock clears an active attempt conservatively") &&
		passed;
	return passed ? 0 : 1;
}
