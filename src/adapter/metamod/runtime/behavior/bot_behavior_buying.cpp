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
constexpr int kShieldEnabledGameState = 0;

float readBuyMinutes(const enginefuncs_t *engine, bool *rulesAvailable)
{
	*rulesAvailable = engine && engine->pfnCVarGetPointer && engine->pfnCVarGetFloat &&
		engine->pfnCVarGetPointer("mp_buytime") != nullptr;
	return *rulesAvailable ? engine->pfnCVarGetFloat("mp_buytime") : 0.0f;
}
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
	const auto *engine = services_->service_engineFunctions();
	const bool native = actors_[index].buyUsesCsBot;
	const bool observed = services_->observation_publicObservations().read({handle.actor.slot, handle.actor.actorGeneration},
			{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
				services_->service_adapterFrameCount()},
			&observation);
	if (!native && (!observed || !observation.buyZoneKnown || !observation.inBuyZone))
		return;
	compat::BuyContext context{};
	context.nowSeconds = services_->service_globals()->time;
	context.stateEnteredSeconds = services_->service_roundFreezeStartTime();
	if (native)
	{
		context.stateEnteredSeconds = (std::max)(context.stateEnteredSeconds,
			static_cast<double>(actors_[index].managedBotStateMachines.stateTimestamp()));
		if (context.nowSeconds < context.stateEnteredSeconds || actors_[index].buyingComplete)
			return;
	}
	context.generation = handle.actor.actorGeneration;
	context.round = services_->service_lifecycle().roundGeneration();
	context.alive = true;
	context.inBuyZone = observation.buyZoneKnown && observation.inBuyZone;
	if (!native)
	{
		const float buyTime = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_buytime");
		context.buyTimeOpen = std::isfinite(buyTime) &&
			context.nowSeconds <= services_->service_roundFreezeStartTime() + buyTime * kSecondsPerMinute;
	}
	const unsigned team = services_->observation_resolvePlayerTeam(handle.entity);
	context.team = team == kTerroristTeam
		? compat::ProfileTeam::Terrorist
		: (native ? team : services_->observation_resolvePlayerTeam(handle.entity)) == kCounterTerroristTeam
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
	if (native)
	{
		if (!context.bombScenario && team == kCounterTerroristTeam && engine != nullptr &&
			engine->pfnFindEntityByString != nullptr && engine->pfnSzFromIndex != nullptr)
		{
			const auto hasSite = [engine](const char *classname)
			{
				const auto *site = engine->pfnFindEntityByString(nullptr, "classname", classname);
				return site && !site->free && ActionAdapter::isBombTargetClassname(
					engine->pfnSzFromIndex(site->v.classname));
			};
			context.bombScenario = hasSite("func_bomb_target") || hasSite("info_bomb_target");
		}
		const bool shieldSupported = engine->pfnCVarGetPointer("bot_allow_shield") != nullptr;
		context.allowShield = shieldSupported && engine->pfnCVarGetFloat("bot_allow_shield") != 0.0f;
		context.hasShield = shieldSupported && handle.entity->v.gamestate == kShieldEnabledGameState;
		context.activeWeapon = observation.weaponKnown ? observation.weaponId : 0;
		context.teamAllBots = true;
		if (engine != nullptr && engine->pfnPEntityOfEntIndex != nullptr)
		{
			for (int slot = 1; slot <= services_->service_globals()->maxClients; ++slot)
			{
				const auto *player = engine->pfnPEntityOfEntIndex(slot);
				if (player && player != handle.entity && !player->free && (player->v.flags & FL_CLIENT) &&
					!(player->v.flags & (FL_FAKECLIENT | FL_DORMANT)) &&
					services_->observation_resolvePlayerTeam(player) == static_cast<int>(team))
				{
					context.teamAllBots = false;
					break;
				}
			}
		}
		const auto &configuration = services_->service_compatibilitySurface().configuration();
		const auto current = [&, actor = handle.actor, entity = handle.entity, serial = handle.entity->serialnumber,
								 map = services_->service_lifecycle().mapGeneration()]
		{
			return handle.actor.actorGeneration == actor.actorGeneration && handle.entity == entity && !entity->free &&
				entity->serialnumber == serial && entity->v.deadflag == DEAD_NO &&
				services_->service_lifecycle().mapGeneration() == map &&
				services_->service_lifecycle().roundGeneration() == context.round;
		};
		const auto buy = [this, entity = handle.entity, shieldSupported, &current](const char *alias)
		{
			if (!current() || !services_->command_dispatchClientCommand(entity, alias, nullptr) || !current())
				return compat::BuyInventory{};
			return compat::BuyInventory{static_cast<std::uint32_t>(entity->v.weapons),
				shieldSupported && entity->v.gamestate == kShieldEnabledGameState};
		};
		auto controller = actors_[index].buyControllers;
		const auto status = controller.updateCsBot(
			context, actors_[index].managedProfiles, configuration.botDifficulty, buy);
		if (!current())
			return;
		actors_[index].buyControllers = controller;
		actors_[index].buyingComplete = status == compat::BuyStatus::Complete;
		return;
	}
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

bool BotBehaviorRuntime::usesCsBotBuying() const
{
	return services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		services_->service_roundFreezeGeneration() == services_->service_lifecycle().roundGeneration() &&
		services_->service_navPublisher().snapshot().isValid();
}

void BotBehaviorRuntime::resetRoundBuyingState()
{
	reset_buyingComplete();
	for (auto &actor : actors_)
		actor.buyUsesCsBot = false;
	reset_escapingWithKnife();
	reset_buyReceiptTimeoutLogged();
}

bool BotBehaviorRuntime::shouldBuy(std::size_t index, const compat::StateUpdateContext &context)
{
	auto &actor = actors_[index];
	const auto *globals = services_->service_globals();
	bool rulesAvailable = false;
	const float buyMinutes = readBuyMinutes(services_->service_engineFunctions(), &rulesAvailable);
	actor.buyUsesCsBot = rulesAvailable && usesCsBotBuying();
	if (!context.alive || globals == nullptr)
		return false;

	const float roundStart = services_->service_roundFreezeStartTime();
	if (actor.buyUsesCsBot)
	{
		if (actor.buyingComplete)
			return globals->time < services_->service_roundFreezeUntil();
		return !services_->service_roundOver() ||
			globals->time >= roundStart ||
			actor.managedBotStateMachines.state() == compat::CompatibilityStateId::Buy;
	}
	if (actor.buyingComplete)
		return false;

	// Native Buy waits through the scheduled restart, before buy-zone signals are available.
	const bool awaitingRestart = actor.managedBotStateMachines.state() == compat::CompatibilityStateId::Buy &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		services_->service_roundOver() && services_->service_roundFreezeGeneration() == context.frame.roundGeneration &&
		globals->time < roundStart;

	const bool buyTimeOpen = std::isfinite(buyMinutes) && buyMinutes > 0.0f &&
		globals->time <= roundStart + buyMinutes * kSecondsPerMinute;
	if (!awaitingRestart && !buyTimeOpen)
		return false;

	compat::PublicActorState buyingObservation{};
	if (!services_->observation_publicObservations().read(context.actor, context.frame, &buyingObservation) ||
		!buyingObservation.buyZoneKnown)
		return awaitingRestart;
	return buyTimeOpen && buyingObservation.inBuyZone;
}

} // namespace metamod
} // namespace astrabot
