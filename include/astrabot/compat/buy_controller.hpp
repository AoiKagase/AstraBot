#ifndef ASTRABOT_COMPAT_BUY_CONTROLLER_HPP
#define ASTRABOT_COMPAT_BUY_CONTROLLER_HPP

#include "astrabot/compat/profile_catalog.hpp"
#include <cstdint>
#include <functional>

namespace astrabot
{
namespace compat
{
enum class BuyStatus
{
	Command,
	UnknownObservation,
	WaitingReceipt,
	Complete,
	Unavailable,
	ReceiptTimeout
};
struct BuyContext
{
	double nowSeconds = 0;
	std::uint32_t generation = 0, round = 0;
	bool alive = false, inBuyZone = false, buyTimeOpen = false;
	ProfileTeam team = ProfileTeam::Any;
	bool moneyKnown = false;
	int money = 0;
	bool weaponMaskKnown = false;
	std::uint32_t weaponMask = 0;
	bool allowRifles = false, allowSnipers = false, allowShotguns = false;
	bool allowMachineGuns = false, allowPistols = false, allowGrenades = false;
	bool allowArmor = false;
	bool armorKnown = false, helmetKnown = false, hasHelmet = false;
	int armor = 0;
	bool defuseKitKnown = false, hasDefuseKit = false, allowDefuseKit = false;
	bool ammoKnown = false, needsPrimaryAmmo = false, needsSecondaryAmmo = false;
	ICompatibilityRandomSource *random = nullptr;
	RandomActor randomActor = {};
	RandomTimingContext randomTiming = {};
	double stateEnteredSeconds = 0;
	bool allowSubMachineGuns = false, allowShield = false;
	bool bombScenario = false, career = false, teamAllBots = false;
	bool shieldKnown = false, hasShield = false;
	int activeWeapon = 0;
};
struct BuyDecision
{
	BuyStatus status;
	const char *command;
	int weaponId;
};
struct BuyInventory
{
	std::uint32_t weapons;
	bool hasShield;
};
// Receipts use inventory observations: sending an alias never mutates observed money/loadout.
class BuyController
{
public:
	BuyDecision update(const BuyContext &context, const ProfileRecord &profile);
	// Dispatch a GameDLL command and read back the resulting public inventory.
	BuyStatus updateCsBot(const BuyContext &context, const ProfileRecord &profile, int difficulty,
		const std::function<BuyInventory(const char *)> &buy);
	void reset();

private:
	void bind(const BuyContext &context);
	bool initialize(const BuyContext &context, bool hasPrimary, bool hasPistol);
	std::uint32_t generation_ = 0, round_ = 0;
	int pendingWeapon_ = 0, retries_ = 0;
	double sentAt_ = 0;
	float csBotBuyTimestamp_ = 0;
	bool csBotInitialDelay_ = true;
	bool complete_ = false;
	int pendingEquipment_ = 0;
	bool initialized_ = false, buyKit_ = false, buyGrenade_ = false, buyPistol_ = false, buyShield_ = false;
	int stage_ = 0, primaryAttempts_ = 0;
	std::size_t preferenceIndex_ = 0;
	int preferenceAttempts_ = 0;
	bool receiptFailed_ = false;
	int initializationStep_ = 0;
};
}
}
#endif
