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
			// seen as dead or destroyed (the soldier keeps it as target after its death), or the claim is not released.
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
			const bool bLost = TickUntil([Controller]() { return Controller->GetCurrentTarget() == nullptr; }, 5.0);
			TestTrue(TEXT("C2: after the servant died the soldier no longer has it as target"), bLost);
			TestNull(TEXT("C2: and has no target at all (it was the only destructible)"), Controller->GetCurrentTarget());
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

		It("C8: a FieldHarvest servant never fights, even hurt by an enemy: it stays alive, assigned, and does not retaliate", [this]()
		{
			// Mutation: the role is not tested, or a hit makes the servant fight back / changes its task.
			// Not automated: "IsHarvesting stays true when hit": a servant only stays in harvest while it WALKS to a field (needs a NavMesh
			// projection, see the report). Here we check what is observable without it: no fight, no retaliation, task kept.
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

		It("Edge: a servant killed in the middle of a fight stops fighting: no further hit, no leftover registration", [this]()
		{
			// Mutation: the fight timer outlives the servant's death (StopWandering does not cut CombatTimer) and keeps striking, or the
			// dead servant stays registered. Weak: when the controller is destroyed with the pawn, the timer dies with it either way.
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
