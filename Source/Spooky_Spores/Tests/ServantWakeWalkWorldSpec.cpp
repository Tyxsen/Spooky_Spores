// Automation Spec for the wake-up of a servant that is WALKING (not paused), inside a minimal test world.
// Source: docs/specs/servant-replant.md (section "Reveil", removed at merge) + the corrected behaviour reported after the PIE test:
// "je recolte 1 graine pendant que le serviteur erre et il continue son chemin jusqu'a son point, attend le temps de delay et ensuite va
// vers le champ". The wake-up specs of ServantReplantWorldSpec only simulate the PAUSE of the wander, never a walk: this file does.
//
// Corrected behaviour under test: a FieldHarvest servant that wanders, has no field job and is walking (decision timer inactive,
// LastAction Wander) does NOT turn around on a wake signal (field Grown / Empty, or Seed added to player 0's counter). The private flag
// bWakePending is raised; the next OnMoveCompleted (Success or Failed) consumes it and schedules the next decision in
// FRandRange(0, MaxWakeDelay) instead of the normal pause (Success: MinWanderPause..MaxWanderPause, Failed: RetryDelay).
//
// World: same recipe as ServantReplantWorldSpec (shared helpers in Tests/ServantFieldTestHelpers.h): Game world, time advances only through
// TickTestWorld, servant movement tick disabled, NO NavMesh. Without NavMesh the servant never really walks, so:
//   - "walking" is simulated by clearing every timer bound to the controller (its only one is the decision timer): this is exactly the state of
//     a servant between MoveToLocation and OnMoveCompleted. The servant first rests (a real Wander decision) so LastAction is Wander;
//   - "arrival" is a direct call to OnMoveCompleted with a Success / Failed result, as ServantWanderWorldSpec does.
// "Decision happened" is observed through a plant (an Empty field in reach is planted when the servant has a seed); "pause" length is the
// time before that plant. bWakePending is read by reflection (VisibleInstanceOnly, name given by the brief).
// Not testable here (see the report): a wake signal DURING a field job (needs a trip, hence a NavMesh), and the reset of the flag by
// CancelFieldWork / at the start of a field job (same reason).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/FieldPlotTestHelpers.h"
#include "Tests/ServantWanderTestHelpers.h"
#include "Tests/ServantFieldTestHelpers.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "Servants/ServantTask.h"
#include "Resources/FieldPlot.h"
#include "Resources/FieldPlotSubsystem.h"
#include "Resources/ResourceCounterComponent.h"
#include "Resources/ResourceTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Tests/AutomationCommon.h"

namespace SFT = ServantFieldTest;

BEGIN_DEFINE_SPEC(FServantWakeWalkWorldSpec, "SpookySpores.Servants.WakeWalk.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = SFT::TickStep;
	static constexpr double TimerSlack = SFT::TimerSlack;

	const FVector ServantLocation = FVector(0.0, 0.0, 100.0);
	// 60 cm from the servant in 2D, inside HarvestReach: the servant acts on it without a move.
	const FVector NearField = FVector(60.0, 0.0, 0.0);
	static constexpr int32 BaseWheat = 10;
	static constexpr int32 PlantCost = 1;
	static constexpr int32 YieldWheat = 7;
	static constexpr int32 YieldSeed = 2;
	static constexpr double NeverGrownDuration = 100000.0;
	static constexpr double WakeGrowth = 0.3;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	double InitialDecisionDelay = 0.0;
	double RetryDelay = 0.0;
	double MinWanderPause = 0.0;
	double MaxWanderPause = 0.0;
	double HarvestNextDelay = 0.0;
	double MaxWakeDelay = 0.0;

	bool RequireWorld();
	bool ReadSettings(const AServantAIController* Controller);
	bool CheckWindows();
	AFieldPlot* SpawnField(const TCHAR* Name, const FVector& Location, EFieldState State);
	AServantAIController* SetupServant(const TCHAR* Name, AServant*& OutServant, EServantTask Task);
	bool StartResting(AServantAIController* Controller);
	void StartWalking(AServantAIController* Controller);
	void Arrive(AServantAIController* Controller, EPathFollowingResult::Type Code);
	bool IsWakePending(const AServantAIController* Controller);
	bool IsInState(const AFieldPlot* Field, EFieldState State) const;
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);

	// One walk scenario: servant at rest, then walking, a signal (or none), then the arrival.
	struct FWalkCase
	{
		FString Name;
		bool bSignal = true;
		EResourceType SignalType = EResourceType::Seed;
		EPathFollowingResult::Type Arrival = EPathFollowingResult::Success;
		bool bExpectWake = true;
	};
	void RunWalkCase(const FWalkCase& Case);
	void RunResetCase(bool bUnpossess);
	void RunNotHarvestCase(EServantTask Task);

END_DEFINE_SPEC(FServantWakeWalkWorldSpec)

bool FServantWakeWalkWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

bool FServantWakeWalkWorldSpec::ReadSettings(const AServantAIController* Controller)
{
	const bool bRead = ServantWanderTest::ReadNumber(Controller, TEXT("InitialDecisionDelay"), InitialDecisionDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("RetryDelay"), RetryDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MinWanderPause"), MinWanderPause)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MaxWanderPause"), MaxWanderPause)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("HarvestNextDelay"), HarvestNextDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MaxWakeDelay"), MaxWakeDelay);
	if (!bRead)
	{
		AddError(TEXT("A tunable (InitialDecisionDelay, RetryDelay, Min/MaxWanderPause, HarvestNextDelay, MaxWakeDelay) was not found by reflection."));
	}
	return bRead;
}

bool FServantWakeWalkWorldSpec::CheckWindows()
{
	// A wake-up (<= MaxWakeDelay + slack) must be clearly faster than both normal pauses, or the tests cannot tell them apart.
	const double WakeWindow = MaxWakeDelay + TimerSlack + 4.0 * TickStep;
	return TestTrue(TEXT("harness: the wake window must stay well below MinWanderPause and RetryDelay (tunables changed?)"),
	                WakeWindow < MinWanderPause && WakeWindow < RetryDelay && MinWanderPause <= MaxWanderPause);
}

AFieldPlot* FServantWakeWalkWorldSpec::SpawnField(const TCHAR* Name, const FVector& Location, EFieldState State)
{
	AFieldPlot* Field = FieldPlotTest::SpawnField(World, Name, Location, State);
	if (Field == nullptr)
	{
		AddError(FString::Printf(TEXT("harness: field %s could not be spawned"), Name));
		return nullptr;
	}
	FieldPlotTest::SetDrops(Field, FieldPlotTest::MakeYieldTable()); // Wheat 7, Seed 2
	if (!TestTrue(TEXT("harness: GrowthDuration writable by reflection"), FieldPlotTest::SetGrowthDuration(Field, NeverGrownDuration))) { return nullptr; }
	TestEqual(TEXT("harness: the field's SeedCost is the spec default"), Field->GetSeedCost(), PlantCost);
	return Field;
}

AServantAIController* FServantWakeWalkWorldSpec::SetupServant(const TCHAR* Name, AServant*& OutServant, EServantTask Task)
{
	OutServant = SFT::SpawnServant(World, Name, ServantLocation);
	if (!TestNotNull(TEXT("servant spawned"), OutServant)) { return nullptr; }
	AServantAIController* Controller = Cast<AServantAIController>(OutServant->GetController());
	if (!TestNotNull(TEXT("servant possessed by an AServantAIController"), Controller)) { return nullptr; }
	if (!ReadSettings(Controller)) { return nullptr; }
	if (Task != EServantTask::Unassigned && !TestTrue(TEXT("harness: servant assigned"), SFT::Assign(World, OutServant, Task))) { return nullptr; }
	return Controller;
}

bool FServantWakeWalkWorldSpec::StartResting(AServantAIController* Controller)
{
	// First decision with nothing to do = Wander; without NavMesh the move fails at once and the next decision is scheduled.
	TickFor(InitialDecisionDelay + 2.0 * TickStep);
	return TestTrue(TEXT("precondition: the first decision was Wander (nothing to do yet)"), Controller->GetLastAction() == EServantWanderAction::Wander)
		&& TestTrue(TEXT("precondition: the servant is wandering"), Controller->IsWandering());
}

void FServantWakeWalkWorldSpec::StartWalking(AServantAIController* Controller)
{
	// Same state as a servant between MoveToLocation and OnMoveCompleted: decision timer inactive, LastAction untouched (Wander).
	World->GetTimerManager().ClearAllTimersForObject(Controller);
}

void FServantWakeWalkWorldSpec::Arrive(AServantAIController* Controller, EPathFollowingResult::Type Code)
{
	Controller->OnMoveCompleted(FAIRequestID::CurrentRequest, FPathFollowingResult(Code, FPathFollowingResultFlags::None));
}

bool FServantWakeWalkWorldSpec::IsWakePending(const AServantAIController* Controller)
{
	const FBoolProperty* Property = FindFProperty<FBoolProperty>(Controller->GetClass(), TEXT("bWakePending"));
	if (Property == nullptr)
	{
		AddError(TEXT("harness: bool property bWakePending not found by reflection on the controller."));
		return false;
	}
	return Property->GetPropertyValue_InContainer(Controller);
}

bool FServantWakeWalkWorldSpec::IsInState(const AFieldPlot* Field, EFieldState State) const
{
	EFieldState Actual = EFieldState::Empty;
	return FieldPlotTest::GetState(Field, Actual) && Actual == State;
}

void FServantWakeWalkWorldSpec::TickFor(double Seconds)
{
	SFT::TickFor(*Wrapper, Seconds);
}

bool FServantWakeWalkWorldSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	return SFT::TickUntil(*Wrapper, Condition, MaxSeconds);
}

void FServantWakeWalkWorldSpec::RunWalkCase(const FWalkCase& Case)
{
	if (!RequireWorld()) { return; }
	SFT::SpawnZone(World, TEXT("ZoneWalk"));
	// Seed signal rows: stock 0 + Empty field (the arriving seed is what makes the field plantable).
	// Other rows: stock 1 + a field that is silently turned Empty during the walk (no notification, no wake signal): the field is plantable
	// at the next decision, whatever woke or did not wake the servant.
	const bool bSeedSignal = Case.bSignal && Case.SignalType == EResourceType::Seed;
	const int32 Stock = bSeedSignal ? 0 : 1;
	UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, Stock);
	AFieldPlot* Field = SpawnField(TEXT("FieldWalk"), NearField, bSeedSignal ? EFieldState::Empty : EFieldState::Growing);
	AServant* Servant = nullptr;
	AServantAIController* Controller = SetupServant(TEXT("ServantWalk"), Servant, EServantTask::FieldHarvest);
	if (Controller == nullptr || !Field || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
	if (!CheckWindows()) { return; }

	if (!StartResting(Controller)) { return; }
	TestFalse(TEXT("precondition: no wake pending at rest"), IsWakePending(Controller));
	StartWalking(Controller);
	if (!bSeedSignal)
	{
		TestTrue(TEXT("harness: silent state change to Empty"), FieldPlotTest::SetState(Field, EFieldState::Empty));
	}
	if (Case.bSignal)
	{
		Counter->AddResource(Case.SignalType, 1);
	}
	const int32 SeedWhileWalking = Counter->GetResourceCount(EResourceType::Seed);

	TestEqual(TEXT("the flag right after the signal"), IsWakePending(Controller), Case.bExpectWake);

	// The servant keeps walking: whatever the signal, nothing is decided before the arrival. Long enough for a "turn around" bug
	// (decision within MaxWakeDelay) to show: the Empty field would be planted.
	TickFor(MinWanderPause);
	FieldPlotTest::ExpectState(*this, TEXT("still walking: the field was not touched"), Field, EFieldState::Empty);
	TestEqual(TEXT("still walking: no seed spent"), Counter->GetResourceCount(EResourceType::Seed), SeedWhileWalking);
	TestEqual(TEXT("the flag is not consumed while walking"), IsWakePending(Controller), Case.bExpectWake);
	SFT::ExpectAction(*this, TEXT("still walking"), Controller->GetLastAction(), EServantWanderAction::Wander);

	Arrive(Controller, Case.Arrival);
	TestFalse(TEXT("the arrival consumes the flag (it is false afterwards in every case)"), IsWakePending(Controller));

	if (Case.bExpectWake)
	{
		const bool bPlanted = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, MaxWakeDelay + TimerSlack);
		TestTrue(TEXT("after the arrival the decision comes within MaxWakeDelay + slack (the normal pause would be far longer)"), bPlanted);
	}
	else
	{
		const double Min = Case.Arrival == EPathFollowingResult::Success ? MinWanderPause : RetryDelay;
		const double Max = Case.Arrival == EPathFollowingResult::Success ? MaxWanderPause : RetryDelay;
		TickFor(Min - 2.0 * TickStep);
		FieldPlotTest::ExpectState(*this, TEXT("no flag: the normal pause is kept (nothing decided before its minimum)"), Field, EFieldState::Empty);
		const bool bPlanted = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, (Max - Min) + 2.0 * TickStep + TimerSlack);
		TestTrue(TEXT("no flag: the decision still comes at the end of the normal pause (polling safety net intact)"), bPlanted);
	}
	TestEqual(TEXT("one seed spent by the plant"), Counter->GetResourceCount(EResourceType::Seed), SeedWhileWalking - PlantCost);
}

void FServantWakeWalkWorldSpec::RunResetCase(bool bUnpossess)
{
	if (!RequireWorld()) { return; }
	SFT::SpawnZone(World, TEXT("ZoneReset"));
	UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
	AFieldPlot* Field = SpawnField(TEXT("FieldReset"), NearField, EFieldState::Empty);
	AServant* Servant = nullptr;
	AServantAIController* Controller = SetupServant(TEXT("ServantReset"), Servant, EServantTask::FieldHarvest);
	if (Controller == nullptr || !Field || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
	if (!CheckWindows() || !StartResting(Controller)) { return; }

	StartWalking(Controller);
	Counter->AddResource(EResourceType::Seed, 1);
	if (!TestTrue(TEXT("harness: the flag is raised before the reset"), IsWakePending(Controller))) { return; }

	if (bUnpossess)
	{
		Controller->UnPossess(); // OnUnPossess -> StopWandering
	}
	else
	{
		Controller->StopWandering();
	}
	TestFalse(TEXT("the flag is reset"), IsWakePending(Controller));

	// Not wandering any more: a new signal neither raises the flag nor schedules a decision.
	Counter->AddResource(EResourceType::Seed, 1);
	TestFalse(TEXT("outside the wander a seed signal does not raise the flag"), IsWakePending(Controller));
	TickFor(MaxWakeDelay + TimerSlack);
	FieldPlotTest::ExpectState(*this, TEXT("no decision after the stop: the Empty field is untouched"), Field, EFieldState::Empty);
}

void FServantWakeWalkWorldSpec::RunNotHarvestCase(EServantTask Task)
{
	if (!RequireWorld()) { return; }
	SFT::SpawnZone(World, TEXT("ZoneNotHarvest"));
	UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
	AFieldPlot* Field = SpawnField(TEXT("FieldNotHarvest"), NearField, EFieldState::Empty);
	AActor* Planter = FieldPlotTest::SpawnPlainActor(World, TEXT("NotHarvestPlanter"));
	AServant* Subject = nullptr;
	AServant* Control = nullptr;
	AServantAIController* SubjectController = SetupServant(TEXT("ServantNotHarvest"), Subject, Task);
	AServantAIController* ControlController = SetupServant(TEXT("ServantControl"), Control, EServantTask::FieldHarvest);
	if (SubjectController == nullptr || ControlController == nullptr || !Field || !Planter || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
	if (!CheckWindows() || !TestTrue(TEXT("harness: GrowthDuration written"), FieldPlotTest::SetGrowthDuration(Field, WakeGrowth))) { return; }
	UResourceCounterComponent* PlanterStock = FieldPlotTest::MakeCounter(Planter, 0, PlantCost);

	// Both servants rest (real Wander decision), then both "walk". The control is on FieldHarvest: it proves that the signal below
	// really reaches a controller's wake handler; without it a "flag stays false" assertion on the subject could pass for nothing.
	if (!StartResting(SubjectController) || !StartResting(ControlController)) { return; }
	StartWalking(SubjectController);
	StartWalking(ControlController);

	// Signal received by every servant from OnPossess: the field notification (Growing -> Grown). The player's stock stays 0.
	TestTrue(TEXT("harness: the test plants the field"), Field->TryPlant(PlanterStock));
	const bool bGrown = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Grown); }, WakeGrowth + TimerSlack);
	if (!TestTrue(TEXT("harness: the field finished growing during the walk"), bGrown)) { return; }
	if (!TestTrue(TEXT("precondition: the same signal DID raise the flag of the FieldHarvest control servant"), IsWakePending(ControlController))) { return; }

	TestFalse(TEXT("a servant not on FieldHarvest does not raise the flag on that signal"), IsWakePending(SubjectController));
	TickFor(MinWanderPause);
	Arrive(SubjectController, EPathFollowingResult::Success);
	TestFalse(TEXT("after the arrival the flag is still false"), IsWakePending(SubjectController));
	TickFor(MaxWanderPause + TimerSlack);
	FieldPlotTest::ExpectState(*this, TEXT("a servant not on FieldHarvest never harvests"), Field, EFieldState::Grown);
	SFT::ExpectCounters(*this, TEXT("counter untouched"), Counter, BaseWheat, 0);
}

void FServantWakeWalkWorldSpec::Define()
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

	Describe("Seed signal while walking (the PIE bug)", [this]()
	{
		// Mutation (rows 1 and 2, the same defect): WakeIfIdle ignores "walking" and re-arms the decision timer (the former behaviour:
		//   turn around) -> the field is planted during the walk: the "still walking" lines turn red;
		//   the flag is never raised -> the flag line and, after the arrival, the wake window turn red;
		//   OnMoveCompleted does not consume the flag, or ignores it on Success -> the flag-after-arrival line / the wake window turn red;
		//   the pause after the arrival stays MinWanderPause..MaxWanderPause -> the wake window expires first: red;
		//   Failed arrival: the flag is ignored in the failure branch (RetryDelay kept) -> row 2 turns red.
		TArray<FWalkCase> Cases;
		{
			FWalkCase Row;
			Row.Name = TEXT("Success arrival: flag raised during the walk, then the plant comes within MaxWakeDelay");
			Cases.Add(Row);
		}
		{
			FWalkCase Row;
			Row.Name = TEXT("Failed arrival: the wake delay replaces RetryDelay");
			Row.Arrival = EPathFollowingResult::Failed;
			Cases.Add(Row);
		}
		// Mutation: the flag is raised for any resource type -> the flag line turns red (and the pause is not normal any more);
		//   the flag is raised for any signal even without one (e.g. at every move) -> "no flag" rows turn red;
		//   the flag survives the arrival without a signal being needed (never consumed) -> same rows turn red on a later arrival (see F below).
		{
			FWalkCase Row;
			Row.Name = TEXT("Wood signal (not a Seed): flag stays false, normal pause kept");
			Row.SignalType = EResourceType::Wood;
			Row.bExpectWake = false;
			Cases.Add(Row);
		}
		{
			FWalkCase Row;
			Row.Name = TEXT("no signal, Success arrival: normal pause MinWanderPause..MaxWanderPause kept");
			Row.bSignal = false;
			Row.bExpectWake = false;
			Cases.Add(Row);
		}
		{
			FWalkCase Row;
			Row.Name = TEXT("no signal, Failed arrival: RetryDelay kept");
			Row.bSignal = false;
			Row.Arrival = EPathFollowingResult::Failed;
			Row.bExpectWake = false;
			Cases.Add(Row);
		}
		// Note: the "not on FieldHarvest" cases are NOT here. A servant that is not on FieldHarvest never subscribes to the seed counter
		// (WatchStorage is only called by TryStartFieldWork), so a Seed signal never reaches it: such a row could not fail. They use the
		// field notification, which every servant receives from OnPossess (see "Field signal while walking").
		for (const FWalkCase& Case : Cases)
		{
			It(*Case.Name, [this, Case]()
			{
				RunWalkCase(Case);
			});
		}
	});

	Describe("Field signal while walking", [this]()
	{
		// Mutation: the field notification does not raise the flag while walking (or turns the servant around) -> the flag line /
		//           the "not harvested during the walk" line / the wake window turn red.
		It("a field that becomes Grown while the servant walks: no turn-around, harvested within MaxWakeDelay after the arrival", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneWalkGrown"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
			AFieldPlot* Field = SpawnField(TEXT("FieldWalkGrown"), NearField, EFieldState::Empty);
			AActor* Planter = FieldPlotTest::SpawnPlainActor(World, TEXT("WalkGrownPlanter"));
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantWalkGrown"), Servant, EServantTask::FieldHarvest);
			if (Controller == nullptr || !Field || !Planter || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWindows() || !TestTrue(TEXT("harness: GrowthDuration written"), FieldPlotTest::SetGrowthDuration(Field, WakeGrowth))) { return; }
			// The test plants with its own seed: the player's stock stays 0, so the servant cannot plant the Empty field itself.
			UResourceCounterComponent* PlanterStock = FieldPlotTest::MakeCounter(Planter, 0, PlantCost);

			if (!StartResting(Controller)) { return; }
			StartWalking(Controller);
			TestTrue(TEXT("harness: the test plants the field"), Field->TryPlant(PlanterStock));
			const bool bGrown = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Grown); }, WakeGrowth + TimerSlack);
			if (!TestTrue(TEXT("harness: the field finished growing during the walk"), bGrown)) { return; }
			TestTrue(TEXT("the growth end raised the flag"), IsWakePending(Controller));

			TickFor(MinWanderPause);
			FieldPlotTest::ExpectState(*this, TEXT("still walking: the Grown field was not harvested"), Field, EFieldState::Grown);
			SFT::ExpectCounters(*this, TEXT("still walking: counter untouched"), Counter, BaseWheat, 0);

			Arrive(Controller, EPathFollowingResult::Success);
			TestFalse(TEXT("the arrival consumes the flag"), IsWakePending(Controller));
			const bool bHarvested = TickUntil([Counter]() { return Counter->GetResourceCount(EResourceType::Wheat) > BaseWheat; }, MaxWakeDelay + TimerSlack);
			TestTrue(TEXT("harvested within MaxWakeDelay + slack after the arrival"), bHarvested);
			TickFor(TickStep);
			SFT::ExpectCounters(*this, TEXT("harvested then replanted with the yield's seed"), Counter, BaseWheat + YieldWheat, YieldSeed - PlantCost);
			FieldPlotTest::ExpectState(*this, TEXT("replanted"), Field, EFieldState::Growing);
		});

		// Mutation: WakeIfIdle does not check the assigned task (raises the flag for any servant) -> the subject's flag line turns red.
		//           The control FieldHarvest servant (precondition) guarantees the signal reaches the handler: if it did not, the test fails there.
		It("Unassigned servant: a field that becomes Grown while it walks does not raise the flag", [this]()
		{
			RunNotHarvestCase(EServantTask::Unassigned);
		});

		It("AttackDefense servant: a field that becomes Grown while it walks does not raise the flag", [this]()
		{
			RunNotHarvestCase(EServantTask::AttackDefense);
		});

		// Mutation: only Grown raises the flag (Empty ignored) -> the flag line / the wake window turn red.
		It("a field that becomes Empty (harvested by someone else) while the servant walks, with a stock: planted within MaxWakeDelay after the arrival", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneWalkEmpty"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 3);
			AFieldPlot* Field = SpawnField(TEXT("FieldWalkEmpty"), NearField, EFieldState::Grown);
			AActor* Holder = FieldPlotTest::SpawnPlainActor(World, TEXT("WalkEmptyHolder"));
			UFieldPlotSubsystem* Fields = World->GetSubsystem<UFieldPlotSubsystem>();
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantWalkEmpty"), Servant, EServantTask::FieldHarvest);
			if (Controller == nullptr || !Field || !Holder || !Fields || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWindows()) { return; }
			// The Grown field is held by another actor: the servant has nothing to do and rests.
			if (!TestTrue(TEXT("harness: the holder reserves the Grown field"), Fields->TryReserve(Field, Holder))) { return; }

			if (!StartResting(Controller)) { return; }
			FieldPlotTest::ExpectState(*this, TEXT("the reserved Grown field was left alone"), Field, EFieldState::Grown);
			StartWalking(Controller);
			Fields->Release(Field, Holder); // silent
			TMap<EResourceType, int32> Yield;
			TestTrue(TEXT("harness: someone else harvests the field (Grown -> Empty, notified)"), Field->TryHarvestYield(Yield));
			TestTrue(TEXT("the field change raised the flag"), IsWakePending(Controller));

			TickFor(MinWanderPause);
			FieldPlotTest::ExpectState(*this, TEXT("still walking: the Empty field was not planted"), Field, EFieldState::Empty);
			TestEqual(TEXT("still walking: no seed spent"), Counter->GetResourceCount(EResourceType::Seed), 3);

			Arrive(Controller, EPathFollowingResult::Success);
			TestFalse(TEXT("the arrival consumes the flag"), IsWakePending(Controller));
			const bool bPlanted = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, MaxWakeDelay + TimerSlack);
			TestTrue(TEXT("planted within MaxWakeDelay + slack after the arrival"), bPlanted);
			TestEqual(TEXT("one SeedCost spent"), Counter->GetResourceCount(EResourceType::Seed), 3 - PlantCost);
		});
	});

	Describe("Lifetime of the flag", [this]()
	{
		// Mutation: the flag is not cleared when consumed (or only on Success) -> the second arrival, without any new signal, also uses
		//           the wake delay: the field is planted before MinWanderPause -> the "no flag" line turns red.
		It("a flag consumed by one arrival does not survive: the next arrival without a signal gets the normal pause", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneConsume"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
			AFieldPlot* Field = SpawnField(TEXT("FieldConsume"), NearField, EFieldState::Empty);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantConsume"), Servant, EServantTask::FieldHarvest);
			if (Controller == nullptr || !Field || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWindows() || !StartResting(Controller)) { return; }

			StartWalking(Controller);
			Counter->AddResource(EResourceType::Seed, 2);
			TestTrue(TEXT("harness: the flag is raised"), IsWakePending(Controller));
			Arrive(Controller, EPathFollowingResult::Success);
			const bool bFirst = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, MaxWakeDelay + TimerSlack);
			if (!TestTrue(TEXT("first arrival: planted within MaxWakeDelay"), bFirst)) { return; }
			TestEqual(TEXT("one seed left"), Counter->GetResourceCount(EResourceType::Seed), 1);

			// The follow-up decision after the plant finds nothing to do (field Growing): Wander, as a rest again.
			const bool bNext = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::Plant; }, HarvestNextDelay + TimerSlack);
			if (!TestTrue(TEXT("harness: the follow-up decision happened"), bNext)) { return; }
			SFT::ExpectAction(*this, TEXT("nothing left to do"), Controller->GetLastAction(), EServantWanderAction::Wander);

			// Second walk, NO signal: the field becomes plantable silently (stock 1 is enough).
			StartWalking(Controller);
			TestTrue(TEXT("harness: silent state change to Empty"), FieldPlotTest::SetState(Field, EFieldState::Empty));
			TestFalse(TEXT("the first flag did not survive"), IsWakePending(Controller));
			Arrive(Controller, EPathFollowingResult::Success);
			TickFor(MinWanderPause - 2.0 * TickStep);
			FieldPlotTest::ExpectState(*this, TEXT("no flag: nothing is decided before MinWanderPause"), Field, EFieldState::Empty);
			const bool bSecond = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, (MaxWanderPause - MinWanderPause) + 2.0 * TickStep + TimerSlack);
			TestTrue(TEXT("no flag: the normal pause ends with the plant"), bSecond);
			TestEqual(TEXT("last seed spent"), Counter->GetResourceCount(EResourceType::Seed), 0);
		});

		// Mutation: StopWandering does not reset the flag -> the first flag line turns red; the signal handler ignores bIsWandering -> the
		//           "outside the wander" line turns red.
		It("StopWandering resets the flag, and a signal outside the wander does not raise it", [this]()
		{
			RunResetCase(false);
		});

		// Mutation: OnUnPossess does not go through the reset (flag kept) -> the first flag line turns red.
		It("OnUnPossess (through StopWandering) resets the flag", [this]()
		{
			RunResetCase(true);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
