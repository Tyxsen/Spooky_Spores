// Automation Spec for the soft target reservation and the NavMesh move of the Engage state (item 3b).
// Source of truth: docs/specs/enemy-navmesh-reservation.md (removed at merge), criteria C1 to C12 and "Cas limites".
// Written from the spec only: the tests observe AEnemyAIController::GetCurrentTarget(), UEnemyTargetSubsystem::GetClaimCount() and
// the requests recorded by the test seam (ATestEnemyAIController), never the position of a soldier (test worlds have no NavMesh).
//
// Each test names, in a "Mutation:" comment, the behaviour change that must turn it red. Every choice test fails if the choice ignores
// the reservations (cross-cutting rule of the spec).
//
// Layout shared by the world tests (X axis, zone centred on (4000, 0), half extent (2000, 1000): X in [2000, 6000], Y in [-1000, 1000]).
// Soldiers are spawned out of the zone, at the X given below, and walk their 60 u spline to reach Engage; with the seam they never move
// afterwards, so their 2D distance to the targets is the distance from their spline end (X + 60).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/EnemyTestHelpers.h"
#include "Tests/EnemyTestActors.h"
#include "AIController.h"
#include "Base/BaseZone.h"
#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemyPathSpline.h"
#include "Enemies/EnemySoldier.h"
#include "Enemies/EnemyTargetSubsystem.h"
#include "Servants/Servant.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Templates/Function.h"
#include "Tests/AutomationCommon.h"

namespace EnemyReservationTest
{
	inline AEnemyAIController* ControllerOf(const APawn* Pawn)
	{
		return (Pawn != nullptr) ? Cast<AEnemyAIController>(Pawn->GetController()) : nullptr;
	}

	inline ATestEnemyAIController* SeamOf(const APawn* Pawn)
	{
		return (Pawn != nullptr) ? Cast<ATestEnemyAIController>(Pawn->GetController()) : nullptr;
	}
}

BEGIN_DEFINE_SPEC(FEnemyTargetReservationSpec, "SpookySpores.Enemies.TargetReservation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = 0.05f;
	static constexpr double MaxWaitTime = 30.0;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	bool RequireWorld();
	ABaseZone* SpawnZone(const FVector& Center, const FVector& Scale);
	ABaseZone* SpawnLayoutZone();
	ATestDestructible* SpawnTarget(const FVector& Location);
	// A servant standing still (its movement tick is off, no floor in the test world): a destructible that is a PAWN (it can move).
	AServant* SpawnServantTarget(const FVector& Location);
	AController* SpawnPlainController();
	ATestEnemySoldier* SpawnSoldier(const FVector& Location, TSubclassOf<ATestEnemySoldier> SoldierClass);
	ATestSeamEnemySoldier* SpawnSeamSoldier(const FVector& Location);
	// Spawns a seam soldier and ticks until it has chosen a target. Null (and an error) when it never does.
	ATestSeamEnemySoldier* SpawnChosenSoldier(const FVector& Location);
	UEnemyTargetSubsystem* Claims() const;
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);

END_DEFINE_SPEC(FEnemyTargetReservationSpec)

bool FEnemyTargetReservationSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

ABaseZone* FEnemyTargetReservationSpec::SpawnZone(const FVector& Center, const FVector& Scale)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABaseZone* Zone = World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform(Center), Params);
	if (Zone != nullptr) { Zone->GetZoneBox()->SetWorldScale3D(Scale); }
	return Zone;
}

ABaseZone* FEnemyTargetReservationSpec::SpawnLayoutZone()
{
	return SpawnZone(FVector(4000.0, 0.0, 0.0), FVector(4.0, 2.0, 1.0));
}

ATestDestructible* FEnemyTargetReservationSpec::SpawnTarget(const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ATestDestructible>(ATestDestructible::StaticClass(), FTransform(Location), Params);
}

AServant* FEnemyTargetReservationSpec::SpawnServantTarget(const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AServant* Servant = World->SpawnActor<AServant>(AServant::StaticClass(), FTransform(Location), Params);
	if (Servant != nullptr && Servant->GetCharacterMovement() != nullptr)
	{
		Servant->GetCharacterMovement()->SetComponentTickEnabled(false);
	}
	return Servant;
}

AController* FEnemyTargetReservationSpec::SpawnPlainController()
{
	return World->SpawnActor<AAIController>(AAIController::StaticClass(), FTransform::Identity);
}

ATestEnemySoldier* FEnemyTargetReservationSpec::SpawnSoldier(const FVector& Location, TSubclassOf<ATestEnemySoldier> SoldierClass)
{
	AEnemyPathSpline* Spline = EnemyTest::SpawnSpline(World, Location, FVector(1.0, 0.0, 0.0), 60.0);
	if (!TestNotNull(TEXT("harness: spline spawned"), Spline)) { return nullptr; }
	// Same sequence as the wave spawner: setter before FinishSpawning, so OnPossess sees the spline.
	ATestEnemySoldier* Soldier = World->SpawnActorDeferred<ATestEnemySoldier>(SoldierClass, FTransform(Location), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("harness: soldier spawned"), Soldier)) { return nullptr; }
	Soldier->SetPathSplineActor(Spline);
	Soldier->FinishSpawning(FTransform(Location));
	Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	return Soldier;
}

ATestSeamEnemySoldier* FEnemyTargetReservationSpec::SpawnSeamSoldier(const FVector& Location)
{
	return Cast<ATestSeamEnemySoldier>(SpawnSoldier(Location, ATestSeamEnemySoldier::StaticClass()));
}

ATestSeamEnemySoldier* FEnemyTargetReservationSpec::SpawnChosenSoldier(const FVector& Location)
{
	ATestSeamEnemySoldier* Soldier = SpawnSeamSoldier(Location);
	if (Soldier == nullptr) { return nullptr; }
	const AEnemyAIController* Controller = EnemyReservationTest::ControllerOf(Soldier);
	if (!TestNotNull(TEXT("harness: the soldier is possessed by an AEnemyAIController"), Controller)) { return nullptr; }
	if (!TestTrue(TEXT("harness: the soldier chose a target"), TickUntil([Controller]() { return Controller->GetCurrentTarget() != nullptr; }, MaxWaitTime)))
	{
		return nullptr;
	}
	return Soldier;
}

UEnemyTargetSubsystem* FEnemyTargetReservationSpec::Claims() const
{
	return World->GetSubsystem<UEnemyTargetSubsystem>();
}

void FEnemyTargetReservationSpec::TickFor(double Seconds)
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(TickStep);
	}
}

bool FEnemyTargetReservationSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt32(MaxSeconds / TickStep));
	for (int32 Index = 0; Index < MaxSteps; ++Index)
	{
		if (Condition()) { return true; }
		Wrapper->TickTestWorld(TickStep);
	}
	return Condition();
}

void FEnemyTargetReservationSpec::Define()
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
	// Registry and pick rule, without any soldier: plain AAIControllers stand in for the claimants.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Subsystem registry and pick rule", [this]()
	{
		It("SetClaim counts per target and replaces the previous claim of the same controller", [this]()
		{
			// Mutation: SetClaim adds instead of replacing (A would stay at 1), or one claim per target instead of per controller.
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* C1 = SpawnPlainController();
			AController* C2 = SpawnPlainController();
			ATestDestructible* A = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(2000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && C1 && C2 && A && B)) { return; }

			TestEqual(TEXT("no claim yet: 0"), Subsystem->GetClaimCount(A), 0);
			Subsystem->SetClaim(C1, A);
			Subsystem->SetClaim(C2, A);
			TestEqual(TEXT("two controllers on A: 2"), Subsystem->GetClaimCount(A), 2);
			Subsystem->SetClaim(C1, B);
			TestEqual(TEXT("C1 moved to B: A drops to 1"), Subsystem->GetClaimCount(A), 1);
			TestEqual(TEXT("C1 moved to B: B is 1"), Subsystem->GetClaimCount(B), 1);
			Subsystem->SetClaim(C1, B);
			TestEqual(TEXT("claiming the same target twice does not count twice"), Subsystem->GetClaimCount(B), 1);
		});

		It("ReleaseClaim removes only the claim of that controller; releasing a controller without claim changes nothing", [this]()
		{
			// Mutation: ReleaseClaim clears every claim on the target (or all claims); or it asserts/crashes without a claim.
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* C1 = SpawnPlainController();
			AController* C2 = SpawnPlainController();
			AController* Stranger = SpawnPlainController();
			ATestDestructible* A = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && C1 && C2 && Stranger && A)) { return; }
			Subsystem->SetClaim(C1, A);
			Subsystem->SetClaim(C2, A);

			Subsystem->ReleaseClaim(C1);
			TestEqual(TEXT("C1 released: only C2 remains"), Subsystem->GetClaimCount(A), 1);
			Subsystem->ReleaseClaim(C1);
			Subsystem->ReleaseClaim(Stranger);
			TestEqual(TEXT("releasing twice / a controller without claim leaves C2 alone"), Subsystem->GetClaimCount(A), 1);
		});

		It("GetClaimCount ignores a claimant that was destroyed and a null target", [this]()
		{
			// Mutation: the table keeps counting dead controllers (strong count instead of weak validity check).
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* Alive = SpawnPlainController();
			AController* Doomed = SpawnPlainController();
			ATestDestructible* A = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && Alive && Doomed && A)) { return; }
			Subsystem->SetClaim(Alive, A);
			Subsystem->SetClaim(Doomed, A);
			TestEqual(TEXT("precondition: 2 claims"), Subsystem->GetClaimCount(A), 2);

			Doomed->Destroy();
			TestEqual(TEXT("destroyed claimant is not counted"), Subsystem->GetClaimCount(A), 1);
			TestEqual(TEXT("null target: 0"), Subsystem->GetClaimCount(nullptr), 0);
		});

		It("PickTarget: the free target nearest in 2D wins (a target high above is nearer in 2D than in 3D)", [this]()
		{
			// Mutation: 3D distance instead of 2D (picks Low), or farthest/first candidate instead of nearest.
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* C1 = SpawnPlainController();
			ATestDestructible* High = SpawnTarget(FVector(1000.0, 0.0, 5000.0)); // 1000 u in 2D, ~5100 in 3D
			ATestDestructible* Low = SpawnTarget(FVector(1500.0, 0.0, 0.0));     // 1500 u in both
			if (!TestTrue(TEXT("harness"), Subsystem && C1 && High && Low)) { return; }
			const TArray<AActor*> Candidates = { Low, High };

			TestTrue(TEXT("2D-nearest free target"), Subsystem->PickTarget(C1, FVector::ZeroVector, Candidates) == High);
			TestEqual(TEXT("PickTarget is a pure query: no claim was created"), Subsystem->GetClaimCount(High) + Subsystem->GetClaimCount(Low), 0);
			TestNull(TEXT("no candidate: null"), Subsystem->PickTarget(C1, FVector::ZeroVector, TArray<AActor*>()));
		});

		It("PickTarget: a free target beats a nearer occupied one", [this]()
		{
			// Mutation: PickTarget ignores the reservations (would return Near).
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* Picker = SpawnPlainController();
			AController* Other = SpawnPlainController();
			ATestDestructible* Near = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			ATestDestructible* Far = SpawnTarget(FVector(3000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && Picker && Other && Near && Far)) { return; }
			Subsystem->SetClaim(Other, Near);
			const TArray<AActor*> Candidates = { Near, Far };

			TestTrue(TEXT("the free one, although farther"), Subsystem->PickTarget(Picker, FVector::ZeroVector, Candidates) == Far);
		});

		It("PickTarget: when all are occupied, the least occupied wins; on equal occupation the nearest in 2D", [this]()
		{
			// Mutation: ties broken by farthest/first, or the least-occupied rule replaced by "nearest" (counts 2 and 1 would pick Near).
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* Picker = SpawnPlainController();
			AController* O1 = SpawnPlainController();
			AController* O2 = SpawnPlainController();
			AController* O3 = SpawnPlainController();
			ATestDestructible* Near = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			ATestDestructible* Far = SpawnTarget(FVector(3000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && Picker && O1 && O2 && O3 && Near && Far)) { return; }
			const TArray<AActor*> Candidates = { Far, Near };

			Subsystem->SetClaim(O1, Near);
			Subsystem->SetClaim(O2, Near);
			Subsystem->SetClaim(O3, Far);
			TestTrue(TEXT("counts Near 2, Far 1: the least occupied, although farther"), Subsystem->PickTarget(Picker, FVector::ZeroVector, Candidates) == Far);

			// O2 moves to Far: Near 1 (O1), Far 2 (O2, O3). The least occupied is now the nearer one.
			Subsystem->SetClaim(O2, Far);
			TestTrue(TEXT("counts Near 1, Far 2: the least occupied (Near)"), Subsystem->PickTarget(Picker, FVector::ZeroVector, Candidates) == Near);

			// O3 moves to Near: Near 2 (O1, O3), Far 1 (O2) -> the least occupied is Far again, whatever the candidate order.
			Subsystem->SetClaim(O3, Near);
			TestTrue(TEXT("counts Near 2, Far 1, other candidate order"), Subsystem->PickTarget(Picker, FVector::ZeroVector, { Near, Far }) == Far);
		});

		It("PickTarget: equal occupation is decided by the 2D distance from the origin, both ways", [this]()
		{
			// Mutation: tie broken by candidate order or by the larger distance.
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* Picker = SpawnPlainController();
			AController* O1 = SpawnPlainController();
			AController* O2 = SpawnPlainController();
			ATestDestructible* Left = SpawnTarget(FVector(-1000.0, 0.0, 0.0));
			ATestDestructible* Right = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && Picker && O1 && O2 && Left && Right)) { return; }
			Subsystem->SetClaim(O1, Left);
			Subsystem->SetClaim(O2, Right);

			TestTrue(TEXT("tie 1/1, origin near Right"), Subsystem->PickTarget(Picker, FVector(800.0, 0.0, 0.0), { Left, Right }) == Right);
			TestTrue(TEXT("tie 1/1, origin near Left, candidates in the other order"),
				Subsystem->PickTarget(Picker, FVector(-800.0, 0.0, 0.0), { Right, Left }) == Left);
		});

		It("PickTarget: the claim of the requester itself is not counted", [this]()
		{
			// Mutation: the requester's own claim counts (Mine would look occupied and Other's target would be preferred).
			if (!RequireWorld()) { return; }
			UEnemyTargetSubsystem* Subsystem = Claims();
			AController* Requester = SpawnPlainController();
			AController* Other = SpawnPlainController();
			ATestDestructible* Mine = SpawnTarget(FVector(1000.0, 0.0, 0.0));
			ATestDestructible* Theirs = SpawnTarget(FVector(3000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness"), Subsystem && Requester && Other && Mine && Theirs)) { return; }
			Subsystem->SetClaim(Requester, Mine);
			Subsystem->SetClaim(Other, Theirs);

			// Origin next to Theirs. Without its own claim: Mine 0 (free), Theirs 1 -> Mine, although farther. If its own claim were
			// counted: 1 and 1, a tie, and the nearest (Theirs) would win.
			TestTrue(TEXT("requester re-picks its own target when it is the free one"),
				Subsystem->PickTarget(Requester, FVector(2900.0, 0.0, 0.0), { Mine, Theirs }) == Mine);
			TestEqual(TEXT("the pick did not modify the claims"), Subsystem->GetClaimCount(Mine), 1);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// C1, C2: choice with soldiers.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Choice (C1, C2)", [this]()
	{
		It("C1: the first soldier takes the nearest target, the second takes the other one even though the first is nearer to it", [this]()
		{
			// Mutation: the choice ignores the reservations (the second soldier would take A, nearest to it).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* A = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), A && B)) { return; }

			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			if (S1 == nullptr) { return; }
			TestTrue(TEXT("C1: the first soldier takes A (the nearest)"), EnemyReservationTest::ControllerOf(S1)->GetCurrentTarget() == A);

			// From X = 3000 (60 u further once in Engage) A is 560 u away and B 1440 u away.
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(3000.0, 0.0, 0.0));
			if (S2 == nullptr) { return; }
			TestTrue(TEXT("C1: the second soldier takes B, A being reserved"), EnemyReservationTest::ControllerOf(S2)->GetCurrentTarget() == B);
			TestTrue(TEXT("C1: the first soldier keeps A (no periodic re-choice)"), EnemyReservationTest::ControllerOf(S1)->GetCurrentTarget() == A);
			TestEqual(TEXT("C1: A has one claim"), Claims()->GetClaimCount(A), 1);
			TestEqual(TEXT("C1: B has one claim"), Claims()->GetClaimCount(B), 1);
		});

		It("C2: free first; on a tie in occupation the nearest; with counts 2 and 1 a newcomer joins the one at 1 even if farther; no error", [this]()
		{
			// Mutation: ties broken by "farthest" or by order (S3 or S5 would go to the wrong target); least-occupied rule replaced by
			// "nearest" (S4 would join B at 2); any error log fails the test (unexpected Error).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* A = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), A && B)) { return; }

			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(0.0, 200.0, 0.0));
			if (S1 == nullptr || S2 == nullptr) { return; }
			TestTrue(TEXT("C2: S1 takes A"), EnemyReservationTest::ControllerOf(S1)->GetCurrentTarget() == A);
			TestTrue(TEXT("C2: S2 takes B (free)"), EnemyReservationTest::ControllerOf(S2)->GetCurrentTarget() == B);

			// Tie 1/1: S3 stands near B (460 u from B, 1560 u from A): the nearest of the two.
			ATestSeamEnemySoldier* S3 = SpawnChosenSoldier(FVector(4000.0, -200.0, 0.0));
			if (S3 == nullptr) { return; }
			TestTrue(TEXT("C2: tie 1/1, S3 joins the nearest (B)"), EnemyReservationTest::ControllerOf(S3)->GetCurrentTarget() == B);
			TestEqual(TEXT("C2: counts A=1"), Claims()->GetClaimCount(A), 1);
			TestEqual(TEXT("C2: counts B=2"), Claims()->GetClaimCount(B), 2);

			// Counts A=1, B=2: S4 stands right next to B but joins A, the least occupied, although far (about 1900 u).
			ATestSeamEnemySoldier* S4 = SpawnChosenSoldier(FVector(4300.0, 300.0, 0.0));
			if (S4 == nullptr) { return; }
			TestTrue(TEXT("C2: counts 2 and 1, S4 joins the one at 1 (A), even farther"), EnemyReservationTest::ControllerOf(S4)->GetCurrentTarget() == A);
			TestEqual(TEXT("C2: counts A=2"), Claims()->GetClaimCount(A), 2);
			TestEqual(TEXT("C2: counts B=2"), Claims()->GetClaimCount(B), 2);

			// Tie 2/2 again, S5 near A: the nearest is A (the tie-break works in both directions).
			ATestSeamEnemySoldier* S5 = SpawnChosenSoldier(FVector(2000.0, 600.0, 0.0));
			if (S5 == nullptr) { return; }
			TestTrue(TEXT("C2: tie 2/2, S5 joins the nearest (A)"), EnemyReservationTest::ControllerOf(S5)->GetCurrentTarget() == A);
			TestEqual(TEXT("C2: counts A=3"), Claims()->GetClaimCount(A), 3);
			TestEqual(TEXT("C2: counts B=2"), Claims()->GetClaimCount(B), 2);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// C3: errors.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Errors (C3)", [this]()
	{
		It("C3: no living destructible in the zone: exactly one Error per soldier, no reservation", [this]()
		{
			// Mutation: a log per tick (count > 2), no log, or a claim created on a dead / out-of-zone target.
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 2);
			if (!TestNotNull(TEXT("harness: zone"), SpawnZone(FVector(500.0, 0.0, 0.0), FVector::OneVector))) { return; }
			ATestDestructible* Dead = SpawnTarget(FVector(500.0, 0.0, 0.0));
			ATestDestructible* Outside = SpawnTarget(FVector(3000.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), Dead && Outside)) { return; }
			EnemyTest::KillActor(Dead);

			ATestSeamEnemySoldier* S1 = SpawnSeamSoldier(FVector(0.0, 0.0, 0.0));
			ATestSeamEnemySoldier* S2 = SpawnSeamSoldier(FVector(0.0, 3000.0, 0.0));
			if (S1 == nullptr || S2 == nullptr) { return; }

			TickFor(13.0); // 260 ticks: one error per soldier, not one per tick
			TestNull(TEXT("C3: S1 has no target"), EnemyReservationTest::ControllerOf(S1)->GetCurrentTarget());
			TestNull(TEXT("C3: S2 has no target"), EnemyReservationTest::ControllerOf(S2)->GetCurrentTarget());
			TestEqual(TEXT("C3: no claim on the dead destructible"), Claims()->GetClaimCount(Dead), 0);
			TestEqual(TEXT("C3: no claim on the out-of-zone destructible"), Claims()->GetClaimCount(Outside), 0);
		});

		It("C3: without any registered zone: one Error, no reservation on a living destructible next to the soldier", [this]()
		{
			// Mutation: the soldier falls back on any destructible when there is no zone (a claim appears).
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoTarget, EAutomationExpectedErrorFlags::Contains, 1);
			ATestDestructible* Target = SpawnTarget(FVector(500.0, 0.0, 0.0));
			ATestSeamEnemySoldier* Soldier = SpawnSeamSoldier(FVector::ZeroVector);
			if (Target == nullptr || Soldier == nullptr) { return; }

			TickFor(5.0);
			TestNull(TEXT("C3: no target"), EnemyReservationTest::ControllerOf(Soldier)->GetCurrentTarget());
			TestEqual(TEXT("C3: no claim"), Claims()->GetClaimCount(Target), 0);
		});

		It("C3: all targets reserved is normal: three soldiers on one target, no Error", [this]()
		{
			// Mutation: "all reserved" treated as "no target" (an Error is logged, which fails the test, and S2/S3 get no target).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* Only = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			if (Only == nullptr) { return; }

			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(0.0, 200.0, 0.0));
			ATestSeamEnemySoldier* S3 = SpawnChosenSoldier(FVector(0.0, -200.0, 0.0));
			if (S1 == nullptr || S2 == nullptr || S3 == nullptr) { return; }
			TickFor(2.0);
			TestEqual(TEXT("C3: the three soldiers share the only target"), Claims()->GetClaimCount(Only), 3);
			TestTrue(TEXT("C3: S2 on it"), EnemyReservationTest::ControllerOf(S2)->GetCurrentTarget() == Only);
			TestTrue(TEXT("C3: S3 on it"), EnemyReservationTest::ControllerOf(S3)->GetCurrentTarget() == Only);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// C4, C5, C6: release and re-choice.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Release (C4, C5, C6)", [this]()
	{
		// A target A (2500) held by S1, B (4500) held by S2. S1 ends, then a newcomer standing next to B must take the freed A.
		auto ReleaseOnSoldierEnd = [this](const TCHAR* Case, TFunctionRef<void(AActor*)> EndSoldier)
		{
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* A = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), A && B)) { return; }
			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(0.0, 200.0, 0.0));
			if (S1 == nullptr || S2 == nullptr) { return; }
			if (!TestTrue(*FString::Printf(TEXT("%s precondition: 1 claim on A and 1 on B"), Case), Claims()->GetClaimCount(A) == 1 && Claims()->GetClaimCount(B) == 1)) { return; }

			EndSoldier(S1);
			const bool bReleased = TickUntil([this, A]() { return Claims()->GetClaimCount(A) == 0; }, 10.0);
			TestTrue(*FString::Printf(TEXT("%s: the claim of the ended soldier is released (A back to 0)"), Case), bReleased);
			TestEqual(*FString::Printf(TEXT("%s: the other soldier's claim is untouched"), Case), Claims()->GetClaimCount(B), 1);

			// The newcomer stands 460 u from B and ~1940 u from A: it only goes to A if A counts as free again.
			ATestSeamEnemySoldier* S3 = SpawnChosenSoldier(FVector(4000.0, -200.0, 0.0));
			if (S3 == nullptr) { return; }
			TestTrue(*FString::Printf(TEXT("%s: a new soldier takes the released target as a free one"), Case),
				EnemyReservationTest::ControllerOf(S3)->GetCurrentTarget() == A);
			TestEqual(*FString::Printf(TEXT("%s: A has exactly one claim again"), Case), Claims()->GetClaimCount(A), 1);
		};

		It("C4: a destroyed soldier releases its reservation", [this, ReleaseOnSoldierEnd]()
		{
			// What this checks: the observable effect (the target is free again and a newcomer picks it as a free one).
			// What it does NOT detect: removing the explicit ReleaseClaim from OnUnPossess/EndPlay. Destroying a pawn also destroys its
			// AIController (AController::PawnPendingDestroy), a destroyed actor is invalid at once, so the weak key of the table is
			// dead and GetClaimCount/PickTarget skip it with no release at all. The explicit release is covered by the
			// "controller survives its pawn" test below.
			// Mutation (honest): the table counts claimants that are no longer valid (e.g. a strong key, or no validity check in
			// GetClaimCount/PickTarget): A stays at 1 and S3 goes to B.
			ReleaseOnSoldierEnd(TEXT("C4 destroy"), [](AActor* Soldier) { Soldier->Destroy(); });
		});

		It("C4: a killed soldier releases its reservation", [this, ReleaseOnSoldierEnd]()
		{
			// Mutation (honest): a dead soldier is never ended (death neither destroys nor unpossesses it) while its claim keeps
			// counting; or the validity check of the table is missing (same remark as the destroy case: the explicit ReleaseClaim
			// is not what this detects when the controller dies with the pawn).
			ReleaseOnSoldierEnd(TEXT("C4 death"), [](AActor* Soldier) { EnemyTest::KillActor(Soldier); });
		});

		It("C4/C12: a controller that survives its pawn (UnPossess of a living soldier) releases the claim explicitly", [this, ReleaseOnSoldierEnd]()
		{
			// Here the controller stays valid, so its weak key cannot save the day: only an explicit ReleaseClaim can bring A back to 0.
			// Mutation: ReleaseClaim removed from OnUnPossess (A stays at 1 because the controller is alive, the newcomer S3 sees A as
			// occupied and joins B, the nearest on the 1/1 tie), or the controller re-claiming a target while it has no pawn.
			ReleaseOnSoldierEnd(TEXT("C4 unpossess"), [this](AActor* Soldier)
			{
				const APawn* Pawn = Cast<APawn>(Soldier);
				AController* Controller = (Pawn != nullptr) ? Pawn->GetController() : nullptr;
				if (!TestNotNull(TEXT("harness: the soldier has a controller"), Controller)) { return; }
				Controller->UnPossess();
				TestTrue(TEXT("precondition: the controller is still alive"), IsValid(Controller));
				TestTrue(TEXT("precondition: the soldier is still alive"), IsValid(Soldier));
				TestNull(TEXT("precondition: the controller no longer has a pawn"), Controller->GetPawn());
			});
		});

		// Three targets in the zone, T1 held by S1, T2 held by S2; T3 free. When T1 ends, S1 must go to the FREE T3, not to the nearer T2.
		auto RetargetOnTargetEnd = [this](const TCHAR* Case, TFunctionRef<void(ATestDestructible*)> EndTarget, bool bTargetStillExists)
		{
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* T1 = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* T2 = SpawnTarget(FVector(2500.0, -500.0, 0.0));
			ATestDestructible* T3 = SpawnTarget(FVector(2500.0, 900.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), T1 && T2 && T3)) { return; }
			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(0.0, -300.0, 0.0));
			if (S1 == nullptr || S2 == nullptr) { return; }
			const AEnemyAIController* C1 = EnemyReservationTest::ControllerOf(S1);
			if (!TestTrue(*FString::Printf(TEXT("%s precondition: S1 on T1, S2 on T2"), Case),
				C1->GetCurrentTarget() == T1 && EnemyReservationTest::ControllerOf(S2)->GetCurrentTarget() == T2)) { return; }

			EndTarget(T1);
			const bool bMoved = TickUntil([C1, T3]() { return C1->GetCurrentTarget() == T3; }, MaxWaitTime);
			TestTrue(*FString::Printf(TEXT("%s: S1 re-chose the free T3, not the nearer but reserved T2"), Case), bMoved);
			if (bTargetStillExists)
			{
				TestEqual(*FString::Printf(TEXT("%s: the old target (dead, still present) has no claim left"), Case), Claims()->GetClaimCount(T1), 0);
			}
			TestEqual(*FString::Printf(TEXT("%s: T2 still has S2 only"), Case), Claims()->GetClaimCount(T2), 1);
			TestEqual(*FString::Printf(TEXT("%s: T3 has exactly one claim"), Case), Claims()->GetClaimCount(T3), 1);
		};

		It("C5: when the target is killed (dead but still in the world) the soldier re-chooses with the reservations", [this, RetargetOnTargetEnd]()
		{
			// Mutation: the claim on the dead target is not released at re-choice (T1 stays at 1), or the re-choice ignores the
			// reservations (S1 would take T2, nearer, and T2 would have 2).
			RetargetOnTargetEnd(TEXT("C5 kill"), [](ATestDestructible* Target) { EnemyTest::KillActor(Target); }, true);
		});

		It("C5: when the target is destroyed the soldier re-chooses with the reservations", [this, RetargetOnTargetEnd]()
		{
			// Mutation: a destroyed target (invalid weak pointer) is not detected and the soldier stays on nothing, or the re-choice
			// ignores the reservations.
			RetargetOnTargetEnd(TEXT("C5 destroy"), [](ATestDestructible* Target) { Target->Destroy(); }, false);
		});

		It("C6: the sum of the claims of the destructibles equals the soldiers in Engage that have a target, at every stage", [this]()
		{
			// Mutation (honest): a re-choice after a target KILL that adds a claim without replacing the old one (the killed target is
			// still valid, so its stale claim is counted: sum 4), or a table keyed per target instead of per controller. The soldier and
			// target DESTRUCTION steps check the effect only: invalid weak keys/targets are skipped even with no explicit release
			// (see the "controller survives its pawn" test for the explicit release).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			TArray<TWeakObjectPtr<ATestDestructible>> Targets;
			for (const FVector& Location : { FVector(2500.0, 0.0, 0.0), FVector(2500.0, -500.0, 0.0), FVector(2500.0, 500.0, 0.0), FVector(4500.0, 0.0, 0.0) })
			{
				ATestDestructible* Target = SpawnTarget(Location);
				if (!TestNotNull(TEXT("harness: target"), Target)) { return; }
				Targets.Add(Target);
			}
			auto SumOfClaims = [this, &Targets]()
			{
				int32 Sum = 0;
				for (const TWeakObjectPtr<ATestDestructible>& Target : Targets)
				{
					if (Target.IsValid()) { Sum += Claims()->GetClaimCount(Target.Get()); }
				}
				return Sum;
			};

			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(0.0, 200.0, 0.0));
			ATestSeamEnemySoldier* S3 = SpawnChosenSoldier(FVector(0.0, -200.0, 0.0));
			if (S1 == nullptr || S2 == nullptr || S3 == nullptr) { return; }
			const AEnemyAIController* C1 = EnemyReservationTest::ControllerOf(S1);
			const AEnemyAIController* C3 = EnemyReservationTest::ControllerOf(S3);
			TestEqual(TEXT("C6: 3 soldiers with a target -> sum 3"), SumOfClaims(), 3);

			// A target dies: its holder re-chooses, the sum stays at 3.
			ATestDestructible* Held = Cast<ATestDestructible>(C1->GetCurrentTarget());
			if (!TestNotNull(TEXT("harness: S1 target"), Held)) { return; }
			EnemyTest::KillActor(Held);
			TestTrue(TEXT("C6: S1 re-chose"), TickUntil([C1, Held]() { return C1->GetCurrentTarget() != nullptr && C1->GetCurrentTarget() != Held; }, MaxWaitTime));
			TestEqual(TEXT("C6: after a target kill, sum still 3"), SumOfClaims(), 3);

			// A soldier is destroyed: sum 2.
			S2->Destroy();
			TestTrue(TEXT("C6: the destroyed soldier's claim disappears"), TickUntil([&SumOfClaims]() { return SumOfClaims() == 2; }, 10.0));

			// A target is destroyed: its holder re-chooses, the sum stays at 2.
			ATestDestructible* Held3 = Cast<ATestDestructible>(C3->GetCurrentTarget());
			if (!TestNotNull(TEXT("harness: S3 target"), Held3)) { return; }
			Held3->Destroy();
			TestTrue(TEXT("C6: S3 re-chose"), TickUntil([C3, Held3]() { return C3->GetCurrentTarget() != nullptr && C3->GetCurrentTarget() != Held3; }, MaxWaitTime));
			TestEqual(TEXT("C6: after a target destruction, sum still 2"), SumOfClaims(), 2);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// C7, C8: move requests (test seam).
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Move request (C7, C8)", [this]()
	{
		It("C7: exactly one request per chosen target, towards it, none over the next 100 ticks", [this]()
		{
			// Mutation: the request is renewed each tick (or each re-evaluation), or sent to a position/another actor, or sent twice
			// when the target is chosen (e.g. in OnPossess and in Tick).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* First = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* Second = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), First && Second)) { return; }

			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector(0.0, 0.0, 0.0));
			if (Soldier == nullptr) { return; }
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			TickFor(TickStep); // the request may be issued on the tick that follows the choice
			if (!TestEqual(TEXT("C7: one request for the chosen target"), Seam->RequestedTargets.Num(), 1)) { return; }
			TestTrue(TEXT("C7: towards the chosen target"), Seam->RequestedTargets[0].Get() == First);

			TickFor(5.0); // 100 ticks
			TestEqual(TEXT("C7: no further request over 100 ticks"), Seam->RequestedTargets.Num(), 1);

			// A new target (the first one dies): exactly one new request, towards the new target.
			EnemyTest::KillActor(First);
			TestTrue(TEXT("C7: the soldier moves on to the second target"), TickUntil([Seam, Second]() { return Seam->GetCurrentTarget() == Second; }, MaxWaitTime));
			TickFor(TickStep);
			TestEqual(TEXT("C7: one more request after the re-choice"), Seam->RequestedTargets.Num(), 2);
			TestTrue(TEXT("C7: towards the new target"), Seam->RequestedTargets.Num() == 2 && Seam->RequestedTargets[1].Get() == Second);
			TickFor(5.0);
			TestEqual(TEXT("C7: and none over the next 100 ticks"), Seam->RequestedTargets.Num(), 2);
		});

		It("C8: a soldier already in attack range at the choice attacks without any move request", [this]()
		{
			// Mutation: the move request is sent before the range test (RequestedTargets not empty), or the attack never starts when
			// the soldier is already in range (the target keeps full health).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnZone(FVector(130.0, 0.0, 0.0), FVector::OneVector))) { return; }
			// Soldier spline ends at X = 60; the target box starts at X = 80: 20 u of gap, then the capsule radius is subtracted.
			ATestDestructible* Target = SpawnTarget(FVector(130.0, 0.0, 0.0));
			if (Target == nullptr) { return; }

			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }

			const bool bAttacked = TickUntil([Target]() { return Target->GetHealthPercent() < 1.0f; }, MaxWaitTime);
			TestTrue(TEXT("C8: the soldier in range attacked (the target lost health)"), bAttacked);
			TestEqual(TEXT("C8: no move request was sent"), Seam->RequestedTargets.Num(), 0);
			TestEqual(TEXT("C8: the target stays reserved while attacked"), Claims()->GetClaimCount(Target), 1);
		});

		It("C8: a soldier that comes into range after a move request stops asking and attacks", [this]()
		{
			// Mutation: the attack test is only evaluated at the choice, not at each Tick (the soldier teleported in range would
			// never attack), or a new request is issued once in range.
			// Not observable here: that StopMovement is called (the seam replaces the real path following).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* Target = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			if (Target == nullptr) { return; }

			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			TickFor(TickStep);
			if (!TestEqual(TEXT("precondition: one request was sent, far from the target"), Seam->RequestedTargets.Num(), 1)) { return; }
			TestEqual(TEXT("precondition: the target is intact while the soldier is far"), Target->GetHealthPercent(), 1.0f);

			// The seam never moves the soldier: put it right next to the target, as the real move would have done.
			Soldier->SetActorLocation(FVector(2400.0, 0.0, 0.0));
			TestTrue(TEXT("C8: once in range, the soldier attacks"), TickUntil([Target]() { return Target->GetHealthPercent() < 1.0f; }, MaxWaitTime));
			TestEqual(TEXT("C8: no further request once in range"), Seam->RequestedTargets.Num(), 1);
			TestTrue(TEXT("C8: the soldier keeps its target"), Seam->GetCurrentTarget() == Target);
		});

		// Not automated: "a move request in progress is stopped when the soldier enters the range" (C8, second half). The seam
		// replaces the real path following, so StopMovement has nothing to stop; verify in PIE (a soldier walking to a target
		// must stop walking when it starts attacking).

		It("C11: a soldier pushed out of range with no move in progress asks again for the same target, once, without changing target or claim", [this]()
		{
			// Mutation: the request is conditioned on "never requested for this target" (no second request: the soldier stays idle out
			// of range), or the relaunch re-chooses (the soldier is put nearer to B than to A, so a re-choice would switch to B), or
			// the relaunch is repeated every tick (more than 2 requests over the 100 ticks), or releases/re-takes the claim.
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* A = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), A && B)) { return; }

			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			if (Soldier == nullptr) { return; }
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			if (!TestTrue(TEXT("precondition: the soldier chose A"), Seam->GetCurrentTarget() == A)) { return; }
			TickFor(TickStep);
			if (!TestEqual(TEXT("precondition: one request so far"), Seam->RequestedTargets.Num(), 1)) { return; }

			// Arrival: put the soldier next to A and wait for its first hit, which proves the arrival was registered (no move in progress).
			Soldier->SetActorLocation(FVector(2400.0, 0.0, 0.0));
			if (!TestTrue(TEXT("precondition: the soldier arrived and attacks"), TickUntil([A]() { return A->GetHealthPercent() < 1.0f; }, MaxWaitTime))) { return; }
			if (!TestEqual(TEXT("precondition: no request while in range"), Seam->RequestedTargets.Num(), 1)) { return; }

			// Pushed out of range, nearer to B (800 u) than to A (1200 u).
			Soldier->SetActorLocation(FVector(3700.0, 0.0, 0.0));
			if (!TestTrue(TEXT("C11: a new request is sent"), TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 2; }, 2.0))) { return; }
			TestEqual(TEXT("C11: exactly one new request"), Seam->RequestedTargets.Num(), 2);
			TestTrue(TEXT("C11: towards the same target (A)"), Seam->RequestedTargets[1].Get() == A);
			TestTrue(TEXT("C11: the target did not change (not a re-choice, B is nearer)"), Seam->GetCurrentTarget() == A);
			TestEqual(TEXT("C11: A is still reserved once"), Claims()->GetClaimCount(A), 1);
			TestEqual(TEXT("C11: B is still free"), Claims()->GetClaimCount(B), 0);

			TickFor(5.0); // 100 ticks, the seam never completes the move: it is still in progress
			TestEqual(TEXT("C11: no further request while the relaunched move is in progress"), Seam->RequestedTargets.Num(), 2);
			TestTrue(TEXT("C11: target unchanged after 100 ticks"), Seam->GetCurrentTarget() == A);
			TestEqual(TEXT("C11: claim unchanged after 100 ticks"), Claims()->GetClaimCount(A), 1);
		});

		It("C13: a soldier whose attack timer is running deals no damage while pushed out of range, hits resume on return; a soldier left in range keeps hitting", [this]()
		{
			// Mutation 1: HandleAttackTimer has no range check (target A loses health while S1 is out of range -> red).
			// Mutation 2: the range check also cuts the hits in range, or cancels the timer for good (control target B stops losing
			// health, or A never loses health again after the return -> red).
			// Durations come from the soldier's own GetAttackInterval() (default 1.5 s); the targets keep their default 100 health with
			// 10 damage per hit by default: the test spans about 4 intervals plus a few more, far from killing a target (a dead target
			// would end the attack and make later checks meaningless, hence the guards below). If AttackDamage is raised a lot or the
			// target health lowered, the "alive" preconditions fail loudly instead of passing silently.
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* A = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), A && B)) { return; }

			// S1 chooses A (nearest free); S2 then takes B (the only free one) and stands next to it, in range.
			ATestSeamEnemySoldier* S1 = SpawnChosenSoldier(FVector::ZeroVector);
			if (S1 == nullptr) { return; }
			ATestSeamEnemySoldier* S2 = SpawnChosenSoldier(FVector(4400.0, 0.0, 0.0));
			if (S2 == nullptr) { return; }
			const AEnemyAIController* Controller1 = EnemyReservationTest::ControllerOf(S1);
			const AEnemyAIController* Controller2 = EnemyReservationTest::ControllerOf(S2);
			if (!TestTrue(TEXT("harness: S1 targets A and S2 targets B"),
				Controller1 && Controller2 && Controller1->GetCurrentTarget() == A && Controller2->GetCurrentTarget() == B)) { return; }

			const double Interval = S1->GetAttackInterval();
			if (!TestTrue(TEXT("harness: a usable attack interval"), Interval > 0.1)) { return; }
			const double HitWindow = Interval + 0.25; // one interval plus tick granularity and margin

			// S1 arrives in range of A; both attack timers are running once each target took a first hit.
			S1->SetActorLocation(FVector(2400.0, 0.0, 0.0));
			if (!TestTrue(TEXT("precondition: both soldiers attack"),
				TickUntil([A, B]() { return A->GetHealthPercent() < 1.0f && B->GetHealthPercent() < 1.0f; }, MaxWaitTime))) { return; }

			// No tick ran since the hits were observed: the next hit of S1 is one interval away. Push S1 out of range.
			S1->SetActorLocation(FVector(3700.0, 0.0, 0.0));
			const float HealthA = A->GetHealthPercent();
			const float HealthB = B->GetHealthPercent();
			if (!TestTrue(TEXT("precondition: targets still alive"), HealthA > 0.5f && HealthB > 0.5f)) { return; }

			TickFor(4.0 * Interval); // four attack intervals of world time
			TestEqual(TEXT("C13: no damage on A while S1 is out of range"), A->GetHealthPercent(), HealthA);
			// Four intervals contain at least three hits of a soldier that stays in range (the exact count depends on the timer phase).
			TestTrue(TEXT("C13: the soldier left in range kept hitting B at each interval (at least 3 hits in 4 intervals)"),
				B->GetHealthPercent() <= HealthB - 0.3f + KINDA_SMALL_NUMBER);
			if (!TestTrue(TEXT("precondition: B survived the control window"), !B->IsDead())) { return; }

			// Back in range: hits resume, then keep coming at the normal interval.
			S1->SetActorLocation(FVector(2400.0, 0.0, 0.0));
			const bool bResumed = TickUntil([A, HealthA]() { return A->GetHealthPercent() < HealthA; }, HitWindow);
			if (!TestTrue(TEXT("C13: hits resume within one interval of the return to range"), bResumed)) { return; }
			const float HealthAfterResume = A->GetHealthPercent();
			TestTrue(TEXT("C13: and continue at the normal interval"),
				TickUntil([A, HealthAfterResume]() { return A->GetHealthPercent() < HealthAfterResume; }, HitWindow));
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// C9: unreachable target.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Unreachable target (C9)", [this]()
	{
		It("C9: production controller without NavMesh: the request fails, claim released, target excluded, one Error, retries over time", [this]()
		{
			// Mutation: the claim is kept after the failure (checked every tick), a straight-line fallback is used (the soldier
			// would keep a target), the error is logged at each retry (expected exactly 1 over several retry cycles), or the failed
			// target is never retried.
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoReachableTarget, EAutomationExpectedErrorFlags::Contains, 1);
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* Target = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			if (Target == nullptr) { return; }
			ATestEnemySoldier* Soldier = SpawnSoldier(FVector::ZeroVector, ATestEnemySoldier::StaticClass()); // production controller
			if (Soldier == nullptr) { return; }
			const AEnemyAIController* Controller = EnemyReservationTest::ControllerOf(Soldier);
			if (!TestNotNull(TEXT("harness: production controller"), Controller)) { return; }
			double Delay = 0.0;
			if (!TestTrue(TEXT("harness: UnreachableRetryDelay is readable"), EnemyTest::ReadNumber(Controller, TEXT("UnreachableRetryDelay"), Delay))) { return; }
			if (!TestTrue(TEXT("harness: the delay is positive"), Delay > 0.0)) { return; }

			// Three and a half delays: several retry cycles. Every tick, the failed attempt must have left no claim and no target.
			const int32 Steps = FMath::CeilToInt32(3.5 * Delay / TickStep);
			bool bClaimLeaked = false;
			bool bTargetKept = false;
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Wrapper->TickTestWorld(TickStep);
				bClaimLeaked |= (Claims()->GetClaimCount(Target) != 0);
				bTargetKept |= (Controller->GetCurrentTarget() != nullptr);
			}
			TestFalse(TEXT("C9: the claim is released as soon as the move fails (at every tick)"), bClaimLeaked);
			TestFalse(TEXT("C9: the soldier does not keep a target it cannot path to (no straight-line fallback)"), bTargetKept);
			TestTrue(TEXT("C9: the soldier is still alive and waiting"), IsValid(Soldier));
		});

		It("C9: with the seam failing, the failed target is excluded for UnreachableRetryDelay (world time), then retried; Error logged once", [this]()
		{
			// Mutation: no exclusion (the failed target is re-requested at once, many requests), exclusion never expiring (no second
			// attempt), exclusion shorter than the delay, the other target not tried, or the Error logged per cycle.
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoReachableTarget, EAutomationExpectedErrorFlags::Contains, 1);
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* A = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* B = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), A && B)) { return; }
			ATestSeamEnemySoldier* Soldier = SpawnSeamSoldier(FVector::ZeroVector);
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			Seam->NextResult = EPathFollowingRequestResult::Failed; // before the first choice: the soldier is still on its spline
			double Delay = 0.0;
			if (!TestTrue(TEXT("harness: UnreachableRetryDelay is readable"), EnemyTest::ReadNumber(Seam, TEXT("UnreachableRetryDelay"), Delay))) { return; }

			if (!TestTrue(TEXT("C9: A (nearest) is requested first"), TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 1; }, MaxWaitTime))) { return; }
			const double FirstRequestTime = World->GetTimeSeconds();
			TestTrue(TEXT("C9: the first request goes to A"), Seam->RequestedTargets[0].Get() == A);
			TestTrue(TEXT("C9: A failed, B (the other target) is tried next"), TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 2; }, MaxWaitTime));
			TestTrue(TEXT("C9: the second request goes to B, not back to the excluded A"), Seam->RequestedTargets.Num() >= 2 && Seam->RequestedTargets[1].Get() == B);

			// Everything excluded: the soldier waits, holding no claim and no target, and sends nothing for most of the delay.
			TickFor(0.5 * Delay);
			TestEqual(TEXT("C9: no request while every target is excluded"), Seam->RequestedTargets.Num(), 2);
			TestNull(TEXT("C9: the waiting soldier has no target"), Seam->GetCurrentTarget());
			TestEqual(TEXT("C9: A released"), Claims()->GetClaimCount(A), 0);
			TestEqual(TEXT("C9: B released"), Claims()->GetClaimCount(B), 0);

			// First retry: A again (its exclusion started first), not before the delay and not long after it.
			const bool bRetried = TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 3; }, Delay + 2.0);
			const double Elapsed = World->GetTimeSeconds() - FirstRequestTime;
			TestTrue(TEXT("C9: a new attempt happens after the delay"), bRetried);
			TestTrue(*FString::Printf(TEXT("C9: not before the delay (elapsed %.2f s for a delay of %.2f s)"), Elapsed, Delay), Elapsed >= Delay - TickStep);
			TestTrue(*FString::Printf(TEXT("C9: not much later than the delay (elapsed %.2f s)"), Elapsed), Elapsed <= Delay + 1.0);
			TestTrue(TEXT("C9: the retry goes to A (nearest, first to expire)"), Seam->RequestedTargets.Num() >= 3 && Seam->RequestedTargets[2].Get() == A);
		});

		It("C9: a request answered AlreadyAtGoal while out of attack range is a failure too", [this]()
		{
			// Mutation: AlreadyAtGoal treated as success (the soldier keeps its target and claim without moving, which would block
			// the end of a wave, instead of excluding the target).
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoReachableTarget, EAutomationExpectedErrorFlags::Contains, 1);
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* Target = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			if (Target == nullptr) { return; }
			ATestSeamEnemySoldier* Soldier = SpawnSeamSoldier(FVector::ZeroVector);
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			Seam->NextResult = EPathFollowingRequestResult::AlreadyAtGoal;

			if (!TestTrue(TEXT("C9: a request was sent"), TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 1; }, MaxWaitTime))) { return; }
			TickFor(1.0);
			TestNull(TEXT("C9: the soldier does not keep a target it was told it is already at"), Seam->GetCurrentTarget());
			TestEqual(TEXT("C9: the claim is released"), Claims()->GetClaimCount(Target), 0);
			TestEqual(TEXT("C9: only one request in that second (target excluded)"), Seam->RequestedTargets.Num(), 1);
		});

		It("C12: a move end reported synchronously inside the request (Blocked) leaves one exclusion, no claim and a single Error", [this]()
		{
			// Mutation: the controller writes its state (target, claim, "move requested") AFTER calling RequestMoveToTarget instead of
			// before. The synchronous OnMoveCompleted then runs against a state that does not exist yet (it is ignored as "not
			// awaited", or finds no target to exclude), and the late write resurrects target + claim + "moving": the target stays
			// reserved and is not excluded, so the claim count is 1 and the soldier holds a target it never reaches. Also fails if
			// the exclusion is applied twice/never, or the Error is logged per attempt.
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoReachableTarget, EAutomationExpectedErrorFlags::Contains, 1);
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* Target = SpawnTarget(FVector(2500.0, 0.0, 0.0)); // the only candidate
			if (Target == nullptr) { return; }
			ATestSyncBlockedEnemySoldier* Soldier = Cast<ATestSyncBlockedEnemySoldier>(
				SpawnSoldier(FVector::ZeroVector, ATestSyncBlockedEnemySoldier::StaticClass()));
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: synchronous seam controller"), Seam)) { return; }
			double Delay = 0.0;
			if (!TestTrue(TEXT("harness: UnreachableRetryDelay is readable"), EnemyTest::ReadNumber(Seam, TEXT("UnreachableRetryDelay"), Delay))) { return; }
			if (!TestTrue(TEXT("harness: the delay is positive"), Delay > 0.0)) { return; }

			if (!TestTrue(TEXT("C12: the single target is requested"), TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 1; }, MaxWaitTime))) { return; }
			const double FirstRequestTime = World->GetTimeSeconds();
			TestNull(TEXT("C12: the soldier holds no target after the synchronous Blocked"), Seam->GetCurrentTarget());
			TestEqual(TEXT("C12: no reservation"), Claims()->GetClaimCount(Target), 0);

			// The only target is excluded: it is not chosen again before the delay (each tick would add a request otherwise).
			TickFor(0.5 * Delay);
			TestEqual(TEXT("C12: one request only, the target is excluded"), Seam->RequestedTargets.Num(), 1);
			TestNull(TEXT("C12: still no target while waiting"), Seam->GetCurrentTarget());
			TestEqual(TEXT("C12: still no reservation while waiting"), Claims()->GetClaimCount(Target), 0);

			// The exclusion is temporary: the target is tried again after the delay (and fails again, still one Error in total).
			const bool bRetried = TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 2; }, Delay + 2.0);
			const double Elapsed = World->GetTimeSeconds() - FirstRequestTime;
			TestTrue(TEXT("C12: a new attempt happens after the delay"), bRetried);
			TestTrue(*FString::Printf(TEXT("C12: not before the delay (elapsed %.2f s for a delay of %.2f s)"), Elapsed, Delay), Elapsed >= Delay - TickStep);
			TestEqual(TEXT("C12: no reservation after the retry either"), Claims()->GetClaimCount(Target), 0);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// Servant combat (docs/specs/servant-combat.md, criterion 10): a target that is a PAWN moves. A move that ends in Success out of
	// range does not make it unreachable (it has moved), and the soldier follows it by re-requesting at most once per RepathInterval.
	// A fixed destructible keeps the 3b behaviour: "no re-request" is already covered by C7 (none over 100 ticks, 5 s > RepathInterval)
	// and C11 (one re-request only after being pushed out of range).
	// The end of the move is reported by hand (OnMoveCompleted is public): the seam never completes a request by itself.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Moving target, servant combat (criterion 10)", [this]()
	{
		// Soldier chosen on the nearest target of the layout, with its first request recorded.
		auto EndMove = [](ATestEnemyAIController* Seam, EPathFollowingResult::Type Result)
		{
			Seam->OnMoveCompleted(FAIRequestID::InvalidRequest, FPathFollowingResult(Result, FPathFollowingResultFlags::None));
		};

		It("SC10: a move that ends in Success out of range on a servant does not exclude it: same target, same claim, re-requested once per RepathInterval", [this, EndMove]()
		{
			// Mutation 1: Success out of range still excludes the target 3 s whatever it is (the soldier switches to the farther
			// destructible D, the claim on the servant is released, the new request goes to D).
			// Mutation 2: the RepathInterval throttle (LastMoveRequestTime) is removed for pawn targets: a request is made at every tick
			// after the move ended, so the "no request before ~0.8 x RepathInterval" and the upper bound below fail.
			// Mutation 3: no re-request at all after the interval (the soldier stands still behind a moving servant).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			AServant* Servant = SpawnServantTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* Other = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), Servant && Other)) { return; }
			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			TickFor(TickStep);
			if (!TestTrue(TEXT("precondition: the servant (nearest) is the target"), Seam->GetCurrentTarget() == Servant)) { return; }
			if (!TestEqual(TEXT("precondition: one request, towards the servant"), Seam->RequestedTargets.Num(), 1)) { return; }

			double Interval = 0.0;
			if (!TestTrue(TEXT("harness: RepathInterval readable by reflection"), EnemyTest::ReadNumber(Seam, TEXT("RepathInterval"), Interval))) { return; }
			if (!TestTrue(TEXT("harness: RepathInterval long enough for the tick step"), Interval >= 10.0 * TickStep)) { return; }

			EndMove(Seam, EPathFollowingResult::Success); // still 2400 u away: the servant moved, the soldier did not arrive
			// The first request was one tick ago: the throttle must hold the next one back until RepathInterval after it.
			TickFor(0.7 * Interval);
			TestEqual(TEXT("SC10: no new request before ~0.8 x RepathInterval (throttled, not one per tick)"), Seam->RequestedTargets.Num(), 1);
			TestTrue(TEXT("SC10: the target is kept while waiting"), Seam->GetCurrentTarget() == Servant);
			TestEqual(TEXT("SC10: the claim is kept while waiting"), Claims()->GetClaimCount(Servant), 1);
			const bool bAsked = TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 2; }, 0.3 * Interval + 0.3);
			TestTrue(TEXT("SC10: a new request follows once RepathInterval has elapsed"), bAsked);
			TickFor(0.2 * Interval);
			TestEqual(TEXT("SC10: exactly one new request in the whole window (upper bound)"), Seam->RequestedTargets.Num(), 2);
			TestTrue(TEXT("SC10: the new request is towards the servant"), Seam->RequestedTargets.Num() >= 2 && Seam->RequestedTargets[1].Get() == Servant);
			TestTrue(TEXT("SC10: the target is still the servant"), Seam->GetCurrentTarget() == Servant);
			TestEqual(TEXT("SC10: the servant is still reserved"), Claims()->GetClaimCount(Servant), 1);
			TestEqual(TEXT("SC10: the other destructible stays free"), Claims()->GetClaimCount(Other), 0);
		});

		It("SC10: a move that ends in failure on a servant excludes it as before: the soldier switches to the other destructible", [this, EndMove]()
		{
			// Mutation: the exclusion is removed for pawns whatever the result (the soldier asks for the servant again instead of D).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			AServant* Servant = SpawnServantTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* Other = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), Servant && Other)) { return; }
			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			TickFor(TickStep);
			if (!TestTrue(TEXT("precondition: the servant is the target"), Seam->GetCurrentTarget() == Servant)) { return; }

			EndMove(Seam, EPathFollowingResult::Blocked);
			TestTrue(TEXT("SC10: the soldier moves on to the other destructible"), TickUntil([Seam, Other]() { return Seam->GetCurrentTarget() == Other; }, 1.0));
			TestEqual(TEXT("SC10: the claim on the servant is released"), Claims()->GetClaimCount(Servant), 0);
			TestEqual(TEXT("SC10: the other destructible is claimed"), Claims()->GetClaimCount(Other), 1);
			TestTrue(TEXT("SC10: the last request goes to the other destructible"), Seam->RequestedTargets.Num() >= 2 && Seam->RequestedTargets.Last().Get() == Other);
		});

		It("SC10: a move that ends in Success out of range on a FIXED destructible is still 'unreachable' (excluded)", [this, EndMove]()
		{
			// Mutation: the Success exception is applied to every target instead of pawns only (the fixed destructible is kept: the
			// soldier stays on N instead of moving on to F, and a blocked wave would never end).
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			ATestDestructible* Near = SpawnTarget(FVector(2500.0, 0.0, 0.0));
			ATestDestructible* Far = SpawnTarget(FVector(4500.0, 0.0, 0.0));
			if (!TestTrue(TEXT("harness: targets"), Near && Far)) { return; }
			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			TickFor(TickStep);
			if (!TestTrue(TEXT("precondition: the nearest is the target"), Seam->GetCurrentTarget() == Near)) { return; }

			EndMove(Seam, EPathFollowingResult::Success);
			TestTrue(TEXT("SC10: the soldier moves on to the other destructible"), TickUntil([Seam, Far]() { return Seam->GetCurrentTarget() == Far; }, 1.0));
			TestEqual(TEXT("SC10: the claim on the first one is released"), Claims()->GetClaimCount(Near), 0);
		});

		It("SC10: a servant target that moved is followed: a new request towards it after RepathInterval, not before, not every tick", [this]()
		{
			// Mutation: no re-request towards a pawn while a move is in progress (Num stays 1), a re-request every tick or without the
			// interval (Num > 1 before RepathInterval, or more than 1 + elapsed/RepathInterval), or an interval other than RepathInterval.
			if (!RequireWorld()) { return; }
			if (!TestNotNull(TEXT("harness: zone"), SpawnLayoutZone())) { return; }
			AServant* Servant = SpawnServantTarget(FVector(2500.0, 0.0, 0.0));
			if (Servant == nullptr) { return; }
			ATestSeamEnemySoldier* Soldier = SpawnChosenSoldier(FVector::ZeroVector);
			ATestEnemyAIController* Seam = EnemyReservationTest::SeamOf(Soldier);
			if (!TestNotNull(TEXT("harness: seam controller"), Seam)) { return; }
			double Interval = 0.0;
			if (!TestTrue(TEXT("harness: RepathInterval readable by reflection"), EnemyTest::ReadNumber(Seam, TEXT("RepathInterval"), Interval))) { return; }
			if (!TestTrue(TEXT("harness: RepathInterval is long enough for the tick step"), Interval >= 8.0 * TickStep)) { return; }
			TickFor(TickStep);
			if (!TestTrue(TEXT("precondition: one request, towards the servant"), Seam->RequestedTargets.Num() == 1 && Seam->RequestedTargets[0].Get() == Servant)) { return; }
			const double FirstRequestTime = World->GetTimeSeconds();

			// The servant walks away by more than any attack range, still inside the zone.
			Servant->SetActorLocation(FVector(3300.0, 0.0, 0.0));
			TickFor(0.8 * Interval);
			TestEqual(TEXT("SC10: no new request before RepathInterval"), Seam->RequestedTargets.Num(), 1);
			const bool bFollowed = TickUntil([Seam]() { return Seam->RequestedTargets.Num() >= 2; }, 0.2 * Interval + 0.3);
			const double Elapsed = World->GetTimeSeconds() - FirstRequestTime;
			TestTrue(TEXT("SC10: a new request is sent after RepathInterval"), bFollowed);
			TestTrue(*FString::Printf(TEXT("SC10: not before the interval (elapsed %.2f s for %.2f s)"), Elapsed, Interval), Elapsed >= Interval - 2.0 * TickStep);
			TestTrue(TEXT("SC10: towards the servant"), Seam->RequestedTargets.Num() >= 2 && Seam->RequestedTargets[1].Get() == Servant);
			TestTrue(TEXT("SC10: the target and its claim did not change"), Seam->GetCurrentTarget() == Servant && Claims()->GetClaimCount(Servant) == 1);

			// Never faster than once per interval: over 2.5 intervals at most two more requests.
			TickFor(2.5 * Interval);
			TestTrue(TEXT("SC10: at most one request per RepathInterval"), Seam->RequestedTargets.Num() <= 4);
		});
	});

	// ------------------------------------------------------------------------------------------------------------------------
	// C10: CDO.
	// ------------------------------------------------------------------------------------------------------------------------
	Describe("Soldier defaults (C10)", [this]()
	{
		It("C10: AEnemySoldier uses acceleration for paths", [this]()
		{
			// Mutation: the line is removed from the soldier constructor (default false: sliding without walk cycle).
			const AEnemySoldier* Default = GetDefault<AEnemySoldier>();
			if (!TestNotNull(TEXT("harness: CDO"), Default)) { return; }
			UCharacterMovementComponent* Movement = Default->GetCharacterMovement();
			if (!TestNotNull(TEXT("harness: CDO movement component"), Movement)) { return; }
			TestTrue(TEXT("C10: bUseAccelerationForPaths"), Movement->GetNavMovementProperties()->bUseAccelerationForPaths);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
