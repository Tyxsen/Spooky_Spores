// Automation Spec for the servant wandering cycle inside a minimal test world.
// Source of truth: the servant wandering spec (docs/specs/2.2c-servant-wandering.md, removed at merge), criteria A3-A9,
// plus the "Comportement" timing rules (first decision after InitialDecisionDelay, OnUnPossess) and the "several servants" edge case.
//
// World: FTestWorldWrapper (Engine, Tests/AutomationCommon.h) creates a transient Game world with a GameInstance and a GameMode.
// - Game world => APawn::PostInitializeComponents auto-possesses a spawned pawn whose AutoPossessAI is PlacedInWorldOrSpawned
//   (World->IsGameWorld() is true, World->bStartup is false after BeginPlay).
// - TickTestWorld(DeltaTime) runs UWorld::Tick(LEVELTICK_All), which ticks the FTimerManager, and increments GFrameCounter so the
//   timer manager accepts one tick per call. Time therefore advances only through TickTestWorld.
// - No NavMesh on purpose: every ProjectPointToNavigation fails, which is the "projection failed" case of the spec.
// The servant's CharacterMovement tick is disabled by the tests: there is no floor, and gravity would otherwise move the pawn,
// which would make "position unchanged" (A7) meaningless. The logic under test is timer driven and does not rely on that tick.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantWanderTestHelpers.h"
#include "Tests/BaseZoneTestHelpers.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "Base/BaseZone.h"
#include "Base/BaseZoneSubsystem.h"
#include "Spooky_Spores.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Templates/Function.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FServantWanderWorldSpec, "SpookySpores.Servants.Wander.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	// Simulated frame length. Small compared with every delay of the spec, so that "one decision per frame" would show up
	// as dozens of decisions inside a single RetryDelay window.
	static constexpr float TickStep = 0.05f;
	// Slack after a timer is due: one frame of pending-timer activation plus float accumulation.
	static constexpr double TimerSlack = 0.25;

	// Locations used by most tests. The zone has the ABaseZone default extent (500, 500, 200) and ignores height by default.
	const FVector ZoneCenter = FVector(0.0, 0.0, 0.0);
	const FVector OutsideLocation = FVector(5000.0, 0.0, 100.0);

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	// Tunable values read from the controller under test (spec names), so the tests follow the configured delays.
	double InitialDecisionDelay = 0.0;
	double RetryDelay = 0.0;
	int32 MaxFailedReturnAttempts = 0;

	bool RequireWorld();
	AServant* SpawnServant(const TCHAR* Name, const FVector& Location);
	ABaseZone* SpawnZone(const TCHAR* Name, const FVector& Location);
	AServantAIController* RequireController(AServant* Servant);
	bool ReadTimings(const AServantAIController* Controller);
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);
	void ExpectAction(const FString& What, EServantWanderAction Actual, EServantWanderAction Expected);

END_DEFINE_SPEC(FServantWanderWorldSpec)

bool FServantWanderWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

AServant* FServantWanderWorldSpec::SpawnServant(const TCHAR* Name, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AServant* Servant = World->SpawnActor<AServant>(AServant::StaticClass(), FTransform(Location), Params);
	if (Servant != nullptr && Servant->GetCharacterMovement() != nullptr)
	{
		// No floor in the test world: freeze physical movement (see file header).
		Servant->GetCharacterMovement()->SetComponentTickEnabled(false);
	}
	return Servant;
}

ABaseZone* FServantWanderWorldSpec::SpawnZone(const TCHAR* Name, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform(Location), Params);
}

AServantAIController* FServantWanderWorldSpec::RequireController(AServant* Servant)
{
	if (!TestNotNull(TEXT("servant spawned"), Servant)) { return nullptr; }
	AServantAIController* Controller = Cast<AServantAIController>(Servant->GetController());
	if (!TestNotNull(TEXT("servant is possessed by an AServantAIController right after SpawnActor"), Controller)) { return nullptr; }
	if (!ReadTimings(Controller)) { return nullptr; }
	return Controller;
}

bool FServantWanderWorldSpec::ReadTimings(const AServantAIController* Controller)
{
	double MaxFailed = 0.0;
	const bool bRead = ServantWanderTest::ReadNumber(Controller, TEXT("InitialDecisionDelay"), InitialDecisionDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("RetryDelay"), RetryDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MaxFailedReturnAttempts"), MaxFailed);
	if (!bRead)
	{
		AddError(TEXT("InitialDecisionDelay, RetryDelay or MaxFailedReturnAttempts not found by reflection (names from the spec)."));
		return false;
	}
	MaxFailedReturnAttempts = FMath::RoundToInt32(MaxFailed);
	// The "no decision per frame" windows need a RetryDelay that spans many frames.
	if (RetryDelay < 10.0 * TickStep || MaxFailedReturnAttempts < 1)
	{
		AddError(FString::Printf(TEXT("RetryDelay (%f) too short for these tests or MaxFailedReturnAttempts (%d) < 1."), RetryDelay, MaxFailedReturnAttempts));
		return false;
	}
	return true;
}

void FServantWanderWorldSpec::TickFor(double Seconds)
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(TickStep);
	}
}

bool FServantWanderWorldSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt32(MaxSeconds / TickStep));
	for (int32 Index = 0; Index < MaxSteps; ++Index)
	{
		if (Condition()) { return true; }
		Wrapper->TickTestWorld(TickStep);
	}
	return Condition();
}

void FServantWanderWorldSpec::ExpectAction(const FString& What, EServantWanderAction Actual, EServantWanderAction Expected)
{
	TestTrue(FString::Printf(TEXT("%s: expected %s, got %s"), *What, *ServantWanderTest::ActionName(Expected), *ServantWanderTest::ActionName(Actual)),
	         Actual == Expected);
}

void FServantWanderWorldSpec::Define()
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
			for (const FString& Message : Messages)
			{
				AddError(FString::Printf(TEXT("FTestWorldWrapper: %s"), *Message));
			}
		}
	});

	AfterEach([this]()
	{
		// The wrapper destructor ends play and destroys the world (servants, controllers, zones, timers).
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	Describe("Possession and start (A3)", [this]()
	{
		It("A3: a servant spawned by SpawnActor in a game world is possessed by an AServantAIController that is wandering", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantA3"), ZoneCenter);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }

			TestTrue(TEXT("A3: IsWandering() == true right after spawn"), Controller->IsWandering());
			TestTrue(TEXT("A3: the controller possesses this very servant"), Controller->GetPawn() == Servant);
		});

		It("First decision only after InitialDecisionDelay, so a zone spawned after the servant is seen", [this]()
		{
			if (!RequireWorld()) { return; }
			// Possession happens in PostInitializeComponents, before the zone exists (spec: the zone's BeginPlay may come later).
			AServant* Servant = SpawnServant(TEXT("ServantFirstDecision"), ZoneCenter + FVector(0, 0, 100));
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("ZoneFirstDecision"), ZoneCenter);
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }

			ExpectAction(TEXT("no decision is taken synchronously at possession"), Controller->GetLastAction(), EServantWanderAction::None);
			TickFor(0.8 * InitialDecisionDelay);
			ExpectAction(TEXT("still no decision before InitialDecisionDelay"), Controller->GetLastAction(), EServantWanderAction::None);

			const bool bDecided = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::None; },
			                                0.2 * InitialDecisionDelay + TimerSlack);
			TestTrue(TEXT("a first decision is taken shortly after InitialDecisionDelay"), bDecided);
			ExpectAction(TEXT("the first decision sees the zone spawned after the servant"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestEqual(TEXT("counter is 0 after a Wander decision"), Controller->GetFailedReturnAttempts(), 0);
		});

		It("OnUnPossess stops the wandering", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantUnpossess"), OutsideLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }
			SpawnZone(TEXT("ZoneUnpossess"), ZoneCenter);

			Controller->UnPossess();
			TestFalse(TEXT("IsWandering() == false after UnPossess"), Controller->IsWandering());
			TickFor(InitialDecisionDelay + 2.0 * RetryDelay + TimerSlack);
			ExpectAction(TEXT("no decision runs on an unpossessed controller"), Controller->GetLastAction(), EServantWanderAction::None);
		});
	});

	Describe("No usable zone (A4, A5)", [this]()
	{
		It("A4: without a registered zone, the controller keeps waiting, requests no move, and keeps re-deciding", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantA4"), ZoneCenter + FVector(0, 0, 100));
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }
			ServantWanderTest::FMoveRequestCounter Moves(Controller);
			if (!TestTrue(TEXT("harness: move request counter bound to the path following component"), Moves.IsBound())) { return; }
			const FVector StartLocation = Servant->GetActorLocation();
			const UBaseZoneSubsystem* Subsystem = World->GetSubsystem<UBaseZoneSubsystem>();
			if (!TestNotNull(TEXT("harness: the world has a UBaseZoneSubsystem"), Subsystem)) { return; }
			TestNull(TEXT("precondition: no zone registered"), Subsystem->GetBaseZone());

			TickFor(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack);

			ExpectAction(TEXT("A4: LastAction"), Controller->GetLastAction(), EServantWanderAction::Wait);
			TestEqual(TEXT("A4: counter stays at 0"), Controller->GetFailedReturnAttempts(), 0);
			TestEqual(TEXT("A4: no move request reached the path following component"), Moves.Count, 0);
			TestTrue(TEXT("A4: path following is idle"), Controller->GetMoveStatus() == EPathFollowingStatus::Idle);
			TestTrue(TEXT("A4: the servant did not move"), Servant->GetActorLocation().Equals(StartLocation, 0.01));
			TestTrue(TEXT("A4: still wandering"), Controller->IsWandering());

			// "Timer still active", proven by behaviour: a zone appearing now is used by the next decision, within one RetryDelay
			// (also proves the zone is re-read from the subsystem at each decision, never cached).
			ABaseZone* Zone = SpawnZone(TEXT("ZoneA4Late"), ZoneCenter);
			if (!TestNotNull(TEXT("late zone spawned"), Zone)) { return; }
			TickFor(RetryDelay + TimerSlack);
			ExpectAction(TEXT("A4: the decision timer is still running and picks up the new zone"), Controller->GetLastAction(), EServantWanderAction::Wander);
		});

		It("A5: an invalid zone (zero scale) behaves like no zone, and validity is re-read at each decision", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("ZoneA5"), ZoneCenter);
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetWorldScale3D(FVector(1.0, 0.0, 1.0));
			if (!TestFalse(TEXT("precondition: the zone is invalid"), Zone->IsZoneValid())) { return; }
			const UBaseZoneSubsystem* Subsystem = World->GetSubsystem<UBaseZoneSubsystem>();
			if (!TestNotNull(TEXT("harness: the world has a UBaseZoneSubsystem"), Subsystem)) { return; }
			TestTrue(TEXT("precondition: the invalid zone is the registered one"), Subsystem->GetBaseZone() == Zone);

			// Placed outside: with the zone wrongly treated as valid, the servant would go Return and count failures.
			AServant* Servant = SpawnServant(TEXT("ServantA5"), OutsideLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }
			ServantWanderTest::FMoveRequestCounter Moves(Controller);
			const FVector StartLocation = Servant->GetActorLocation();

			TickFor(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack);

			ExpectAction(TEXT("A5: LastAction"), Controller->GetLastAction(), EServantWanderAction::Wait);
			TestEqual(TEXT("A5: counter stays at 0"), Controller->GetFailedReturnAttempts(), 0);
			TestEqual(TEXT("A5: no move request"), Moves.Count, 0);
			TestTrue(TEXT("A5: the servant did not move (no teleport into an invalid zone)"), Servant->GetActorLocation().Equals(StartLocation, 0.01));
			TestTrue(TEXT("A5: still wandering"), Controller->IsWandering());

			// Repair the zone: the very next decision must see it valid (timer alive, no cached validity).
			Zone->GetZoneBox()->SetWorldScale3D(FVector::OneVector);
			TickFor(RetryDelay + TimerSlack);
			ExpectAction(TEXT("A5: once valid again, the out-of-zone servant tries to return"), Controller->GetLastAction(), EServantWanderAction::Return);
		});
	});

	Describe("Out of zone without NavMesh (A6, A7)", [this]()
	{
		It("A6: each decision spaced by RetryDelay increments the counter by exactly one, never per frame", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneA6"), ZoneCenter);
			AServant* Servant = SpawnServant(TEXT("ServantA6"), OutsideLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }
			ServantWanderTest::FMoveRequestCounter Moves(Controller);

			const bool bFirst = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::None; },
			                              InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("A6: first decision happened"), bFirst)) { return; }
			ExpectAction(TEXT("A6: first decision out of the zone"), Controller->GetLastAction(), EServantWanderAction::Return);
			TestEqual(TEXT("A6: the first Return is not yet a failure (no previous Return)"), Controller->GetFailedReturnAttempts(), 0);

			// Up to the last value before Teleport, so that every step is a plain Return failure.
			for (int32 Expected = 1; Expected < MaxFailedReturnAttempts; ++Expected)
			{
				// 90% of RetryDelay = dozens of frames: a per-frame decision would have moved the counter here.
				TickFor(0.9 * RetryDelay);
				TestEqual(*FString::Printf(TEXT("A6: counter unchanged during 0.9 x RetryDelay (step %d)"), Expected),
				          Controller->GetFailedReturnAttempts(), Expected - 1);

				const bool bChanged = TickUntil([Controller, Expected]() { return Controller->GetFailedReturnAttempts() != Expected - 1; },
				                                0.1 * RetryDelay + TimerSlack);
				TestTrue(*FString::Printf(TEXT("A6: a decision happened around RetryDelay (step %d)"), Expected), bChanged);
				TestEqual(*FString::Printf(TEXT("A6: counter incremented by exactly one (step %d)"), Expected),
				          Controller->GetFailedReturnAttempts(), Expected);
				ExpectAction(FString::Printf(TEXT("A6: still Return below the max (step %d)"), Expected), Controller->GetLastAction(), EServantWanderAction::Return);
			}
			TestEqual(TEXT("A6: projection always fails without NavMesh, so no MoveToLocation was issued"), Moves.Count, 0);
		});

		It("A7: at the max the decision is Teleport; without NavMesh it fails, warns, retries after RetryDelay, and the counter is capped", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneA7"), ZoneCenter);
			AServant* Servant = SpawnServant(TEXT("ServantA7"), OutsideLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }
			const FVector StartLocation = Servant->GetActorLocation();
			const int32 Max = MaxFailedReturnAttempts;

			const bool bReachedPreviousStep = TickUntil([Controller, Max]() { return Controller->GetFailedReturnAttempts() == Max - 1 && Controller->GetLastAction() == EServantWanderAction::Return; },
			                                            InitialDecisionDelay + Max * RetryDelay + TimerSlack);
			if (!TestTrue(TEXT("A7: precondition, counter reached Max - 1 with Return"), bReachedPreviousStep)) { return; }

			BaseZoneTest::FCategoryLogCounter Log(LogSpooky_Spores.GetCategoryName());

			// Spec order: increment first, then DecideNextAction with the new counter => the decision that reaches Max is Teleport.
			const bool bTeleportDecided = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::Return; },
			                                        RetryDelay + TimerSlack);
			TestTrue(TEXT("A7: a new decision happened"), bTeleportDecided);
			ExpectAction(TEXT("A7: decision once the counter reaches the max"), Controller->GetLastAction(), EServantWanderAction::Teleport);
			TestEqual(TEXT("A7: counter == MaxFailedReturnAttempts"), Controller->GetFailedReturnAttempts(), Max);
			Log.FlushPendingLogs();
			TestTrue(TEXT("A7: the failed teleport logs a Warning in LogSpooky_Spores"), Log.WarningCount >= 1);
			TestTrue(TEXT("A7: failed teleport leaves the servant where it was"), Servant->GetActorLocation().Equals(StartLocation, 0.01));

			// Two more retry cycles: each one is a new Teleport attempt with its Warning, none in between, counter capped.
			for (int32 Retry = 1; Retry <= 2; ++Retry)
			{
				Log.FlushPendingLogs();
				const int32 WarningsBefore = Log.WarningCount;
				TickFor(0.9 * RetryDelay);
				Log.FlushPendingLogs();
				TestEqual(*FString::Printf(TEXT("A7: no new attempt (no Warning) during 0.9 x RetryDelay (retry %d)"), Retry), Log.WarningCount, WarningsBefore);

				const bool bRetried = TickUntil([&Log, WarningsBefore]() { Log.FlushPendingLogs(); return Log.WarningCount > WarningsBefore; },
				                                0.1 * RetryDelay + TimerSlack);
				TestTrue(*FString::Printf(TEXT("A7: a new Teleport attempt warns after RetryDelay (retry %d)"), Retry), bRetried);
				ExpectAction(FString::Printf(TEXT("A7: retry is a Teleport (retry %d)"), Retry), Controller->GetLastAction(), EServantWanderAction::Teleport);
				TestEqual(*FString::Printf(TEXT("A7: counter does not exceed the max (retry %d)"), Retry), Controller->GetFailedReturnAttempts(), Max);
			}
			TestTrue(TEXT("A7: after every failed attempt the servant is still where it was"), Servant->GetActorLocation().Equals(StartLocation, 0.01));
		});
	});

	Describe("Back in the zone (A8)", [this]()
	{
		It("A8: moving the zone onto an out-of-zone servant resets the counter and switches to Wander at the next decision", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("ZoneA8"), ZoneCenter);
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantA8"), OutsideLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }

			const bool bFailedOnce = TickUntil([Controller]() { return Controller->GetFailedReturnAttempts() >= 1; },
			                                   InitialDecisionDelay + 2.0 * RetryDelay + TimerSlack);
			if (!TestTrue(TEXT("A8: precondition, counter > 0"), bFailedOnce)) { return; }
			if (!TestTrue(TEXT("A8: precondition, counter still below the max"), Controller->GetFailedReturnAttempts() < MaxFailedReturnAttempts)) { return; }

			Zone->SetActorLocation(FVector(OutsideLocation.X, OutsideLocation.Y, 0.0));
			TestTrue(TEXT("A8: precondition, the servant is now inside the zone"), Zone->IsPointInZone(Servant->GetActorLocation()));

			TickFor(RetryDelay + TimerSlack);
			TestEqual(TEXT("A8: counter reset to 0"), Controller->GetFailedReturnAttempts(), 0);
			ExpectAction(TEXT("A8: LastAction"), Controller->GetLastAction(), EServantWanderAction::Wander);
		});
	});

	Describe("Stop and restart (A9)", [this]()
	{
		It("A9: StopWandering freezes the cycle; StartWandering restarts it with a counter at 0", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneA9"), ZoneCenter);
			AServant* Servant = SpawnServant(TEXT("ServantA9"), OutsideLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr) { return; }

			const bool bFailedOnce = TickUntil([Controller]() { return Controller->GetFailedReturnAttempts() >= 1; },
			                                   InitialDecisionDelay + 2.0 * RetryDelay + TimerSlack);
			if (!TestTrue(TEXT("A9: precondition, the cycle is running (counter > 0)"), bFailedOnce)) { return; }
			const int32 CounterAtStop = Controller->GetFailedReturnAttempts();
			const EServantWanderAction ActionAtStop = Controller->GetLastAction();

			Controller->StopWandering();
			TestFalse(TEXT("A9: IsWandering() == false"), Controller->IsWandering());

			// Well past several RetryDelay and the longest pause: a surviving timer would run a decision and move the counter.
			TickFor(3.0 * RetryDelay + TimerSlack);
			ExpectAction(TEXT("A9: LastAction frozen after StopWandering"), Controller->GetLastAction(), ActionAtStop);
			TestEqual(TEXT("A9: counter frozen after StopWandering"), Controller->GetFailedReturnAttempts(), CounterAtStop);

			Controller->StartWandering();
			TestTrue(TEXT("A9: IsWandering() == true after StartWandering"), Controller->IsWandering());
			TestEqual(TEXT("A9: StartWandering resets the counter to 0"), Controller->GetFailedReturnAttempts(), 0);

			// Restarted cycle: first decision after InitialDecisionDelay, a second one RetryDelay later; stop half a period after it.
			// The counter is then 1 or 2 depending on whether StartWandering also resets LastAction (the spec does not say).
			TickFor(InitialDecisionDelay + 1.5 * RetryDelay);
			ExpectAction(TEXT("A9: the restarted cycle decides again"), Controller->GetLastAction(), EServantWanderAction::Return);
			const int32 CounterAfterRestart = Controller->GetFailedReturnAttempts();
			TestTrue(FString::Printf(TEXT("A9: the restarted cycle counts failures again from 0 (got %d, expected 1 or 2)"), CounterAfterRestart),
			         CounterAfterRestart >= 1 && CounterAfterRestart <= 2);
		});
	});

	Describe("Several servants (edge case)", [this]()
	{
		It("Each servant has its own controller and state", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneMulti"), ZoneCenter);
			AServant* Inside = SpawnServant(TEXT("ServantInside"), ZoneCenter + FVector(0, 0, 100));
			AServant* Outside = SpawnServant(TEXT("ServantOutside"), OutsideLocation);
			AServantAIController* InsideController = RequireController(Inside);
			AServantAIController* OutsideController = RequireController(Outside);
			if (InsideController == nullptr || OutsideController == nullptr) { return; }
			TestTrue(TEXT("two distinct controllers"), InsideController != OutsideController);

			// Half a period after the third decision: far from both the third and the fourth one.
			TickFor(InitialDecisionDelay + 2.5 * RetryDelay);

			ExpectAction(TEXT("servant inside: LastAction"), InsideController->GetLastAction(), EServantWanderAction::Wander);
			TestEqual(TEXT("servant inside: counter"), InsideController->GetFailedReturnAttempts(), 0);
			ExpectAction(TEXT("servant outside: LastAction"), OutsideController->GetLastAction(), EServantWanderAction::Return);
			TestEqual(TEXT("servant outside: counter (3 decisions, the first one is not a failure)"), OutsideController->GetFailedReturnAttempts(), 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
