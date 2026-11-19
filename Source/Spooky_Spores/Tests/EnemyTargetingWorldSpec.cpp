// Automation Spec for the Engage targeting of AEnemyAIController inside a minimal test world.
// Source of truth: docs/specs/enemy-wave-spawner.md (removed at merge), "Ciblage en Engage", "Zone", "Pas de cible" and criteria 8 to 10.
// FindNearestDestructible is private: the choice of target is observed through the behaviour (health of the destructibles, position of
// the soldier, error logs), as the spec asks.
//
// World: FTestWorldWrapper (transient Game world), time advanced only by TickTestWorld. No NavMesh and no floor: ATestEnemySoldier flies
// and follows its spline with AddMovementInput. The soldier's spline is 60 u long: it reaches its end (Engage) within a fraction of a
// second at the default MoveSpeed, then walks to its target. Which destructible it attacks is read from the one whose health drops first.
//
// Layout shared by the targeting tests (X axis, soldier at the origin, zone centred on (4000, 0) with half extent (2000, 1000)):
//   zone covers X in [2000, 6000]; the nearest in-zone destructible is at 2500 u from the soldier, i.e. beyond the former 2000 u radius.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/EnemyTestHelpers.h"
#include "Tests/EnemyTestActors.h"
#include "Base/BaseZone.h"
#include "Base/BaseZoneSubsystem.h"
#include "Enemies/EnemySoldier.h"
#include "Enemies/EnemyPathSpline.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Templates/Function.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FEnemyTargetingWorldSpec, "SpookySpores.Enemies.Targeting.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = 0.05f;
	// Generous simulated time for a soldier to cross the layout and land its first hit.
	static constexpr double MaxApproachTime = 60.0;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	bool RequireWorld();
	ABaseZone* SpawnZone(const FVector& Center, const FVector& Scale);
	ATestDestructible* SpawnTarget(const FVector& Location);
	ATestEnemySoldier* SpawnSoldier(const FVector& Location);
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);
	// Standard zone of the layout above (X in [2000, 6000], Y in [-1000, 1000]).
	ABaseZone* SpawnLayoutZone();

END_DEFINE_SPEC(FEnemyTargetingWorldSpec)

bool FEnemyTargetingWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

ABaseZone* FEnemyTargetingWorldSpec::SpawnZone(const FVector& Center, const FVector& Scale)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABaseZone* Zone = World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform(Center), Params);
	if (Zone != nullptr) { Zone->GetZoneBox()->SetWorldScale3D(Scale); }
	return Zone;
}

ABaseZone* FEnemyTargetingWorldSpec::SpawnLayoutZone()
{
	return SpawnZone(FVector(4000.0, 0.0, 0.0), FVector(4.0, 2.0, 1.0));
}

ATestDestructible* FEnemyTargetingWorldSpec::SpawnTarget(const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ATestDestructible>(ATestDestructible::StaticClass(), FTransform(Location), Params);
}

ATestEnemySoldier* FEnemyTargetingWorldSpec::SpawnSoldier(const FVector& Location)
{
	AEnemyPathSpline* Spline = EnemyTest::SpawnSpline(World, Location, FVector(1.0, 0.0, 0.0), 60.0);
	if (!TestNotNull(TEXT("harness: spline spawned"), Spline)) { return nullptr; }
	// Same sequence as the wave spawner: setter before FinishSpawning, so OnPossess sees the spline.
	ATestEnemySoldier* Soldier = World->SpawnActorDeferred<ATestEnemySoldier>(ATestEnemySoldier::StaticClass(), FTransform(Location), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("harness: soldier spawned"), Soldier)) { return nullptr; }
	Soldier->SetPathSplineActor(Spline);
	Soldier->FinishSpawning(FTransform(Location));
	return Soldier;
}

void FEnemyTargetingWorldSpec::TickFor(double Seconds)
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(TickStep);
	}
}

bool FEnemyTargetingWorldSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt32(MaxSeconds / TickStep));
	for (int32 Index = 0; Index < MaxSteps; ++Index)
	{
		if (Condition()) { return true; }
		Wrapper->TickTestWorld(TickStep);
	}
	return Condition();
}

void FEnemyTargetingWorldSpec::Define()
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

	Describe("Target choice (criteria 8 and 9)", [this]()
	{
		It("C8: attacks the in-zone destructible nearest to the soldier, even beyond 2000 u; ignores nearer ones out of zone, on the edge, or dead", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnLayoutZone();
			if (!TestNotNull(TEXT("harness: zone"), Zone)) { return; }

			ATestDestructible* Outside = SpawnTarget(FVector(800.0, 300.0, 0.0));    // 854 u away, out of zone
			ATestDestructible* Straddling = SpawnTarget(FVector(1960.0, 0.0, 0.0));  // centre out of zone (zone starts at 2000), its box overlaps the zone
			ATestDestructible* Dead = SpawnTarget(FVector(2200.0, -200.0, 0.0));     // in zone but dead, nearer than the nearest living one
			ATestDestructible* Nearest = SpawnTarget(FVector(2500.0, 0.0, 0.0));     // in zone, 2500 u away: the expected target
			ATestDestructible* Far = SpawnTarget(FVector(4500.0, 0.0, 0.0));         // in zone, 4500 u away
			if (!TestTrue(TEXT("harness: targets spawned"), Outside && Straddling && Dead && Nearest && Far)) { return; }
			EnemyTest::KillActor(Dead);
			TestTrue(TEXT("precondition: the dead one is dead"), Dead->IsDead());
			TestFalse(TEXT("precondition: 'Outside' is out of the zone"), Zone->IsPointInZone(Outside->GetActorLocation()));
			TestFalse(TEXT("precondition: 'Straddling' centre is out of the zone"), Zone->IsPointInZone(Straddling->GetActorLocation()));
			TestTrue(TEXT("precondition: 'Nearest' is in the zone"), Zone->IsPointInZone(Nearest->GetActorLocation()));
			TestTrue(TEXT("precondition: 'Far' is in the zone"), Zone->IsPointInZone(Far->GetActorLocation()));

			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			const bool bAttacked = TickUntil([Nearest]() { return Nearest->GetHealthPercent() < 1.0f; }, MaxApproachTime);
			TestTrue(TEXT("C8: the nearest in-zone living destructible (2500 u away) was attacked"), bAttacked);
			TickFor(5.0); // more hits: the soldier must keep its target, not wander to another
			TestEqual(TEXT("C8: the farther in-zone destructible was not attacked"), Far->GetHealthPercent(), 1.0f);
			TestEqual(TEXT("C8: the nearer out-of-zone destructible was not attacked"), Outside->GetHealthPercent(), 1.0f);
			TestEqual(TEXT("C8: the destructible whose centre is out of the zone (box overlapping it) was not attacked"), Straddling->GetHealthPercent(), 1.0f);
			TestTrue(TEXT("C8: the dead destructible stays dead and was not the target"), Dead->IsDead() && Nearest->GetHealthPercent() < 1.0f);
		});

		It("C9: when the target is killed, the soldier attacks the nearest living in-zone destructible left", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnLayoutZone();
			if (!TestNotNull(TEXT("harness: zone"), Zone)) { return; }
			ATestDestructible* First = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* Second = SpawnTarget(FVector(2500.0, -500.0, 0.0)); // 500 u from the soldier once it stands next to 'First'
			ATestDestructible* Third = SpawnTarget(FVector(2500.0, 1500.0, 0.0));
			if (!TestTrue(TEXT("harness: targets spawned"), First && Second && Third)) { return; }

			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			if (!TestTrue(TEXT("precondition: 'First' is the first target"), TickUntil([First]() { return First->GetHealthPercent() < 1.0f; }, MaxApproachTime))) { return; }
			TestEqual(TEXT("precondition: nobody else was hit"), Second->GetHealthPercent() + Third->GetHealthPercent(), 2.0f);

			EnemyTest::KillActor(First);
			const bool bRetargeted = TickUntil([Second]() { return Second->GetHealthPercent() < 1.0f; }, MaxApproachTime);
			TestTrue(TEXT("C9: after the target died, the nearest remaining one is attacked"), bRetargeted);
			TestEqual(TEXT("C9: not the farther one"), Third->GetHealthPercent(), 1.0f);
		});

		It("C9: when the target is destroyed (not just dead), the soldier moves on to the nearest remaining one", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnLayoutZone();
			if (!TestNotNull(TEXT("harness: zone"), Zone)) { return; }
			ATestDestructible* First = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* Second = SpawnTarget(FVector(2500.0, -500.0, 0.0));
			if (!TestTrue(TEXT("harness: targets spawned"), First && Second)) { return; }

			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			if (!TestTrue(TEXT("precondition: 'First' is the first target"), TickUntil([First]() { return First->GetHealthPercent() < 1.0f; }, MaxApproachTime))) { return; }
			First->Destroy();
			TestTrue(TEXT("C9: after the target was destroyed, the remaining one is attacked"),
				TickUntil([Second]() { return Second->GetHealthPercent() < 1.0f; }, MaxApproachTime));
		});
	});

	Describe("Target choice, 2D distance (spec line 21)", [this]()
	{
		It("C8: the nearest in 2D is attacked even when a destructible far above it is farther in 3D", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnLayoutZone();
			if (!TestNotNull(TEXT("harness: zone"), Zone)) { return; }
			// From the soldier at the origin: High is 2100 u away in 2D but 3660 u in 3D; Low is 2563 u away in both.
			// They lie in different directions (Y = 0 vs Y = 900), so the heading of the soldier tells which one it chose.
			ATestDestructible* High = SpawnTarget(FVector(2100.0, 0.0, 3000.0));
			ATestDestructible* Low = SpawnTarget(FVector(2400.0, 900.0, 0.0));
			if (!TestTrue(TEXT("harness: targets spawned"), High && Low)) { return; }
			TestTrue(TEXT("precondition: both in the zone (2D)"), Zone->IsPointInZone(High->GetActorLocation()) && Zone->IsPointInZone(Low->GetActorLocation()));

			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			const bool bAdvanced = TickUntil([Soldier]() { return Soldier->GetActorLocation().X > 1500.0; }, MaxApproachTime);
			if (!TestTrue(TEXT("precondition: the soldier crossed the first 1500 u"), bAdvanced)) { return; }
			TickFor(3.0);
			const FVector Position = Soldier->GetActorLocation();
			TestTrue(FString::Printf(TEXT("C8 2D: the soldier heads for the 2D-nearest target (Y stays near 0, got %s); a 3D choice would steer it to Y = 900"), *Position.ToString()),
				FMath::Abs(Position.Y) < 200.0);
			TestEqual(TEXT("C8 2D: the 3D-nearest destructible was not attacked"), Low->GetHealthPercent(), 1.0f);
		});
	});

	Describe("No target (criterion 10)", [this]()
	{
		// Idle soldier in Engage: no movement, still alive, nothing attacked. The "exactly one error over 200 ticks (not one per tick)"
		// check is the AddExpectedErrorPlain(..., 1) declared by each test before the soldier is spawned.
		auto ExpectIdle = [this](const FString& Case, ATestEnemySoldier* Soldier, const TArray<ATestDestructible*>& Targets)
		{
			TickFor(3.0); // spline end reached within a fraction of a second, movement settled
			const FVector Settled = Soldier->GetActorLocation();
			TickFor(10.0); // 200 ticks
			TestTrue(*FString::Printf(TEXT("C10 %s: the soldier stays immobile"), *Case), Soldier->GetActorLocation().Equals(Settled, 1.0));
			TestTrue(*FString::Printf(TEXT("C10 %s: the soldier stays alive and in play"), *Case), IsValid(Soldier));
			for (const ATestDestructible* Target : Targets)
			{
				TestEqual(*FString::Printf(TEXT("C10 %s: no destructible was attacked"), *Case), Target->GetHealthPercent(), Target->IsDead() ? 0.0f : 1.0f);
			}
		};

		It("C10: without any registered zone, the soldier idles with one error (a living destructible is right next to it)", [this, ExpectIdle]()
		{
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 1);
			const UBaseZoneSubsystem* Subsystem = World->GetSubsystem<UBaseZoneSubsystem>();
			if (!TestNotNull(TEXT("harness: zone subsystem"), Subsystem)) { return; }
			TestNull(TEXT("precondition: no zone registered"), Subsystem->GetBaseZone());
			ATestDestructible* Target = SpawnTarget(FVector(500.0, 0.0, 0.0));
			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Target == nullptr || Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			ExpectIdle(TEXT("no zone"), Soldier, { Target });
		});

		It("C10: a zone registered after the soldier already logged its error is used by the next search (no cached null)", [this, ExpectIdle]()
		{
			if (!RequireWorld()) { return; }
			// One error in total for the whole test: the one logged before the zone exists, none once the target is found.
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 1);
			ATestDestructible* Target = SpawnTarget(FVector(500.0, 0.0, 0.0));
			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Target == nullptr || Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			ExpectIdle(TEXT("before the zone exists"), Soldier, { Target });

			ABaseZone* Zone = SpawnZone(FVector(500.0, 0.0, 0.0), FVector::OneVector);
			if (!TestNotNull(TEXT("harness: late zone"), Zone)) { return; }
			TestTrue(TEXT("precondition: the target is inside the late zone"), Zone->IsPointInZone(Target->GetActorLocation()));
			const bool bAttacked = TickUntil([Target]() { return Target->GetHealthPercent() < 1.0f; }, MaxApproachTime);
			TestTrue(TEXT("C10: the soldier picked up the zone registered after the fact and attacked"), bAttacked);
		});

		It("C10: a zone whose only destructible is dead gives no target", [this, ExpectIdle]()
		{
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 1);
			if (!TestNotNull(TEXT("harness: zone"), SpawnZone(FVector(500.0, 0.0, 0.0), FVector::OneVector))) { return; }
			ATestDestructible* Dead = SpawnTarget(FVector(500.0, 0.0, 0.0));
			if (Dead == nullptr) { return; }
			EnemyTest::KillActor(Dead);
			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			ExpectIdle(TEXT("only a dead destructible"), Soldier, { Dead });
		});

		It("C10: a zone whose only destructible is outside it gives no target", [this, ExpectIdle]()
		{
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 1);
			ABaseZone* Zone = SpawnZone(FVector(3000.0, 0.0, 0.0), FVector::OneVector); // X in [2500, 3500]
			ATestDestructible* Outside = SpawnTarget(FVector(500.0, 0.0, 0.0));
			if (!TestNotNull(TEXT("harness: zone"), Zone) || Outside == nullptr) { return; }
			TestFalse(TEXT("precondition: the destructible is out of the zone"), Zone->IsPointInZone(Outside->GetActorLocation()));
			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			ExpectIdle(TEXT("only an out-of-zone destructible"), Soldier, { Outside });
		});

		It("C10: the single error is per soldier: two soldiers without target log two errors", [this]()
		{
			if (!RequireWorld()) { return; }
			// One error per soldier, none per tick: exactly 2 for 2 soldiers over 260 ticks.
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 2);
			ATestEnemySoldier* A = SpawnSoldier(FVector(0.0, 0.0, 0.0));
			ATestEnemySoldier* B = SpawnSoldier(FVector(0.0, 3000.0, 0.0));
			if (A == nullptr || B == nullptr) { return; }
			A->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			B->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

			TickFor(13.0);
			TestTrue(TEXT("C10: both soldiers alive"), IsValid(A) && IsValid(B));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
