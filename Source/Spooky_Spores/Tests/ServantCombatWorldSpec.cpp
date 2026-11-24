// Automation Spec for the servant combat (AttackDefense role) inside a minimal test world.
// Source of truth: docs/specs/servant-combat.md (removed at merge), acceptance criteria 1 to 9 and "Cas limites". Criterion 4 (enemy registry)
// is in EnemyRegistrySpec.cpp; criterion 10 (Success out of range / repath of the ENEMY soldier) is in EnemyTargetReservationSpec.cpp.
// Written from the spec only (no implementation file read): tunable values are read by reflection, by the names of the spec.
//
// World: FTestWorldWrapper Game world, time advances only through TickTestWorld, NO NavMesh and no floor. Servants are ATestCombatServant
// (their chase goes through the RequestMoveToEnemy seam: recorded, never moves) and enemies are ATestPassiveEnemySoldier (they do nothing,
// so every observation is "what the servant did to them"). Characters never move by themselves: a test moves an enemy by hand to play
// the arrival / the exit of the zone, as the real chase or the real enemy would.
// Layout (X axis, everything at height 100, default zone: X and Y in [-500, 500], height ignored):
//   servant at the origin unless said otherwise; "in range" = measured gap of half the attack range, "out of range" = range + 150.
//   Gaps are converted to centre distances with the capsule radii (the range is measured from the capsule, spec "Combat").
// Every test names, in a "Mutation:" comment, the behaviour change that must turn it red.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantCombatTestHelpers.h"
#include "Tests/ServantFieldTestHelpers.h"
#include "Tests/ServantWanderTestHelpers.h"
#include "Base/BaseZone.h"
#include "Enemies/Destructible.h"
#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemyRegistrySubsystem.h"
#include "Enemies/EnemySoldier.h"
#include "Enemies/EnemyTargetSubsystem.h"
#include "Health/HealthComponent.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "Servants/ServantSubsystem.h"
#include "Servants/ServantTask.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Templates/Function.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FServantCombatWorldSpec, "SpookySpores.Servants.Combat.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr double TimerSlack = 0.25;
	static constexpr float TickStep = 0.05f;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	// Read by reflection from the objects under test.
	double InitialDecisionDelay = 0.0;
	double RetryDelay = 0.0;
	double CombatNextDelay = 0.0;
	double CombatWalkSpeed = 0.0;
	double WanderWalkSpeed = 0.0;
	double AttackDamage = 0.0;
	double EnemyMaxHealth = 0.0;

	struct FFighter
	{
		ATestCombatServant* Servant = nullptr;
		ATestCombatServantAIController* Ctl = nullptr;
		double Range = 0.0;
		double Interval = 0.0;
	};

	bool RequireWorld();
	ABaseZone* SpawnZone();
	// Spawns the servant on the seam controller, reads its tunables, assigns the task (Unassigned: left as is).
	bool MakeFighter(FFighter& Out, const TCHAR* Name, const FVector& Location, EServantTask Task);
	ATestPassiveEnemySoldier* MakeEnemy(const TCHAR* Name, const FVector& Location);
	// Centre-to-centre distance for a measured gap (see ServantCombatTest::CentreDistanceForGap), from the default capsules.
	double CentreDistance(double Gap) const;
	// Number of hits the enemy has taken, deduced from its health (-1 if it no longer exists).
	int32 HitsOn(const AEnemySoldier* Enemy) const;
	bool StartFight(const FFighter& Fighter);
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);
	void TickWatching(double Seconds, TFunctionRef<void()> Observer);
	void ExpectNeverFights(EServantTask Task, bool bHitTheServantFirst);

	// Enemy-scan wake-up tests (spec "Reveil par detection").
	// Observation budget before the wake-up under test: the pause of the failed wander is RetryDelay, so PreWatch + the wake window
	// must stay below it, or the regular decision timer (not the scan) could be what starts the fight.
	static constexpr double PreWatch = 0.6;
	double EnemyScanInterval = 0.0;
	// A wake-up must happen within EnemyScanInterval + slack (+ two ticks of granularity).
	double WakeWindow() const { return EnemyScanInterval + TimerSlack + 2.0 * TickStep; }
	bool ReadScanSettings(const FFighter& Fighter, bool bNeedPauseLength);
	// Servant on the seam controller, first decision done: it is wandering (a real Wander decision, paused for RetryDelay: no NavMesh).
	bool MakeWanderingFighter(FFighter& Out, const FVector& Location, EServantTask Task);
	void ExpectWanderNotWoken(EServantTask Task);

	// Unreachable-enemy tests (spec "Ennemi inatteignable"): a servant whose chase of a lone out-of-range enemy fails synchronously.
	double FailedEnemyCooldown = 0.0;
	double FailTime = 0.0; // world time at which the failed request was seen (up to one tick after the real failure)
	// Spawns zone, servant (AttackDefense) and one out-of-range enemy, lets the first chase fail, then waits for the Wander decision that
	// follows (a pause of RetryDelay, the servant wanders). Fills Out/OutEnemy/FailTime/FailedEnemyCooldown.
	bool MakeFailedChase(FFighter& Out, ATestPassiveEnemySoldier*& OutEnemy);

END_DEFINE_SPEC(FServantCombatWorldSpec)

bool FServantCombatWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

ABaseZone* FServantCombatWorldSpec::SpawnZone()
{
	ABaseZone* Zone = ServantFieldTest::SpawnZone(World, TEXT("CombatZone"), true);
	TestNotNull(TEXT("harness: zone"), Zone);
	return Zone;
}

bool FServantCombatWorldSpec::MakeFighter(FFighter& Out, const TCHAR* Name, const FVector& Location, EServantTask Task)
{
	Out.Servant = ServantCombatTest::SpawnServant(World, Name, Location);
	if (!TestNotNull(TEXT("harness: servant spawned"), Out.Servant)) { return false; }
	Out.Ctl = ServantCombatTest::ControllerOf(Out.Servant);
	if (!TestNotNull(TEXT("harness: servant possessed by the seam controller"), Out.Ctl)) { return false; }
	const bool bRead = ServantWanderTest::ReadNumber(Out.Ctl, TEXT("InitialDecisionDelay"), InitialDecisionDelay)
		&& ServantWanderTest::ReadNumber(Out.Ctl, TEXT("RetryDelay"), RetryDelay)
		&& ServantWanderTest::ReadNumber(Out.Ctl, TEXT("CombatNextDelay"), CombatNextDelay)
		&& ServantWanderTest::ReadNumber(Out.Ctl, TEXT("CombatWalkSpeed"), CombatWalkSpeed)
		&& ServantWanderTest::ReadNumber(Out.Ctl, TEXT("WanderWalkSpeed"), WanderWalkSpeed)
		&& ServantWanderTest::ReadNumber(Out.Servant, TEXT("AttackDamage"), AttackDamage);
	if (!TestTrue(TEXT("harness: tunables readable by reflection (InitialDecisionDelay, RetryDelay, CombatNextDelay, CombatWalkSpeed, WanderWalkSpeed, AttackDamage)"), bRead)) { return false; }
	Out.Range = Out.Servant->GetAttackRange();
	Out.Interval = Out.Servant->GetAttackInterval();
	if (!TestTrue(TEXT("harness: positive range, interval and damage"), Out.Range > 0.0 && Out.Interval > 0.0 && AttackDamage > 0.0)) { return false; }
	if (RetryDelay < 10.0 * TickStep || CombatNextDelay < 4.0 * TickStep || Out.Interval < 10.0 * TickStep)
	{
		AddError(TEXT("harness: a delay is too short for the tick step of these tests."));
		return false;
	}
	return TestTrue(TEXT("harness: task assigned"), ServantFieldTest::Assign(World, Out.Servant, Task));
}

ATestPassiveEnemySoldier* FServantCombatWorldSpec::MakeEnemy(const TCHAR* Name, const FVector& Location)
{
	ATestPassiveEnemySoldier* Enemy = ServantCombatTest::SpawnEnemy(World, Name, Location);
	if (!TestNotNull(TEXT("harness: enemy spawned"), Enemy)) { return nullptr; }
	if (!TestTrue(TEXT("harness: enemy MaxHealth readable by reflection"), ServantWanderTest::ReadNumber(ServantCombatTest::HealthOf(Enemy), TEXT("MaxHealth"), EnemyMaxHealth)))
	{
		return nullptr;
	}
	return Enemy;
}

double FServantCombatWorldSpec::CentreDistance(double Gap) const
{
	return ServantCombatTest::CentreDistanceForGap(GetDefault<ATestCombatServant>(), GetDefault<ATestPassiveEnemySoldier>(), Gap);
}

int32 FServantCombatWorldSpec::HitsOn(const AEnemySoldier* Enemy) const
{
	const UHealthComponent* Health = IsValid(Enemy) ? ServantCombatTest::HealthOf(Enemy) : nullptr;
	if (Health == nullptr || AttackDamage <= 0.0) { return -1; }
	return FMath::RoundToInt((1.0 - Health->GetHealthPercent()) * EnemyMaxHealth / AttackDamage);
}

bool FServantCombatWorldSpec::StartFight(const FFighter& Fighter)
{
	const AServantAIController* Ctl = Fighter.Ctl;
	return TestTrue(TEXT("precondition: the servant starts a fight at its first decision"),
		TickUntil([Ctl]() { return Ctl->IsFighting(); }, InitialDecisionDelay + TimerSlack));
}

void FServantCombatWorldSpec::TickFor(double Seconds)
{
	ServantFieldTest::TickFor(*Wrapper, Seconds);
}

bool FServantCombatWorldSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	return ServantFieldTest::TickUntil(*Wrapper, Condition, MaxSeconds);
}

void FServantCombatWorldSpec::TickWatching(double Seconds, TFunctionRef<void()> Observer)
{
	ServantFieldTest::TickWatching(*Wrapper, Seconds, Observer);
}

// Servant with a task that must never fight, an enemy right in reach, a long watch. Optionally the servant is hit first (a hit enemy
// would retaliate if it could; here the enemy is passive: the point is that being hit changes nothing for a non-fighter).
void FServantCombatWorldSpec::ExpectNeverFights(EServantTask Task, bool bHitTheServantFirst)
{
	if (!RequireWorld()) { return; }
	if (SpawnZone() == nullptr) { return; }
	FFighter F;
	if (!MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), Task)) { return; }
	ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
	if (Enemy == nullptr) { return; }

	bool bEverFighting = false;
	bool bEverAttack = false;
	auto Observe = [&]()
	{
		bEverFighting |= F.Ctl->IsFighting();
		bEverAttack |= (F.Ctl->GetLastAction() == EServantWanderAction::Attack);
	};
	TickWatching(InitialDecisionDelay + TimerSlack, Observe); // past the first decision
	if (bHitTheServantFirst)
	{
		UGameplayStatics::ApplyDamage(F.Servant, static_cast<float>(AttackDamage), nullptr, Enemy, UDamageType::StaticClass());
		const UHealthComponent* Health = ServantCombatTest::HealthOf(F.Servant);
		TestTrue(TEXT("precondition: the servant was hurt but is alive"), IsValid(F.Servant) && Health != nullptr && Health->GetHealthPercent() < 1.0f && !Health->IsDead());
	}
	TickWatching(12.0, Observe); // several decisions (each pause is at most MaxWanderPause)

	TestFalse(TEXT("never IsFighting, not even transiently"), bEverFighting);
	TestFalse(TEXT("the action was never Attack"), bEverAttack);
	TestNull(TEXT("GetFightTarget is null"), F.Ctl->GetFightTarget());
	TestEqual(TEXT("the enemy in reach was never hit"), HitsOn(Enemy), 0);
	TestEqual(TEXT("no chase request was ever made"), F.Ctl->RequestedEnemies.Num(), 0);
	const UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
	TestTrue(TEXT("the task is unchanged"), Subsystem != nullptr && Subsystem->GetServantTask(F.Servant) == Task);
}

bool FServantCombatWorldSpec::ReadScanSettings(const FFighter& Fighter, bool bNeedPauseLength)
{
	double MinWanderPause = 0.0;
	const bool bRead = ServantWanderTest::ReadNumber(Fighter.Ctl, TEXT("EnemyScanInterval"), EnemyScanInterval)
		&& ServantWanderTest::ReadNumber(Fighter.Ctl, TEXT("MinWanderPause"), MinWanderPause);
	if (!TestTrue(TEXT("harness: EnemyScanInterval and MinWanderPause readable by reflection"), bRead)) { return false; }
	// RetryDelay was read by MakeFighter. The scan must be clearly faster than any regular pause, or the tests cannot tell them apart.
	return TestTrue(TEXT("harness: PreWatch + wake window must stay below RetryDelay and MinWanderPause (tunables changed?)"),
		EnemyScanInterval > 0.0 && (!bNeedPauseLength || (PreWatch + WakeWindow() < RetryDelay && PreWatch + WakeWindow() < MinWanderPause)));
}

bool FServantCombatWorldSpec::MakeWanderingFighter(FFighter& Out, const FVector& Location, EServantTask Task)
{
	if (!MakeFighter(Out, TEXT("Servant"), Location, Task)) { return false; }
	if (!ReadScanSettings(Out, true)) { return false; }
	const AServantAIController* Ctl = Out.Ctl;
	const bool bWandering = TickUntil([Ctl]() { return Ctl->IsWandering() && Ctl->GetLastAction() == EServantWanderAction::Wander; }, InitialDecisionDelay + TimerSlack);
	return TestTrue(TEXT("precondition: the first decision was a Wander (nothing to fight yet) and the servant is wandering"), bWandering);
}

// A servant that must not be woken by the scan, an enemy that appears in reach AFTER the wander began (the scan is the only path to a fight).
void FServantCombatWorldSpec::ExpectWanderNotWoken(EServantTask Task)
{
	if (!RequireWorld()) { return; }
	if (SpawnZone() == nullptr) { return; }
	FFighter F;
	if (!MakeWanderingFighter(F, FVector(0.0, 0.0, 100.0), Task)) { return; }
	ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
	if (Enemy == nullptr) { return; }

	bool bEverFighting = false;
	TickWatching(PreWatch + WakeWindow(), [&]() { bEverFighting |= F.Ctl->IsFighting(); });
	TestFalse(TEXT("never IsFighting after an enemy entered the zone"), bEverFighting);
	TestNull(TEXT("no fight target"), F.Ctl->GetFightTarget());
	TestEqual(TEXT("the enemy in reach was not hit"), HitsOn(Enemy), 0);
	TestEqual(TEXT("no chase request"), F.Ctl->RequestedEnemies.Num(), 0);
}

bool FServantCombatWorldSpec::MakeFailedChase(FFighter& Out, ATestPassiveEnemySoldier*& OutEnemy)
{
	ABaseZone* Zone = SpawnZone();
	if (Zone == nullptr || !MakeFighter(Out, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return false; }
	if (!ReadScanSettings(Out, true)) { return false; }
	if (!TestTrue(TEXT("harness: FailedEnemyCooldown readable by reflection"), ServantWanderTest::ReadNumber(Out.Ctl, TEXT("FailedEnemyCooldown"), FailedEnemyCooldown))) { return false; }
	// The scenario needs room: RetryDelay (post-failure pause) + one wake window must fit inside the cooldown.
	if (!TestTrue(TEXT("harness: FailedEnemyCooldown > RetryDelay + wake window (tunables changed?)"), FailedEnemyCooldown > RetryDelay + WakeWindow() + 4.0 * TickStep)) { return false; }
	OutEnemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(Out.Range + 150.0), 0.0, 100.0));
	if (OutEnemy == nullptr) { return false; }
	TestTrue(TEXT("precondition: the enemy is in the zone"), Zone->IsPointInZone(OutEnemy->GetActorLocation()));

	Out.Ctl->bFailSynchronously = true;
	const ATestCombatServantAIController* Ctl = Out.Ctl;
	if (!TestTrue(TEXT("precondition: the first decision requests a chase"), TickUntil([Ctl]() { return Ctl->RequestedEnemies.Num() >= 1; }, InitialDecisionDelay + TimerSlack))) { return false; }
	FailTime = World->GetTimeSeconds();
	TestFalse(TEXT("precondition: the chase failed at once (not fighting)"), Out.Ctl->IsFighting());
	// The decision that follows the failure (RetryDelay later) is a Wander, with the enemy still in the zone: it is what the memory must produce.
	const bool bWander = TickUntil([Ctl]() { return Ctl->GetLastAction() == EServantWanderAction::Wander && Ctl->IsWandering(); }, RetryDelay + 2.0 * TimerSlack);
	return TestTrue(TEXT("precondition: after the failure the next decision is a Wander (the unreachable enemy is not retried at once)"), bWander)
		&& TestEqual(TEXT("precondition: still a single chase request"), Out.Ctl->RequestedEnemies.Num(), 1);
}

void FServantCombatWorldSpec::Define()
{
	BeforeEach([this]()
	{
		Wrapper = MakeUnique<FTestWorldWrapper>();
		bWorldReady = Wrapper->CreateTestWorld(EWorldType::Game) && Wrapper->BeginPlayInTestWorld();
		World = Wrapper->GetTestWorld();
		if (!bWorldReady)
		{
			TArray<FString> Messages;
			Wrapper->AppendErrorMessages(Messages);
			for (const FString& Message : Messages) { AddError(FString::Printf(TEXT("FTestWorldWrapper: %s"), *Message)); }
		}
	});

	AfterEach([this]()
	{
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criteria 1 and 3: the servant is a destructible; its death.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Servant as a destructible (criteria 1, 3)", [this]()
	{
		It("C1: implements IDestructible, exposes its own health component, starts alive at full health", [this]()
		{
			// Mutation: AServant does not implement IDestructible (enemies could never target it), GetHealthComponent returns null or
			// another component, or the servant starts dead / below full health.
			if (!RequireWorld()) { return; }
			if (SpawnZone() == nullptr) { return; }
			AServant* Servant = ServantCombatTest::SpawnServant(World, TEXT("Servant"), FVector(0.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: servant"), Servant)) { return; }
			TestTrue(TEXT("Implements<UDestructible>"), Servant->Implements<UDestructible>());
			UHealthComponent* Health = Servant->Implements<UDestructible>() ? IDestructible::Execute_GetHealthComponent(Servant) : nullptr;
			if (!TestNotNull(TEXT("GetHealthComponent is not null"), Health)) { return; }
			TestTrue(TEXT("it is the servant's own health component"), Health == Servant->FindComponentByClass<UHealthComponent>());
			TestFalse(TEXT("not dead at the start"), Health->IsDead());
			TestEqual(TEXT("full health at the start"), Health->GetHealthPercent(), 1.0f);
		});

		It("C3: lethal damage destroys the servant, unregisters it, spawns no drop (a non-lethal hit does none of it); numbers are not reused", [this]()
		{
			// Mutation: OnDeath does not destroy the servant (still valid), it stays registered (EndPlay does not unregister, or Destroy is
			// replaced by a hide), it spawns an AResourceDrop (like the enemy soldier does), a non-lethal hit kills it, or the number of the
			// dead servant is handed out again.
			if (!RequireWorld()) { return; }
			if (SpawnZone() == nullptr) { return; }
			UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
			AServant* Servant = ServantCombatTest::SpawnServant(World, TEXT("Servant"), FVector(0.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: subsystem"), Subsystem) || !TestNotNull(TEXT("harness: servant"), Servant)) { return; }
			TestTrue(TEXT("precondition: registered"), Subsystem->IsServantRegistered(Servant));
			const int32 DeadNumber = Subsystem->GetServantNumber(Servant);
			const int32 DropsBefore = FieldPlotTest::CountDrops(World);
			TWeakObjectPtr<AServant> Weak(Servant);

			UGameplayStatics::ApplyDamage(Servant, 1.0f, nullptr, nullptr, UDamageType::StaticClass());
			TickFor(TickStep);
			TestTrue(TEXT("a non-lethal hit leaves the servant alive and registered"), Weak.IsValid() && Subsystem->IsServantRegistered(Servant));
			TestTrue(TEXT("and it lowered the health"), ServantCombatTest::HealthOf(Servant)->GetHealthPercent() < 1.0f);

			EnemyTest::KillActor(Servant);
			TickFor(2.0 * TickStep);
			TestFalse(TEXT("the servant is destroyed"), Weak.IsValid());
			TestFalse(TEXT("and no longer registered"), Subsystem->IsServantRegistered(Weak.Get()));
			TestEqual(TEXT("no servant left in the registry"), Subsystem->GetServantCount(), 0);
			TestEqual(TEXT("no AResourceDrop was created"), FieldPlotTest::CountDrops(World), DropsBefore);

			AServant* Next = ServantCombatTest::SpawnServant(World, TEXT("NextServant"), FVector(0.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: next servant"), Next)) { return; }
			TestTrue(TEXT("the number of a dead servant is never reused"), Subsystem->GetServantNumber(Next) > DeadNumber);
		});

		// Not automated: "reservation of a field in progress released at the servant's death" (criterion 3, last part). A reservation is only
		// held while the servant WALKS to a field, which needs a NavMesh projection that test worlds do not have (a field in reach is
		// resolved at once, an out-of-reach one fails at the projection). See the report: it needs a seam on the field approach, or PIE.
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion 2: the enemy soldier takes the servant for a target.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Enemy targeting a servant (criterion 2)", [this]()
	{
		It("C2: a soldier in Engage whose only destructible is a servant targets it (reserved), and has no such target once the servant is dead", [this]()
		{
			// Mutation: AServant is not a destructible / the soldier filters it out (no target, only the logged error), the servant is not
			// seen as dead or destroyed (the soldier keeps it as target after its death), or the soldier never re-decides after the loss (no "no target" error, or more than once).
			if (!RequireWorld()) { return; }
			// The servant is the only candidate: once it is dead the soldier finds nothing and logs its single "no target" error.
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 1);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABaseZone* Zone = World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform(FVector(4000.0, 0.0, 0.0)), Params);
			if (!TestNotNull(TEXT("harness: zone"), Zone)) { return; }
			Zone->GetZoneBox()->SetWorldScale3D(FVector(4.0, 2.0, 1.0)); // X in [2000, 6000]
			AServant* Servant = ServantCombatTest::SpawnServant(World, TEXT("Servant"), FVector(2500.0, 0.0, 0.0));
			ATestSeamEnemySoldier* Soldier = ServantCombatTest::SpawnSeamSoldier(World, FVector::ZeroVector);
			AEnemyAIController* Controller = (Soldier != nullptr) ? Cast<AEnemyAIController>(Soldier->GetController()) : nullptr;
			const UEnemyTargetSubsystem* Claims = World->GetSubsystem<UEnemyTargetSubsystem>();
			if (!TestNotNull(TEXT("harness: servant"), Servant) || !TestNotNull(TEXT("harness: soldier controller"), Controller) || !TestNotNull(TEXT("harness: claims"), Claims)) { return; }
			TestTrue(TEXT("precondition: the servant is in the zone"), Zone->IsPointInZone(Servant->GetActorLocation()));

			const bool bChose = TickUntil([Controller]() { return Controller->GetCurrentTarget() != nullptr; }, 60.0);
			TestTrue(TEXT("the soldier chose a target"), bChose);
			TestTrue(TEXT("C2: the target is the servant"), Controller->GetCurrentTarget() == Servant);
			TestEqual(TEXT("C2: the servant is reserved by that soldier"), Claims->GetClaimCount(Servant), 1);

			EnemyTest::KillActor(Servant);
			// Destroying the servant nulls the soldier's weak target at once, so waiting for "no target" would end the test before the soldier
			// ticked. Run a fixed span of world time instead: the soldier must really notice the loss, re-decide and log its single error.
			// (The claim count is not asserted: the destroyed servant would be queried through a dangling pointer.)
			TickFor(1.0);
			TestNull(TEXT("C2: after the servant died the soldier has no target at all (it was the only destructible)"), Controller->GetCurrentTarget());
			// Further ticks must not log the error again (once per soldier): the expected count of 1 is exact.
			TickFor(0.5);
			TestNull(TEXT("C2: and still none after more ticks"), Controller->GetCurrentTarget());
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion 5: the decision.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Decision (criterion 5)", [this]()
	{
		It("C5: fights the enemy nearest to the SERVANT (not the first registered, not the nearest to the zone centre, not the farthest); LastAction Attack; one chase request; no hit out of range", [this]()
		{
			// Mutation: the nearest is taken from the zone centre / the first registered / the last (a wrong enemy), or the farthest; the
			// decision is not Attack; the chase request is not made (or made every step while a move is in progress: Num > 1); the servant
			// strikes out of range (a hit on any enemy).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(400.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			// Registration order: Centre first (distance 50 from the zone centre, 450 from the servant), Far, then Near (300 from the servant).
			ATestPassiveEnemySoldier* Centre = MakeEnemy(TEXT("EnemyCentre"), FVector(-50.0, 0.0, 100.0));
			ATestPassiveEnemySoldier* Far = MakeEnemy(TEXT("EnemyFar"), FVector(-300.0, 0.0, 100.0));
			ATestPassiveEnemySoldier* Near = MakeEnemy(TEXT("EnemyNear"), FVector(100.0, 0.0, 100.0));
			if (!TestTrue(TEXT("harness: enemies"), Centre && Far && Near)) { return; }
			TestTrue(TEXT("precondition: all in the zone"), Zone->IsPointInZone(Centre->GetActorLocation()) && Zone->IsPointInZone(Far->GetActorLocation()) && Zone->IsPointInZone(Near->GetActorLocation()));
			TestTrue(TEXT("precondition: Near is out of attack range"), 300.0 - ServantCombatTest::CapsuleRadius(F.Servant) - ServantCombatTest::CapsuleRadius(Near) > F.Range);
			TestFalse(TEXT("before the first decision: not fighting"), F.Ctl->IsFighting());
			TestNull(TEXT("before the first decision: no fight target"), F.Ctl->GetFightTarget());

			if (!StartFight(F)) { return; }
			TestTrue(TEXT("C5: the target is the enemy nearest to the servant"), F.Ctl->GetFightTarget() == Near);
			ServantFieldTest::ExpectAction(*this, TEXT("C5: the decision"), F.Ctl->GetLastAction(), EServantWanderAction::Attack);
			if (!TestEqual(TEXT("C5: one chase request"), F.Ctl->RequestedEnemies.Num(), 1)) { return; }
			TestTrue(TEXT("C5: towards that enemy"), F.Ctl->RequestedEnemies[0].Get() == Near);

			TickFor(2.5 * F.Interval); // the move is in progress (the seam never ends it) and nobody is in range
			TestTrue(TEXT("C5: still fighting the same enemy"), F.Ctl->IsFighting() && F.Ctl->GetFightTarget() == Near);
			TestEqual(TEXT("C5: still one chase request while the move is in progress"), F.Ctl->RequestedEnemies.Num(), 1);
			TestTrue(TEXT("C5: no hit out of range, on any enemy"), HitsOn(Centre) == 0 && HitsOn(Far) == 0 && HitsOn(Near) == 0);
		});

		It("C5: the nearest enemy is measured in 2D (an enemy far above is nearer than one that is closer in 3D)", [this]()
		{
			// Mutation: FVector::Dist instead of Dist2D in the choice (Lower, nearer in 3D, would be chosen).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(400.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			// From the servant: High is 200 u away in 2D but ~830 u in 3D; Lower is 300 u away in both.
			ATestPassiveEnemySoldier* Lower = MakeEnemy(TEXT("EnemyLower"), FVector(100.0, 0.0, 100.0));
			ATestPassiveEnemySoldier* High = MakeEnemy(TEXT("EnemyHigh"), FVector(200.0, 0.0, 900.0));
			if (!TestTrue(TEXT("harness: enemies"), Lower && High)) { return; }
			TestTrue(TEXT("precondition: both in the zone (height ignored)"), Zone->IsPointInZone(Lower->GetActorLocation()) && Zone->IsPointInZone(High->GetActorLocation()));
			TestTrue(TEXT("precondition: High is out of attack range in 2D"), 200.0 - ServantCombatTest::CapsuleRadius(F.Servant) - ServantCombatTest::CapsuleRadius(High) > F.Range);

			if (!StartFight(F)) { return; }
			TestTrue(TEXT("C5 2D: the 2D-nearest enemy is chosen; a 3D choice would take the other one"), F.Ctl->GetFightTarget() == High);
		});

		It("C5: an enemy whose origin is outside the zone is ignored, even when it is nearer than one inside", [this]()
		{
			// Mutation: the zone test is dropped (Out, nearer, would be chosen), or is done on the capsule overlap instead of the origin.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(400.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Out = MakeEnemy(TEXT("EnemyOut"), FVector(700.0, 0.0, 100.0)); // 300 u away, out of the zone
			ATestPassiveEnemySoldier* In = MakeEnemy(TEXT("EnemyIn"), FVector(-100.0, 0.0, 100.0));  // 500 u away, in the zone
			if (!TestTrue(TEXT("harness: enemies"), Out && In)) { return; }
			TestFalse(TEXT("precondition: Out is out of the zone"), Zone->IsPointInZone(Out->GetActorLocation()));
			TestTrue(TEXT("precondition: In is in the zone"), Zone->IsPointInZone(In->GetActorLocation()));

			if (!StartFight(F)) { return; }
			TestTrue(TEXT("C5: the enemy inside the zone is the target"), F.Ctl->GetFightTarget() == In);
		});

		It("C5: with only an enemy outside the zone the servant never fights it and goes on wandering", [this]()
		{
			// Mutation: the zone filter is dropped (the servant fights/chases the outside enemy).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(400.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Out = MakeEnemy(TEXT("EnemyOut"), FVector(520.0, 0.0, 100.0)); // in range of the servant's reach, but out of the zone
			if (Out == nullptr) { return; }
			TestFalse(TEXT("precondition: Out is out of the zone"), Zone->IsPointInZone(Out->GetActorLocation()));

			bool bEverFighting = false;
			TickWatching(InitialDecisionDelay + 2.0 * RetryDelay + TimerSlack, [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("never fighting"), bEverFighting);
			TestEqual(TEXT("no chase request"), F.Ctl->RequestedEnemies.Num(), 0);
			TestEqual(TEXT("no hit"), HitsOn(Out), 0);
			ServantFieldTest::ExpectAction(*this, TEXT("the decision is a plain Wander"), F.Ctl->GetLastAction(), EServantWanderAction::Wander);
		});

		It("C5: a dead enemy that is still in the world and registered is not a target (alive = !IsDead()); the living one is", [this]()
		{
			// Mutation: "alive" only means registered and valid (the nearer corpse is chosen), or the registry is not consulted for IsDead.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Corpse = MakeEnemy(TEXT("EnemyCorpse"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0)); // nearer
			ATestPassiveEnemySoldier* Alive = MakeEnemy(TEXT("EnemyAlive"), FVector(-CentreDistance(F.Range + 150.0), 0.0, 100.0));
			if (!TestTrue(TEXT("harness: enemies"), Corpse && Alive)) { return; }
			if (!TestTrue(TEXT("harness: death reaction detached"), ServantCombatTest::MakeDeathLinger(Corpse))) { return; }
			EnemyTest::KillActor(Corpse);
			TestTrue(TEXT("precondition: dead, yet still in the world"), Corpse->IsDead() && IsValid(Corpse));

			if (!StartFight(F)) { return; }
			TestTrue(TEXT("C5: the target is the living enemy"), F.Ctl->GetFightTarget() == Alive);
			TestTrue(TEXT("C5: the chase goes to the living enemy"), F.Ctl->RequestedEnemies.Num() == 1 && F.Ctl->RequestedEnemies[0].Get() == Alive);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion 6: strikes.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Strikes (criterion 6)", [this]()
	{
		It("C6: in range at the start, the first hit is immediate, then one per AttackInterval, and no chase request", [this]()
		{
			// Mutation: the first hit waits one AttackInterval (the "immediately" check fails), the period is not AttackInterval (the
			// 0.8 x interval check or the second/third hit checks fail), the damage is not AttackDamage (the hit count is wrong), a move
			// request is sent although the enemy is in range.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: the enemy is in the zone"), Zone->IsPointInZone(Enemy->GetActorLocation()));

			if (!StartFight(F)) { return; }
			TestEqual(TEXT("C6: the first hit lands at the very decision (hits, in units of AttackDamage)"), HitsOn(Enemy), 1);
			TestTrue(TEXT("C6: fighting that enemy"), F.Ctl->GetFightTarget() == Enemy);
			TestEqual(TEXT("C6: no chase request when already in range"), F.Ctl->RequestedEnemies.Num(), 0);

			TickFor(0.8 * F.Interval);
			TestEqual(TEXT("C6: no second hit before AttackInterval"), HitsOn(Enemy), 1);
			TickFor(0.2 * F.Interval + TimerSlack);
			TestEqual(TEXT("C6: the second hit after one AttackInterval"), HitsOn(Enemy), 2);
			TickFor(F.Interval);
			TestEqual(TEXT("C6: the third hit one AttackInterval later (and not a fourth)"), HitsOn(Enemy), 3);
		});

		It("C6: the range is measured in 2D: an enemy far above the servant but in range on the ground is struck", [this]()
		{
			// Mutation: the range test is 3D (the 800 u of height put the enemy out of range: no hit, a chase request instead).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 900.0));
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: the enemy is in the zone (height ignored)"), Zone->IsPointInZone(Enemy->GetActorLocation()));

			if (!StartFight(F)) { return; }
			TestEqual(TEXT("C6 2D: struck at once"), HitsOn(Enemy), 1);
			TestEqual(TEXT("C6 2D: no chase request"), F.Ctl->RequestedEnemies.Num(), 0);
		});

		It("C6: out of range, no hit and one chase request; once the enemy is in range the servant strikes within one AttackInterval, without asking again", [this]()
		{
			// Mutation: the servant strikes without the range test (a hit during the out-of-range watch), never re-evaluates the range
			// after the chase (no hit once the enemy is in range), or keeps sending requests while a move is in progress / once in range.
			// Not observable here: that StopMovement is called when the range is reached (the seam replaces the path following).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(F.Range + 150.0), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: the enemy is in the zone"), Zone->IsPointInZone(Enemy->GetActorLocation()));

			if (!StartFight(F)) { return; }
			TestEqual(TEXT("C6: no hit at the first step, out of range"), HitsOn(Enemy), 0);
			TestEqual(TEXT("C6: one chase request"), F.Ctl->RequestedEnemies.Num(), 1);
			TickFor(2.5 * F.Interval);
			TestEqual(TEXT("C6: still no hit over several steps out of range"), HitsOn(Enemy), 0);
			TestEqual(TEXT("C6: still one request (the move is in progress)"), F.Ctl->RequestedEnemies.Num(), 1);

			// The real chase would have brought the servant next to the enemy: the seam never moves, so bring the enemy in range instead.
			Enemy->SetActorLocation(FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			const bool bStruck = TickUntil([this, Enemy]() { return HitsOn(Enemy) >= 1; }, F.Interval + TimerSlack);
			TestTrue(TEXT("C6: the servant strikes once the enemy is in range"), bStruck);
			TestEqual(TEXT("C6: no further request once in range"), F.Ctl->RequestedEnemies.Num(), 1);
			TestTrue(TEXT("C6: still fighting"), F.Ctl->IsFighting() && F.Ctl->GetFightTarget() == Enemy);
		});

		It("C6: two servants may fight the same enemy (no reservation): both strike it", [this]()
		{
			// Mutation: an enemy is reserved by the first servant (the second one finds no target and does not fight), or the second
			// servant chooses another enemy while the first is taken.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter A;
			FFighter B;
			if (Zone == nullptr || !MakeFighter(A, TEXT("ServantA"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			if (!MakeFighter(B, TEXT("ServantB"), FVector(0.0, -80.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * A.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: B is in range of the enemy too"),
				FVector::Dist2D(B.Servant->GetActorLocation(), Enemy->GetActorLocation()) - ServantCombatTest::CapsuleRadius(B.Servant) - ServantCombatTest::CapsuleRadius(Enemy) <= B.Range);

			const bool bBoth = TickUntil([&]() { return A.Ctl->IsFighting() && B.Ctl->IsFighting(); }, InitialDecisionDelay + TimerSlack);
			TestTrue(TEXT("C6: both servants fight"), bBoth);
			TestTrue(TEXT("C6: the same enemy"), A.Ctl->GetFightTarget() == Enemy && B.Ctl->GetFightTarget() == Enemy);
			TickFor(TickStep);
			TestEqual(TEXT("C6: each servant struck once: two hits"), HitsOn(Enemy), 2);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion 7: enemy killed.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Enemy killed (criterion 7)", [this]()
	{
		// How the sole enemy leaves: killed through the damage path (HandleDeath), dead but lingering (IsDead true, still valid),
		// or destroyed alive (weak pointer invalid). The three are different checks of the fight step.
		enum class EEnd : uint8 { Killed, DeadLingering, Destroyed };
		auto RunSoleEnemyCase = [this](EEnd End)
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			if (End == EEnd::DeadLingering && !TestTrue(TEXT("harness: death reaction detached"), ServantCombatTest::MakeDeathLinger(Enemy))) { return; }
			if (!StartFight(F)) { return; }
			TestEqual(TEXT("precondition: the fight is on, one hit"), HitsOn(Enemy), 1);
			TestTrue(TEXT("C7: fighting speed during the fight"), FMath::IsNearlyEqual(F.Servant->GetCharacterMovement()->MaxWalkSpeed, static_cast<float>(CombatWalkSpeed), 0.5f));

			if (End == EEnd::Destroyed) { Enemy->Destroy(); } else { EnemyTest::KillActor(Enemy); }
			const bool bEnded = TickUntil([&]() { return !F.Ctl->IsFighting(); }, F.Interval + TimerSlack);
			TestTrue(TEXT("C7: IsFighting becomes false within one step"), bEnded);
			TestNull(TEXT("C7: no fight target any more"), F.Ctl->GetFightTarget());
			TestTrue(TEXT("C7: the wander speed is restored"), FMath::IsNearlyEqual(F.Servant->GetCharacterMovement()->MaxWalkSpeed, static_cast<float>(WanderWalkSpeed), 0.5f));

			// The next decision comes after CombatNextDelay, and with no enemy left it is a Wander.
			const bool bWander = TickUntil([&]() { return F.Ctl->GetLastAction() == EServantWanderAction::Wander; }, CombatNextDelay + 2.0 * TimerSlack);
			TestTrue(TEXT("C7: with no enemy left the next decision is Wander"), bWander);
			TestTrue(TEXT("C7: wandering again"), F.Ctl->IsWandering());
			TestFalse(TEXT("C7: and not fighting"), F.Ctl->IsFighting());
			// No lasting chase either: nothing left to chase.
			TestEqual(TEXT("C7: no chase request was made"), F.Ctl->RequestedEnemies.Num(), 0);
		};

		It("C7: the only enemy is killed: IsFighting false, wander speed back, then Wander", [this, RunSoleEnemyCase]()
		{
			// Mutation: the death is not noticed (IsFighting stays true, the servant strikes a corpse or loops), the next decision is never
			// scheduled (no Wander: EndCombat forgets ScheduleDecision), or CombatWalkSpeed is not restored to WanderWalkSpeed.
			RunSoleEnemyCase(EEnd::Killed);
		});

		It("C7: the only enemy is dead but still in the world: the fight ends all the same (IsDead is checked, not only validity)", [this, RunSoleEnemyCase]()
		{
			// Mutation: the step only tests the weak pointer (a corpse that is not destroyed is struck forever and IsFighting never ends).
			RunSoleEnemyCase(EEnd::DeadLingering);
		});

		It("C7: the only enemy is destroyed (not killed): the fight ends too (the weak pointer is checked)", [this, RunSoleEnemyCase]()
		{
			// Mutation: the step dereferences/keeps the destroyed enemy (no end), or only tests IsDead.
			RunSoleEnemyCase(EEnd::Destroyed);
		});

		It("C7: another enemy remains in the zone: after the kill the servant takes it", [this]()
		{
			// Mutation: no new decision after a kill (the servant never takes the second enemy), or the decision picks a dead/dying one
			// (not B), or the fight target keeps pointing at the dead one.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* First = MakeEnemy(TEXT("EnemyFirst"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			ATestPassiveEnemySoldier* Second = MakeEnemy(TEXT("EnemySecond"), FVector(-CentreDistance(F.Range + 150.0), 0.0, 100.0));
			if (!TestTrue(TEXT("harness: enemies"), First && Second)) { return; }
			if (!StartFight(F)) { return; }
			if (!TestTrue(TEXT("precondition: the nearest enemy is fought first"), F.Ctl->GetFightTarget() == First)) { return; }

			EnemyTest::KillActor(First);
			const bool bNext = TickUntil([&]() { return F.Ctl->IsFighting() && F.Ctl->GetFightTarget() == Second; }, F.Interval + CombatNextDelay + 2.0 * TimerSlack);
			TestTrue(TEXT("C7: the servant takes the remaining enemy"), bNext);
			ServantFieldTest::ExpectAction(*this, TEXT("C7: the decision"), F.Ctl->GetLastAction(), EServantWanderAction::Attack);
			TestTrue(TEXT("C7: it chases it (one new request, towards it)"), F.Ctl->RequestedEnemies.Num() == 1 && F.Ctl->RequestedEnemies[0].Get() == Second);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion 8: who fights.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Roles (criterion 8)", [this]()
	{
		It("C8: an Unassigned servant never fights the enemy in its reach", [this]()
		{
			// Mutation: the role is not tested (every servant fights), or Unassigned is treated as AttackDefense.
			ExpectNeverFights(EServantTask::Unassigned, false);
		});

		It("C8 (partial): a hurt FieldHarvest servant does not retaliate and stays alive, assigned; IsHarvesting is NOT covered here", [this]()
		{
			// Mutation: the role is not tested, or a hit makes the servant fight back / changes its task.
			// NOT covered: the second half of criterion 8 ("IsHarvesting is kept when a harvesting FieldHarvest servant is hit"). A servant
			// is only in harvest while it WALKS to a field, which needs a NavMesh projection that test worlds do not have. That part is
			// checked by hand in PIE (see TODO.md, technical debt "tests non automatises"). This test only checks: no fight, no
			// retaliation, task kept, servant alive.
			ExpectNeverFights(EServantTask::FieldHarvest, true);
		});

		It("C8: an AttackDefense servant outside the zone (Return) does not fight an enemy of the zone", [this]()
		{
			// Mutation: Attack replaces a Return decision (the servant fights from outside the zone).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(5000.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(0.0, 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			TestFalse(TEXT("precondition: the servant is out of the zone"), Zone->IsPointInZone(F.Servant->GetActorLocation()));

			bool bEverFighting = false;
			// Two Return decisions (0.5 s and 0.5 + RetryDelay): before the failed returns end in a teleport into the zone.
			TickWatching(InitialDecisionDelay + 1.5 * RetryDelay, [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("never fighting while out of the zone"), bEverFighting);
			TestEqual(TEXT("no chase request"), F.Ctl->RequestedEnemies.Num(), 0);
			TestEqual(TEXT("no hit"), HitsOn(Enemy), 0);
			ServantFieldTest::ExpectAction(*this, TEXT("the decision is still Return"), F.Ctl->GetLastAction(), EServantWanderAction::Return);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion 9: end of the fight by zone exit or unassignment.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("End of the fight (criterion 9)", [this]()
	{
		It("C9: an enemy that leaves the zone, though still in reach, ends the fight and takes no more hits", [this]()
		{
			// Mutation: the zone is only tested at the choice, not at each step (the servant keeps striking an enemy out of the zone).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr) { return; }
			// The enemy starts 10 u inside the zone edge (X = 490) and steps 20 u across it: it stays within the servant's reach.
			const double InsideX = 490.0;
			const double ServantX = InsideX - CentreDistance(0.5 * 100.0);
			if (!MakeFighter(F, TEXT("Servant"), FVector(ServantX, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(InsideX, 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: the enemy starts in the zone"), Zone->IsPointInZone(Enemy->GetActorLocation()));
			TestTrue(TEXT("precondition: the exit point is out of the zone"), !Zone->IsPointInZone(FVector(InsideX + 20.0, 0.0, 100.0)));
			TestTrue(TEXT("precondition: the exit point is still in the servant's reach"),
				InsideX + 20.0 - ServantX - ServantCombatTest::CapsuleRadius(F.Servant) - ServantCombatTest::CapsuleRadius(Enemy) <= F.Range);

			if (!StartFight(F)) { return; }
			TestEqual(TEXT("precondition: one hit while inside"), HitsOn(Enemy), 1);

			Enemy->SetActorLocation(FVector(InsideX + 20.0, 0.0, 100.0));
			const bool bEnded = TickUntil([&]() { return !F.Ctl->IsFighting(); }, F.Interval + TimerSlack);
			TestTrue(TEXT("C9: IsFighting becomes false within one step"), bEnded);
			TickFor(3.0 * F.Interval);
			TestEqual(TEXT("C9: no hit after the exit"), HitsOn(Enemy), 1);
			TestFalse(TEXT("C9: the out-of-zone enemy is not taken again"), F.Ctl->IsFighting());
		});

		It("C9: unassigning a fighting servant ends the fight at once and no more hits follow", [this]()
		{
			// Mutation: OnServantAssignmentsChanged does not end the fight (the servant keeps striking, IsFighting stays true), or
			// ends it only at the next step / only when another task is set.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
			if (Enemy == nullptr || !TestNotNull(TEXT("harness: subsystem"), Subsystem)) { return; }
			if (!StartFight(F)) { return; }
			TestEqual(TEXT("precondition: one hit"), HitsOn(Enemy), 1);

			TestTrue(TEXT("harness: the unassignment changed the task"), Subsystem->AssignTask(F.Servant, EServantTask::Unassigned));
			TestFalse(TEXT("C9: IsFighting is false right after the unassignment"), F.Ctl->IsFighting());
			TestNull(TEXT("C9: no fight target"), F.Ctl->GetFightTarget());
			bool bEverFighting = false;
			TickWatching(3.0 * F.Interval, [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("C9: the servant does not start another fight (it is Unassigned)"), bEverFighting);
			TestEqual(TEXT("C9: no hit after the unassignment"), HitsOn(Enemy), 1);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion "Reveil par detection": the enemy scan wakes a wandering AttackDefense servant (PIE feedback of Timeo).
	// No NavMesh: the wander decision fails at once and the servant PAUSES (RetryDelay, 2 s) with LastAction Wander; the scan period (0.5 s)
	// is far below that pause, so a fight that starts within EnemyScanInterval + slack of the enemy's arrival can only come from the scan.
	// The WALK is reproduced with a real request in progress (ServantCombatTest::BeginInjectedWalk), see the walk test.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Enemy scan wake-up (criterion 7a)", [this]()
	{
		It("Scan, pause: an enemy entering the zone wakes a paused AttackDefense servant within EnemyScanInterval; it takes the enemy nearest to the SERVANT, asks once, and the pending decision does not start a second fight", [this]()
		{
			// Mutation: no scan (the fight only starts at the pending decision, after RetryDelay: TickUntil times out); the scan picks the first
			// registered / the nearest to the zone centre / the farthest (wrong target); the scan ignores the zone; after the wake the pending
			// decision timer is not neutralised and decides again (a second RequestMoveToEnemy: Num 2, or a changed target).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeWanderingFighter(F, FVector(400.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			TestFalse(TEXT("precondition: not fighting while no enemy exists"), F.Ctl->IsFighting());
			// Registration order: Centre first (50 from the zone centre, 450 from the servant), Far, then Near (300 from the servant).
			ATestPassiveEnemySoldier* Centre = MakeEnemy(TEXT("EnemyCentre"), FVector(-50.0, 0.0, 100.0));
			ATestPassiveEnemySoldier* Far = MakeEnemy(TEXT("EnemyFar"), FVector(-300.0, 0.0, 100.0));
			ATestPassiveEnemySoldier* Near = MakeEnemy(TEXT("EnemyNear"), FVector(100.0, 0.0, 100.0));
			if (!TestTrue(TEXT("harness: enemies"), Centre && Far && Near)) { return; }
			TestTrue(TEXT("precondition: all in the zone"), Zone->IsPointInZone(Centre->GetActorLocation()) && Zone->IsPointInZone(Far->GetActorLocation()) && Zone->IsPointInZone(Near->GetActorLocation()));
			TestTrue(TEXT("precondition: Near is out of attack range (a chase request is expected, no hit)"), 300.0 - ServantCombatTest::CapsuleRadius(F.Servant) - ServantCombatTest::CapsuleRadius(Near) > F.Range);

			const bool bWoken = TickUntil([&]() { return F.Ctl->IsFighting(); }, WakeWindow());
			if (!TestTrue(TEXT("Scan: IsFighting within EnemyScanInterval + margin of the enemy entering the zone"), bWoken)) { return; }
			TestTrue(TEXT("Scan: the target is the enemy nearest to the servant"), F.Ctl->GetFightTarget() == Near);
			TestTrue(TEXT("Scan: one chase request, towards that enemy"), F.Ctl->RequestedEnemies.Num() == 1 && F.Ctl->RequestedEnemies[0].Get() == Near);

			// Past the pending decision of the pause (RetryDelay) and a few scans: nothing may decide a second time.
			TickFor(RetryDelay + CombatNextDelay);
			TestTrue(TEXT("Scan: still the same single fight"), F.Ctl->IsFighting() && F.Ctl->GetFightTarget() == Near);
			TestEqual(TEXT("Scan: still one chase request (no double decision, no re-scan during the fight)"), F.Ctl->RequestedEnemies.Num(), 1);
			TestTrue(TEXT("Scan: no hit out of range, on any enemy"), HitsOn(Centre) == 0 && HitsOn(Far) == 0 && HitsOn(Near) == 0);
		});

		It("Scan, walk: an enemy entering the zone CANCELS the wander walk in progress (the servant does not go on to its point) and starts the fight", [this]()
		{
			// The PIE bug: the servant kept walking to its wander point and only then attacked. A real request is in progress here (hand-made
			// path, status Moving); it can only end if the production code cancels it, since the pawn never moves.
			// Mutation: the scan starts the fight but does not stop the movement (StopMovement missing): GetMoveStatus stays Moving and no
			// request ends (Count 0); or the scan only fires once the walk is over (no fight within the window).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeWanderingFighter(F, FVector(400.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ServantWanderTest::FMoveRequestCounter Walk(F.Ctl);
			if (!TestTrue(TEXT("harness: path following component bound"), Walk.IsBound())) { return; }
			if (!TestTrue(TEXT("harness: a wander walk is in progress (status Moving)"), ServantCombatTest::BeginInjectedWalk(F.Ctl, F.Servant->GetActorLocation(), FVector(-300.0, 0.0, 100.0)))) { return; }
			TestEqual(TEXT("precondition: the walk has not ended"), Walk.Count, 0);
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(100.0, 0.0, 100.0)); // 300 u away: out of range
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: the enemy is in the zone"), Zone->IsPointInZone(Enemy->GetActorLocation()));

			const bool bWoken = TickUntil([&]() { return F.Ctl->IsFighting(); }, WakeWindow());
			if (!TestTrue(TEXT("Scan: IsFighting within EnemyScanInterval + margin, although a walk was in progress"), bWoken)) { return; }
			TestTrue(TEXT("Scan: the target is the enemy"), F.Ctl->GetFightTarget() == Enemy);
			TestEqual(TEXT("Scan: the wander walk ended exactly once"), Walk.Count, 1);
			TestTrue(TEXT("Scan: and it was CANCELLED (Aborted), not completed"), Walk.Codes.Num() == 1 && Walk.Codes[0] == EPathFollowingResult::Aborted);
			TestTrue(TEXT("Scan: no wander move is left in progress"), F.Ctl->GetMoveStatus() != EPathFollowingStatus::Moving);
			TickFor(RetryDelay);
			TestEqual(TEXT("Scan: the cancelled walk never completes or restarts later"), Walk.Count, 1);
			TestTrue(TEXT("Scan: still fighting the same enemy"), F.Ctl->IsFighting() && F.Ctl->GetFightTarget() == Enemy);
			TestEqual(TEXT("Scan: still one chase request"), F.Ctl->RequestedEnemies.Num(), 1);
		});

		It("Scan: only enemies whose origin is in the zone count, tested at the scan (spawned inside then gone out before it: no wake); once inside, it wakes", [this]()
		{
			// Mutation: the scan does not filter by zone (the outside enemy wakes the servant), or the zone test is cached at the enemy's
			// registration (the enemy spawned inside, then moved out, still wakes it), or an enemy entering the zone later is never seen.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeWanderingFighter(F, FVector(450.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			const double Offset = CentreDistance(0.5 * F.Range);
			const FVector InsidePoint(450.0 - Offset, 0.0, 100.0);
			const FVector OutsidePoint(450.0 + Offset, 0.0, 100.0);
			TestTrue(TEXT("precondition: the inside point is in the zone, the outside point is not"), Zone->IsPointInZone(InsidePoint) && !Zone->IsPointInZone(OutsidePoint));
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), InsidePoint); // registered with an in-zone origin...
			if (Enemy == nullptr) { return; }
			Enemy->SetActorLocation(OutsidePoint); // ...and gone out before any scan could run (no tick in between)

			bool bEverFighting = false;
			TickWatching(PreWatch, [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("Scan: an enemy outside the zone never wakes the servant"), bEverFighting);
			TestEqual(TEXT("Scan: no hit, no chase request"), HitsOn(Enemy) + F.Ctl->RequestedEnemies.Num(), 0);

			Enemy->SetActorLocation(InsidePoint);
			const bool bWoken = TickUntil([&]() { return F.Ctl->IsFighting(); }, WakeWindow());
			TestTrue(TEXT("Scan: once the enemy is in the zone the servant is woken within EnemyScanInterval + margin"), bWoken);
			TestTrue(TEXT("Scan: and fights it"), F.Ctl->GetFightTarget() == Enemy);
			TestEqual(TEXT("Scan: it is in range, so the first hit lands at once"), HitsOn(Enemy), 1);
		});

		It("Scan: an Unassigned servant is never woken by an enemy entering the zone", [this]()
		{
			// Mutation: the scan does not look at the task (any wandering servant fights).
			ExpectWanderNotWoken(EServantTask::Unassigned);
		});

		It("Scan: a FieldHarvest servant is never woken by an enemy entering the zone", [this]()
		{
			// Mutation: the scan only tests "assigned" instead of AttackDefense (a harvester fights).
			ExpectWanderNotWoken(EServantTask::FieldHarvest);
		});

		It("Scan: an AttackDefense servant in Return (outside the zone) is not woken by an enemy of the zone", [this]()
		{
			// Mutation: the scan runs during a Return (the servant fights from outside the zone, or cancels its return).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(5000.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			if (!ReadScanSettings(F, true)) { return; }
			TestFalse(TEXT("precondition: the servant is outside the zone"), Zone->IsPointInZone(F.Servant->GetActorLocation()));
			const AServantAIController* Ctl = F.Ctl;
			if (!TestTrue(TEXT("precondition: the first decision is a Return"), TickUntil([Ctl]() { return Ctl->GetLastAction() == EServantWanderAction::Return; }, InitialDecisionDelay + TimerSlack))) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(0.0, 0.0, 100.0));
			if (Enemy == nullptr) { return; }

			bool bEverFighting = false;
			TickWatching(PreWatch + WakeWindow(), [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("Scan: never fighting while returning"), bEverFighting);
			TestEqual(TEXT("Scan: no hit, no chase request"), HitsOn(Enemy) + F.Ctl->RequestedEnemies.Num(), 0);
			ServantFieldTest::ExpectAction(*this, TEXT("Scan: the decision is still Return"), F.Ctl->GetLastAction(), EServantWanderAction::Return);
		});

		It("Scan: a servant assigned to AttackDefense DURING its wander is woken by the enemy already in the zone", [this]()
		{
			// Spec "Cas limites": an assignment during the wander must also reach the scan. Not asserted: whether the fight starts at the
			// assignment itself or at the next scan (both are within the window).
			// Mutation: the scan is only armed at the start of the wander (an assignment made afterwards never arms it: no fight within the window).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
			if (Zone == nullptr || !TestNotNull(TEXT("harness: subsystem"), Subsystem)) { return; }
			if (!MakeWanderingFighter(F, FVector(0.0, 0.0, 100.0), EServantTask::Unassigned)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			bool bEverFighting = false;
			TickWatching(PreWatch, [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			if (!TestFalse(TEXT("precondition: an Unassigned servant ignores the enemy"), bEverFighting)) { return; }

			TestTrue(TEXT("harness: assignment changed the task"), Subsystem->AssignTask(F.Servant, EServantTask::AttackDefense));
			const bool bWoken = TickUntil([&]() { return F.Ctl->IsFighting(); }, WakeWindow());
			TestTrue(TEXT("Scan: IsFighting within EnemyScanInterval + margin of the assignment"), bWoken);
			TestTrue(TEXT("Scan: the target is the enemy"), F.Ctl->GetFightTarget() == Enemy);
			TestEqual(TEXT("Scan: in range, one hit at the start of the fight"), HitsOn(Enemy), 1);
		});

		It("Scan: unassigning a wandering AttackDefense servant stops the scan (an enemy entering the zone afterwards does not wake it)", [this]()
		{
			// Mutation: the scan timer is not cut at the unassignment (it keeps waking a servant that no longer has the role).
			// Positive control at the end: the same enemy, still in reach, wakes the servant once it is assigned AttackDefense again. Without
			// it this test would stay green if the enemy were invisible to the servant for any other reason (zone, registration, range).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
			if (Zone == nullptr || !TestNotNull(TEXT("harness: subsystem"), Subsystem)) { return; }
			if (!MakeWanderingFighter(F, FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			TestTrue(TEXT("harness: unassignment changed the task"), Subsystem->AssignTask(F.Servant, EServantTask::Unassigned));
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }

			bool bEverFighting = false;
			TickWatching(PreWatch + WakeWindow(), [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("Scan: never fighting after the unassignment"), bEverFighting);
			TestEqual(TEXT("Scan: no hit, no chase request"), HitsOn(Enemy) + F.Ctl->RequestedEnemies.Num(), 0);

			// Positive control: the enemy is visible to a servant that has the role.
			TestTrue(TEXT("harness: reassignment changed the task"), Subsystem->AssignTask(F.Servant, EServantTask::AttackDefense));
			const bool bWoken = TickUntil([&]() { return F.Ctl->IsFighting(); }, WakeWindow());
			TestTrue(TEXT("Control: once assigned AttackDefense again, the same enemy wakes the servant within the window"), bWoken);
			TestTrue(TEXT("Control: and it is the target"), F.Ctl->GetFightTarget() == Enemy);
		});

		It("Scan: a controller that lost its pawn (UnPossess) while wandering is fully stopped: not wandering, never fights, no request, no crash", [this]()
		{
			// HONEST SCOPE: the scan timer handle is a private, non-reflected FTimerHandle, so "the timer was cut" is not observable, and a
			// scan that survived UnPossess would be stopped by the null-pawn guard anyway: this test does NOT fail on a surviving scan timer alone.
			// What it proves, from the outside: StopWandering ran (IsWandering cleared; no decision fires later although a pause of
			// RetryDelay was pending), and a pawnless controller facing an eligible enemy neither fights nor requests a chase nor crashes.
			// Mutation: OnUnPossess no longer calls StopWandering (IsWandering stays true; the pending decision or the scan then acts on
			// the pawnless controller: a chase request, a fight, or a null dereference).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeWanderingFighter(F, FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			F.Ctl->UnPossess();
			TestTrue(TEXT("precondition: the controller has no pawn"), F.Ctl->GetPawn() == nullptr);
			TestFalse(TEXT("StopWandering ran: the controller is no longer wandering"), F.Ctl->IsWandering());
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }

			bool bEverFighting = false;
			// Long enough for the pending decision of the pause (RetryDelay) to have fired if StopWandering had not cancelled it.
			TickWatching(RetryDelay + TimerSlack, [&]() { bEverFighting |= F.Ctl->IsFighting() || F.Ctl->IsWandering(); });
			TestFalse(TEXT("Scan: a pawnless controller never fights nor resumes wandering"), bEverFighting);
			TestEqual(TEXT("Scan: no hit, no chase request"), HitsOn(Enemy) + F.Ctl->RequestedEnemies.Num(), 0);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion "Orientation": at every strike the servant turns (yaw only, instantly) towards its target (PIE feedback of Timeo).
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Facing the target (criterion 7b)", [this]()
	{
		It("Facing: in range at the start (no walk), the yaw points at the enemy at EVERY strike, even when the enemy moves; pitch and roll are untouched", [this]()
		{
			// The servant starts with a non-zero pitch and roll and a yaw far from every direction used, and the enemy stands well above it,
			// then goes to two other bearings (still in range) between strikes.
			// Mutation: no alignment when the servant did not walk (yaw stays 170 deg: error > 100 deg at the first strike); alignment only on the
			// first strike (the second and third errors are > 100 deg); sign or axis error (wrong bearing); the alignment uses a full look-at
			// (the 400 u of height would pitch the servant: pitch deviation) or resets roll/pitch to zero.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			constexpr double StartPitch = 7.0;
			constexpr double StartRoll = 4.0;
			F.Servant->SetActorRotation(FRotator(StartPitch, 170.0, StartRoll));
			const FRotator Start = F.Servant->GetActorRotation();
			if (!TestTrue(TEXT("harness: the servant kept the pitch and roll that were set"), FMath::IsNearlyEqual(Start.Pitch, StartPitch, 0.01) && FMath::IsNearlyEqual(Start.Roll, StartRoll, 0.01))) { return; }

			const double Distance = CentreDistance(0.5 * F.Range);
			auto PlaceAt = [Distance](double BearingDegrees)
			{
				const double Radians = FMath::DegreesToRadians(BearingDegrees);
				return FVector(Distance * FMath::Cos(Radians), Distance * FMath::Sin(Radians), 500.0);
			};
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), PlaceAt(35.0));
			if (Enemy == nullptr) { return; }
			TestTrue(TEXT("precondition: the enemy is in the zone, the initial yaw is far from its bearing"),
				Zone->IsPointInZone(Enemy->GetActorLocation()) && ServantCombatTest::YawErrorTowards(F.Servant, Enemy) > 30.0);

			TArray<double> YawErrors;
			TArray<double> AxisDeviations; // largest pitch / roll deviation from the start, per strike
			auto Sample = [&]()
			{
				const FRotator Now = F.Servant->GetActorRotation();
				YawErrors.Add(ServantCombatTest::YawErrorTowards(F.Servant, Enemy));
				AxisDeviations.Add(FMath::Max(FMath::Abs(FRotator::NormalizeAxis(Now.Pitch - StartPitch)), FMath::Abs(FRotator::NormalizeAxis(Now.Roll - StartRoll))));
			};

			if (!StartFight(F)) { return; }
			if (!TestEqual(TEXT("precondition: first strike landed, no walk"), HitsOn(Enemy), 1)) { return; }
			Sample();
			Enemy->SetActorLocation(PlaceAt(-140.0));
			TestTrue(TEXT("second strike lands"), TickUntil([&]() { return HitsOn(Enemy) >= 2; }, F.Interval + TimerSlack));
			Sample();
			Enemy->SetActorLocation(PlaceAt(90.0));
			TestTrue(TEXT("third strike lands"), TickUntil([&]() { return HitsOn(Enemy) >= 3; }, F.Interval + TimerSlack));
			Sample();

			if (!TestEqual(TEXT("three strikes sampled"), YawErrors.Num(), 3)) { return; }
			for (int32 Index = 0; Index < YawErrors.Num(); ++Index)
			{
				TestTrue(FString::Printf(TEXT("Facing: strike %d, yaw within 1 degree of the enemy bearing (error %.2f deg)"), Index + 1, YawErrors[Index]), YawErrors[Index] < 1.0);
				TestTrue(FString::Printf(TEXT("Facing: strike %d, pitch and roll unchanged (deviation %.3f deg)"), Index + 1, AxisDeviations[Index]), AxisDeviations[Index] < 0.01);
			}
			TestEqual(TEXT("Facing: the servant never asked to walk (all strikes were in range)"), F.Ctl->RequestedEnemies.Num(), 0);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Criterion "Poursuite en echec": an enemy whose chase failed is ignored for FailedEnemyCooldown (spec "Ennemi inatteignable").
	// The chase fails synchronously (seam bFailSynchronously), so a new fight on the enemy is observable as a NEW chase request
	// (IsFighting is already false again when the request returns). After the failure the servant wanders (pause of RetryDelay).
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Unreachable enemy (criterion 7c)", [this]()
	{
		It("Failed chase: the lone enemy is not fought again before FailedEnemyCooldown, neither by the scan (pause or walk) nor by the decision; afterwards it is taken again", [this]()
		{
			// Mutation: the failure is not remembered (the scan wakes the servant within EnemyScanInterval: a 2nd request, or the injected
			// walk is aborted), only the scan honours the memory (the decision at FailTime + 2 x RetryDelay requests again before the
			// cooldown), only the decision honours it (the scan aborts the injected walk), the enemy is excluded for good (no new request
			// after the cooldown: TickUntil times out), or the delay is never reached (same). A cooldown much shorter than 5 s would also fail the
			// "no request before the cooldown" check.
			if (!RequireWorld()) { return; }
			FFighter F;
			ATestPassiveEnemySoldier* Enemy = nullptr;
			if (!MakeFailedChase(F, Enemy)) { return; }

			// Walk phase: a real wander walk is in progress and the scan runs; it must neither abort it nor start a fight. Shorter than
			// RetryDelay (MakeFailedChase leaves us at the start of the pause), so the pending decision cannot interfere.
			ServantWanderTest::FMoveRequestCounter Walk(F.Ctl);
			if (!TestTrue(TEXT("harness: path following component bound"), Walk.IsBound())) { return; }
			if (!TestTrue(TEXT("harness: a wander walk is in progress"), ServantCombatTest::BeginInjectedWalk(F.Ctl, F.Servant->GetActorLocation(), FVector(-300.0, 0.0, 100.0)))) { return; }
			TickFor(WakeWindow());
			TestEqual(TEXT("Cooldown: no new chase request during a wander walk (the scan ignores the failed enemy)"), F.Ctl->RequestedEnemies.Num(), 1);
			TestEqual(TEXT("Cooldown: the wander walk was not cancelled by the scan"), Walk.Count, 0);
			TestTrue(TEXT("Cooldown: the wander walk is still in progress"), F.Ctl->GetMoveStatus() == EPathFollowingStatus::Moving);

			// Until the end of the cooldown (margin of 3 ticks: FailTime is read up to one tick after the real failure): the decisions too.
			TickFor(FMath::Max(0.0, FailTime + FailedEnemyCooldown - 3.0 * TickStep - World->GetTimeSeconds()));
			TestEqual(TEXT("Cooldown: no chase request at all before FailedEnemyCooldown has elapsed"), F.Ctl->RequestedEnemies.Num(), 1);
			TestFalse(TEXT("Cooldown: not fighting"), F.Ctl->IsFighting());

			// After the delay the enemy is eligible again: a new request, towards it (scan or next decision, at most RetryDelay later).
			const double Remaining = FMath::Max(0.0, FailTime + FailedEnemyCooldown - World->GetTimeSeconds());
			const bool bRetaken = TickUntil([&]() { return F.Ctl->RequestedEnemies.Num() >= 2; }, Remaining + RetryDelay + CombatNextDelay + 2.0 * TimerSlack);
			TestTrue(TEXT("After the cooldown: the enemy is taken again (a new chase request)"), bRetaken);
			TestTrue(TEXT("After the cooldown: the new request is towards the same enemy"), F.Ctl->RequestedEnemies.Num() >= 2 && F.Ctl->RequestedEnemies[1].Get() == Enemy);
			TestTrue(TEXT("After the cooldown: not before FailedEnemyCooldown (world time)"), World->GetTimeSeconds() >= FailTime + FailedEnemyCooldown - 4.0 * TickStep);
		});

		It("Failed chase: a SECOND eligible enemy is taken at once (within EnemyScanInterval), even farther than the failed one, which is not retried instead", [this]()
		{
			// The memory is per enemy, not a global "stop chasing".
			// Mutation: the memory is ignored (the nearest, the failed one, is requested again: Requests[1] is the first enemy), or it blocks
			// every enemy / the whole servant (the second enemy never gets a request within the window).
			if (!RequireWorld()) { return; }
			FFighter F;
			ATestPassiveEnemySoldier* First = nullptr;
			if (!MakeFailedChase(F, First)) { return; }

			// Farther from the servant than the failed one (opposite side), so that only the exclusion can explain the choice.
			ATestPassiveEnemySoldier* Second = MakeEnemy(TEXT("EnemySecond"), FVector(-CentreDistance(F.Range + 250.0), 0.0, 100.0));
			if (Second == nullptr) { return; }
			TestTrue(TEXT("precondition: the second enemy is in the zone and farther than the failed one"),
				FVector::Dist2D(F.Servant->GetActorLocation(), Second->GetActorLocation()) > FVector::Dist2D(F.Servant->GetActorLocation(), First->GetActorLocation())
				&& FMath::Abs(Second->GetActorLocation().X) < 500.0); // default zone: X in [-500, 500]

			const bool bTaken = TickUntil([&]() { return F.Ctl->RequestedEnemies.Num() >= 2; }, WakeWindow());
			TestTrue(TEXT("Second enemy: a chase request within EnemyScanInterval + margin (the failed enemy is still in cooldown)"), bTaken);
			TestTrue(TEXT("Second enemy: the request is towards the second enemy, not the failed one"), F.Ctl->RequestedEnemies.Num() >= 2 && F.Ctl->RequestedEnemies[1].Get() == Second);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// "Cas limites" of the spec.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Edge cases", [this]()
	{
		It("Edge: a chase that fails synchronously inside the request ends the fight at once, spares the retry loop, and a decision stays scheduled", [this]()
		{
			// Mutation: the fight state is written AFTER RequestMoveToEnemy (the synchronous end is ignored, or the late write resurrects
			// IsFighting: it stays true with no move), EndCombat does not end with a ScheduleDecision (no decision ever again: LastAction
			// stays Attack and no request follows), or the failure is retried at once (a second request before RetryDelay).
			// Not asserted (the spec only says RetryDelay and "bSkipNextHarvest"): whether the decision after the failure may fight again.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(F.Range + 150.0), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			F.Ctl->bFailSynchronously = true;

			if (!TestTrue(TEXT("a chase request is made at the first decision"), TickUntil([&]() { return F.Ctl->RequestedEnemies.Num() >= 1; }, InitialDecisionDelay + TimerSlack))) { return; }
			TestFalse(TEXT("Edge: IsFighting is false once the move failed synchronously"), F.Ctl->IsFighting());
			TestNull(TEXT("Edge: no fight target"), F.Ctl->GetFightTarget());
			TickFor(0.5 * RetryDelay);
			TestEqual(TEXT("Edge: no new request before RetryDelay"), F.Ctl->RequestedEnemies.Num(), 1);
			TestFalse(TEXT("Edge: still not fighting"), F.Ctl->IsFighting());
			const bool bDecided = TickUntil([&]() { return F.Ctl->GetLastAction() == EServantWanderAction::Wander || F.Ctl->RequestedEnemies.Num() >= 2; }, 0.5 * RetryDelay + 2.0 * TimerSlack);
			TestTrue(TEXT("Edge: a new decision happens after RetryDelay (EndCombat scheduled one)"), bDecided);
			TestTrue(TEXT("Edge: the servant still wanders"), F.Ctl->IsWandering());
		});

		It("Edge: a servant that stays ALIVE but loses its controller (UnPossess) in the middle of a fight stops fighting for good", [this]()
		{
			// Spec: "Mort pendant le combat : StopWandering coupe CombatTimer avant StopMovement". UnPossess runs OnUnPossess ->
			// StopWandering, the same path as the death, but the servant is NOT destroyed: nothing else (EndPlay, unregistration,
			// OnAssignmentsChanged -> EndCombat) can mask a missing cleanup, unlike in the death test below.
			// Mutation: StopWandering does not clear the combat state (ClearCombatState removed): IsFighting stays true and the fight target
			// is kept, and/or CombatTimer keeps ticking on a pawnless controller (a further decision or chase request follows).
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			if (!StartFight(F)) { return; }
			TestEqual(TEXT("precondition: one hit"), HitsOn(Enemy), 1);
			const int32 RequestsBefore = F.Ctl->RequestedEnemies.Num();
			TWeakObjectPtr<AServant> WeakServant(F.Servant);

			F.Ctl->UnPossess();

			TestTrue(TEXT("precondition: the servant is still alive"), WeakServant.IsValid() && !ServantCombatTest::HealthOf(F.Servant)->IsDead());
			TestTrue(TEXT("precondition: the controller has no pawn any more"), F.Ctl->GetPawn() == nullptr);
			TestFalse(TEXT("IsFighting is false right after the controller lost its pawn"), F.Ctl->IsFighting());
			TestNull(TEXT("no fight target is kept"), F.Ctl->GetFightTarget());
			TestFalse(TEXT("no wander either"), F.Ctl->IsWandering());

			// Long enough for a pending combat step AND for a rescheduled decision (RetryDelay, pause) to fire.
			bool bEverFighting = false;
			TickWatching(3.0 * F.Interval + RetryDelay + InitialDecisionDelay, [&]() { bEverFighting |= F.Ctl->IsFighting(); });
			TestFalse(TEXT("it never fights again on a pawnless controller"), bEverFighting);
			TestEqual(TEXT("no hit after UnPossess"), HitsOn(Enemy), 1);
			TestEqual(TEXT("no new chase request after UnPossess"), F.Ctl->RequestedEnemies.Num(), RequestsBefore);
			TestFalse(TEXT("no decision was rescheduled (not wandering)"), F.Ctl->IsWandering());
		});

		It("Edge: a servant killed in the middle of a fight stops fighting: no further hit, no leftover registration", [this]()
		{
			// Mutation: the dead servant stays registered, or is not destroyed. WEAK on the timer: the death goes through EndPlay,
			// UnregisterServant and OnAssignmentsChanged -> EndCombat, which mask a StopWandering that forgets to cut the combat; the
			// UnPossess test above is the one that covers it.
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone();
			FFighter F;
			if (Zone == nullptr || !MakeFighter(F, TEXT("Servant"), FVector(0.0, 0.0, 100.0), EServantTask::AttackDefense)) { return; }
			ATestPassiveEnemySoldier* Enemy = MakeEnemy(TEXT("Enemy"), FVector(CentreDistance(0.5 * F.Range), 0.0, 100.0));
			if (Enemy == nullptr) { return; }
			TWeakObjectPtr<AServant> WeakServant(F.Servant);
			TWeakObjectPtr<ATestCombatServantAIController> WeakController(F.Ctl);
			if (!StartFight(F)) { return; }
			TestEqual(TEXT("precondition: one hit"), HitsOn(Enemy), 1);

			EnemyTest::KillActor(F.Servant);
			TickFor(3.0 * F.Interval);
			TestFalse(TEXT("the servant is destroyed"), WeakServant.IsValid());
			TestFalse(TEXT("the servant is no longer registered"), World->GetSubsystem<UServantSubsystem>()->IsServantRegistered(WeakServant.Get()));
			TestEqual(TEXT("no hit after the servant's death"), HitsOn(Enemy), 1);
			TestFalse(TEXT("its controller does not go on fighting"), WeakController.IsValid() && WeakController->IsFighting());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
