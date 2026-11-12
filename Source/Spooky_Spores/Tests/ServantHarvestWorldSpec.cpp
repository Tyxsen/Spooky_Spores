// Automation Spec for the servant field harvest behaviour inside a minimal test world.
// Source of truth: the servant field harvest spec (docs/specs/servant-field-harvest.md, removed at merge), acceptance criteria 6 to 9,
// plus the "Comportement" rules (2D reach with the capsule radius, hors zone Return before harvest, failed projection = bSkipNextHarvest,
// reservation by another owner) and the edge cases of the spec.
//
// World: same recipe as ServantWanderWorldSpec (FTestWorldWrapper Game world, time advances only through TickTestWorld, servant movement
// tick disabled, NO NavMesh). Consequence: a servant already within reach of a field harvests without any move (reachable here), while a
// field out of reach needs a NavMesh projection, which always fails (the "NavMesh absent near the field" edge case, also testable here).
// What cannot be built here is a servant walking to a field with a held reservation: criterion 10 is NOT covered, see the report.
// Likewise, reservation-based exclusion between two servants on their way is not covered; criterion 9 here only detects a double credit
// (field not emptied), and the reservation read by the controller is covered by the "reserved by another owner" test.
// Player 0: an APlayerController registered in the world, with a pawn assigned through SetPawn (no possession logic needed),
// because the spec resolves the storage with UGameplayStatics::GetPlayerPawn(this, 0).
// Every test lists its mutation: the change of behaviour that must turn it red.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/FieldPlotTestHelpers.h"
#include "Tests/ServantWanderTestHelpers.h"
#include "Tests/BaseZoneTestHelpers.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "Servants/ServantSubsystem.h"
#include "Servants/ServantTask.h"
#include "Base/BaseZone.h"
#include "Resources/FieldPlot.h"
#include "Resources/FieldPlotSubsystem.h"
#include "Resources/ResourceCounterComponent.h"
#include "Resources/ResourceDrop.h"
#include "Resources/ResourceTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Templates/Function.h"
#include "UObject/Class.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FServantHarvestWorldSpec, "SpookySpores.Servants.Harvest.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = 0.05f;
	static constexpr double TimerSlack = 0.25;

	// Servants stand at this height (like ServantWanderWorldSpec); the zone has the ABaseZone default extent (500, 500, 200).
	const FVector ZoneCenter = FVector(0.0, 0.0, 0.0);
	const FVector ServantLocation = FVector(0.0, 0.0, 100.0);
	const FVector OutsideLocation = FVector(5000.0, 0.0, 100.0);
	// A field close to the servant (60 cm in 2D), well inside HarvestReach (default 100).
	const FVector NearField = FVector(60.0, 0.0, 0.0);
	// Counter baselines written before each scenario, so that "set instead of add" and "touched anyway" are visible.
	static constexpr int32 BaseWheat = 10;
	static constexpr int32 BaseSeed = 5;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	// Read from the controller under test by reflection (names from the spec / servant wandering spec).
	double InitialDecisionDelay = 0.0;
	double RetryDelay = 0.0;
	double MaxWanderPause = 0.0;
	double HarvestReach = 0.0;
	double HarvestNextDelay = 0.0;

	bool RequireWorld();
	ABaseZone* SpawnZone(const TCHAR* Name, bool bIgnoreHeight = true);
	AServant* SpawnServant(const TCHAR* Name, const FVector& Location);
	AFieldPlot* SpawnGrownField(const TCHAR* Name, const FVector& Location, EFieldState State = EFieldState::Grown);
	UResourceCounterComponent* SpawnPlayer(bool bWithCounter);
	AServantAIController* RequireController(AServant* Servant);
	bool ReadSettings(const AServantAIController* Controller);
	bool Assign(AServant* Servant, EServantTask Task);
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);
	void TickWatching(double Seconds, TFunctionRef<void()> Observer);
	bool IsReservedNow(const AFieldPlot* Field) const;
	void ExpectAction(const FString& What, EServantWanderAction Actual, EServantWanderAction Expected);
	void ExpectCounters(const FString& What, const UResourceCounterComponent* Counter, int32 Wheat, int32 Seed);

	// Scenario "nothing to do" (criterion 8), one row per sub-case.
	struct FIdleCase
	{
		FString Name;
		bool bZone = true;
		bool bServantInZone = true;
		bool bHasField = true;
		EFieldState FieldState = EFieldState::Grown;
		FVector FieldLocation = FVector(60.0, 0.0, 0.0);
		bool bDestroyField = false;
		bool bPlayer = true;
		bool bCounter = true;
		EServantWanderAction ExpectedAction = EServantWanderAction::Wander;
		double DecisionsToWait = 3.0; // in RetryDelay units, after InitialDecisionDelay
	};
	void RunIdleCase(const FIdleCase& Case);

END_DEFINE_SPEC(FServantHarvestWorldSpec)

bool FServantHarvestWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

ABaseZone* FServantHarvestWorldSpec::SpawnZone(const TCHAR* Name, bool bIgnoreHeight)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABaseZone* Zone = World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform(ZoneCenter), Params);
	if (Zone != nullptr)
	{
		// Explicit, so that a height difference between servant and field never decides "in zone".
		BaseZoneTest::SetIgnoreHeight(Zone, bIgnoreHeight);
	}
	return Zone;
}

AServant* FServantHarvestWorldSpec::SpawnServant(const TCHAR* Name, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AServant* Servant = World->SpawnActor<AServant>(AServant::StaticClass(), FTransform(Location), Params);
	if (Servant != nullptr && Servant->GetCharacterMovement() != nullptr)
	{
		// No floor in the test world: freeze physical movement so positions (and 2D distances) stay what the test set.
		Servant->GetCharacterMovement()->SetComponentTickEnabled(false);
	}
	return Servant;
}

AFieldPlot* FServantHarvestWorldSpec::SpawnGrownField(const TCHAR* Name, const FVector& Location, EFieldState State)
{
	AFieldPlot* Field = FieldPlotTest::SpawnField(World, Name, Location, State);
	if (Field != nullptr)
	{
		FieldPlotTest::SetDrops(Field, FieldPlotTest::MakeYieldTable()); // Wheat 7, Seed 2
	}
	return Field;
}

UResourceCounterComponent* FServantHarvestWorldSpec::SpawnPlayer(bool bWithCounter)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Name = FName(TEXT("TestPlayerController"));
	APlayerController* Controller = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	Params.Name = FName(TEXT("TestPlayerPawn"));
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(ADefaultPawn::StaticClass(), FTransform(FVector(0.0, 0.0, 5000.0)), Params);
	if (Controller == nullptr || Pawn == nullptr)
	{
		return nullptr;
	}
	// SetPawn only stores the pawn: enough for GetPlayerPawn(World, 0), without running the possession machinery in a test world.
	Controller->SetPawn(Pawn);
	if (!bWithCounter)
	{
		return nullptr;
	}
	UResourceCounterComponent* Counter = NewObject<UResourceCounterComponent>(Pawn);
	Counter->RegisterComponent();
	Counter->AddResource(EResourceType::Wheat, BaseWheat);
	Counter->AddResource(EResourceType::Seed, BaseSeed);
	return Counter;
}

AServantAIController* FServantHarvestWorldSpec::RequireController(AServant* Servant)
{
	if (!TestNotNull(TEXT("servant spawned"), Servant)) { return nullptr; }
	AServantAIController* Controller = Cast<AServantAIController>(Servant->GetController());
	if (!TestNotNull(TEXT("servant possessed by an AServantAIController"), Controller)) { return nullptr; }
	if (!ReadSettings(Controller)) { return nullptr; }
	return Controller;
}

bool FServantHarvestWorldSpec::ReadSettings(const AServantAIController* Controller)
{
	const bool bRead = ServantWanderTest::ReadNumber(Controller, TEXT("InitialDecisionDelay"), InitialDecisionDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("RetryDelay"), RetryDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MaxWanderPause"), MaxWanderPause)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("HarvestReach"), HarvestReach)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("HarvestNextDelay"), HarvestNextDelay);
	if (!bRead)
	{
		AddError(TEXT("InitialDecisionDelay, RetryDelay, MaxWanderPause, HarvestReach or HarvestNextDelay not found by reflection (names from the specs)."));
		return false;
	}
	if (RetryDelay < 10.0 * TickStep || HarvestNextDelay < 4.0 * TickStep)
	{
		AddError(FString::Printf(TEXT("RetryDelay (%f) or HarvestNextDelay (%f) too short for the tick step of these tests."), RetryDelay, HarvestNextDelay));
		return false;
	}
	return true;
}

bool FServantHarvestWorldSpec::Assign(AServant* Servant, EServantTask Task)
{
	UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
	return TestNotNull(TEXT("harness: UServantSubsystem"), Subsystem) && Subsystem->IsServantRegistered(Servant) && (Subsystem->AssignTask(Servant, Task) || Subsystem->GetServantTask(Servant) == Task);
}

void FServantHarvestWorldSpec::TickFor(double Seconds)
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(TickStep);
	}
}

bool FServantHarvestWorldSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt32(MaxSeconds / TickStep));
	for (int32 Index = 0; Index < MaxSteps; ++Index)
	{
		if (Condition()) { return true; }
		Wrapper->TickTestWorld(TickStep);
	}
	return Condition();
}

void FServantHarvestWorldSpec::TickWatching(double Seconds, TFunctionRef<void()> Observer)
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(TickStep);
		Observer();
	}
}

bool FServantHarvestWorldSpec::IsReservedNow(const AFieldPlot* Field) const
{
	const UFieldPlotSubsystem* Subsystem = World->GetSubsystem<UFieldPlotSubsystem>();
	return Subsystem != nullptr && Subsystem->IsReserved(Field);
}

void FServantHarvestWorldSpec::ExpectAction(const FString& What, EServantWanderAction Actual, EServantWanderAction Expected)
{
	TestTrue(FString::Printf(TEXT("%s: expected %s, got %s"), *What, *ServantWanderTest::ActionName(Expected), *ServantWanderTest::ActionName(Actual)),
	         Actual == Expected);
}

void FServantHarvestWorldSpec::ExpectCounters(const FString& What, const UResourceCounterComponent* Counter, int32 Wheat, int32 Seed)
{
	TestEqual(FString::Printf(TEXT("%s: Wheat"), *What), Counter->GetResourceCount(EResourceType::Wheat), Wheat);
	TestEqual(FString::Printf(TEXT("%s: Seed"), *What), Counter->GetResourceCount(EResourceType::Seed), Seed);
	// Types absent from the yield must never move.
	TestEqual(FString::Printf(TEXT("%s: Wood"), *What), Counter->GetResourceCount(EResourceType::Wood), 0);
	TestEqual(FString::Printf(TEXT("%s: Stone"), *What), Counter->GetResourceCount(EResourceType::Stone), 0);
	TestEqual(FString::Printf(TEXT("%s: Bone"), *What), Counter->GetResourceCount(EResourceType::Bone), 0);
}

void FServantHarvestWorldSpec::RunIdleCase(const FIdleCase& Case)
{
	if (!RequireWorld()) { return; }
	if (Case.bZone) { SpawnZone(TEXT("IdleZone")); }
	UResourceCounterComponent* Counter = Case.bPlayer ? SpawnPlayer(Case.bCounter) : nullptr;
	AFieldPlot* Field = Case.bHasField ? SpawnGrownField(TEXT("IdleField"), Case.FieldLocation, Case.FieldState) : nullptr;
	AServant* Servant = SpawnServant(TEXT("IdleServant"), Case.bServantInZone ? ServantLocation : OutsideLocation);
	AServantAIController* Controller = RequireController(Servant);
	if (Controller == nullptr) { return; }
	if (!TestTrue(TEXT("harness: servant assigned to FieldHarvest"), Assign(Servant, EServantTask::FieldHarvest))) { return; }
	if (Case.bPlayer && Case.bCounter && !TestNotNull(TEXT("harness: player counter"), Counter)) { return; }
	if (Case.bPlayer && !TestNotNull(TEXT("harness: GetPlayerPawn(0) resolves"), UGameplayStatics::GetPlayerPawn(World, 0))) { return; }
	if (Case.bHasField && !TestNotNull(TEXT("harness: field spawned"), Field)) { return; }
	if (Case.bDestroyField) { Field->Destroy(); }

	bool bEverReserved = false;
	bool bEverHarvest = false;
	const AFieldPlot* WatchedField = Case.bDestroyField ? nullptr : Field;
	TickWatching(InitialDecisionDelay + Case.DecisionsToWait * RetryDelay + TimerSlack, [&]()
	{
		bEverReserved |= (WatchedField != nullptr && IsReservedNow(WatchedField));
		bEverHarvest |= (Controller->GetLastAction() == EServantWanderAction::Harvest);
	});

	TestFalse(TEXT("the servant never reserved the field, not even transiently"), bEverReserved);
	TestFalse(TEXT("the action was never Harvest"), bEverHarvest);
	ExpectAction(TEXT("after several decisions"), Controller->GetLastAction(), Case.ExpectedAction);
	TestFalse(TEXT("IsHarvesting() is false"), Controller->IsHarvesting());
	TestNull(TEXT("GetHarvestTarget() is null"), Controller->GetHarvestTarget());
	TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
	if (WatchedField != nullptr)
	{
		EFieldState State = EFieldState::Empty;
		TestTrue(TEXT("harness: state readable"), FieldPlotTest::GetState(WatchedField, State));
		TestTrue(FString::Printf(TEXT("the field is untouched (state %s)"), *UEnum::GetValueAsString(State)), State == Case.FieldState);
	}
	if (Counter != nullptr)
	{
		ExpectCounters(TEXT("counter unchanged"), Counter, BaseWheat, BaseSeed);
	}
}

void FServantHarvestWorldSpec::Define()
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
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	Describe("Harvest at reach (criterion 6)", [this]()
	{
		// Mutation: the controller never picks Harvest (or ignores FieldHarvest) -> TickUntil(Harvest) fails: red.
		//           It spawns drops instead of crediting -> drop count and counters turn red.
		//           It sets the counter instead of adding, or credits something other than OutYield -> exact amounts turn red.
		//           The field is left Grown -> the state line and the "no second credit" window turn red.
		//           The next decision does not wait HarvestNextDelay (synchronous or too early) -> the "still Harvest" line turns red;
		//           it never comes back to the wandering -> the final Wander line turns red.
		//           The reservation is not released after the harvest -> the final IsReserved line turns red.
		It("credits the player counter with exactly the yield, leaves no drop, then goes back to Wander", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneHarvest"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AFieldPlot* Field = SpawnGrownField(TEXT("FieldHarvest"), NearField);
			AServant* Servant = SpawnServant(TEXT("ServantHarvest"), ServantLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!TestNotNull(TEXT("harness: GetPlayerPawn(0) resolves"), UGameplayStatics::GetPlayerPawn(World, 0))) { return; }
			if (!TestTrue(TEXT("harness: assigned to FieldHarvest"), Assign(Servant, EServantTask::FieldHarvest))) { return; }
			const TMap<EResourceType, int32> Yield = FieldPlotTest::ExpectedYield(FieldPlotTest::MakeYieldTable());

			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; },
			                                InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is Harvest"), bHarvest)) { return; }
			TickFor(TickStep); // the credit is part of the same decision, one frame of slack for safety

			ExpectCounters(TEXT("credited"), Counter, BaseWheat + Yield.FindRef(EResourceType::Wheat), BaseSeed + Yield.FindRef(EResourceType::Seed));
			EFieldState State = EFieldState::Grown;
			FieldPlotTest::GetState(Field, State);
			TestTrue(TEXT("the field is Empty"), State == EFieldState::Empty);
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);

			// The next decision comes after HarvestNextDelay: not before, then Wander (the field is Empty, nothing left to do).
			TickFor(0.5 * HarvestNextDelay);
			ExpectAction(TEXT("before HarvestNextDelay the action is still Harvest"), Controller->GetLastAction(), EServantWanderAction::Harvest);
			const bool bWander = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::Harvest; },
			                               0.5 * HarvestNextDelay + TimerSlack);
			TestTrue(TEXT("a decision happens after HarvestNextDelay"), bWander);
			ExpectAction(TEXT("after HarvestNextDelay + one decision"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestFalse(TEXT("IsHarvesting() is false again"), Controller->IsHarvesting());
			TestNull(TEXT("GetHarvestTarget() is null again"), Controller->GetHarvestTarget());
			TestFalse(TEXT("the reservation is released"), IsReservedNow(Field));

			// Several more decisions: no second credit.
			TickFor(3.0 * RetryDelay);
			ExpectCounters(TEXT("still credited once"), Counter, BaseWheat + Yield.FindRef(EResourceType::Wheat), BaseSeed + Yield.FindRef(EResourceType::Seed));
		});

		// Mutation: the reach test is 3D (the servant 300 cm above the field is "too far") -> no immediate harvest: red.
		It("measures the reach in 2D: a field far below the servant is within reach", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneHarvest2D"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AFieldPlot* Field = SpawnGrownField(TEXT("Field2D"), NearField);
			AServant* Servant = SpawnServant(TEXT("Servant2D"), FVector(0.0, 0.0, 400.0)); // 400 cm above the field, 60 cm away in 2D
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!TestTrue(TEXT("harness: assigned"), Assign(Servant, EServantTask::FieldHarvest))) { return; }

			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; },
			                                InitialDecisionDelay + TimerSlack);
			TestTrue(TEXT("Harvest decided"), bHarvest);
			TickFor(TickStep);
			TestEqual(TEXT("the Wheat was credited without any move"), Counter->GetResourceCount(EResourceType::Wheat), BaseWheat + 7);
			TestTrue(TEXT("the servant did not move"), Servant->GetActorLocation().Equals(FVector(0.0, 0.0, 400.0), 0.01));
		});

		// Mutation: the capsule radius is not added to HarvestReach -> a field at Reach + Radius - 5 is "too far": red.
		It("harvests immediately at HarvestReach + capsule radius (just inside)", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneReachIn"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AServant* Servant = SpawnServant(TEXT("ServantReachIn"), ServantLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: counter"), Counter) || !TestNotNull(TEXT("harness: capsule"), Servant->GetCapsuleComponent())) { return; }
			const double Distance = HarvestReach + Servant->GetCapsuleComponent()->GetScaledCapsuleRadius() - 5.0;
			AFieldPlot* Field = SpawnGrownField(TEXT("FieldReachIn"), FVector(Distance, 0.0, 0.0));
			if (!TestNotNull(TEXT("harness: field"), Field) || !TestTrue(TEXT("harness: assigned"), Assign(Servant, EServantTask::FieldHarvest))) { return; }

			TickFor(InitialDecisionDelay + TimerSlack);
			TestEqual(TEXT("credited without a move (NavMesh is absent: only the immediate path can succeed)"),
			          Counter->GetResourceCount(EResourceType::Wheat), BaseWheat + 7);
		});
	});

	Describe("Out of reach without NavMesh (edge case, criterion 6 complement)", [this]()
	{
		// Mutation: the reach is too lax (a field at Reach + Radius + 50 is harvested) -> the counter changes: red.
		//           bSkipNextHarvest not set after the failed projection -> the controller picks Harvest again at every decision,
		//           so Wander is never seen after the first Harvest: red.
		//           The reservation is not released after the failure -> the "reserved while Wander" flag turns red.
		//           The field is harvested without a path (teleport credit) -> counter / state turn red.
		It("a field just out of reach and without NavMesh: failure, reservation released, a Wander decision follows, field intact", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneReachOut"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AServant* Servant = SpawnServant(TEXT("ServantReachOut"), ServantLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: counter"), Counter) || !TestNotNull(TEXT("harness: capsule"), Servant->GetCapsuleComponent())) { return; }
			const double Distance = HarvestReach + Servant->GetCapsuleComponent()->GetScaledCapsuleRadius() + 50.0;
			AFieldPlot* Field = SpawnGrownField(TEXT("FieldReachOut"), FVector(Distance, 0.0, 0.0));
			if (!TestNotNull(TEXT("harness: field"), Field) || !TestTrue(TEXT("harness: assigned"), Assign(Servant, EServantTask::FieldHarvest))) { return; }

			bool bSawHarvest = false;
			bool bWanderAfterHarvest = false;
			bool bReservedWhileWander = false;
			TickWatching(InitialDecisionDelay + 4.0 * RetryDelay + TimerSlack, [&]()
			{
				const EServantWanderAction Action = Controller->GetLastAction();
				bSawHarvest |= (Action == EServantWanderAction::Harvest);
				if (bSawHarvest && Action == EServantWanderAction::Wander)
				{
					bWanderAfterHarvest = true;
					bReservedWhileWander |= IsReservedNow(Field);
				}
			});

			TestTrue(TEXT("the field was chosen (Harvest decided)"), bSawHarvest);
			TestTrue(TEXT("after the failed attempt a Wander decision follows (bSkipNextHarvest), no blocking loop"), bWanderAfterHarvest);
			TestFalse(TEXT("the reservation is released once the attempt failed"), bReservedWhileWander);
			ExpectCounters(TEXT("nothing credited"), Counter, BaseWheat, BaseSeed);
			TestTrue(TEXT("the field is still Grown"), Field->IsReadyToHarvest());
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
		});
	});

	Describe("Task must be FieldHarvest (criterion 7)", [this]()
	{
		// Mutation: the controller ignores the task (harvests for Unassigned or AttackDefense) -> everHarvest / counter / state turn red.
		//           It reserves before checking the task -> bEverReserved turns red.
		//           Positive control at the end: if the setup could never harvest (reach, zone, player), the control turns red,
		//           so the negative phases cannot pass for the wrong reason.
		It("Unassigned and AttackDefense servants leave the Grown field and the counter alone; FieldHarvest harvests it", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneTask"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AFieldPlot* Field = SpawnGrownField(TEXT("FieldTask"), NearField);
			AServant* Servant = SpawnServant(TEXT("ServantTask"), ServantLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!TestNotNull(TEXT("harness: GetPlayerPawn(0) resolves"), UGameplayStatics::GetPlayerPawn(World, 0))) { return; }

			bool bEverReserved = false;
			bool bEverHarvest = false;
			auto Observe = [&]()
			{
				bEverReserved |= IsReservedNow(Field);
				bEverHarvest |= (Controller->GetLastAction() == EServantWanderAction::Harvest);
			};
			const double Window = InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack;

			// Phase 1: default task (Unassigned, no AssignTask call).
			TickWatching(Window, Observe);
			TestFalse(TEXT("Unassigned: never reserved"), bEverReserved);
			TestFalse(TEXT("Unassigned: never Harvest"), bEverHarvest);
			ExpectAction(TEXT("Unassigned: the servant wanders"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestTrue(TEXT("Unassigned: the field is still Grown"), Field->IsReadyToHarvest());
			ExpectCounters(TEXT("Unassigned: counter"), Counter, BaseWheat, BaseSeed);

			// Phase 2: AttackDefense.
			if (!TestTrue(TEXT("harness: assigned to AttackDefense"), Assign(Servant, EServantTask::AttackDefense))) { return; }
			TickWatching(Window, Observe);
			TestFalse(TEXT("AttackDefense: never reserved"), bEverReserved);
			TestFalse(TEXT("AttackDefense: never Harvest"), bEverHarvest);
			ExpectAction(TEXT("AttackDefense: the servant wanders"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestTrue(TEXT("AttackDefense: the field is still Grown"), Field->IsReadyToHarvest());
			ExpectCounters(TEXT("AttackDefense: counter"), Counter, BaseWheat, BaseSeed);

			// Positive control: the very same setup harvests once the task is FieldHarvest.
			if (!TestTrue(TEXT("harness: assigned to FieldHarvest"), Assign(Servant, EServantTask::FieldHarvest))) { return; }
			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; },
			                                2.0 * RetryDelay + MaxWanderPause + TimerSlack);
			TestTrue(TEXT("control: with FieldHarvest the same field is harvested at a following decision"), bHarvest);
			TickFor(TickStep);
			TestEqual(TEXT("control: Wheat credited"), Counter->GetResourceCount(EResourceType::Wheat), BaseWheat + 7);
		});
	});

	Describe("Nothing to do (criterion 8)", [this]()
	{
		// Mutation (one per row, all of the form "the controller harvests / reserves although the rule says not to"):
		//   no field -> a null target is dereferenced or Harvest decided; Empty/Growing -> the eligibility test ignores the state;
		//   Grown outside the zone -> IsPointInZone(field) is not checked; no player / no counter -> the storage is not checked
		//   before reserving (reservation observed, or Harvest decided and the field loses its yield);
		//   destroyed field -> a stale registry entry is still chosen; no zone -> Wait becomes Harvest;
		//   servant outside the zone -> the harvest takes priority over Return.
		TArray<FIdleCase> Cases;
		{
			FIdleCase Row;
			Row.Name = TEXT("no field at all");
			Row.bHasField = false;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("an Empty field in reach");
			Row.FieldState = EFieldState::Empty;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("a Growing field in reach");
			Row.FieldState = EFieldState::Growing;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("a Grown field outside the zone");
			Row.FieldLocation = FVector(3000.0, 0.0, 0.0);
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("no player at all");
			Row.bPlayer = false;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("a player pawn without a resource counter");
			Row.bCounter = false;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("the Grown field is destroyed before the decision");
			Row.bDestroyField = true;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("no zone: Wait, the field is ignored");
			Row.bZone = false;
			Row.ExpectedAction = EServantWanderAction::Wait;
			Cases.Add(Row);
		}
		{
			FIdleCase Row;
			Row.Name = TEXT("servant outside the zone: Return comes before the harvest");
			Row.bServantInZone = false;
			Row.ExpectedAction = EServantWanderAction::Return;
			Row.DecisionsToWait = 1.5; // stays below MaxFailedReturnAttempts, so the action is still Return and not Teleport
			Cases.Add(Row);
		}

		for (const FIdleCase& Case : Cases)
		{
			It(*FString::Printf(TEXT("%s: no reservation, field intact, action %s"), *Case.Name, *ServantWanderTest::ActionName(Case.ExpectedAction)), [this, Case]()
			{
				RunIdleCase(Case);
			});
		}
	});

	Describe("Closest field first (eligibility rule: nearest in 2D)", [this]()
	{
		// Mutation: the controller picks the first registered field, or the farthest one, instead of the nearest -> the far field
		//           (spawned and registered FIRST) is emptied first: "far still Grown" and "near Empty" lines turn red.
		//           The next decision does not look for another field (the loop stops after one harvest) -> the far field stays
		//           Grown and the final counter stays at one yield: red.
		It("harvests the nearest Grown field first, then the farther one after HarvestNextDelay", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneNearest"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AServant* Servant = SpawnServant(TEXT("ServantNearest"), ServantLocation);
			AServantAIController* Controller = RequireController(Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: counter"), Counter) || !TestNotNull(TEXT("harness: capsule"), Servant->GetCapsuleComponent())) { return; }
			// Both fields are within immediate reach (no NavMesh needed): the near one at 40 cm, the far one just inside
			// HarvestReach + capsule radius (on the opposite side), spawned and therefore registered FIRST.
			const double FarDistance = HarvestReach + Servant->GetCapsuleComponent()->GetScaledCapsuleRadius() - 5.0;
			AFieldPlot* FarField = SpawnGrownField(TEXT("FieldFarFirst"), FVector(-FarDistance, 0.0, 0.0));
			AFieldPlot* NearField2D = SpawnGrownField(TEXT("FieldNearSecond"), FVector(40.0, 0.0, 0.0));
			if (!TestNotNull(TEXT("harness: far field"), FarField) || !TestNotNull(TEXT("harness: near field"), NearField2D)) { return; }
			if (!TestTrue(TEXT("harness: assigned"), Assign(Servant, EServantTask::FieldHarvest))) { return; }

			// First decision: the nearest field (2D) only.
			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; },
			                                InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is Harvest"), bHarvest)) { return; }
			TickFor(TickStep);
			EFieldState NearState = EFieldState::Grown;
			EFieldState FarState = EFieldState::Empty;
			FieldPlotTest::GetState(NearField2D, NearState);
			FieldPlotTest::GetState(FarField, FarState);
			TestTrue(TEXT("the nearest field (2D) is Empty after the first harvest"), NearState == EFieldState::Empty);
			TestTrue(TEXT("the farther field is still Grown (the first registered one was not chosen)"), FarState == EFieldState::Grown);
			ExpectCounters(TEXT("after the first harvest: exactly one yield"), Counter, BaseWheat + 7, BaseSeed + 2);

			// Next decision after HarvestNextDelay: the farther field.
			const bool bFarHarvested = TickUntil([FarField]() { return !FarField->IsReadyToHarvest(); }, HarvestNextDelay + TimerSlack);
			TestTrue(TEXT("the farther field is harvested at the next decision"), bFarHarvested);
			TickFor(TickStep);
			ExpectCounters(TEXT("after the second harvest: exactly two yields"), Counter, BaseWheat + 14, BaseSeed + 4);
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
		});
	});

	Describe("Two servants, one field (criterion 9)", [this]()
	{
		// Mutation: the field is not emptied after the credit, or the credit ignores TryHarvestYield's result (no Grown check),
		//           so the second servant credits it again -> Wheat + 14 and both servants reach Harvest: red.
		// NOT detected here: the absence of the RESERVATION. Both servants are within immediate reach, A harvests synchronously
		//           in its decision and the field is already Empty when B decides, so only the Grown state stops B. The exclusion by
		//           reservation between two servants on their way is covered only by criterion 10 (PIE, NavMesh); the reservation
		//           honoured by a servant is covered by the "reserved by another owner" test below.
		It("credits the counter once and only one servant ever harvests", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneTwo"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AFieldPlot* Field = SpawnGrownField(TEXT("FieldTwo"), NearField);
			AServant* ServantA = SpawnServant(TEXT("ServantTwoA"), ServantLocation);
			AServant* ServantB = SpawnServant(TEXT("ServantTwoB"), ServantLocation + FVector(0.0, 40.0, 0.0));
			AServantAIController* ControllerA = RequireController(ServantA);
			AServantAIController* ControllerB = RequireController(ServantB);
			if (ControllerA == nullptr || ControllerB == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!TestTrue(TEXT("harness: both assigned"), Assign(ServantA, EServantTask::FieldHarvest) && Assign(ServantB, EServantTask::FieldHarvest))) { return; }

			bool bHarvestA = false;
			bool bHarvestB = false;
			TickWatching(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack, [&]()
			{
				bHarvestA |= (ControllerA->GetLastAction() == EServantWanderAction::Harvest);
				bHarvestB |= (ControllerB->GetLastAction() == EServantWanderAction::Harvest);
			});

			TestTrue(TEXT("exactly one of the two servants harvested"), bHarvestA != bHarvestB);
			ExpectCounters(TEXT("credited exactly once"), Counter, BaseWheat + 7, BaseSeed + 2);
			TestFalse(TEXT("the field is Empty"), Field->IsReadyToHarvest());
			TestFalse(TEXT("no reservation left"), IsReservedNow(Field));
		});

		// Mutation: the controller ignores the reservations of others (eligible = every Grown field) -> a Harvest is observed
		//           and the field is emptied while reserved: red.
		//           The servant gives up for good after seeing the field reserved (no re-evaluation) -> the final harvest never comes: red.
		It("a field reserved by another owner is left alone, and harvested once it is released", [this]()
		{
			if (!RequireWorld()) { return; }
			SpawnZone(TEXT("ZoneReserved"));
			UResourceCounterComponent* Counter = SpawnPlayer(true);
			AFieldPlot* Field = SpawnGrownField(TEXT("FieldReserved"), NearField);
			AServant* Servant = SpawnServant(TEXT("ServantReserved"), ServantLocation);
			AServantAIController* Controller = RequireController(Servant);
			AActor* Holder = FieldPlotTest::SpawnPlainActor(World, TEXT("ForeignHolder"));
			UFieldPlotSubsystem* Fields = World->GetSubsystem<UFieldPlotSubsystem>();
			if (Controller == nullptr || Holder == nullptr || Fields == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!TestTrue(TEXT("harness: assigned"), Assign(Servant, EServantTask::FieldHarvest))) { return; }
			if (!TestTrue(TEXT("harness: the holder reserves the field"), Fields->TryReserve(Field, Holder))) { return; }

			bool bEverHarvest = false;
			TickWatching(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack, [&]()
			{
				bEverHarvest |= (Controller->GetLastAction() == EServantWanderAction::Harvest);
			});
			TestFalse(TEXT("never Harvest while another owner holds the field"), bEverHarvest);
			ExpectCounters(TEXT("counter untouched while reserved"), Counter, BaseWheat, BaseSeed);
			TestTrue(TEXT("the field is still Grown"), Field->IsReadyToHarvest());
			TestTrue(TEXT("the foreign reservation was not stolen"), Fields->IsReserved(Field));
			// The foreign holder keeps it: a second owner is still refused, so the servant did not replace the holder.
			AActor* Other = FieldPlotTest::SpawnPlainActor(World, TEXT("OtherOwner"));
			TestFalse(TEXT("the holder is still the foreign owner"), Other != nullptr && Fields->TryReserve(Field, Other));

			Fields->Release(Field, Holder);
			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; },
			                                2.0 * RetryDelay + MaxWanderPause + TimerSlack);
			TestTrue(TEXT("once released, the servant harvests at a following decision"), bHarvest);
			TickFor(TickStep);
			ExpectCounters(TEXT("credited after the release"), Counter, BaseWheat + 7, BaseSeed + 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
