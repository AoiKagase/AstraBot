#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr unsigned kTerroristTeam = 1U;
constexpr unsigned kCounterTerroristTeam = 2U;
constexpr float kSecondsPerMinute = 60.0f;
constexpr int kPrimaryWeaponSlot = 0;
constexpr int kSecondaryWeaponSlot = 1;
}
using namespace runtime_detail;

void BotBehaviorRuntime::updatePublicBuying(std::size_t index)
{
	if (index >= services_->service_managedBotHandles().size() || services_->service_globals() == nullptr)
		return;
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.entity->free || handle.entity->v.deadflag != DEAD_NO)
		return;
	compat::PublicActorState observation{};
	if (!services_->observation_publicObservations().read({handle.actor.slot, handle.actor.actorGeneration},
			{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
				services_->service_adapterFrameCount()},
			&observation))
		return;
	// The controller cannot buy outside the observed zone. Avoid querying
	// weapon policies for a newly spawned native Buy state that cannot use them.
	if (!observation.buyZoneKnown || !observation.inBuyZone)
		return;
	compat::BuyContext context{};
	context.nowSeconds = services_->service_globals()->time;
	context.stateEnteredSeconds = services_->service_roundFreezeStartTime();
	context.generation = handle.actor.actorGeneration;
	context.round = services_->service_lifecycle().roundGeneration();
	context.alive = true;
	context.inBuyZone = observation.buyZoneKnown && observation.inBuyZone;
	const float buyTime = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_buytime");
	context.buyTimeOpen = std::isfinite(buyTime) &&
		services_->service_globals()->time <= services_->service_roundFreezeStartTime() + buyTime * kSecondsPerMinute;
	context.team = services_->observation_resolvePlayerTeam(handle.entity) == kTerroristTeam
		? compat::ProfileTeam::Terrorist
		: services_->observation_resolvePlayerTeam(handle.entity) == kCounterTerroristTeam
		? compat::ProfileTeam::CounterTerrorist
		: compat::ProfileTeam::Any;
	context.moneyKnown = observation.moneyKnown;
	context.money = observation.money;
	context.weaponMaskKnown = true;
	context.weaponMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	const auto allowed = [this](const char *name)
	{
		if (services_->service_engineFunctions() == nullptr ||
			services_->service_engineFunctions()->pfnCVarGetPointer == nullptr ||
			services_->service_engineFunctions()->pfnCVarGetPointer(name) == nullptr)
			return true;
		const float value = readOptionalCvarFloat(services_->service_engineFunctions(), name);
		return !std::isfinite(value) || value != 0.0f;
	};
	context.allowRifles = allowed("bot_allow_rifles");
	context.allowSnipers = allowed("bot_allow_snipers");
	context.allowShotguns = allowed("bot_allow_shotguns");
	context.allowMachineGuns = allowed("bot_allow_machine_guns");
	context.allowSubMachineGuns = allowed("bot_allow_sub_machine_guns");
	context.allowPistols = allowed("bot_allow_pistols");
	context.allowGrenades = allowed("bot_allow_grenades");
	context.allowArmor = true;
	context.armorKnown = std::isfinite(handle.entity->v.armorvalue);
	context.armor = static_cast<int>(handle.entity->v.armorvalue);
	context.helmetKnown = observation.helmetKnown;
	context.hasHelmet = observation.hasHelmet;
	context.allowDefuseKit = true;
	context.defuseKitKnown = observation.defuseKitKnown;
	context.hasDefuseKit = observation.hasDefuseKit;
	context.random = &services_->service_compatibilityRandomSource();
	context.randomActor = {handle.actor.slot, handle.actor.actorGeneration};
	context.randomTiming = {services_->command_managedBotCommandSequences(index), 0U,
		services_->service_managedBotFullUpdateSequences()[index]};
	context.bombScenario = managedObjectiveSiteRegistry_.registeredCount != 0U;
	const compat::PublicWeaponDefinition weapon =
		services_->observation_publicObservations().definition(observation.weaponId);
	context.ammoKnown = observation.weaponKnown && weapon.known && weapon.ammoIndex >= 0 &&
		observation.ammoKnown[static_cast<std::size_t>(weapon.ammoIndex)];
	if (context.ammoKnown)
	{
		const bool needsAmmo = observation.ammo[static_cast<std::size_t>(weapon.ammoIndex)] < weapon.maxAmmo;
		context.needsPrimaryAmmo = weapon.slot == kPrimaryWeaponSlot && needsAmmo;
		context.needsSecondaryAmmo = weapon.slot == kSecondaryWeaponSlot && needsAmmo;
	}
	const compat::BuyDecision decision = actors_[index].buyControllers.update(context, actors_[index].managedProfiles);
	actors_[index].buyingComplete = decision.status == compat::BuyStatus::Complete;
	if (decision.status != compat::BuyStatus::ReceiptTimeout)
		actors_[index].buyReceiptTimeoutLogged = false;
	if (decision.status == compat::BuyStatus::ReceiptTimeout && !actors_[index].buyReceiptTimeoutLogged &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		actors_[index].buyReceiptTimeoutLogged = true;
		importantLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"buy receipt unconfirmed actor=%u map=%u round=%u; equipment remains unknown",
			static_cast<unsigned int>(handle.actor.slot), services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration());
	}
	if (decision.status == compat::BuyStatus::Command && decision.command != nullptr)
		(void)services_->command_dispatchClientCommand(handle.entity, decision.command, "");
}

void BotBehaviorRuntime::resetRoundBuyingState()
{
	reset_buyingComplete();
	reset_escapingWithKnife();
	reset_buyReceiptTimeoutLogged();
}

bool BotBehaviorRuntime::shouldBuy(std::size_t index, const compat::StateUpdateContext &context) const
{
	compat::PublicActorState buyingObservation{};
	const float buyMinutes = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_buytime");
	return !actors_[index].buyingComplete && context.alive && services_->service_globals() != nullptr &&
		std::isfinite(buyMinutes) && buyMinutes > 0.0f &&
		services_->service_globals()->time <=
		services_->service_roundFreezeStartTime() + buyMinutes * kSecondsPerMinute &&
		services_->observation_publicObservations().read(context.actor, context.frame, &buyingObservation) &&
		buyingObservation.buyZoneKnown && buyingObservation.inBuyZone;
}

} // namespace metamod
} // namespace astrabot
