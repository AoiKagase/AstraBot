#include "astrabot/compat/buy_controller.hpp"
#include <algorithm>
#include <cmath>
#include <array>

namespace astrabot
{
namespace compat
{
namespace
{
constexpr int kWeaponMaskBits = 32;
constexpr int kChancePercentMaximum = 100;
constexpr int kKitDecisionStep = 0;
constexpr int kShieldDecisionStep = 1;
constexpr int kGrenadeDecisionStep = 2;
constexpr int kPistolDecisionStep = 3;
constexpr int kKitBuyChance = 50;
constexpr int kShieldDecisionMoney = 2500;
constexpr int kHighMoneyThreshold = 4000;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BUY_CONTROLLER_LOW_MONEY_SHIELD_CHANCE 33.3f
constexpr float kHighMoneyShieldChance = 10.0f;
#define ASTRABOT_BUY_CONTROLLER_GRENADE_BUY_CHANCE 33.3f
constexpr int kGlockWeaponId = 17;
constexpr int kUspWeaponId = 16;
constexpr int kLowMoneyPistolThreshold = 1000;
constexpr float kHighPistolBuyChance = 75.0f;
#define ASTRABOT_BUY_CONTROLLER_NORMAL_PISTOL_BUY_CHANCE 33.3f
constexpr double kInitialBuyDelaySeconds = 2.0;
constexpr int kArmorReceipt = 1;
constexpr int kHelmetReceipt = 2;
constexpr int kDefuseKitReceipt = 3;
constexpr int kPrimaryAmmoReceipt = 4;
constexpr int kSecondaryAmmoReceipt = 5;
constexpr int kShieldReceipt = 6;
constexpr int kFullArmor = 100;
#define ASTRABOT_BUY_CONTROLLER_RECEIPT_WAIT_SECONDS 0.2
constexpr int kMaximumReceiptRetries = 3;
constexpr int kPrimaryStage = 0;
constexpr int kPrimaryAmmoStage = 1;
constexpr int kArmorStage = 2;
constexpr int kPistolStage = 3;
constexpr int kSecondaryAmmoStage = 4;
constexpr int kGrenadeStage = 5;
constexpr int kDefuseKitStage = 6;
constexpr int kMaximumStagePasses = 9;
constexpr int kMaximumWeaponPreferences = 16;
constexpr int kMaximumPreferenceAttempts = 2;
constexpr int kShieldPreferenceId = 99;
constexpr int kShieldPrice = 2200;
constexpr int kMaximumPrimaryAttempts = 6;
constexpr int kSniperBuyChance = 50;
constexpr int kPrimaryCandidateCapacity = 32;
constexpr int kPreferredSecondaryCapacity = 3;
constexpr int kPreferredPrimaryDifficulty = 2;
constexpr int kPrimaryAmmoPrice = 60;
constexpr int kHelmetPrice = 1000;
constexpr int kArmorPrice = 650;
constexpr int kSecondaryAmmoPrice = 20;
constexpr int kGrenadeBudget = 300;
constexpr int kSmokeChance = 10;
constexpr int kFlashChance = 35;
constexpr int kSmokeWeaponId = 9;
constexpr int kFlashWeaponId = 25;
constexpr int kHeWeaponId = 4;
constexpr int kDefuseKitPrice = 200;
enum class Group
{
	Pistol,
	Shotgun,
	Smg,
	Rifle,
	Sniper,
	MachineGun,
	Grenade
};
struct Item
{
	int id, price;
	const char *alias;
	ProfileTeam team;
	Group group;
	bool preferred;
};
const Item stock[] = {{21, 1700, "m3", ProfileTeam::Any, Group::Shotgun, false},
	{5, 3000, "xm1014", ProfileTeam::Any, Group::Shotgun, false},
	{23, 1250, "tmp", ProfileTeam::CounterTerrorist, Group::Smg, false},
	{7, 1400, "mac10", ProfileTeam::Terrorist, Group::Smg, false},
	{19, 1500, "mp5", ProfileTeam::Any, Group::Smg, false}, {12, 1700, "ump45", ProfileTeam::Any, Group::Smg, false},
	{30, 2350, "p90", ProfileTeam::Any, Group::Smg, false},
	{15, 2250, "famas", ProfileTeam::CounterTerrorist, Group::Rifle, true},
	{14, 2000, "galil", ProfileTeam::Terrorist, Group::Rifle, true},
	{28, 2500, "ak47", ProfileTeam::Terrorist, Group::Rifle, true},
	{27, 3500, "sg552", ProfileTeam::Terrorist, Group::Rifle, true},
	{3, 2750, "scout", ProfileTeam::Any, Group::Sniper, false},
	{22, 3100, "m4a1", ProfileTeam::CounterTerrorist, Group::Rifle, true},
	{8, 3500, "aug", ProfileTeam::CounterTerrorist, Group::Rifle, false},
	{13, 4200, "sg550", ProfileTeam::CounterTerrorist, Group::Sniper, true},
	{18, 4750, "awp", ProfileTeam::Any, Group::Sniper, true},
	{24, 5000, "g3sg1", ProfileTeam::Terrorist, Group::Sniper, true},
	{20, 5750, "m249", ProfileTeam::Any, Group::MachineGun, false},
	{1, 600, "p228", ProfileTeam::Any, Group::Pistol, true}, {26, 650, "deagle", ProfileTeam::Any, Group::Pistol, true},
	{11, 750, "fn57", ProfileTeam::CounterTerrorist, Group::Pistol, true},
	{10, 800, "elites", ProfileTeam::Terrorist, Group::Pistol, true},
	{16, 500, "usp", ProfileTeam::CounterTerrorist, Group::Pistol, false},
	{17, 400, "glock", ProfileTeam::Terrorist, Group::Pistol, false},
	{4, 300, "hegren", ProfileTeam::Any, Group::Grenade, false},
	{9, 300, "sgren", ProfileTeam::Any, Group::Grenade, false},
	{25, 200, "flash", ProfileTeam::Any, Group::Grenade, false}};
bool owns(const BuyContext &c, int id)
{
	return id > 0 && id < kWeaponMaskBits && (c.weaponMask & (1U << id)) != 0;
}

bool primary(Group g)
{
	return g != Group::Pistol && g != Group::Grenade;
}

bool allowedCategory(const Item &w, const BuyContext &c)
{
	switch (w.group)
	{
	case Group::Pistol:
		return c.allowPistols;
	case Group::Shotgun:
		return c.allowShotguns;
	case Group::Smg:
		return c.allowSubMachineGuns;
	case Group::Rifle:
		return c.allowRifles;
	case Group::Sniper:
		return c.allowSnipers;
	case Group::MachineGun:
		return c.allowMachineGuns;
	case Group::Grenade:
		return c.allowGrenades;
	}
	return false;
}

bool allowed(const Item &w, const BuyContext &c)
{
	return (w.team == ProfileTeam::Any || w.team == c.team) && allowedCategory(w, c);
}

bool draw(const BuyContext &c, const char *name, float chance, bool &value)
{
	if (!c.random)
		return false;
	const auto r =
		c.random->nextFloat(RandomRequest::floatRequest(name, c.randomActor, c.randomTiming, 0, kChancePercentMaximum));
	if (r.status != RandomStatus::Ok || !std::isfinite(r.value) || r.value < 0 || r.value > kChancePercentMaximum)
		return false;
	value = r.value < chance;
	return true;
}

const Item *choose(const BuyContext &c, const Item *const *candidates, std::size_t count, const char *name)
{
	if (count == 0 || !c.random)
		return nullptr;
	const auto r = c.random->nextLong(
		RandomRequest::longRequest(name, c.randomActor, c.randomTiming, 0, static_cast<int>(count) - 1));
	return r.status == RandomStatus::Ok && r.value >= 0 && static_cast<std::size_t>(r.value) < count
		? candidates[r.value]
		: nullptr;
}

const Item *stockItem(int id)
{
	for (const auto &item : stock)
		if (item.id == id)
			return &item;
	return nullptr;
}

const char *csBotPreferenceAlias(int id, const BuyContext &c)
{
	constexpr int c4Id = 6;
	constexpr int knifeId = 29;
	if (id == kShieldPreferenceId)
		return c.allowShield ? "shield" : nullptr;
	if (id == c4Id)
		return "c4";
	if (id == knifeId)
		return "knife";
	const Item *item = stockItem(id);
	if (!item)
		return nullptr;
	// The reference applies category policy only to aliases in its random stock tables.
	const bool outsideStock = id == kUspWeaponId || id == kGlockWeaponId || item->group == Group::Grenade;
	return outsideStock || allowedCategory(*item, c) ? item->alias : nullptr;
}

bool hasPrimaryWeapon(const BuyContext &c)
{
	if (c.hasShield)
		return true;
	for (const auto &item : stock)
		if (primary(item.group) && owns(c, item.id))
			return true;
	return false;
}
}
void BuyController::reset()
{
	*this = BuyController();
}

void BuyController::bind(const BuyContext &c)
{
	if (generation_ != c.generation || round_ != c.round)
	{
		reset();
		generation_ = c.generation;
		round_ = c.round;
	}
}

bool BuyController::initialize(const BuyContext &c, bool hasPrimary, bool hasPistol)
{
	if (!initialized_)
	{
		if (initializationStep_ == kKitDecisionStep)
		{
			if (c.team == ProfileTeam::CounterTerrorist && c.bombScenario && !c.career && c.allowDefuseKit &&
				!draw(c, "buy.defuse_kit", kKitBuyChance, buyKit_))
				return false;
			++initializationStep_;
		}
		if (initializationStep_ == kShieldDecisionStep)
		{
			if (c.team == ProfileTeam::CounterTerrorist && !hasPrimary && c.allowShield &&
				c.money > kShieldDecisionMoney &&
				!draw(c, "buy.shield",
					c.money < kHighMoneyThreshold ? ASTRABOT_BUY_CONTROLLER_LOW_MONEY_SHIELD_CHANCE
												  : kHighMoneyShieldChance,
					buyShield_))
				return false;
			++initializationStep_;
		}
		if (initializationStep_ == kGrenadeDecisionStep)
		{
			if (c.allowGrenades && !draw(c, "buy.grenade", ASTRABOT_BUY_CONTROLLER_GRENADE_BUY_CHANCE, buyGrenade_))
				return false;
			++initializationStep_;
		}
		if (initializationStep_ == kPistolDecisionStep && c.allowPistols)
		{
			if (!hasPistol)
				buyPistol_ = true;
			else if (owns(c, c.team == ProfileTeam::Terrorist ? kGlockWeaponId : kUspWeaponId))
			{
				const bool pistolOnly = !c.allowShotguns && !c.allowSubMachineGuns && !c.allowRifles &&
					!c.allowMachineGuns && !c.allowShield && !c.allowSnipers;
				if (!draw(c, "buy.pistol",
						pistolOnly || c.money < kLowMoneyPistolThreshold
							? kHighPistolBuyChance
							: ASTRABOT_BUY_CONTROLLER_NORMAL_PISTOL_BUY_CHANCE,
						buyPistol_))
					return false;
			}
		}
		initialized_ = true;
	}
	return true;
}

BuyDecision BuyController::update(const BuyContext &c, const ProfileRecord &profile)
{
	bind(c);
	if (!c.alive || !c.inBuyZone || !c.buyTimeOpen || !std::isfinite(c.nowSeconds) ||
		!std::isfinite(c.stateEnteredSeconds))
		return {BuyStatus::Unavailable, nullptr, 0};
	if (!c.moneyKnown || !c.weaponMaskKnown || c.money < 0)
		return {BuyStatus::UnknownObservation, nullptr, 0};
	bool hasPrimary = false, hasPistol = false, hasGrenade = false;
	for (const auto &w : stock)
		if (owns(c, w.id))
		{
			hasPrimary = hasPrimary || primary(w.group);
			hasPistol = hasPistol || w.group == Group::Pistol;
			hasGrenade = hasGrenade || w.group == Group::Grenade;
		}
	if (!initialize(c, hasPrimary, hasPistol))
		return {BuyStatus::Unavailable, nullptr, 0};
	if (c.nowSeconds - c.stateEnteredSeconds < kInitialBuyDelaySeconds)
		return {BuyStatus::WaitingReceipt, nullptr, 0};
	if (complete_)
		return {BuyStatus::Complete, nullptr, 0};
	if (receiptFailed_)
		return {BuyStatus::ReceiptTimeout, nullptr, 0};
	if (pendingWeapon_ || pendingEquipment_)
	{
		const bool uncertainEquipment = pendingEquipment_ < 0;
		const int equipmentReceipt = uncertainEquipment ? -pendingEquipment_ : pendingEquipment_;
		bool receipt = pendingWeapon_ && owns(c, pendingWeapon_);
		if (equipmentReceipt == kArmorReceipt)
			receipt = c.armorKnown && c.armor >= kFullArmor;
		if (equipmentReceipt == kHelmetReceipt)
			receipt = c.armorKnown && c.armor >= kFullArmor && c.helmetKnown && c.hasHelmet;
		if (equipmentReceipt == kDefuseKitReceipt)
			receipt = c.defuseKitKnown && c.hasDefuseKit;
		if (equipmentReceipt == kPrimaryAmmoReceipt)
			receipt = c.ammoKnown && !c.needsPrimaryAmmo;
		if (equipmentReceipt == kSecondaryAmmoReceipt)
			receipt = c.ammoKnown && !c.needsSecondaryAmmo;
		if (equipmentReceipt == kShieldReceipt)
			receipt = c.shieldKnown && c.hasShield;
		if (!receipt && c.nowSeconds - sentAt_ < ASTRABOT_BUY_CONTROLLER_RECEIPT_WAIT_SECONDS)
			return {BuyStatus::WaitingReceipt, nullptr, pendingWeapon_};
		if (!receipt && uncertainEquipment)
		{
			// Unknown equipment permits one authoritative GameDLL attempt. A missing
			// receipt is not success: report it once and skip only this equipment so
			// later purchases are not indefinitely blocked by absent HUD messages.
			pendingWeapon_ = 0;
			pendingEquipment_ = 0;
			retries_ = 0;
			++stage_;
			return {BuyStatus::ReceiptTimeout, nullptr, 0};
		}
		if (!receipt && retries_ >= kMaximumReceiptRetries && stage_ != kPrimaryStage)
		{
			receiptFailed_ = true;
			return {BuyStatus::ReceiptTimeout, nullptr, 0};
		}
		if (receipt)
		{
			if (stage_ == kPrimaryStage)
				stage_ = kPrimaryAmmoStage;
			else
				++stage_;
			retries_ = 0;
		}
		pendingWeapon_ = 0;
		pendingEquipment_ = 0;
	}
	const auto equipment = [this, &c](const char *alias, int receipt, bool uncertain = false) -> BuyDecision
	{
		pendingEquipment_ = uncertain ? -receipt : receipt;
		sentAt_ = c.nowSeconds;
		++retries_;
		return {BuyStatus::Command, alias, 0};
	};
	const auto weapon = [this, &c](const Item &item) -> BuyDecision
	{
		pendingWeapon_ = item.id;
		sentAt_ = c.nowSeconds;
		++retries_;
		return {BuyStatus::Command, item.alias, item.id};
	};
	for (int guard = 0; guard < kMaximumStagePasses; ++guard)
	{
		if (stage_ == kPrimaryStage)
		{
			if (hasPrimary || (c.shieldKnown && c.hasShield))
			{
				stage_ = kPrimaryAmmoStage;
				continue;
			}
			while (preferenceIndex_ < profile.weaponPreferenceCount && preferenceIndex_ < kMaximumWeaponPreferences)
			{
				if (preferenceAttempts_ >= kMaximumPreferenceAttempts)
				{
					++preferenceIndex_;
					preferenceAttempts_ = 0;
					continue;
				}
				if (profile.weaponPreferences[preferenceIndex_] == kShieldPreferenceId &&
					c.team == ProfileTeam::CounterTerrorist && c.allowShield && c.money >= kShieldPrice)
				{
					++preferenceAttempts_;
					return equipment("shield", kShieldReceipt);
				}
				const Item *preferred = nullptr;
				for (const auto &w : stock)
					if (w.id == profile.weaponPreferences[preferenceIndex_] && allowed(w, c))
					{
						preferred = &w;
						break;
					}
				if (!preferred || !primary(preferred->group) || c.money < preferred->price)
				{
					++preferenceIndex_;
					preferenceAttempts_ = 0;
					continue;
				}
				++preferenceAttempts_;
				return weapon(*preferred);
			}
			if (primaryAttempts_++ >= kMaximumPrimaryAttempts)
			{
				stage_ = kPrimaryAmmoStage;
				continue;
			}
			if (buyShield_ && c.money >= kShieldPrice)
				return equipment("shield", kShieldReceipt);
			bool wantSniper = false;
			if (!draw(c, "buy.want_sniper", kSniperBuyChance, wantSniper))
				return {BuyStatus::Unavailable, nullptr, 0};
			std::array<const Item *, kPrimaryCandidateCapacity> candidates = {};
			std::size_t count = 0;
			for (const auto &w : stock)
				if (primary(w.group) && allowed(w, c) && (w.group != Group::Sniper || wantSniper) && c.money >= w.price)
					candidates[count++] = &w;
			if (count == 0)
			{
				stage_ = kPrimaryAmmoStage;
				continue;
			}
			if (primaryAttempts_ == 1 && profile.difficulty >= kPreferredPrimaryDifficulty)
			{
				std::array<const Item *, kPrimaryCandidateCapacity> preferred = {};
				std::size_t preferredCount = 0;
				for (std::size_t i = 0; i < count; ++i)
					if (candidates[i]->preferred)
						preferred[preferredCount++] = candidates[i];
				if (preferredCount)
				{
					candidates = preferred;
					count = preferredCount;
				}
			}
			const Item *selected = choose(c, candidates.data(), count, "buy.primary");
			if (!selected)
				return {BuyStatus::Unavailable, nullptr, 0};
			return weapon(*selected);
		}
		if (stage_ == kPrimaryAmmoStage)
		{
			if (c.ammoKnown && c.needsPrimaryAmmo && hasPrimary && c.money >= kPrimaryAmmoPrice)
				return equipment("primammo", kPrimaryAmmoReceipt);
			++stage_;
			continue;
		}
		if (stage_ == kArmorStage)
		{
			if (c.allowArmor && (!c.helmetKnown || !c.hasHelmet) && c.money >= kHelmetPrice)
				return equipment("vesthelm", kHelmetReceipt, !c.armorKnown || !c.helmetKnown);
			if (c.allowArmor && (!c.armorKnown || c.armor < kFullArmor) && c.money >= kArmorPrice)
				return equipment("vest", kArmorReceipt, !c.armorKnown);
			++stage_;
			continue;
		}
		if (stage_ == kPistolStage)
		{
			if (c.allowPistols && buyPistol_)
			{
				const Item *selected = nullptr;
				for (std::size_t i = 0; i < profile.weaponPreferenceCount && i < kMaximumWeaponPreferences && !selected;
					 ++i)
					for (const auto &w : stock)
						if (w.group == Group::Pistol && w.id == profile.weaponPreferences[i] && allowed(w, c) &&
							c.money >= w.price)
						{
							selected = &w;
							break;
						}
				if (!selected)
				{
					std::array<const Item *, kPreferredSecondaryCapacity> candidates = {};
					std::size_t count = 0;
					for (const auto &w : stock)
						if (w.group == Group::Pistol && w.preferred && allowed(w, c) && c.money >= w.price &&
							count < candidates.size())
							candidates[count++] = &w;
					if (count)
						selected = choose(c, candidates.data(), count, "buy.secondary");
					if (count && !selected)
						return {BuyStatus::Unavailable, nullptr, 0};
				}
				if (selected && !owns(c, selected->id))
					return weapon(*selected);
			}
			++stage_;
			continue;
		}
		if (stage_ == kSecondaryAmmoStage)
		{
			if (c.allowPistols && c.ammoKnown && c.needsSecondaryAmmo && c.money >= kSecondaryAmmoPrice)
				return equipment("secammo", kSecondaryAmmoReceipt);
			++stage_;
			continue;
		}
		if (stage_ == kGrenadeStage)
		{
			if (buyGrenade_ && !hasGrenade && c.money >= kGrenadeBudget)
			{
				if (!c.random)
					return {BuyStatus::Unavailable, nullptr, 0};
				const auto r = c.random->nextFloat(RandomRequest::floatRequest(
					"buy.grenade_type", c.randomActor, c.randomTiming, 0, kChancePercentMaximum));
				if (r.status != RandomStatus::Ok || !std::isfinite(r.value) || r.value < 0 ||
					r.value > kChancePercentMaximum)
					return {BuyStatus::Unavailable, nullptr, 0};
				const int id = r.value < kSmokeChance
					? kSmokeWeaponId
					: (c.teamAllBots && r.value < kFlashChance ? kFlashWeaponId : kHeWeaponId);
				for (const auto &w : stock)
					if (w.id == id)
						return weapon(w);
			}
			++stage_;
			continue;
		}
		if (stage_ == kDefuseKitStage)
		{
			if (buyKit_ && (!c.defuseKitKnown || !c.hasDefuseKit) && c.money >= kDefuseKitPrice)
				return equipment("defuser", kDefuseKitReceipt, !c.defuseKitKnown);
			++stage_;
			continue;
		}
		complete_ = true;
		return {BuyStatus::Complete, nullptr, 0};
	}
	return {BuyStatus::Unavailable, nullptr, 0};
}

BuyStatus BuyController::updateCsBot(const BuyContext &c, const ProfileRecord &profile, int difficulty,
	const std::function<BuyInventory(const char *)> &buy)
{
	constexpr float buyInterval = 0.2f;
	// These tables preserve the reference's candidate indices and team order.
	constexpr std::array<int, 13> ctPrimary = {21, 5, 23, 19, 12, 30, 15, 3, 22, 8, 13, 18, 20};
	constexpr std::array<int, 13> tPrimary = {21, 5, 7, 19, 12, 30, 14, 28, 3, 27, 18, 24, 20};
	bind(c);
	if (complete_)
		return BuyStatus::Complete;
	if (!c.alive || !c.weaponMaskKnown || c.team == ProfileTeam::Any || !std::isfinite(c.nowSeconds) ||
		!std::isfinite(c.stateEnteredSeconds) || c.nowSeconds < c.stateEnteredSeconds)
		return BuyStatus::Unavailable;
	const float now = static_cast<float>(c.nowSeconds);
	bool hasPrimary = hasPrimaryWeapon(c);
	bool hasPistol = false;
	bool hasGrenade = false;
	for (const auto &item : stock)
	{
		if (owns(c, item.id))
		{
			hasPistol = hasPistol || item.group == Group::Pistol;
			hasGrenade = hasGrenade || item.group == Group::Grenade;
		}
	}
	if (!initialized_)
	{
		if (!c.moneyKnown)
		{
			if (now - static_cast<float>(c.stateEnteredSeconds) < kInitialBuyDelaySeconds)
				return BuyStatus::UnknownObservation;
			if (!c.inBuyZone)
			{
				complete_ = true;
				return BuyStatus::Complete;
			}
		}
		if (!initialize(c, hasPrimary, hasPistol))
			return BuyStatus::Unavailable;
		csBotBuyTimestamp_ = static_cast<float>(c.stateEnteredSeconds);
	}
	if (csBotInitialDelay_)
	{
		if (now - csBotBuyTimestamp_ < kInitialBuyDelaySeconds)
			return BuyStatus::WaitingReceipt;
		csBotInitialDelay_ = false;
	}
	if (!c.inBuyZone)
	{
		complete_ = true;
		return BuyStatus::Complete;
	}
	if (now - csBotBuyTimestamp_ <= buyInterval)
		return BuyStatus::WaitingReceipt;
	csBotBuyTimestamp_ = now;

	const auto preferenceCount = (std::min)(profile.weaponPreferenceCount,
		static_cast<std::size_t>(kMaximumWeaponPreferences));
	if (preferenceIndex_ < preferenceCount)
	{
		if (preferenceAttempts_ >= kMaximumPreferenceAttempts)
		{
			++preferenceIndex_;
			preferenceAttempts_ = 0;
			return BuyStatus::WaitingReceipt;
		}
		const int id = profile.weaponPreferences[preferenceIndex_];
		if (c.activeWeapon == id || (id == kShieldPreferenceId && c.hasShield))
		{
			preferenceIndex_ = preferenceCount;
			return BuyStatus::WaitingReceipt;
		}
		++preferenceAttempts_;
		if (const char *alias = csBotPreferenceAlias(id, c))
		{
			buy(alias);
			return BuyStatus::Command;
		}
	}

	if (!hasPrimary)
	{
		const char *alias = nullptr;
		if (buyShield_)
			alias = "shield";
		else
		{
			bool wantSniper = false;
			if (!draw(c, "buy.want_sniper", kSniperBuyChance, wantSniper))
				return BuyStatus::Unavailable;
			std::array<const Item *, ctPrimary.size()> candidates{}, preferred{};
			std::size_t count = 0, preferredCount = 0;
			for (int id : c.team == ProfileTeam::Terrorist ? tPrimary : ctPrimary)
			{
				const Item *item = stockItem(id);
				if (allowed(*item, c) && (item->group != Group::Sniper || wantSniper))
				{
					candidates[count++] = item;
					if (item->preferred)
						preferred[preferredCount++] = item;
				}
			}
			if (count > 0)
			{
				const bool pickPreferred = primaryAttempts_ == 0 &&
					difficulty >= kPreferredPrimaryDifficulty && preferredCount > 0;
				const Item *item = choose(c, pickPreferred ? preferred.data() : candidates.data(),
					pickPreferred ? preferredCount : count, "buy.primary");
				if (!item)
					return BuyStatus::Unavailable;
				alias = item->alias;
			}
		}
		if (alias)
		{
			const auto inventory = buy(alias);
			BuyContext observed = c;
			observed.weaponMask = inventory.weapons;
			observed.hasShield = inventory.hasShield;
			hasPrimary = hasPrimaryWeapon(observed);
		}
	}
	if (!hasPrimary && primaryAttempts_++ < kMaximumPrimaryAttempts)
		return BuyStatus::WaitingReceipt;

	if (hasPrimary)
		buy("primammo");
	buy("vesthelm");
	buy("vest");
	if (c.allowPistols)
	{
		bool hasPistolPreference = false;
		for (std::size_t i = 0; i < preferenceCount; ++i)
		{
			const Item *item = stockItem(profile.weaponPreferences[i]);
			hasPistolPreference = hasPistolPreference || (item && item->group == Group::Pistol);
		}
		if (buyPistol_ && !hasPistolPreference)
		{
			const std::array<const Item *, kPreferredSecondaryCapacity> pistols = {
				stockItem(1), stockItem(26), stockItem(c.team == ProfileTeam::Terrorist ? 10 : 11)};
			const Item *item = choose(c, pistols.data(), pistols.size(), "buy.secondary");
			if (item)
				buy(item->alias);
		}
		buy("secammo");
	}
	if (buyGrenade_ && !hasGrenade && c.random)
	{
		const auto grenade = c.random->nextFloat(RandomRequest::floatRequest(
			"buy.grenade_type", c.randomActor, c.randomTiming, 0, kChancePercentMaximum));
		if (grenade.status == RandomStatus::Ok && std::isfinite(grenade.value) && grenade.value >= 0 &&
			grenade.value <= kChancePercentMaximum)
			buy(grenade.value < 10.0f ? "sgren" : c.teamAllBots && grenade.value < 35.0f ? "flash" : "hegren");
	}
	if (buyKit_)
		buy("defuser");
	complete_ = true;
	return BuyStatus::Complete;
}
}
}
#undef ASTRABOT_BUY_CONTROLLER_LOW_MONEY_SHIELD_CHANCE
#undef ASTRABOT_BUY_CONTROLLER_GRENADE_BUY_CHANCE
#undef ASTRABOT_BUY_CONTROLLER_NORMAL_PISTOL_BUY_CHANCE
#undef ASTRABOT_BUY_CONTROLLER_RECEIPT_WAIT_SECONDS
