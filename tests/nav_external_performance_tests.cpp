#include "astrabot/metamod/nav_loader.hpp"
#include "astrabot/nav/nav_query.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}
	std::cerr << "FAIL: " << description << '\n';
	return false;
}

bool fileSize(const std::string &path, std::uint32_t *size)
{
	if (size == nullptr)
	{
		return false;
	}
	std::FILE *file = nullptr;
#ifdef _WIN32
	if (fopen_s(&file, path.c_str(), "rb") != 0)
	{
		return false;
	}
#else
	file = std::fopen(path.c_str(), "rb");
#endif
	if (file == nullptr || std::fseek(file, 0L, SEEK_END) != 0)
	{
		if (file != nullptr)
		{
			std::fclose(file);
		}
		return false;
	}
	const long length = std::ftell(file);
	std::fclose(file);
	if (length < 0L || static_cast<unsigned long>(length) > 0xFFFFFFFFUL)
	{
		return false;
	}
	*size = static_cast<std::uint32_t>(length);
	return true;
}

std::string externalNavPath()
{
#ifdef _WIN32
	char *value = nullptr;
	size_t length = 0U;
	if (_dupenv_s(&value, &length, "ASTRABOT_DE_DUST2_NAV") != 0 || value == nullptr)
	{
		return std::string();
	}
	const std::string result(value, length > 0U ? length - 1U : 0U);
	std::free(value);
	return result;
#else
	const char *value = std::getenv("ASTRABOT_DE_DUST2_NAV");
	return value == nullptr ? std::string() : std::string(value);
#endif
}

std::string siblingBsp(const std::string &navPath)
{
	const std::size_t slash = navPath.find_last_of("/\\");
	const std::string directory = slash == std::string::npos
		? std::string()
		: navPath.substr(0U, slash + 1U);
	return directory + "de_dust2.bsp";
}

bool runCase(
	const astrabot::nav::NavSnapshot &snapshot,
	astrabot::nav::AreaId start,
	astrabot::nav::AreaId goal,
	const char *name,
	bool requireFound,
	const astrabot::nav::NavQueryLimits *limits = nullptr,
	bool expectResourceLimit = false)
{
	const astrabot::nav::NavQuery query = limits == nullptr
		? astrabot::nav::NavQuery(snapshot)
		: astrabot::nav::NavQuery(snapshot, *limits);
	astrabot::nav::NavCorridor corridor = {};
	astrabot::nav::NavSearchStats stats = {};
	const auto begin = std::chrono::steady_clock::now();
	const astrabot::nav::NavQueryResult result = query.buildCorridor(
		start, goal, astrabot::nav::NavRouteType::Fastest, &corridor, &stats);
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - begin).count();
	std::cout << "nav_case=" << name
		<< " start=" << start
		<< " goal=" << goal
		<< " corridor_limit=" << (limits == nullptr
			? astrabot::nav::NavQueryLimits::kDefaultMaximumCorridorAreas
			: limits->maximumCorridorAreas)
		<< " search_limit=" << (limits == nullptr
			? astrabot::nav::NavQueryLimits::kDefaultMaximumSearchQueue
			: limits->maximumSearchQueue)
		<< " result=" << static_cast<int>(result)
		<< " wall_usec=" << elapsed
		<< " expanded=" << stats.expandedUniqueAreas
		<< " enqueues=" << stats.enqueueCount
		<< " reopens=" << stats.reopenCount
		<< " staleQueue=" << stats.staleQueueEntries
		<< " equalCostRepl=" << stats.equalCostReplacements
		<< " route_length=" << corridor.areas.size()
		<< " first_search_id=" << stats.firstSearchId
		<< " last_search_id=" << stats.lastSearchId
		<< " search_usec=" << stats.totalUsec
		<< " search_max_usec=" << stats.maxUsec
		<< '\n';
	if (result == astrabot::nav::NavQueryResult::ResourceLimit && expectResourceLimit)
	{
		return true;
	}
	if (result == astrabot::nav::NavQueryResult::ResourceLimit)
	{
		std::cerr << "FAIL: " << name << " returned ResourceLimit\n";
		return false;
	}
	if (expectResourceLimit)
	{
		std::cerr << "FAIL: " << name << " did not return ResourceLimit\n";
		return false;
	}
	return !requireFound ||
		(check(result == astrabot::nav::NavQueryResult::Found,
			"required route must be found") &&
		 check(corridor.areas.size() <= (limits == nullptr
			? astrabot::nav::NavQueryLimits::kDefaultMaximumCorridorAreas
			: limits->maximumCorridorAreas),
			"found route fits final corridor capacity"));
}

bool directedBfs(
	const astrabot::nav::NavSnapshot &snapshot,
	astrabot::nav::AreaId start,
	astrabot::nav::AreaId goal,
	std::size_t *visitedCount,
	std::size_t *routeLength)
{
	const astrabot::nav::NavDocument *document = snapshot.document();
	if (document == nullptr || document->findArea(start) == nullptr ||
		document->findArea(goal) == nullptr)
	{
		return false;
	}
	std::vector<astrabot::nav::AreaId> queue;
	std::vector<astrabot::nav::AreaId> visited;
	std::vector<std::size_t> distances;
	queue.push_back(start);
	visited.push_back(start);
	distances.push_back(0U);
	for (std::size_t index = 0U; index < queue.size(); ++index)
	{
		const astrabot::nav::NavArea *area = document->findArea(queue[index]);
		if (area == nullptr)
		{
			return false;
		}
		if (area->id == goal)
		{
			if (visitedCount != nullptr)
			{
				*visitedCount = visited.size();
			}
			if (routeLength != nullptr)
			{
				*routeLength = distances[index];
			}
			return true;
		}
		for (std::size_t direction = 0U;
			direction < astrabot::nav::NavArea::kDirectionCount;
			++direction)
		{
			for (const astrabot::nav::AreaId next : area->connections[direction])
			{
				if (std::find(visited.begin(), visited.end(), next) == visited.end())
				{
					visited.push_back(next);
					queue.push_back(next);
					distances.push_back(distances[index] + 1U);
				}
			}
		}
	}
	return false;
}
}

int main()
{
	const std::string navPath = externalNavPath();
	if (navPath.empty())
	{
		std::cout << "SKIP: set ASTRABOT_DE_DUST2_NAV to an external de_dust2.nav\n";
		return 77;
	}
	const std::string bspPath = siblingBsp(navPath);
	std::uint32_t bspSize = 0U;
	if (!check(fileSize(bspPath, &bspSize), "external de_dust2.bsp is readable"))
	{
		return 1;
	}

	astrabot::metamod::NavLoader loader;
	astrabot::nav::NavSnapshotPublisher publisher;
	astrabot::metamod::NavLoadDiagnostic diagnostic = {};
	const astrabot::metamod::NavLoadRequest request = {
		navPath.c_str(), "de_dust2", 1U, true, bspSize, false, 0U};
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			astrabot::metamod::NavLoadResult::Loaded,
		"external de_dust2.nav loads through NavLoader"))
	{
		return 1;
	}

	const astrabot::nav::NavSnapshot snapshot = publisher.snapshot();
	if (!check(snapshot.document() != nullptr &&
		snapshot.document()->findArea(41U) != nullptr &&
		snapshot.document()->findArea(90U) != nullptr &&
		snapshot.document()->findArea(1438U) != nullptr &&
		snapshot.document()->findArea(5U) != nullptr &&
		snapshot.document()->findArea(1520U) != nullptr,
		"required de_dust2 areas exist"))
	{
		return 1;
	}
	std::size_t bfsVisited = 0U;
	std::size_t bfsLength = 0U;
	const bool bfsReachable = directedBfs(
		snapshot, 41U, 90U, &bfsVisited, &bfsLength);
	std::cout << "bfs_case=41_to_90 reachable=" << (bfsReachable ? 1 : 0)
		<< " visited=" << bfsVisited << " route_length=" << bfsLength << '\n';
	if (!check(bfsReachable, "directed NAV BFS reaches area 90 from area 41"))
	{
		return 1;
	}

	const bool objectivePass = runCase(
		snapshot, 41U, 90U, "objective_41_to_90_default", true);
	const astrabot::nav::NavQueryLimits explicitBudget256 = {256U, 256U};
	const bool explicitBudgetPass = runCase(
		snapshot, 41U, 90U, "objective_41_to_90_explicit_budget_256", false,
		&explicitBudget256, true);
	const bool firstControlPass = runCase(
		snapshot, 41U, 1438U, "control_41_to_1438", true);
	const bool secondControlPass = runCase(
		snapshot, 5U, 1520U, "control_5_to_1520", true);
	return objectivePass && explicitBudgetPass && firstControlPass && secondControlPass ? 0 : 1;
}
