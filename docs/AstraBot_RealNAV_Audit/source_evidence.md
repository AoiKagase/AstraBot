# 添付ソースの行番号付き根拠

基準: AstraBot(1).zip。製品コード変更なし。各範囲の番号は実ソースの行番号。


## `src/core/nav/locomotion.cpp:189–205`

```cpp
  189 bool LocomotionController::recordProgress(const NavVector &position)
  190 {
  191 	if (hasLastPosition_)
  192 	{
  193 		if (horizontalDistance(lastPosition_, position) <
  194 				kMinimumProgressDistance)
  195 		{
  196 			++stuckFrames_;
  197 		}
  198 		else
  199 		{
  200 			stuckFrames_ = 0U;
  201 		}
  202 	}
  203 	lastPosition_ = position;
  204 	hasLastPosition_ = true;
  205 	return stuckFrames_ >= config_.stuckFrameLimit;
```


## `src/core/nav/locomotion.cpp:211–268`

```cpp
  211 	const NavAreaMatch &currentMatch,
  212 	const NavVector &target,
  213 	AreaId targetArea,
  214 	LocomotionIntent *intent)
  215 {
  216 	const NavDocument *document = snapshot.document();
  217 	const NavArea *currentArea = document->findArea(currentMatch.area);
  218 	const NavArea *destinationArea = document->findArea(targetArea);
  219 	if (currentArea == nullptr || destinationArea == nullptr)
  220 	{
  221 		active_ = false;
  222 		return LocomotionResult::InvalidCorridor;
  223 	}
  224 
  225 	const float stepHeight =
  226 		floorHeight(*destinationArea) - floorHeight(*currentArea);
  227 	if (stepHeight > config_.maximumStepHeight)
  228 	{
  229 		active_ = false;
  230 		return LocomotionResult::StepTooHigh;
  231 	}
  232 
  233 	const bool requiresJump = (destinationArea->attributes & NavArea::kJump) != 0U;
  234 	const bool requiresCrouch =
  235 			(destinationArea->attributes & NavArea::kCrouch) != 0U;
  236 	LocomotionPosture posture = LocomotionPosture::Standing;
  237 	if (!requiresJump &&
  238 			(requiresCrouch || observation.standingClearance < config_.requiredClearance))
  239 	{
  240 		if (observation.crouchingClearance < config_.requiredClearance)
  241 		{
  242 			active_ = false;
  243 			return LocomotionResult::NeedsRecovery;
  244 		}
  245 		posture = LocomotionPosture::Crouching;
  246 	}
  247 
  248 	if (recordProgress(observation.position))
  249 	{
  250 		active_ = false;
  251 		return LocomotionResult::Stuck;
  252 	}
  253 
  254 	intent->direction = normalizeDirection(observation.position, target);
  255 	intent->speed = config_.maximumSpeed;
  256 	intent->posture = posture;
  257 	intent->traversal = requiresJump
  258 			? TraversalAction::Jump
  259 			: (posture == LocomotionPosture::Crouching
  260 					? TraversalAction::Crouch
  261 					: (stepHeight > 0.0f ? TraversalAction::Step : TraversalAction::Walk));
  262 	intent->stepUp = stepHeight > 0.0f && !requiresJump;
  263 	intent->currentArea = currentArea->id;
  264 	intent->targetArea = targetArea;
  265 	return LocomotionResult::IntentReady;
  266 }
  267 
  268 bool LocomotionController::isActive() const
```


## `src/core/nav/locomotion.cpp:311–324`

```cpp
  311 float LocomotionController::floorHeight(const NavArea &area)
  312 {
  313 	return area.northEastZ * 0.5f + area.southWestZ * 0.5f;
  314 }
  315 
  316 NavVector LocomotionController::normalizeDirection(
  317 	const NavVector &from,
  318 	const NavVector &to)
  319 {
  320 	const float deltaX = to.x - from.x;
  321 	const float deltaY = to.y - from.y;
  322 	const float distance = std::hypot(deltaX, deltaY);
  323 	if (!std::isfinite(distance) || distance <= 0.0f)
  324 	{
```


## `src/core/nav/nav_query.cpp:44–65`

```cpp
   44 float floorDistance(const NavArea &area, float z)
   45 {
   46 	const float low = std::min(area.northEastZ, area.southWestZ);
   47 	const float high = std::max(area.northEastZ, area.southWestZ);
   48 	if (z < low)
   49 	{
   50 		return low - z;
   51 	}
   52 	if (z > high)
   53 	{
   54 		return z - high;
   55 	}
   56 	return 0.0f;
   57 }
   58 
   59 NavVector centerOf(const NavArea &area)
   60 {
   61 	return {
   62 		area.extent.lo.x * 0.5f + area.extent.hi.x * 0.5f,
   63 		area.extent.lo.y * 0.5f + area.extent.hi.y * 0.5f,
   64 		area.northEastZ * 0.5f + area.southWestZ * 0.5f
   65 	};
```


## `src/core/nav/nav_query.cpp:93–112`

```cpp
   93 NavVector closestPointOnArea(
   94 	const NavArea &area,
   95 	const NavVector &position)
   96 {
   97 	NavVector point = position;
   98 	point.x = (std::max)(area.extent.lo.x, (std::min)(point.x, area.extent.hi.x));
   99 	point.y = (std::max)(area.extent.lo.y, (std::min)(point.y, area.extent.hi.y));
  100 	point.z = area.northEastZ * 0.5f + area.southWestZ * 0.5f;
  101 	return point;
  102 }
  103 
  104 bool isWithinTarget(
  105 		const NavArea &area,
  106 		const NavVector &position,
  107 	float horizontalTolerance,
  108 	float verticalTolerance)
  109 {
  110 	return rectangleDistanceSquared(area, position) <=
  111 			horizontalTolerance * horizontalTolerance &&
  112 			floorDistance(area, position.z) <= verticalTolerance;
```


## `src/core/nav/nav_query.cpp:224–264`

```cpp
  224 }
  225 
  226 std::uint8_t howForLink(
  227 	const NavDocument *document,
  228 	AreaId fromArea,
  229 	AreaId toArea)
  230 {
  231 	if (document == nullptr)
  232 	{
  233 		return 0U;
  234 	}
  235 	const NavArea *from = document->findArea(fromArea);
  236 	if (from != nullptr)
  237 	{
  238 		for (const NavApproach &approach : from->approaches)
  239 		{
  240 			if (approach.here == fromArea && approach.next == toArea)
  241 			{
  242 				return approach.hereToNextHow;
  243 			}
  244 		}
  245 	}
  246 	const NavArea *to = document->findArea(toArea);
  247 	if (to != nullptr)
  248 	{
  249 		for (const NavApproach &approach : to->approaches)
  250 		{
  251 			if (approach.here == toArea && approach.previous == fromArea)
  252 			{
  253 				return approach.previousToHereHow;
  254 			}
  255 		}
  256 	}
  257 	return 0U;
  258 }
  259 
  260 NavDirectedLink directedLinkFor(
  261 	const NavDocument *document,
  262 	AreaId fromArea,
  263 	AreaId toArea)
  264 {
```


## `src/core/nav/nav_query.cpp:341–453`

```cpp
  341 	float floorTolerance,
  342 	NavAreaMatch *match) const
  343 {
  344 	if (match == nullptr)
  345 	{
  346 		return NavQueryResult::InvalidArgument;
  347 	}
  348 	if (document() == nullptr)
  349 	{
  350 		return NavQueryResult::EmptySnapshot;
  351 	}
  352 	if (!isFinitePosition(position))
  353 	{
  354 		return NavQueryResult::InvalidPosition;
  355 	}
  356 	if (!isFiniteTolerance(floorTolerance))
  357 	{
  358 		return NavQueryResult::InvalidTolerance;
  359 	}
  360 
  361 	bool found = false;
  362 	AreaId bestArea = 0U;
  363 	float bestVerticalDistance = std::numeric_limits<float>::max();
  364 	for (const NavArea &area : document()->areas())
  365 	{
  366 		if (position.x < area.extent.lo.x ||
  367 				position.x > area.extent.hi.x ||
  368 				position.y < area.extent.lo.y ||
  369 				position.y > area.extent.hi.y)
  370 		{
  371 			continue;
  372 		}
  373 
  374 		const float candidateDistance = floorDistance(area, position.z);
  375 		if (candidateDistance > floorTolerance ||
  376 				(found && !isBetterMatch(
  377 					candidateDistance,
  378 					area.id,
  379 					bestVerticalDistance,
  380 					bestArea)))
  381 		{
  382 			continue;
  383 		}
  384 		found = true;
  385 		bestArea = area.id;
  386 		bestVerticalDistance = candidateDistance;
  387 	}
  388 
  389 	if (!found)
  390 	{
  391 		return NavQueryResult::NoAreaContaining;
  392 	}
  393 
  394 	const NavArea *area = document()->findArea(bestArea);
  395 	match->area = bestArea;
  396 	match->distanceSquared = rectangleDistanceSquared(*area, position);
  397 	match->closestPoint = closestPointOnArea(*area, position);
  398 	return NavQueryResult::Found;
  399 }
  400 
  401 NavQueryResult NavQuery::findNearest(
  402 	const NavVector &position,
  403 	float maximumDistance,
  404 	NavAreaMatch *match) const
  405 {
  406 	if (match == nullptr)
  407 	{
  408 		return NavQueryResult::InvalidArgument;
  409 	}
  410 	if (document() == nullptr)
  411 	{
  412 		return NavQueryResult::EmptySnapshot;
  413 	}
  414 	if (!isFinitePosition(position))
  415 	{
  416 		return NavQueryResult::InvalidPosition;
  417 	}
  418 	if (!isFiniteTolerance(maximumDistance) || maximumDistance < 0.0f)
  419 	{
  420 		return NavQueryResult::InvalidTolerance;
  421 	}
  422 
  423 	const float maximumDistanceSquared = maximumDistance * maximumDistance;
  424 	bool found = false;
  425 	AreaId bestArea = 0U;
  426 	float bestDistance = std::numeric_limits<float>::max();
  427 	for (const NavArea &area : document()->areas())
  428 	{
  429 		const float candidateDistance =
  430 			rectangleDistanceSquared(area, position);
  431 		if (candidateDistance > maximumDistanceSquared ||
  432 				(found && !isBetterMatch(
  433 					candidateDistance,
  434 					area.id,
  435 					bestDistance,
  436 					bestArea)))
  437 		{
  438 			continue;
  439 		}
  440 		found = true;
  441 		bestArea = area.id;
  442 		bestDistance = candidateDistance;
  443 	}
  444 
  445 	if (!found)
  446 	{
  447 		return NavQueryResult::NoAreaContaining;
  448 	}
  449 
  450 	match->area = bestArea;
  451 	match->distanceSquared = bestDistance;
  452 	match->closestPoint = closestPointOnArea(*document()->findArea(bestArea), position);
  453 	return NavQueryResult::Found;
```


## `src/core/nav/nav_query.cpp:938–987`

```cpp
  938 	const NavArea *area = currentDocument->findArea(corridor_[currentIndex_]);
  939 	if (area == nullptr)
  940 	{
  941 		return NavFollowerResult::InvalidCorridor;
  942 	}
  943 
  944 	NavVector currentTarget = portalSteeringPoint(*area, position);
  945 	if (currentIndex_ > 0U)
  946 	{
  947 		const NavArea *previousArea =
  948 				currentDocument->findArea(corridor_[currentIndex_ - 1U]);
  949 		if (previousArea != nullptr)
  950 		{
  951 			const std::uint8_t direction = currentIndex_ - 1U < links_.size()
  952 				? links_[currentIndex_ - 1U].direction
  953 				: NavArea::kDirectionCount;
  954 			currentTarget = portalSteeringPoint(
  955 				*previousArea, *area, position, direction);
  956 		}
  957 	}
  958 	*target = currentTarget;
  959 	*targetArea = area->id;
  960 	if (!isWithinTarget(
  961 			*area,
  962 			position,
  963 			horizontalTolerance,
  964 			verticalTolerance))
  965 	{
  966 		return NavFollowerResult::TargetReady;
  967 	}
  968 	if (currentIndex_ + 1U < corridor_.size())
  969 	{
  970 		++currentIndex_;
  971 		const NavArea *nextArea =
  972 			currentDocument->findArea(corridor_[currentIndex_]);
  973 		if (nextArea == nullptr)
  974 		{
  975 			return NavFollowerResult::InvalidCorridor;
  976 		}
  977 		const NavArea *previousArea =
  978 				currentDocument->findArea(corridor_[currentIndex_ - 1U]);
  979 		const std::uint8_t direction = currentIndex_ - 1U < links_.size()
  980 				? links_[currentIndex_ - 1U].direction
  981 				: NavArea::kDirectionCount;
  982 		*target = previousArea != nullptr
  983 				? portalSteeringPoint(*previousArea, *nextArea, position, direction)
  984 				: portalSteeringPoint(*nextArea, position);
  985 		*targetArea = nextArea->id;
  986 		return NavFollowerResult::Advanced;
  987 	}
```


## `src/core/runtime/nav_roam_controller.cpp:44–127`

```cpp
   44 		decision->corridorResult = nav::NavQueryResult::InvalidArgument;
   45 		decision->locomotionResult = nav::LocomotionResult::InvalidArgument;
   46 	}
   47 
   48 	bool buildRecoveryIntent(
   49 		const nav::NavAreaMatch &area,
   50 		const nav::LocomotionObservation &observation,
   51 		nav::LocomotionIntent *intent)
   52 	{
   53 		if (intent == nullptr)
   54 		{
   55 			return false;
   56 		}
   57 		const nav::NavVector delta = {
   58 			area.closestPoint.x - observation.position.x,
   59 			area.closestPoint.y - observation.position.y,
   60 			area.closestPoint.z - observation.position.z};
   61 		const float length = std::sqrt(
   62 			delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
   63 		if (!std::isfinite(length) || length <= 0.0f)
   64 		{
   65 			return false;
   66 		}
   67 		intent->direction = {
   68 			delta.x / length,
   69 			delta.y / length,
   70 			delta.z / length};
   71 		intent->speed = kRoamSpeed;
   72 		intent->posture = nav::LocomotionPosture::Standing;
   73 		intent->stepUp = false;
   74 		intent->currentArea = area.area;
   75 		intent->targetArea = area.area;
   76 		return true;
   77 	}
   78 
   79 	float floorHeight(const nav::NavArea &area)
   80 	{
   81 		return area.northEastZ * 0.5f + area.southWestZ * 0.5f;
   82 	}
   83 
   84 	nav::NavVector centerOf(const nav::NavArea &area)
   85 	{
   86 		return {
   87 			area.extent.lo.x * 0.5f + area.extent.hi.x * 0.5f,
   88 			area.extent.lo.y * 0.5f + area.extent.hi.y * 0.5f,
   89 			floorHeight(area)};
   90 	}
   91 
   92 	nav::TraversalAction traversalActionFor(
   93 		const nav::NavSnapshot &snapshot,
   94 		const nav::NavDirectedLink &link)
   95 	{
   96 		if (link.how == 4U || link.how == 5U)
   97 		{
   98 			return nav::TraversalAction::Ladder;
   99 		}
  100 		if (link.how == 6U)
  101 		{
  102 			const nav::NavDocument *document = snapshot.document();
  103 			if (document != nullptr)
  104 			{
  105 				const nav::NavArea *from = document->findArea(link.fromArea);
  106 				const nav::NavArea *to = document->findArea(link.toArea);
  107 				if (from != nullptr && to != nullptr && floorHeight(*to) < floorHeight(*from))
  108 				{
  109 					return nav::TraversalAction::Drop;
  110 				}
  111 			}
  112 			return nav::TraversalAction::Jump;
  113 		}
  114 		return nav::TraversalAction::Walk;
  115 	}
  116 }
  117 
  118 void addSearchStats(nav::NavSearchStats *total, const nav::NavSearchStats &sample)
  119 {
  120 	if (total == nullptr)
  121 	{
  122 		return;
  123 	}
  124 	total->expandedUniqueAreas += sample.expandedUniqueAreas;
  125 	total->enqueueCount += sample.enqueueCount;
  126 	total->reopenCount += sample.reopenCount;
  127 	total->staleQueueEntries += sample.staleQueueEntries;
```


## `src/core/runtime/nav_roam_controller.cpp:414–507`

```cpp
  414 			 observation.locomotion.position.z},
  415 			64.0f,
  416 			&currentArea);
  417 	if (decision != nullptr)
  418 	{
  419 		decision->currentAreaResult = currentAreaResult;
  420 	}
  421 	nav::AreaId objectiveArea = 0U;
  422 	if (observation.hasObjectiveTarget)
  423 	{
  424 		nav::NavAreaMatch objectiveMatch = {};
  425 		if (query.findNearest(
  426 				observation.objectiveTarget,
  427 				kMaximumRecoveryDistance,
  428 				&objectiveMatch) == nav::NavQueryResult::Found)
  429 		{
  430 			objectiveArea = objectiveMatch.area;
  431 			if (decision != nullptr)
  432 			{
  433 				decision->goalPresent = true;
  434 				decision->goalKind = NavGoalKind::Objective;
  435 				decision->goalArea = objectiveArea;
  436 				decision->goalPosition = observation.objectiveTarget;
  437 			}
  438 		}
  439 		else if (decision != nullptr)
  440 		{
  441 			decision->failureReason = NavFailureReason::GoalAreaMissing;
  442 		}
  443 	}
  444 	if (currentAreaResult == nav::NavQueryResult::NoAreaContaining &&
  445 			!jumpDrop_.isActive() && !specialTraversal_.isActive())
  446 	{
  447 		const nav::NavQueryResult nearestResult = query.findNearest(
  448 				{observation.locomotion.position.x,
  449 				 observation.locomotion.position.y,
  450 				 observation.locomotion.position.z},
  451 			kMaximumRecoveryDistance,
  452 			&currentArea);
  453 		if (decision != nullptr)
  454 		{
  455 			decision->nearestAreaResult = nearestResult;
  456 			decision->recoveryArea = nearestResult == nav::NavQueryResult::Found ?
  457 				currentArea.area : 0U;
  458 			decision->nearestDistanceSquared = currentArea.distanceSquared;
  459 		}
  460 		if (nearestResult != nav::NavQueryResult::Found ||
  461 			!buildRecoveryIntent(currentArea, observation.locomotion, intent))
  462 		{
  463 			if (!hasActiveRoute_)
  464 			{
  465 				resetRoute();
  466 			}
  467 			if (decision != nullptr)
  468 			{
  469 				decision->failureReason = NavFailureReason::CurrentAreaMissing;
  470 				decision->stage = NavRoamStage::Failed;
  471 			}
  472 			return NavRoamResult::NoRoute;
  473 		}
  474 		if (decision != nullptr)
  475 		{
  476 			decision->failureReason = NavFailureReason::CurrentAreaMissing;
  477 			decision->stage = NavRoamStage::OffMeshRecovery;
  478 			decision->targetArea = currentArea.area;
  479 		}
  480 		return NavRoamResult::IntentReady;
  481 	}
  482 	if (currentAreaResult != nav::NavQueryResult::Found &&
  483 			!jumpDrop_.isActive() && !specialTraversal_.isActive())
  484 	{
  485 		if (!hasActiveRoute_)
  486 		{
  487 			resetRoute();
  488 		}
  489 		if (decision != nullptr)
  490 		{
  491 			decision->failureReason = NavFailureReason::CurrentAreaMissing;
  492 			decision->stage = NavRoamStage::Failed;
  493 		}
  494 		return NavRoamResult::NoRoute;
  495 	}
  496 	if (decision != nullptr)
  497 	{
  498 		decision->stage = NavRoamStage::ExactArea;
  499 		decision->currentArea = currentArea.area;
  500 	}
  501 	if (observation.hasObjectiveTarget && objectiveArea == 0U)
  502 	{
  503 		if (!hasActiveRoute_)
  504 		{
  505 			resetRoute();
  506 		}
  507 		if (decision != nullptr)
```


## `src/core/runtime/nav_roam_controller.cpp:942–997`

```cpp
  942 bool NavRoamController::startTraversal(
  943 	const nav::NavSnapshot &snapshot,
  944 	const nav::NavCorridor &corridor,
  945 	const nav::NavDirectedLink &link)
  946 {
  947 	const nav::NavDocument *document = snapshot.document();
  948 	if (document == nullptr || corridor.areas.size() < 2U)
  949 	{
  950 		return false;
  951 	}
  952 	const nav::NavArea *fromArea = document->findArea(link.fromArea);
  953 	const nav::NavArea *toArea = document->findArea(link.toArea);
  954 	if (fromArea == nullptr || toArea == nullptr)
  955 	{
  956 		return false;
  957 	}
  958 	activeTraversal_ = traversalActionFor(snapshot, link);
  959 	const nav::NavVector launchPosition = centerOf(*fromArea);
  960 	const nav::NavVector landingPosition = centerOf(*toArea);
  961 	const float verticalDelta = floorHeight(*toArea) - floorHeight(*fromArea);
  962 	const float horizontalDeltaX = landingPosition.x - launchPosition.x;
  963 	const float horizontalDeltaY = landingPosition.y - launchPosition.y;
  964 	const float horizontalReach = std::sqrt(
  965 		horizontalDeltaX * horizontalDeltaX + horizontalDeltaY * horizontalDeltaY);
  966 
  967 	if (activeTraversal_ == nav::TraversalAction::Jump ||
  968 			activeTraversal_ == nav::TraversalAction::Drop)
  969 	{
  970 		nav::JumpDropEnvelope envelope = {};
  971 		envelope.kind = activeTraversal_ == nav::TraversalAction::Jump
  972 				? nav::JumpDropKind::Jump
  973 				: nav::JumpDropKind::Drop;
  974 		envelope.launch = {launchPosition, link.fromArea};
  975 		envelope.landing = {landingPosition, link.toArea};
  976 		envelope.maximumRise = (std::max)(16.0f, verticalDelta + 16.0f);
  977 		envelope.maximumDrop = (std::max)(16.0f, -verticalDelta + 16.0f);
  978 		envelope.horizontalReach = (std::max)(16.0f, horizontalReach + 16.0f);
  979 		envelope.damageRisk = {4096.0f, 100.0f};
  980 		return jumpDrop_.start(corridor, envelope, actor_.actorGeneration) ==
  981 				nav::JumpDropResult::Ready;
  982 	}
  983 
  984 	if (activeTraversal_ == nav::TraversalAction::Ladder)
  985 	{
  986 		nav::SpecialTraversalCapability capability = {};
  987 		capability.kind = nav::SpecialTraversalKind::Ladder;
  988 		capability.entry = {launchPosition, link.fromArea};
  989 		capability.exit = {landingPosition, link.toArea};
  990 		capability.sourceDirection = link.direction;
  991 		capability.requiredClearance = 36.0f;
  992 		capability.minimumPosture = nav::SpecialTraversalPosture::Standing;
  993 		return specialTraversal_.start(corridor, capability, actor_.actorGeneration) ==
  994 				nav::SpecialTraversalResult::Ready;
  995 	}
  996 
  997 	return locomotion_.start(corridor) == nav::LocomotionResult::Started;
```


## `src/core/runtime/nav_roam_controller.cpp:1063–1104`

```cpp
 1063 	if (objectiveArea != 0U && objectiveArea != currentArea.area)
 1064 	{
 1065 		if (decision != nullptr)
 1066 		{
 1067 			decision->goalPresent = true;
 1068 			decision->goalKind = NavGoalKind::Objective;
 1069 			decision->goalArea = objectiveArea;
 1070 			decision->goalPosition = objectiveTarget_;
 1071 			decision->pathRequested = true;
 1072 		}
 1073 		nav::NavCorridor corridor = {};
 1074 		const nav::NavQueryResult corridorResult = buildCorridor(
 1075 			query, currentArea.area, objectiveArea, &corridor,
 1076 			decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
 1077 		if (decision != nullptr)
 1078 		{
 1079 			decision->corridorResult = corridorResult;
 1080 			decision->pathResult = corridorResult;
 1081 		}
 1082 		if (corridorResult == nav::NavQueryResult::Found && corridor.areas.size() >= 2U)
 1083 		{
 1084 			const nav::AreaId nextArea = corridor.areas[1U];
 1085 			nav::NavDirectedLink link = {currentArea.area, nextArea, 0U, 0U};
 1086 			for (const nav::NavDirectedLink &candidate : links)
 1087 			{
 1088 				if (candidate.toArea == nextArea)
 1089 				{
 1090 					link = candidate;
 1091 					break;
 1092 				}
 1093 			}
 1094 			if (startTraversal(snapshot, corridor, link))
 1095 			{
 1096 				nextLinkIndex_ = 0U;
 1097 				rememberRoute(snapshot, corridor, link);
 1098 				if (decision != nullptr)
 1099 				{
 1100 					populateRouteDecision(decision);
 1101 					decision->targetArea = objectiveArea;
 1102 					decision->targetPosition = objectiveTarget_;
 1103 				}
 1104 				return true;
```


## `src/core/runtime/nav_roam_controller.cpp:744–777`

```cpp
  744 	const nav::LocomotionResult locomotionResult = locomotion_.update(
  745 		snapshot,
  746 		observation.locomotion,
  747 		intent);
  748 	if (hasActiveRoute_)
  749 	{
  750 		activeCorridorIndex_ = locomotion_.currentCorridorIndex();
  751 		if (!activeCorridor_.areas.empty() &&
  752 				activeCorridorIndex_ >= activeCorridor_.areas.size())
  753 		{
  754 			activeCorridorIndex_ = activeCorridor_.areas.size() - 1U;
  755 		}
  756 	}
  757 	if (decision != nullptr)
  758 	{
  759 		decision->locomotionResult = locomotionResult;
  760 		if (intent->targetArea != 0U)
  761 		{
  762 			decision->targetArea = intent->targetArea;
  763 		}
  764 		if (locomotionResult == nav::LocomotionResult::IntentReady)
  765 		{
  766 			decision->intentDirection = intent->direction;
  767 		}
  768 	}
  769 	if (locomotionResult == nav::LocomotionResult::IntentReady)
  770 	{
  771 		lastIntentDirection_ = intent->direction;
  772 	}
  773 	if (locomotionResult == nav::LocomotionResult::IntentReady &&
  774 			activeTraversal_ != nav::TraversalAction::Walk)
  775 	{
  776 		intent->traversal = activeTraversal_;
  777 	}
```


## `src/core/runtime/nav_roam_controller.cpp:1208–1263`

```cpp
 1208 bool NavRoamController::selectRoamRoute(
 1209 	const nav::NavSnapshot &snapshot,
 1210 	const nav::NavAreaMatch &currentArea,
 1211 	NavRoamDecision *decision)
 1212 {
 1213 	const nav::NavDocument *document = snapshot.document();
 1214 	if (document == nullptr || document->areas().empty())
 1215 	{
 1216 		return false;
 1217 	}
 1218 	nav::NavQuery query(snapshot);
 1219 	std::vector<nav::NavDirectedLink> links;
 1220 	if (query.outgoingLinks(currentArea.area, &links) != nav::NavQueryResult::Found)
 1221 	{
 1222 		return false;
 1223 	}
 1224 	if (!modePolicy_.allowsAdaptiveRouteWeighting())
 1225 	{
 1226 		for (const nav::NavDirectedLink &link : links)
 1227 		{
 1228 			if (decision != nullptr)
 1229 			{
 1230 				const nav::NavArea *target = document->findArea(link.toArea);
 1231 				decision->goalPresent = target != nullptr;
 1232 				decision->goalKind = NavGoalKind::Roam;
 1233 				decision->goalArea = link.toArea;
 1234 				decision->goalPosition = target != nullptr
 1235 					? centerOf(*target) : nav::NavVector{0.0f, 0.0f, 0.0f};
 1236 				decision->pathRequested = true;
 1237 			}
 1238 			nav::NavCorridor corridor = {};
 1239 			const nav::NavQueryResult corridorResult = buildCorridor(
 1240 				query, currentArea.area, link.toArea, nav::NavRouteType::Fastest,
 1241 				&corridor, decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
 1242 			if (decision != nullptr)
 1243 			{
 1244 				decision->corridorResult = corridorResult;
 1245 				decision->pathResult = corridorResult;
 1246 			}
 1247 			if (link.toArea == currentArea.area ||
 1248 					corridorResult != nav::NavQueryResult::Found ||
 1249 					corridor.areas.size() < 2U ||
 1250 					!startTraversal(snapshot, corridor, link))
 1251 			{
 1252 				continue;
 1253 			}
 1254 			activeTraversal_ = traversalActionFor(snapshot, link);
 1255 			rememberRoute(snapshot, corridor, link);
 1256 			if (decision != nullptr)
 1257 			{
 1258 				decision->targetArea = link.toArea;
 1259 				populateRouteDecision(decision);
 1260 			}
 1261 			return true;
 1262 		}
 1263 		return false;
```


## `src/core/runtime/nav_roam_controller.cpp:1336–1385`

```cpp
 1336 void NavRoamController::rememberRoute(
 1337 	const nav::NavSnapshot &snapshot,
 1338 	const nav::NavCorridor &corridor,
 1339 	const nav::NavDirectedLink &link)
 1340 {
 1341 	activeCorridor_ = corridor;
 1342 	activeLink_ = link;
 1343 	activeCorridorIndex_ = 0U;
 1344 	hasActiveRoute_ = true;
 1345 	if (pathSequence_ == (std::numeric_limits<std::uint32_t>::max)())
 1346 	{
 1347 		pathSequence_ = 1U;
 1348 	}
 1349 	else
 1350 	{
 1351 		++pathSequence_;
 1352 	}
 1353 	const nav::NavDocument *document = snapshot.document();
 1354 	const nav::NavArea *targetArea = document != nullptr
 1355 			? document->findArea(link.toArea)
 1356 			: nullptr;
 1357 	activeTargetPosition_ = targetArea != nullptr
 1358 			? centerOf(*targetArea)
 1359 			: nav::NavVector{0.0f, 0.0f, 0.0f};
 1360 }
 1361 
 1362 void NavRoamController::populateRouteDecision(NavRoamDecision *decision) const
 1363 {
 1364 	if (decision == nullptr || !hasActiveRoute_)
 1365 	{
 1366 		return;
 1367 	}
 1368 	decision->goalPresent = true;
 1369 	decision->goalKind = hasObjectiveTarget_ ? NavGoalKind::Objective : NavGoalKind::Roam;
 1370 	decision->goalArea = activeLink_.toArea;
 1371 	decision->goalPosition = activeTargetPosition_;
 1372 	decision->targetArea = activeLink_.toArea;
 1373 	decision->targetPosition = activeTargetPosition_;
 1374 	decision->corridorAreaCount = activeCorridor_.areas.size();
 1375 	decision->corridorIndex = activeCorridorIndex_;
 1376 	decision->linkFromArea = activeLink_.fromArea;
 1377 	decision->linkToArea = activeLink_.toArea;
 1378 	decision->linkDirection = activeLink_.direction;
 1379 	decision->linkHow = activeLink_.how;
 1380 	decision->linkResult = nav::NavQueryResult::Found;
 1381 	decision->corridorResult = nav::NavQueryResult::Found;
 1382 	decision->pathSequence = pathSequence_;
 1383 	decision->routeType = activeCorridor_.routeType;
 1384 	decision->pathCost = activeCorridor_.cost;
 1385 	decision->selectedPath = activeCorridor_.areas;
```


## `src/adapter/metamod/plugin_runtime.cpp:2011–2042`

```cpp
 2011 				runtime::NavRoamObservation observation = {};
 2012 				observation.actor = handle.actor;
 2013 				observation.frame = {
 2014 					lifecycle_.mapGeneration(),
 2015 					lifecycle_.roundGeneration(),
 2016 					adapterFrameCount_};
 2017 			float feetOffset = handle.entity->v.mins.z;
 2018 			if (!std::isfinite(feetOffset) || feetOffset >= 0.0f)
 2019 			{
 2020 				feetOffset = -kStandingHalfHumanHeight;
 2021 			}
 2022 		observation.locomotion.position = {
 2023 				handle.entity->v.origin.x,
 2024 				handle.entity->v.origin.y,
 2025 				handle.entity->v.origin.z + feetOffset};
 2026 		observation.locomotion.velocity = {
 2027 			handle.entity->v.velocity.x,
 2028 			handle.entity->v.velocity.y,
 2029 			handle.entity->v.velocity.z};
 2030 		observation.locomotion.standingClearance = 72.0f;
 2031 		observation.locomotion.crouchingClearance = 36.0f;
 2032 		observation.locomotion.grounded = before.grounded;
 2033 		observation.locomotion.ducked = before.ducked;
 2034 		observation.locomotion.onLadder = before.onLadder;
 2035 		observation.airborne = !before.grounded;
 2036 		observation.landingConfirmed = movementWasAirborne_[index] && before.grounded;
 2037 		observation.hasLandingDamage = false;
 2038 		observation.landingDamage = 0.0f;
 2039 		observation.ladderContact = before.onLadder;
 2040 		observation.entryConfirmed = true;
 2041 		observation.exitConfirmed = before.grounded;
 2042 		observation.collectPathStats = runtimeProfiler_.enabled();
```


## `src/adapter/metamod/plugin_runtime.cpp:2131–2143`

```cpp
 2131 		if (roamResult != runtime::NavRoamResult::IntentReady)
 2132 		{
 2133 			if (roamDecision.failureReason == runtime::NavFailureReason::None)
 2134 			{
 2135 				roamDecision.failureReason = runtime::NavFailureReason::MovementNotProduced;
 2136 			}
 2137 			logMovementDiagnostic(index, "roam_no_intent", &roamDecision);
 2138 			prepareNeutralManagedBotCommand(index, handle, before);
 2139 			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
 2140 			recordMovementPhysicsSample(index, before, receipt);
 2141 			continue;
 2142 		}
 2143 
```


## `src/adapter/metamod/plugin_runtime.cpp:2150–2196`

```cpp
 2150 
 2151 			runtime::BotCommand command = {};
 2152 			command.actor = handle.actor;
 2153 			command.lifecycle = token;
 2154 			command.sequence = 0U;
 2155 			command.issueFrame = 0U;
 2156 			const runtime::ViewAngles movementViewAngles = {
 2157 				0.0f,
 2158 				movementYaw(locomotionIntent.direction),
 2159 				0.0f};
 2160 			command.viewAngles = movementViewAngles;
 2161 			std::uint16_t movementButtons =
 2162 				locomotionIntent.posture == nav::LocomotionPosture::Crouching
 2163 						? static_cast<std::uint16_t>(IN_DUCK)
 2164 						: static_cast<std::uint16_t>(0U);
 2165 			if (locomotionIntent.traversal == nav::TraversalAction::Jump)
 2166 			{
 2167 				movementButtons = static_cast<std::uint16_t>(movementButtons | IN_JUMP);
 2168 			}
 2169 			if (locomotionIntent.traversal == nav::TraversalAction::Crouch ||
 2170 					locomotionIntent.traversal == nav::TraversalAction::NarrowPassage)
 2171 			{
 2172 				movementButtons = static_cast<std::uint16_t>(movementButtons | IN_DUCK);
 2173 			}
 2174 		command.movement = {
 2175 			0.0f,
 2176 			0.0f,
 2177 			0.0f,
 2178 			movementButtons,
 2179 			0U,
 2180 			1U};
 2181 		const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
 2182 			decideManagedBotAction(index, before, movementViewAngles, movementButtons),
 2183 			false,
 2184 			movementViewAngles);
 2185 		const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
 2186 		command.viewAngles = actionDispatch.viewAngles;
 2187 		command.movement.buttons = actionDispatch.buttons;
 2188 		const MovementProjection movementProjection = ActionAdapter::projectMovement(
 2189 			locomotionIntent.direction.x,
 2190 			locomotionIntent.direction.y,
 2191 			locomotionIntent.speed,
 2192 			command.viewAngles.yaw);
 2193 		command.movement.forward = actionDispatch.stopMovement ? 0.0f : movementProjection.forward;
 2194 		command.movement.side = actionDispatch.stopMovement ? 0.0f : movementProjection.side;
 2195 		if (!actionDispatch.stopMovement)
 2196 		{
```


## `src/adapter/metamod/plugin_runtime.cpp:3551–3597`

```cpp
 3551 void PluginRuntime::logGoalAssignmentDiagnostic(
 3552 	std::size_t index,
 3553 	const runtime::MovementPhysicsState &before,
 3554 	const runtime::NavRoamDecision &decision,
 3555 	const nav::LocomotionIntent &intent)
 3556 {
 3557 	if (!runtimeProfiler_.enabled() || index >= movementGoalDiagnosticSeconds_.size() ||
 3558 		globals_ == nullptr || gpMetaUtilFuncs == nullptr ||
 3559 		gpMetaUtilFuncs->pfnLogConsole == nullptr || pluginId_ == nullptr)
 3560 	{
 3561 		return;
 3562 	}
 3563 	const double now = std::isfinite(static_cast<double>(globals_->time))
 3564 		? static_cast<double>(globals_->time)
 3565 		: 0.0;
 3566 	const std::uint32_t second = now <= 0.0
 3567 		? 0U
 3568 		: static_cast<std::uint32_t>(now);
 3569 	if (movementGoalDiagnosticSeconds_[index] == second)
 3570 	{
 3571 		return;
 3572 	}
 3573 	movementGoalDiagnosticSeconds_[index] = second;
 3574 	const ManagedObjectiveTargetCache &cache = managedBotObjectiveTargets_[index];
 3575 	const char *goalSource = decision.goalKind == runtime::NavGoalKind::Objective
 3576 		? "objective"
 3577 		: decision.goalKind == runtime::NavGoalKind::Roam ? "roam" : "none";
 3578 	gpMetaUtilFuncs->pfnLogConsole(
 3579 		pluginId_,
 3580 		"profile goalAssignment bot_id=%u team=%d carrying_c4=%d state=%s task=%s "
 3581 		"disposition=UNAVAILABLE goal_present=%d goal_kind=%d goal_area=%u "
 3582 		"goal_position=(%.1f %.1f %.1f) goal_source=%s objective_target_id=%u "
 3583 		"goal_generation=%u cache_hit=%d cache_scope=actor corridor_goal_area=%u "
 3584 		"next_area=%u movement_target=(%.1f %.1f %.1f) current_area=%u",
 3585 		static_cast<unsigned int>(managedBotHandles_[index].actor.slot), before.team,
 3586 		cache.carryingBomb ? 1 : 0,
 3587 		compat::toString(managedBotStateMachines_[index].state()),
 3588 		compat::toString(managedBotStateMachines_[index].task()),
 3589 		decision.goalPresent ? 1 : 0, static_cast<int>(decision.goalKind),
 3590 		static_cast<unsigned int>(decision.goalArea), decision.goalPosition.x,
 3591 		decision.goalPosition.y, decision.goalPosition.z, goalSource,
 3592 		static_cast<unsigned int>(cache.selectedSiteIdentity),
 3593 		static_cast<unsigned int>(cache.generation), cache.lastCacheHit ? 1 : 0,
 3594 		static_cast<unsigned int>(decision.goalArea),
 3595 		static_cast<unsigned int>(intent.targetArea), decision.targetPosition.x,
 3596 		decision.targetPosition.y, decision.targetPosition.z,
 3597 		static_cast<unsigned int>(decision.currentArea));
```


## `src/adapter/metamod/action_adapter.cpp:47–67`

```cpp
   47 MovementProjection ActionAdapter::projectMovement(
   48 	float worldDirectionX,
   49 	float worldDirectionY,
   50 	float speed,
   51 	float viewYaw)
   52 {
   53 	if (!std::isfinite(worldDirectionX) || !std::isfinite(worldDirectionY) ||
   54 			!std::isfinite(speed) || !std::isfinite(viewYaw))
   55 	{
   56 		return {0.0f, 0.0f};
   57 	}
   58 	constexpr float kDegreesToRadians = 0.01745329251994329577f;
   59 	const float yaw = viewYaw * kDegreesToRadians;
   60 	const float forwardX = std::cos(yaw);
   61 	const float forwardY = std::sin(yaw);
   62 	const float rightX = -forwardY;
   63 	const float rightY = forwardX;
   64 	return {
   65 		(worldDirectionX * forwardX + worldDirectionY * forwardY) * speed,
   66 		-(worldDirectionX * rightX + worldDirectionY * rightY) * speed};
   67 }
```


## `src/core/nav/jump_drop.cpp:75–85`

```cpp
   75 		return JumpDropResult::InvalidEnvelope;
   76 	}
   77 	if (exceedsEnvelope(envelope))
   78 	{
   79 		return JumpDropResult::Unsafe;
   80 	}
   81 	if (corridor.areas.front() != envelope.launch.area ||
   82 			corridor.areas.back() != envelope.landing.area)
   83 	{
   84 		return JumpDropResult::InvalidEnvelope;
   85 	}
```


## `tests/nav_external_performance_tests.cpp:292–333`

```cpp
  292 	}
  293 
  294 	astrabot::metamod::NavLoader loader;
  295 	astrabot::nav::NavSnapshotPublisher publisher;
  296 	astrabot::metamod::NavLoadDiagnostic diagnostic = {};
  297 	astrabot::metamod::NavLoadRequest request = {
  298 		navPath.c_str(), "DE_DUST2.BSP", 1U, true, bspSize, false, 0U};
  299 	astrabot::metamod::NavLoadResult loadResult =
  300 		loader.loadFile(&request, &publisher, &diagnostic);
  301 	bool bspIdentityVerified = loadResult == astrabot::metamod::NavLoadResult::Loaded;
  302 	if (loadResult == astrabot::metamod::NavLoadResult::BspSizeMismatch)
  303 	{
  304 		std::cout << "external_nav_load bspIdentity=BLOCKED_BY_ARTIFACT_MISMATCH"
  305 			<< " requestedBspSize=" << bspSize << '\n';
  306 		request.hasBspSize = false;
  307 		request.bspSize = 0U;
  308 		loadResult = loader.loadFile(&request, &publisher, &diagnostic);
  309 	}
  310 	std::cout << "external_nav_load result=" << static_cast<int>(loadResult)
  311 		<< " reader=" << static_cast<int>(diagnostic.readerResult)
  312 		<< " mapName=" << diagnostic.mapName
  313 		<< " bspSize=" << diagnostic.bspSize
  314 		<< " sourceHash=" << diagnostic.sourceHash
  315 		<< " bspIdentityVerified=" << (bspIdentityVerified ? 1 : 0) << '\n';
  316 	if (!check(loadResult == astrabot::metamod::NavLoadResult::Loaded,
  317 		"external de_dust2.nav loads through NavLoader"))
  318 	{
  319 		return 1;
  320 	}
  321 
  322 	const astrabot::nav::NavSnapshot snapshot = publisher.snapshot();
  323 	if (!check(snapshot.document() != nullptr &&
  324 		snapshot.document()->findArea(41U) != nullptr &&
  325 		snapshot.document()->findArea(90U) != nullptr &&
  326 		snapshot.document()->findArea(1438U) != nullptr &&
  327 		snapshot.document()->findArea(5U) != nullptr &&
  328 		snapshot.document()->findArea(1520U) != nullptr,
  329 		"required de_dust2 areas exist"))
  330 	{
  331 		return 1;
  332 	}
  333 	std::size_t bfsVisited = 0U;
```
