#ifndef ASTRABOT_COMPAT_CHATTER_PLACE_NAMES_HPP
#define ASTRABOT_COMPAT_CHATTER_PLACE_NAMES_HPP
#include <cstddef>
#include <cstdint>
#include <string>
namespace astrabot
{
namespace compat
{
// Symbolic NAV name/ID facts used by the native no-Place fallback. Sorted lookup
// is independent of the native table traversal; no voice data is embedded.
inline std::uint32_t defaultChatterPlaceId(const char *name)
{
	if (!name)
		return 0;
	std::string key(name);
	for (auto &c : key)
		if (c >= 'a' && c <= 'z')
			c = static_cast<char>(c - 'a' + 'A');
	struct Entry
	{
		const char *name;
		std::uint32_t id;
	};
	static const Entry entries[] = {
		{"AIRPLANE", 61},
		{"ALLEY", 82},
		{"APARTMENT", 12},
		{"APARTMENTS", 13},
		{"ATRIUM", 96},
		{"ATTIC", 52},
		{"BACK", 66},
		{"BACKALLEY", 83},
		{"BACKDOOR", 33},
		{"BACKHALL", 47},
		{"BACKROOM", 86},
		{"BACKWAY", 35},
		{"BACKYARD", 37},
		{"BALCONY", 81},
		{"BASEMENT", 22},
		{"BATHROOM", 92},
		{"BEDROOM", 90},
		{"BIGOFFICE", 58},
		{"BOMBSITEA", 1},
		{"BOMBSITEB", 2},
		{"BOMBSITEC", 3},
		{"BRIDGE", 9},
		{"BUNKER", 63},
		{"COMPUTERROOM", 57},
		{"CONFERENCEROOM", 56},
		{"COURTYARD", 30},
		{"CRATES", 88},
		{"CRAWLSPACE", 23},
		{"CTSPAWN", 7},
		{"DECK", 101},
		{"DEN", 94},
		{"DOUBLEDOORS", 42},
		{"DOWNSTAIRS", 21},
		{"DUCTS", 17},
		{"DUMPSTER", 60},
		{"ELEVATOR", 41},
		{"ENTRANCE", 78},
		{"ENTRYWAY", 97},
		{"FAMILYROOM", 91},
		{"FARSIDE", 49},
		{"FENCE", 100},
		{"FOYER", 98},
		{"FRONT", 65},
		{"FRONTDOOR", 32},
		{"FRONTHALL", 46},
		{"FRONTROOM", 85},
		{"FRONTYARD", 36},
		{"GARAGE", 29},
		{"GATE", 74},
		{"GATEHOUSE", 75},
		{"GUARDHOUSE", 77},
		{"HOSTAGERESCUEZONE", 5},
		{"HOSTAGES", 4},
		{"HOUSE", 11},
		{"INSIDE", 25},
		{"KITCHEN", 24},
		{"LADDER", 73},
		{"LITTLEOFFICE", 59},
		{"LIVINGROOM", 93},
		{"LOADINGDOCK", 76},
		{"LOBBY", 39},
		{"LOFT", 80},
		{"LONGHALL", 44},
		{"MAINHALL", 48},
		{"MARKET", 14},
		{"MEETINGROOM", 55},
		{"MIDDLE", 10},
		{"MINES", 64},
		{"OFFICE", 95},
		{"OUTSIDE", 26},
		{"OVERPASS", 71},
		{"PATIO", 103},
		{"PORCH", 102},
		{"PROJECTORROOM", 54},
		{"RAMP", 69},
		{"REAR", 67},
		{"ROOF", 19},
		{"SECURITYDOORS", 43},
		{"SEWERS", 15},
		{"SIDE", 68},
		{"SIDEALLEY", 84},
		{"SIDEDOOR", 34},
		{"SIDEHALL", 45},
		{"SIDEROOM", 87},
		{"SIDEYARD", 38},
		{"STAIRS", 72},
		{"STAIRWELL", 99},
		{"STORAGEROOM", 53},
		{"TOWER", 27},
		{"TRUCK", 89},
		{"TSPAWN", 8},
		{"TUNNEL", 16},
		{"UNDERGROUND", 62},
		{"UNDERPASS", 70},
		{"UPSTAIRS", 20},
		{"VAULT", 40},
		{"VENDINGMACHINES", 79},
		{"VILLAGE", 18},
		{"VIPRESCUEZONE", 6},
		{"WALL", 104},
		{"WATER", 31},
		{"WINDOW", 51},
		{"WINDOWS", 50},
		{"WINECELLAR", 28},
	};
	std::size_t first = 0, last = sizeof(entries) / sizeof(entries[0]);
	while (first < last)
	{
		const auto middle = first + (last - first) / 2;
		const int comparison = key.compare(entries[middle].name);
		if (comparison == 0)
			return entries[middle].id;
		if (comparison < 0)
			last = middle;
		else
			first = middle + 1;
	}
	return 0;
}
}
}
#endif
