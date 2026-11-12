# AstraBot 実ソース抜粋

添付 ZIP の未変更ファイルから機械的に抽出。各行先頭の番号は ZIP 内ファイルの行番号。

対応する再現・修正方針は `AUDIT_JA.md` を参照。

## F01 — Roam 先頭隣接 Goal

### `src/core/runtime/nav_roam_controller.cpp:1208–1265`

```cpp
 1208  bool NavRoamController::selectRoamRoute(
 1209  	const nav::NavSnapshot &snapshot,
 1210  	const nav::NavAreaMatch &currentArea,
 1211  	NavRoamDecision *decision)
 1212  {
 1213  	const nav::NavDocument *document = snapshot.document();
 1214  	if (document == nullptr || document->areas().empty())
 1215  	{
 1216  		return false;
 1217  	}
 1218  	nav::NavQuery query(snapshot);
 1219  	std::vector<nav::NavDirectedLink> links;
 1220  	if (query.outgoingLinks(currentArea.area, &links) != nav::NavQueryResult::Found)
 1221  	{
 1222  		return false;
 1223  	}
 1224  	if (!modePolicy_.allowsAdaptiveRouteWeighting())
 1225  	{
 1226  		for (const nav::NavDirectedLink &link : links)
 1227  		{
 1228  			if (decision != nullptr)
 1229  			{
 1230  				const nav::NavArea *target = document->findArea(link.toArea);
 1231  				decision->goalPresent = target != nullptr;
 1232  				decision->goalKind = NavGoalKind::Roam;
 1233  				decision->goalArea = link.toArea;
 1234  				decision->goalPosition = target != nullptr
 1235  					? centerOf(*target) : nav::NavVector{0.0f, 0.0f, 0.0f};
 1236  				decision->pathRequested = true;
 1237  			}
 1238  			nav::NavCorridor corridor = {};
 1239  			const nav::NavQueryResult corridorResult = buildCorridor(
 1240  				query, currentArea.area, link.toArea, nav::NavRouteType::Fastest,
 1241  				&corridor, decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
 1242  			if (decision != nullptr)
 1243  			{
 1244  				decision->corridorResult = corridorResult;
 1245  				decision->pathResult = corridorResult;
 1246  			}
 1247  			if (link.toArea == currentArea.area ||
 1248  					corridorResult != nav::NavQueryResult::Found ||
 1249  					corridor.areas.size() < 2U ||
 1250  					!startTraversal(snapshot, corridor, link))
 1251  			{
 1252  				continue;
 1253  			}
 1254  			activeTraversal_ = traversalActionFor(snapshot, link);
 1255  			rememberRoute(snapshot, corridor, link);
 1256  			if (decision != nullptr)
 1257  			{
 1258  				decision->targetArea = link.toArea;
 1259  				populateRouteDecision(decision);
 1260  			}
 1261  			return true;
 1262  		}
 1263  		return false;
 1264  	}
 1265  	const std::size_t firstCandidate = nextLinkIndex_ % document->areas().size();
```

### `src/core/runtime/nav_roam_controller.cpp:827–835`

```cpp
  827  			hasAvoidedLink_ = true;
  828  			avoidedLink_ = activeLink_;
  829  			++nextLinkIndex_;
  830  			resetRoute();
  831  			if (decision != nullptr)
  832  			{
  833  				decision->stage = NavRoamStage::Failed;
  834  			}
  835  			return NavRoamResult::ReplanRequired;
```

### `src/core/runtime/nav_roam_controller.cpp:1308–1312`

```cpp
 1308  		if (hasAvoidedLink_ && link.fromArea == avoidedLink_.fromArea &&
 1309  				link.toArea == avoidedLink_.toArea)
 1310  		{
 1311  			continue;
 1312  		}
```

### `tests/nav_roam_controller_tests.cpp:335–355`

```cpp
  335  	astrabot::runtime::NavRoamController firstController(
  336  			astrabot::compat::RuntimeMode::Compatibility);
  337  	astrabot::runtime::NavRoamController secondController(
  338  			astrabot::compat::RuntimeMode::Compatibility);
  339  	astrabot::nav::LocomotionIntent firstIntent = {};
  340  	astrabot::nav::LocomotionIntent secondIntent = {};
  341  	astrabot::runtime::NavRoamDecision firstDecision = {};
  342  	astrabot::runtime::NavRoamDecision secondDecision = {};
  343  	if (!check(firstController.update(
  344  				publisher.snapshot(), makeObservation(1U), &firstIntent, &firstDecision) ==
  345  				astrabot::runtime::NavRoamResult::IntentReady &&
  346  				secondController.update(
  347  						publisher.snapshot(), makeObservation(2U), &secondIntent,
  348  						&secondDecision) == astrabot::runtime::NavRoamResult::IntentReady,
  349  				"compatibility controllers emit route intents"))
  350  	{
  351  		return false;
  352  	}
  353  	return check(firstDecision.linkToArea == 2U && secondDecision.linkToArea == 2U,
  354  			"compatibility route selection is independent of actor-derived rotation");
  355  }
```

## F02 — Follower の早すぎる進行

### `src/core/nav/nav_query.cpp:104–113`

```cpp
  104  bool isWithinTarget(
  105  		const NavArea &area,
  106  		const NavVector &position,
  107  	float horizontalTolerance,
  108  	float verticalTolerance)
  109  {
  110  	return rectangleDistanceSquared(area, position) <=
  111  			horizontalTolerance * horizontalTolerance &&
  112  			floorDistance(area, position.z) <= verticalTolerance;
  113  }
```

### `src/core/nav/nav_query.cpp:938–986`

```cpp
  938  	const NavArea *area = currentDocument->findArea(corridor_[currentIndex_]);
  939  	if (area == nullptr)
  940  	{
  941  		return NavFollowerResult::InvalidCorridor;
  942  	}
  943  
  944  	NavVector currentTarget = portalSteeringPoint(*area, position);
  945  	if (currentIndex_ > 0U)
  946  	{
  947  		const NavArea *previousArea =
  948  				currentDocument->findArea(corridor_[currentIndex_ - 1U]);
  949  		if (previousArea != nullptr)
  950  		{
  951  			const std::uint8_t direction = currentIndex_ - 1U < links_.size()
  952  				? links_[currentIndex_ - 1U].direction
  953  				: NavArea::kDirectionCount;
  954  			currentTarget = portalSteeringPoint(
  955  				*previousArea, *area, position, direction);
  956  		}
  957  	}
  958  	*target = currentTarget;
  959  	*targetArea = area->id;
  960  	if (!isWithinTarget(
  961  			*area,
  962  			position,
  963  			horizontalTolerance,
  964  			verticalTolerance))
  965  	{
  966  		return NavFollowerResult::TargetReady;
  967  	}
  968  	if (currentIndex_ + 1U < corridor_.size())
  969  	{
  970  		++currentIndex_;
  971  		const NavArea *nextArea =
  972  			currentDocument->findArea(corridor_[currentIndex_]);
  973  		if (nextArea == nullptr)
  974  		{
  975  			return NavFollowerResult::InvalidCorridor;
  976  		}
  977  		const NavArea *previousArea =
  978  				currentDocument->findArea(corridor_[currentIndex_ - 1U]);
  979  		const std::uint8_t direction = currentIndex_ - 1U < links_.size()
  980  				? links_[currentIndex_ - 1U].direction
  981  				: NavArea::kDirectionCount;
  982  		*target = previousArea != nullptr
  983  				? portalSteeringPoint(*previousArea, *nextArea, position, direction)
  984  				: portalSteeringPoint(*nextArea, position);
  985  		*targetArea = nextArea->id;
  986  		return NavFollowerResult::Advanced;
```

### `src/core/nav/nav_query.cpp:115–224`

```cpp
  115  NavVector portalSteeringPoint(
  116  	const NavArea &area,
  117  	const NavVector &position)
  118  {
  119  	constexpr float kPortalInset = 16.0f;
  120  	const NavVector closest = closestPointOnArea(area, position);
  121  	const NavVector center = centerOf(area);
  122  	const float deltaX = center.x - closest.x;
  123  	const float deltaY = center.y - closest.y;
  124  	const float distance = std::hypot(deltaX, deltaY);
  125  	if (!std::isfinite(distance) || distance <= kPortalInset)
  126  	{
  127  		return center;
  128  	}
  129  	return {
  130  		closest.x + deltaX / distance * kPortalInset,
  131  		closest.y + deltaY / distance * kPortalInset,
  132  		closest.z};
  133  }
  134  
  135  NavVector portalSteeringPoint(
  136  	const NavArea &from,
  137  	const NavArea &to,
  138  	const NavVector &position)
  139  {
  140  	const float overlapLoX = (std::max)(from.extent.lo.x, to.extent.lo.x);
  141  	const float overlapHiX = (std::min)(from.extent.hi.x, to.extent.hi.x);
  142  	const float overlapLoY = (std::max)(from.extent.lo.y, to.extent.lo.y);
  143  	const float overlapHiY = (std::min)(from.extent.hi.y, to.extent.hi.y);
  144  	if (overlapLoX > overlapHiX || overlapLoY > overlapHiY)
  145  	{
  146  		return portalSteeringPoint(to, position);
  147  	}
  148  	const NavVector portal = {
  149  		(overlapLoX + overlapHiX) * 0.5f,
  150  		(overlapLoY + overlapHiY) * 0.5f,
  151  		to.northEastZ * 0.5f + to.southWestZ * 0.5f};
  152  	const NavVector center = centerOf(to);
  153  	const float deltaX = center.x - portal.x;
  154  	const float deltaY = center.y - portal.y;
  155  	const float distance = std::hypot(deltaX, deltaY);
  156  	if (!std::isfinite(distance) || distance <= 16.0f)
  157  	{
  158  		return center;
  159  	}
  160  	return {
  161  		portal.x + deltaX / distance * 16.0f,
  162  		portal.y + deltaY / distance * 16.0f,
  163  		portal.z};
  164  }
  165  
  166  NavVector portalSteeringPoint(
  167  	const NavArea &from,
  168  	const NavArea &to,
  169  	const NavVector &position,
  170  	std::uint8_t direction)
  171  {
  172  	if (direction >= NavArea::kDirectionCount)
  173  	{
  174  		return portalSteeringPoint(to, position);
  175  	}
  176  	const float overlapLoX = (std::max)(from.extent.lo.x, to.extent.lo.x);
  177  	const float overlapHiX = (std::min)(from.extent.hi.x, to.extent.hi.x);
  178  	const float overlapLoY = (std::max)(from.extent.lo.y, to.extent.lo.y);
  179  	const float overlapHiY = (std::min)(from.extent.hi.y, to.extent.hi.y);
  180  	if (overlapLoX > overlapHiX || overlapLoY > overlapHiY)
  181  	{
  182  		return portalSteeringPoint(to, position);
  183  	}
  184  	const bool directionMatchesGeometry =
  185  		(direction == 0U && to.extent.hi.y <= from.extent.lo.y + 1.0f) ||
  186  		(direction == 1U && to.extent.lo.x >= from.extent.hi.x - 1.0f) ||
  187  		(direction == 2U && to.extent.lo.y >= from.extent.hi.y - 1.0f) ||
  188  		(direction == 3U && to.extent.hi.x <= from.extent.lo.x + 1.0f);
  189  	if (!directionMatchesGeometry)
  190  	{
  191  		return portalSteeringPoint(from, to, position);
  192  	}
  193  	constexpr float kPortalMargin = 16.0f;
  194  	const auto clampWithMargin = [kPortalMargin](float value, float lo, float hi) {
  195  		if (hi - lo <= 2.0f * kPortalMargin)
  196  		{
  197  			return (lo + hi) * 0.5f;
  198  		}
  199  		return (std::max)(lo + kPortalMargin,
  200  			(std::min)(hi - kPortalMargin, value));
  201  	};
  202  	NavVector result = {
  203  		clampWithMargin(position.x, overlapLoX, overlapHiX),
  204  		clampWithMargin(position.y, overlapLoY, overlapHiY),
  205  		to.northEastZ * 0.5f + to.southWestZ * 0.5f};
  206  	switch (direction)
  207  	{
  208  	case 0U:
  209  		result.y = from.extent.lo.y - kPortalMargin;
  210  		break;
  211  	case 1U:
  212  		result.x = from.extent.hi.x + kPortalMargin;
  213  		break;
  214  	case 2U:
  215  		result.y = from.extent.hi.y + kPortalMargin;
  216  		break;
  217  	case 3U:
  218  		result.x = from.extent.lo.x - kPortalMargin;
  219  		break;
  220  	default:
  221  		break;
  222  	}
  223  	return result;
  224  }
```

### `tests/locomotion_tests.cpp:309–335`

```cpp
  309  bool testReferenceArrivalTolerance()
  310  {
  311  	astrabot::nav::NavDocument document = routeDocument(0.0f);
  312  	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
  313  	astrabot::nav::LocomotionController controller;
  314  	if (!check(controller.start(corridorFor(snapshot)) ==
  315  				astrabot::nav::LocomotionResult::Started,
  316  			"reference arrival controller starts"))
  317  	{
  318  		return false;
  319  	}
  320  	astrabot::nav::LocomotionObservation observation = {};
  321  	observation.position = {16.0f, 32.0f, 0.0f};
  322  	observation.standingClearance = 72.0f;
  323  	observation.crouchingClearance = 36.0f;
  324  	astrabot::nav::LocomotionIntent intent = {};
  325  	if (!check(controller.update(snapshot, observation, &intent) ==
  326  				astrabot::nav::LocomotionResult::IntentReady,
  327  			"reference arrival emits an approach intent"))
  328  	{
  329  		return false;
  330  	}
  331  	observation.position = {60.0f, 32.0f, 0.0f};
  332  	return check(controller.update(snapshot, observation, &intent) ==
  333  				astrabot::nav::LocomotionResult::TargetReached,
  334  			"reference arrival accepts a point within 20 units of the next area");
  335  }
```

## F03 — 高さと nearest

### `src/core/nav/nav_query.cpp:44–65`

```cpp
   44  float floorDistance(const NavArea &area, float z)
   45  {
   46  	const float low = std::min(area.northEastZ, area.southWestZ);
   47  	const float high = std::max(area.northEastZ, area.southWestZ);
   48  	if (z < low)
   49  	{
   50  		return low - z;
   51  	}
   52  	if (z > high)
   53  	{
   54  		return z - high;
   55  	}
   56  	return 0.0f;
   57  }
   58  
   59  NavVector centerOf(const NavArea &area)
   60  {
   61  	return {
   62  		area.extent.lo.x * 0.5f + area.extent.hi.x * 0.5f,
   63  		area.extent.lo.y * 0.5f + area.extent.hi.y * 0.5f,
   64  		area.northEastZ * 0.5f + area.southWestZ * 0.5f
   65  	};
```

### `src/core/nav/nav_query.cpp:93–101`

```cpp
   93  NavVector closestPointOnArea(
   94  	const NavArea &area,
   95  	const NavVector &position)
   96  {
   97  	NavVector point = position;
   98  	point.x = (std::max)(area.extent.lo.x, (std::min)(point.x, area.extent.hi.x));
   99  	point.y = (std::max)(area.extent.lo.y, (std::min)(point.y, area.extent.hi.y));
  100  	point.z = area.northEastZ * 0.5f + area.southWestZ * 0.5f;
  101  	return point;
```

### `src/core/nav/nav_query.cpp:423–453`

```cpp
  423  	const float maximumDistanceSquared = maximumDistance * maximumDistance;
  424  	bool found = false;
  425  	AreaId bestArea = 0U;
  426  	float bestDistance = std::numeric_limits<float>::max();
  427  	for (const NavArea &area : document()->areas())
  428  	{
  429  		const float candidateDistance =
  430  			rectangleDistanceSquared(area, position);
  431  		if (candidateDistance > maximumDistanceSquared ||
  432  				(found && !isBetterMatch(
  433  					candidateDistance,
  434  					area.id,
  435  					bestDistance,
  436  					bestArea)))
  437  		{
  438  			continue;
  439  		}
  440  		found = true;
  441  		bestArea = area.id;
  442  		bestDistance = candidateDistance;
  443  	}
  444  
  445  	if (!found)
  446  	{
  447  		return NavQueryResult::NoAreaContaining;
  448  	}
  449  
  450  	match->area = bestArea;
  451  	match->distanceSquared = bestDistance;
  452  	match->closestPoint = closestPointOnArea(*document()->findArea(bestArea), position);
  453  	return NavQueryResult::Found;
```

### `src/core/nav/locomotion.cpp:225–233`

```cpp
  225  	const float stepHeight =
  226  		floorHeight(*destinationArea) - floorHeight(*currentArea);
  227  	if (stepHeight > config_.maximumStepHeight)
  228  	{
  229  		active_ = false;
  230  		return LocomotionResult::StepTooHigh;
  231  	}
  232  
  233  	const bool requiresJump = (destinationArea->attributes & NavArea::kJump) != 0U;
```

### `src/core/nav/locomotion.cpp:311–314`

```cpp
  311  float LocomotionController::floorHeight(const NavArea &area)
  312  {
  313  	return area.northEastZ * 0.5f + area.southWestZ * 0.5f;
  314  }
```

## F04 — 特殊リンクへの切替

### `src/core/runtime/nav_roam_controller.cpp:942–997`

```cpp
  942  bool NavRoamController::startTraversal(
  943  	const nav::NavSnapshot &snapshot,
  944  	const nav::NavCorridor &corridor,
  945  	const nav::NavDirectedLink &link)
  946  {
  947  	const nav::NavDocument *document = snapshot.document();
  948  	if (document == nullptr || corridor.areas.size() < 2U)
  949  	{
  950  		return false;
  951  	}
  952  	const nav::NavArea *fromArea = document->findArea(link.fromArea);
  953  	const nav::NavArea *toArea = document->findArea(link.toArea);
  954  	if (fromArea == nullptr || toArea == nullptr)
  955  	{
  956  		return false;
  957  	}
  958  	activeTraversal_ = traversalActionFor(snapshot, link);
  959  	const nav::NavVector launchPosition = centerOf(*fromArea);
  960  	const nav::NavVector landingPosition = centerOf(*toArea);
  961  	const float verticalDelta = floorHeight(*toArea) - floorHeight(*fromArea);
  962  	const float horizontalDeltaX = landingPosition.x - launchPosition.x;
  963  	const float horizontalDeltaY = landingPosition.y - launchPosition.y;
  964  	const float horizontalReach = std::sqrt(
  965  		horizontalDeltaX * horizontalDeltaX + horizontalDeltaY * horizontalDeltaY);
  966  
  967  	if (activeTraversal_ == nav::TraversalAction::Jump ||
  968  			activeTraversal_ == nav::TraversalAction::Drop)
  969  	{
  970  		nav::JumpDropEnvelope envelope = {};
  971  		envelope.kind = activeTraversal_ == nav::TraversalAction::Jump
  972  				? nav::JumpDropKind::Jump
  973  				: nav::JumpDropKind::Drop;
  974  		envelope.launch = {launchPosition, link.fromArea};
  975  		envelope.landing = {landingPosition, link.toArea};
  976  		envelope.maximumRise = (std::max)(16.0f, verticalDelta + 16.0f);
  977  		envelope.maximumDrop = (std::max)(16.0f, -verticalDelta + 16.0f);
  978  		envelope.horizontalReach = (std::max)(16.0f, horizontalReach + 16.0f);
  979  		envelope.damageRisk = {4096.0f, 100.0f};
  980  		return jumpDrop_.start(corridor, envelope, actor_.actorGeneration) ==
  981  				nav::JumpDropResult::Ready;
  982  	}
  983  
  984  	if (activeTraversal_ == nav::TraversalAction::Ladder)
  985  	{
  986  		nav::SpecialTraversalCapability capability = {};
  987  		capability.kind = nav::SpecialTraversalKind::Ladder;
  988  		capability.entry = {launchPosition, link.fromArea};
  989  		capability.exit = {landingPosition, link.toArea};
  990  		capability.sourceDirection = link.direction;
  991  		capability.requiredClearance = 36.0f;
  992  		capability.minimumPosture = nav::SpecialTraversalPosture::Standing;
  993  		return specialTraversal_.start(corridor, capability, actor_.actorGeneration) ==
  994  				nav::SpecialTraversalResult::Ready;
  995  	}
  996  
  997  	return locomotion_.start(corridor) == nav::LocomotionResult::Started;
```

### `src/core/runtime/nav_roam_controller.cpp:1082–1104`

```cpp
 1082  		if (corridorResult == nav::NavQueryResult::Found && corridor.areas.size() >= 2U)
 1083  		{
 1084  			const nav::AreaId nextArea = corridor.areas[1U];
 1085  			nav::NavDirectedLink link = {currentArea.area, nextArea, 0U, 0U};
 1086  			for (const nav::NavDirectedLink &candidate : links)
 1087  			{
 1088  				if (candidate.toArea == nextArea)
 1089  				{
 1090  					link = candidate;
 1091  					break;
 1092  				}
 1093  			}
 1094  			if (startTraversal(snapshot, corridor, link))
 1095  			{
 1096  				nextLinkIndex_ = 0U;
 1097  				rememberRoute(snapshot, corridor, link);
 1098  				if (decision != nullptr)
 1099  				{
 1100  					populateRouteDecision(decision);
 1101  					decision->targetArea = objectiveArea;
 1102  					decision->targetPosition = objectiveTarget_;
 1103  				}
 1104  				return true;
```

### `src/core/runtime/nav_roam_controller.cpp:744–777`

```cpp
  744  	const nav::LocomotionResult locomotionResult = locomotion_.update(
  745  		snapshot,
  746  		observation.locomotion,
  747  		intent);
  748  	if (hasActiveRoute_)
  749  	{
  750  		activeCorridorIndex_ = locomotion_.currentCorridorIndex();
  751  		if (!activeCorridor_.areas.empty() &&
  752  				activeCorridorIndex_ >= activeCorridor_.areas.size())
  753  		{
  754  			activeCorridorIndex_ = activeCorridor_.areas.size() - 1U;
  755  		}
  756  	}
  757  	if (decision != nullptr)
  758  	{
  759  		decision->locomotionResult = locomotionResult;
  760  		if (intent->targetArea != 0U)
  761  		{
  762  			decision->targetArea = intent->targetArea;
  763  		}
  764  		if (locomotionResult == nav::LocomotionResult::IntentReady)
  765  		{
  766  			decision->intentDirection = intent->direction;
  767  		}
  768  	}
  769  	if (locomotionResult == nav::LocomotionResult::IntentReady)
  770  	{
  771  		lastIntentDirection_ = intent->direction;
  772  	}
  773  	if (locomotionResult == nav::LocomotionResult::IntentReady &&
  774  			activeTraversal_ != nav::TraversalAction::Walk)
  775  	{
  776  		intent->traversal = activeTraversal_;
  777  	}
```

### `src/core/nav/jump_drop.cpp:69–85`

```cpp
   69  	if (!corridor.isValid())
   70  	{
   71  		return JumpDropResult::InvalidCorridor;
   72  	}
   73  	if (!isValidEnvelope(envelope))
   74  	{
   75  		return JumpDropResult::InvalidEnvelope;
   76  	}
   77  	if (exceedsEnvelope(envelope))
   78  	{
   79  		return JumpDropResult::Unsafe;
   80  	}
   81  	if (corridor.areas.front() != envelope.launch.area ||
   82  			corridor.areas.back() != envelope.landing.area)
   83  	{
   84  		return JumpDropResult::InvalidEnvelope;
   85  	}
```

## F05 — 観測と停滞

### `src/adapter/metamod/plugin_runtime.cpp:2011–2042`

```cpp
 2011  				runtime::NavRoamObservation observation = {};
 2012  				observation.actor = handle.actor;
 2013  				observation.frame = {
 2014  					lifecycle_.mapGeneration(),
 2015  					lifecycle_.roundGeneration(),
 2016  					adapterFrameCount_};
 2017  			float feetOffset = handle.entity->v.mins.z;
 2018  			if (!std::isfinite(feetOffset) || feetOffset >= 0.0f)
 2019  			{
 2020  				feetOffset = -kStandingHalfHumanHeight;
 2021  			}
 2022  		observation.locomotion.position = {
 2023  				handle.entity->v.origin.x,
 2024  				handle.entity->v.origin.y,
 2025  				handle.entity->v.origin.z + feetOffset};
 2026  		observation.locomotion.velocity = {
 2027  			handle.entity->v.velocity.x,
 2028  			handle.entity->v.velocity.y,
 2029  			handle.entity->v.velocity.z};
 2030  		observation.locomotion.standingClearance = 72.0f;
 2031  		observation.locomotion.crouchingClearance = 36.0f;
 2032  		observation.locomotion.grounded = before.grounded;
 2033  		observation.locomotion.ducked = before.ducked;
 2034  		observation.locomotion.onLadder = before.onLadder;
 2035  		observation.airborne = !before.grounded;
 2036  		observation.landingConfirmed = movementWasAirborne_[index] && before.grounded;
 2037  		observation.hasLandingDamage = false;
 2038  		observation.landingDamage = 0.0f;
 2039  		observation.ladderContact = before.onLadder;
 2040  		observation.entryConfirmed = true;
 2041  		observation.exitConfirmed = before.grounded;
 2042  		observation.collectPathStats = runtimeProfiler_.enabled();
```

### `src/core/nav/locomotion.cpp:8–15`

```cpp
    8  namespace nav
    9  {
   10  namespace
   11  {
   12  	constexpr float kMinimumProgressDistance = 0.01f;
   13  	constexpr float kMaximumRecoveryDistance = 256.0f;
   14  
   15  LocomotionResult mapFollowerResult(NavFollowerResult result)
```

### `src/core/nav/locomotion.cpp:189–205`

```cpp
  189  bool LocomotionController::recordProgress(const NavVector &position)
  190  {
  191  	if (hasLastPosition_)
  192  	{
  193  		if (horizontalDistance(lastPosition_, position) <
  194  				kMinimumProgressDistance)
  195  		{
  196  			++stuckFrames_;
  197  		}
  198  		else
  199  		{
  200  			stuckFrames_ = 0U;
  201  		}
  202  	}
  203  	lastPosition_ = position;
  204  	hasLastPosition_ = true;
  205  	return stuckFrames_ >= config_.stuckFrameLimit;
```

## F06 — BombTarget

### `src/adapter/metamod/plugin_runtime.cpp:405–438`

```cpp
  405  	bool isBombTargetClassname(const char *classname)
  406  	{
  407  		return classname != nullptr &&
  408  			(std::strcmp(classname, "func_bomb_target") == 0 ||
  409  			 std::strcmp(classname, "info_bomb_target") == 0);
  410  	}
  411  
  412  	bool objectiveSiteExtent(
  413  		const edict_t *entity, const char *classname, nav::NavExtent *extent)
  414  	{
  415  		if (entity == nullptr || extent == nullptr)
  416  		{
  417  			return false;
  418  		}
  419  		if (classname != nullptr && std::strcmp(classname, "info_bomb_target") == 0)
  420  		{
  421  			const nav::NavVector center = entityObjectiveCenter(entity);
  422  			if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
  423  				!std::isfinite(center.z))
  424  			{
  425  				return false;
  426  			}
  427  			constexpr float kLegacyBombTargetRadius = 256.0f;
  428  			extent->lo = {
  429  				center.x - kLegacyBombTargetRadius,
  430  				center.y - kLegacyBombTargetRadius,
  431  				center.z - kLegacyBombTargetRadius};
  432  			extent->hi = {
  433  				center.x + kLegacyBombTargetRadius,
  434  				center.y + kLegacyBombTargetRadius,
  435  				center.z + kLegacyBombTargetRadius};
  436  			return true;
  437  		}
  438  		return entityObjectiveBounds(entity, extent);
```

### `src/adapter/metamod/plugin_runtime.cpp:2458–2469`

```cpp
 2458  bool PluginRuntime::buildManagedObjectiveTarget(
 2459  	std::size_t index,
 2460  	nav::NavVector *target,
 2461  	nav::NavSearchStats *searchStats,
 2462  	RuntimeObjectiveSelectionStats *objectiveStats)
 2463  {
 2464  	if (objectiveStats != nullptr)
 2465  	{
 2466  		*objectiveStats = {};
 2467  	}
 2468  	if (index >= managedBotHandles_.size() || target == nullptr ||
 2469  		engineFunctions_ == nullptr ||
```

### `src/adapter/metamod/plugin_runtime.cpp:2492–2538`

```cpp
 2492  	int team = compatObservation.player.team.isAvailable()
 2493  		? compatObservation.player.team.value
 2494  		: static_cast<int>(handle.entity->v.team);
 2495  	if (team < 1 && managedBotTeamNumbers_[index] >= 1U &&
 2496  		managedBotTeamNumbers_[index] <= 2U)
 2497  	{
 2498  		team = static_cast<int>(managedBotTeamNumbers_[index]);
 2499  	}
 2500  	if (team < 1 && joinControllers_[index].teamConfirmed())
 2501  	{
 2502  		team = joinControllers_[index].requestedTeam() == compat::CommandTeam::Terrorist ? 1 :
 2503  			joinControllers_[index].requestedTeam() == compat::CommandTeam::CounterTerrorist ? 2 : 0;
 2504  	}
 2505  	if (team != 1 && team != 2)
 2506  	{
 2507  		return false;
 2508  	}
 2509  	const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
 2510  		compatObservation.objective.carryingC4.value;
 2511  	const bool needsBombSite = team == 1 && carryingBomb;
 2512  	const bool needsPlantedBomb = team == 2;
 2513  	ManagedObjectiveTargetCache &cache = managedBotObjectiveTargets_[index];
 2514  	const bool sameObjectiveState = cache.mapGeneration == lifecycle_.mapGeneration() &&
 2515  		cache.roundGeneration == lifecycle_.roundGeneration() &&
 2516  		cache.team == static_cast<std::uint8_t>(team) &&
 2517  		cache.carryingBomb == carryingBomb;
 2518  	if (sameObjectiveState && needsBombSite && cache.valid)
 2519  	{
 2520  		edict_t *cachedSite = cache.selectedEntityIndex > 0
 2521  			? engineFunctions_->pfnPEntityOfEntIndex(cache.selectedEntityIndex)
 2522  			: nullptr;
 2523  		const char *cachedClassname = cachedSite != nullptr && !cachedSite->free
 2524  			? engineFunctions_->pfnSzFromIndex(cachedSite->v.classname)
 2525  			: nullptr;
 2526  		if (cachedClassname != nullptr && isBombTargetClassname(cachedClassname))
 2527  		{
 2528  			*target = cache.target;
 2529  			cache.lastCacheHit = true;
 2530  			return std::isfinite(target->x) && std::isfinite(target->y) &&
 2531  				std::isfinite(target->z);
 2532  		}
 2533  		cache.valid = false;
 2534  		cache.diagnosticsEmitted = false;
 2535  		cache.lastCacheHit = false;
 2536  		cache.generation = cache.generation ==
 2537  			(std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
 2538  	}
```

### `src/adapter/metamod/plugin_runtime.cpp:2585–2703`

```cpp
 2585  	const int maxEntities = (std::max)(0, (std::min)(globals_->maxEntities, 2048));
 2586  	std::uint32_t targetId = 0U;
 2587  	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
 2588  	{
 2589  		edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
 2590  		if (entity == nullptr || entity->free)
 2591  		{
 2592  			continue;
 2593  		}
 2594  		const char *classname = engineFunctions_->pfnSzFromIndex(entity->v.classname);
 2595  		if (classname == nullptr)
 2596  		{
 2597  			continue;
 2598  		}
 2599  		if (needsBombSite && isBombTargetClassname(classname))
 2600  		{
 2601  			const std::uint32_t siteIdentity = ++targetId;
 2602  			if (objectiveStats != nullptr)
 2603  			{
 2604  				++objectiveStats->bombSites;
 2605  			}
 2606  			const nav::NavVector candidate = entityObjectiveCenter(entity);
 2607  			nav::NavExtent siteExtent = {};
 2608  			const bool hasSiteExtent = objectiveSiteExtent(entity, classname, &siteExtent);
 2609  			float pathCost = (std::numeric_limits<float>::max)();
 2610  			nav::NavVector reachablePoint = candidate;
 2611  			nav::AreaId nearestNavArea = 0U;
 2612  			nav::AreaId candidateNavArea = 0U;
 2613  			bool validCandidate = false;
 2614  			const char *rejectionReason = "no_candidate_nav_area";
 2615  			if (haveCurrentArea && document != nullptr && hasSiteExtent)
 2616  			{
 2617  				nav::NavQuery query(navigation);
 2618  				nav::NavAreaMatch siteMatch = {};
 2619  				if (query.findNearest(
 2620  						candidate, kMaximumObjectiveDistance, &siteMatch) ==
 2621  					nav::NavQueryResult::Found)
 2622  				{
 2623  					nearestNavArea = siteMatch.area;
 2624  					candidateNavArea = siteMatch.area;
 2625  					const nav::NavArea *siteArea = document->findArea(siteMatch.area);
 2626  					if (siteArea == nullptr ||
 2627  							siteMatch.closestPoint.x < siteExtent.lo.x ||
 2628  							siteMatch.closestPoint.x > siteExtent.hi.x ||
 2629  							siteMatch.closestPoint.y < siteExtent.lo.y ||
 2630  							 siteMatch.closestPoint.y > siteExtent.hi.y)
 2631  					{
 2632  						rejectionReason = "candidate_outside_site_extent";
 2633  					}
 2634  					else
 2635  					{
 2636  						if (objectiveStats != nullptr)
 2637  						{
 2638  							++objectiveStats->candidateAreas;
 2639  						}
 2640  						const ObjectiveCandidateKey candidateKey = {
 2641  							entityIndex, siteIdentity, siteMatch.area};
 2642  						if (std::find(
 2643  								evaluatedCandidateKeys.begin(), evaluatedCandidateKeys.end(),
 2644  								candidateKey) != evaluatedCandidateKeys.end())
 2645  						{
 2646  							if (objectiveStats != nullptr)
 2647  							{
 2648  								++objectiveStats->duplicateCandidateAreas;
 2649  							}
 2650  							rejectionReason = "duplicate_candidate_identity";
 2651  						}
 2652  						else
 2653  						{
 2654  							evaluatedCandidateKeys.push_back(candidateKey);
 2655  							if (objectiveStats != nullptr)
 2656  							{
 2657  								++objectiveStats->uniqueCandidateAreas;
 2658  								++objectiveStats->candidateQueries;
 2659  							}
 2660  							const nav::NavArea &area = *siteArea;
 2661  							nav::NavCorridor corridor = {};
 2662  							nav::NavSearchStats sample = {};
 2663  							if (query.buildCorridor(currentMatch.area, area.id, &corridor,
 2664  									searchStats == nullptr ? nullptr : &sample) ==
 2665  								nav::NavQueryResult::Found && std::isfinite(corridor.cost))
 2666  							{
 2667  								pathCost = corridor.cost;
 2668  								reachablePoint = siteMatch.closestPoint;
 2669  								validCandidate = true;
 2670  								rejectionReason = "none";
 2671  							}
 2672  							else
 2673  							{
 2674  								rejectionReason = "no_route";
 2675  							}
 2676  							addNavSearchStats(searchStats, sample);
 2677  						}
 2678  					}
 2679  				}
 2680  				else
 2681  					{
 2682  						rejectionReason = "nearest_nav_area_unavailable";
 2683  					}
 2684  				}
 2685  			else
 2686  			{
 2687  				pathCost = haveCurrentArea ? 1.0f : 0.0f;
 2688  				validCandidate = true;
 2689  				rejectionReason = "nav_unavailable_fallback";
 2690  			}
 2691  			if (objectiveStats != nullptr && !cache.diagnosticsEmitted)
 2692  			{
 2693  				logObjectiveTargetDiagnostic(
 2694  					pluginId_, targetId, entityIndex, classname, entity, candidate,
 2695  					nearestNavArea, candidateNavArea, validCandidate, rejectionReason);
 2696  			}
 2697  			if (validCandidate && pathCost < selectedPathCost)
 2698  			{
 2699  				selectedPathCost = pathCost;
 2700  				selectedBombSite = reachablePoint;
 2701  				selectedBombSiteEntityIndex = entityIndex;
 2702  				selectedSiteIdentity = siteIdentity;
 2703  			}
```

## F07 — ログと実行値

### `src/core/runtime/nav_roam_controller.cpp:1336–1385`

```cpp
 1336  void NavRoamController::rememberRoute(
 1337  	const nav::NavSnapshot &snapshot,
 1338  	const nav::NavCorridor &corridor,
 1339  	const nav::NavDirectedLink &link)
 1340  {
 1341  	activeCorridor_ = corridor;
 1342  	activeLink_ = link;
 1343  	activeCorridorIndex_ = 0U;
 1344  	hasActiveRoute_ = true;
 1345  	if (pathSequence_ == (std::numeric_limits<std::uint32_t>::max)())
 1346  	{
 1347  		pathSequence_ = 1U;
 1348  	}
 1349  	else
 1350  	{
 1351  		++pathSequence_;
 1352  	}
 1353  	const nav::NavDocument *document = snapshot.document();
 1354  	const nav::NavArea *targetArea = document != nullptr
 1355  			? document->findArea(link.toArea)
 1356  			: nullptr;
 1357  	activeTargetPosition_ = targetArea != nullptr
 1358  			? centerOf(*targetArea)
 1359  			: nav::NavVector{0.0f, 0.0f, 0.0f};
 1360  }
 1361  
 1362  void NavRoamController::populateRouteDecision(NavRoamDecision *decision) const
 1363  {
 1364  	if (decision == nullptr || !hasActiveRoute_)
 1365  	{
 1366  		return;
 1367  	}
 1368  	decision->goalPresent = true;
 1369  	decision->goalKind = hasObjectiveTarget_ ? NavGoalKind::Objective : NavGoalKind::Roam;
 1370  	decision->goalArea = activeLink_.toArea;
 1371  	decision->goalPosition = activeTargetPosition_;
 1372  	decision->targetArea = activeLink_.toArea;
 1373  	decision->targetPosition = activeTargetPosition_;
 1374  	decision->corridorAreaCount = activeCorridor_.areas.size();
 1375  	decision->corridorIndex = activeCorridorIndex_;
 1376  	decision->linkFromArea = activeLink_.fromArea;
 1377  	decision->linkToArea = activeLink_.toArea;
 1378  	decision->linkDirection = activeLink_.direction;
 1379  	decision->linkHow = activeLink_.how;
 1380  	decision->linkResult = nav::NavQueryResult::Found;
 1381  	decision->corridorResult = nav::NavQueryResult::Found;
 1382  	decision->pathSequence = pathSequence_;
 1383  	decision->routeType = activeCorridor_.routeType;
 1384  	decision->pathCost = activeCorridor_.cost;
 1385  	decision->selectedPath = activeCorridor_.areas;
```

### `src/adapter/metamod/plugin_runtime.cpp:3580–3597`

```cpp
 3580  		"profile goalAssignment bot_id=%u team=%d carrying_c4=%d state=%s task=%s "
 3581  		"disposition=UNAVAILABLE goal_present=%d goal_kind=%d goal_area=%u "
 3582  		"goal_position=(%.1f %.1f %.1f) goal_source=%s objective_target_id=%u "
 3583  		"goal_generation=%u cache_hit=%d cache_scope=actor corridor_goal_area=%u "
 3584  		"next_area=%u movement_target=(%.1f %.1f %.1f) current_area=%u",
 3585  		static_cast<unsigned int>(managedBotHandles_[index].actor.slot), before.team,
 3586  		cache.carryingBomb ? 1 : 0,
 3587  		compat::toString(managedBotStateMachines_[index].state()),
 3588  		compat::toString(managedBotStateMachines_[index].task()),
 3589  		decision.goalPresent ? 1 : 0, static_cast<int>(decision.goalKind),
 3590  		static_cast<unsigned int>(decision.goalArea), decision.goalPosition.x,
 3591  		decision.goalPosition.y, decision.goalPosition.z, goalSource,
 3592  		static_cast<unsigned int>(cache.selectedSiteIdentity),
 3593  		static_cast<unsigned int>(cache.generation), cache.lastCacheHit ? 1 : 0,
 3594  		static_cast<unsigned int>(decision.goalArea),
 3595  		static_cast<unsigned int>(intent.targetArea), decision.targetPosition.x,
 3596  		decision.targetPosition.y, decision.targetPosition.z,
 3597  		static_cast<unsigned int>(decision.currentArea));
```

### `src/adapter/metamod/plugin_runtime.cpp:3607–3613`

```cpp
 3607  	if (!runtimeProfiler_.enabled() || index >= movementTraversalDiagnosticSeconds_.size() ||
 3608  		globals_ == nullptr || gpMetaUtilFuncs == nullptr ||
 3609  		gpMetaUtilFuncs->pfnLogConsole == nullptr || pluginId_ == nullptr ||
 3610  		intent.traversal == nav::TraversalAction::Walk)
 3611  	{
 3612  		return;
 3613  	}
```

### `src/adapter/metamod/plugin_runtime.cpp:3636–3662`

```cpp
 3636  	const float distanceToTransition = std::hypot(
 3637  		decision.targetPosition.x - before.origin.x,
 3638  		decision.targetPosition.y - before.origin.y);
 3639  	gpMetaUtilFuncs->pfnLogConsole(
 3640  		pluginId_,
 3641  		"profile traversal bot_id=%u current_area=%u next_area=%u "
 3642  		"connection_direction=%u connection_how=%u current_attributes=%u "
 3643  		"next_attributes=%u traversal_intent=%d jump_required=%d "
 3644  		"target_point=(%.1f %.1f %.1f) distance_to_transition=%.1f "
 3645  		"grounded=%d ducked=%d velocity_z=%.1f buttons=%u IN_JUMP=%d "
 3646  		"stuck_time=%d recovery_state=%d corridor_index=%u",
 3647  		static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
 3648  		static_cast<unsigned int>(decision.currentArea),
 3649  		static_cast<unsigned int>(intent.targetArea),
 3650  		static_cast<unsigned int>(decision.linkDirection),
 3651  		static_cast<unsigned int>(decision.linkHow),
 3652  		static_cast<unsigned int>(currentAttributes),
 3653  		static_cast<unsigned int>(nextAttributes),
 3654  		static_cast<int>(intent.traversal),
 3655  		intent.traversal == nav::TraversalAction::Jump ? 1 : 0,
 3656  		decision.targetPosition.x, decision.targetPosition.y, decision.targetPosition.z,
 3657  		distanceToTransition, before.grounded ? 1 : 0, before.ducked ? 1 : 0,
 3658  		before.velocity.z, static_cast<unsigned int>(buttons),
 3659  		(buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U ? 1 : 0,
 3660  		decision.locomotionResult == nav::LocomotionResult::Stuck ? 1 : 0,
 3661  		static_cast<int>(decision.failureReason),
 3662  		static_cast<unsigned int>(decision.corridorIndex));
```

### `src/core/nav/locomotion.cpp:135–169`

```cpp
  135  	NavVector target = {};
  136  	AreaId targetArea = 0U;
  137  	const NavFollowerResult followerResult = pathFollower_.update(
  138  		snapshot,
  139  		observation.position,
  140  		config_.targetHorizontalTolerance,
  141  		config_.targetVerticalTolerance,
  142  		&target,
  143  		&targetArea);
  144  	const LocomotionResult mappedResult = mapFollowerResult(followerResult);
  145  	if (mappedResult == LocomotionResult::StaleSnapshot ||
  146  			mappedResult == LocomotionResult::InvalidSnapshot ||
  147  			mappedResult == LocomotionResult::InvalidCorridor ||
  148  			mappedResult == LocomotionResult::ResourceLimit)
  149  	{
  150  		active_ = false;
  151  		return mappedResult;
  152  	}
  153  	if (mappedResult == LocomotionResult::TargetReached)
  154  	{
  155  		active_ = false;
  156  		return mappedResult;
  157  	}
  158  	if (mappedResult != LocomotionResult::IntentReady)
  159  	{
  160  		return mappedResult;
  161  	}
  162  
  163  	return buildIntent(
  164  		snapshot,
  165  		observation,
  166  		currentMatch,
  167  		target,
  168  		targetArea,
  169  		intent);
```

### `src/adapter/metamod/plugin_runtime.cpp:2131–2141`

```cpp
 2131  		if (roamResult != runtime::NavRoamResult::IntentReady)
 2132  		{
 2133  			if (roamDecision.failureReason == runtime::NavFailureReason::None)
 2134  			{
 2135  				roamDecision.failureReason = runtime::NavFailureReason::MovementNotProduced;
 2136  			}
 2137  			logMovementDiagnostic(index, "roam_no_intent", &roamDecision);
 2138  			prepareNeutralManagedBotCommand(index, handle, before);
 2139  			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
 2140  			recordMovementPhysicsSample(index, before, receipt);
 2141  			continue;
```

### `src/adapter/metamod/plugin_runtime.cpp:2156–2194`

```cpp
 2156  			const runtime::ViewAngles movementViewAngles = {
 2157  				0.0f,
 2158  				movementYaw(locomotionIntent.direction),
 2159  				0.0f};
 2160  			command.viewAngles = movementViewAngles;
 2161  			std::uint16_t movementButtons =
 2162  				locomotionIntent.posture == nav::LocomotionPosture::Crouching
 2163  						? static_cast<std::uint16_t>(IN_DUCK)
 2164  						: static_cast<std::uint16_t>(0U);
 2165  			if (locomotionIntent.traversal == nav::TraversalAction::Jump)
 2166  			{
 2167  				movementButtons = static_cast<std::uint16_t>(movementButtons | IN_JUMP);
 2168  			}
 2169  			if (locomotionIntent.traversal == nav::TraversalAction::Crouch ||
 2170  					locomotionIntent.traversal == nav::TraversalAction::NarrowPassage)
 2171  			{
 2172  				movementButtons = static_cast<std::uint16_t>(movementButtons | IN_DUCK);
 2173  			}
 2174  		command.movement = {
 2175  			0.0f,
 2176  			0.0f,
 2177  			0.0f,
 2178  			movementButtons,
 2179  			0U,
 2180  			1U};
 2181  		const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
 2182  			decideManagedBotAction(index, before, movementViewAngles, movementButtons),
 2183  			false,
 2184  			movementViewAngles);
 2185  		const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
 2186  		command.viewAngles = actionDispatch.viewAngles;
 2187  		command.movement.buttons = actionDispatch.buttons;
 2188  		const MovementProjection movementProjection = ActionAdapter::projectMovement(
 2189  			locomotionIntent.direction.x,
 2190  			locomotionIntent.direction.y,
 2191  			locomotionIntent.speed,
 2192  			command.viewAngles.yaw);
 2193  		command.movement.forward = actionDispatch.stopMovement ? 0.0f : movementProjection.forward;
 2194  		command.movement.side = actionDispatch.stopMovement ? 0.0f : movementProjection.side;
```

## F08 — Team source 不一致

### `src/adapter/metamod/observation_adapter.cpp:199–207`

```cpp
  199  	observation->player.health = publicValue(
  200  		"OBS-PLAYER-HEALTH", actor, frame, timing, entity->v.health);
  201  	observation->player.armor = publicValue(
  202  		"OBS-PLAYER-ARMOR", actor, frame, timing, entity->v.armorvalue);
  203  	observation->player.team = publicValue(
  204  		"OBS-PLAYER-TEAM", actor, frame, timing, static_cast<std::int32_t>(entity->v.team));
  205  	observation->player.deadflag = publicValue(
  206  		"OBS-PLAYER-DEADFLAG", actor, frame, timing,
  207  		static_cast<std::int32_t>(entity->v.deadflag));
```

### `src/adapter/metamod/plugin_runtime.cpp:2492–2508`

```cpp
 2492  	int team = compatObservation.player.team.isAvailable()
 2493  		? compatObservation.player.team.value
 2494  		: static_cast<int>(handle.entity->v.team);
 2495  	if (team < 1 && managedBotTeamNumbers_[index] >= 1U &&
 2496  		managedBotTeamNumbers_[index] <= 2U)
 2497  	{
 2498  		team = static_cast<int>(managedBotTeamNumbers_[index]);
 2499  	}
 2500  	if (team < 1 && joinControllers_[index].teamConfirmed())
 2501  	{
 2502  		team = joinControllers_[index].requestedTeam() == compat::CommandTeam::Terrorist ? 1 :
 2503  			joinControllers_[index].requestedTeam() == compat::CommandTeam::CounterTerrorist ? 2 : 0;
 2504  	}
 2505  	if (team != 1 && team != 2)
 2506  	{
 2507  		return false;
 2508  	}
```

### `src/adapter/metamod/plugin_runtime.cpp:3392–3408`

```cpp
 3392  		if (observationResult == ObservationAdapterResult::Accepted)
 3393  		{
 3394  			const bool carryingC4 = observation.objective.carryingC4.isAvailable() &&
 3395  				observation.objective.carryingC4.value;
 3396  			const bool teamAvailable = observation.player.team.isAvailable();
 3397  			const int team = teamAvailable ? observation.player.team.value : 0;
 3398  			if (carryingC4 && team == 1)
 3399  			{
 3400  				context.requestTransition = machine.state() !=
 3401  					compat::CompatibilityStateId::PlantBomb;
 3402  				context.requestedState = compat::CompatibilityStateId::PlantBomb;
 3403  				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
 3404  				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
 3405  			}
 3406  		}
 3407  	}
 3408  	(void)machine.update(context);
```
