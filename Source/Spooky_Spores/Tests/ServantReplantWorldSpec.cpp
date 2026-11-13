// Automation Spec for the servant replanting (item 2.2b) inside a minimal test world.
// Source of truth: docs/specs/servant-replant.md (removed at merge), acceptance criteria 4 to 9
// (4: harvest then replant on the spot, 5: priority Grown then Empty, 6: never a reservation for the listed cases,
//  7: two servants / one seed budget, 8: wake-up by field, 9: wake-up by seeds).
// Criterion 10 (cancellation on the way: field harvested / planted by the player or destroyed during the trip) needs a NavMesh and is
// NOT covered here; see the report for what to replay by hand.
//
// World: same recipe as ServantHarvestWorldSpec (shared helpers in Tests/ServantFieldTestHelpers.h): Game world, time advances only through
// TickTestWorld, servant movement tick disabled, NO NavMesh. A servant already within reach of a field acts without any move, which is
// the only way to act here. Consequences, stated once:
//   - the reservation / "promised seeds" mechanism between servants cannot be exercised by two real servants (the first one resolves
//     synchronously, nothing is ever "on its way"). It is exercised instead by a plain actor that holds a reservation (a stand-in for a
//     servant on its way) while a real servant decides: see the "promised seeds" tests.
//   - fields are replanted with a huge GrowthDuration so that they never become Grown again during a test, except in the wake-up tests
//     where the growth end IS the event under test.
// Wake-up tests: a servant at rest is one that has just made a Wander decision (no NavMesh: its next decision is RetryDelay away).
// Changes that must wake it are made through real API calls (a reflection write of the state does NOT notify, which the tests use as a
// control). "Woken" is told from "decided by the normal timer" by the window: MaxWakeDelay (0.5 s) + slack against RetryDelay (2 s).
// Every test lists its mutation: the change of behaviour that must turn it red.

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
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Templates/Function.h"
#include "UObject/Class.h"
#include "Tests/AutomationCommon.h"

namespace SFT = ServantFieldTest;

BEGIN_DEFINE_SPEC(FServantReplantWorldSpec, "SpookySpores.Servants.Replant.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = SFT::TickStep;
	static constexpr double TimerSlack = SFT::TimerSlack;

	const FVector ServantLocation = FVector(0.0, 0.0, 100.0);
	// 60 cm from the servant in 2D, well inside HarvestReach (default 100): acts without a move.
	const FVector NearField = FVector(60.0, 0.0, 0.0);
	static constexpr int32 BaseWheat = 10;
	static constexpr int32 BaseSeed = 5;
	// SeedCost of a field (spec: "SeedCost du champ (1) inchange").
	static constexpr int32 PlantCost = 1;
	// The default yield table (FieldPlotTest::MakeYieldTable) gives Wheat 7 and Seed 2.
	static constexpr int32 YieldWheat = 7;
	static constexpr int32 YieldSeed = 2;
	static constexpr double NeverGrownDuration = 100000.0;
	// Growth of the fields whose growth end is the event under test (wake-up by field).
	static constexpr double WakeGrowth = 0.3;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	// Read from the controller under test by reflection (names from the specs).
	double InitialDecisionDelay = 0.0;
	double RetryDelay = 0.0;
	double MaxWanderPause = 0.0;
	double HarvestNextDelay = 0.0;
	double MaxWakeDelay = 0.0;

	bool RequireWorld();
	AFieldPlot* SpawnField(const TCHAR* Name, const FVector& Location, EFieldState State);
	AServantAIController* SetupServant(const TCHAR* Name, const FVector& Location, AServant*& OutServant, EServantTask Task = EServantTask::FieldHarvest);
	bool ReadSettings(const AServantAIController* Controller);
	bool CheckWakeWindow(double ExtraSeconds);
	bool StartResting(AServantAIController* Controller);
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);
	void TickWatching(double Seconds, TFunctionRef<void()> Observer);
	bool IsReservedNow(const AFieldPlot* Field) const;
	bool IsInState(const AFieldPlot* Field, EFieldState State) const;
	void ExpectCounters(const FString& What, const UResourceCounterComponent* Counter, int32 Wheat, int32 Seed);

	// Boundary of the replant after a harvest (SeedCost 3, yield Seed 2).
	struct FBoundaryCase
	{
		FString Name;
		int32 Stock = 0;
		bool bExpectPlanted = false;
	};
	void RunBoundaryCase(const FBoundaryCase& Case);

	// Scenario "never a reservation nor a plantation" (criterion 6), one row per sub-case.
	struct FPlantIdleCase
	{
		FString Name;
		bool bZone = true;
		EServantTask Task = EServantTask::FieldHarvest;
		EFieldState FieldState = EFieldState::Empty;
		FVector FieldLocation = FVector(60.0, 0.0, 0.0);
		bool bPlayer = true;
		bool bCounter = true;
		int32 Seed = BaseSeed;
		bool bReservedByOther = false;
	};
	void RunPlantIdleCase(const FPlantIdleCase& Case);

	// Seeds promised to another reserver (criterion 7).
	struct FPromiseCase
	{
		FString Name;
		int32 Stock = 0;
		bool bExpectImmediatePlant = false;
	};
	void RunPromiseCase(const FPromiseCase& Case);

END_DEFINE_SPEC(FServantReplantWorldSpec)

bool FServantReplantWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

AFieldPlot* FServantReplantWorldSpec::SpawnField(const TCHAR* Name, const FVector& Location, EFieldState State)
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

AServantAIController* FServantReplantWorldSpec::SetupServant(const TCHAR* Name, const FVector& Location, AServant*& OutServant, EServantTask Task)
{
	OutServant = SFT::SpawnServant(World, Name, Location);
	if (!TestNotNull(TEXT("servant spawned"), OutServant)) { return nullptr; }
	AServantAIController* Controller = Cast<AServantAIController>(OutServant->GetController());
	if (!TestNotNull(TEXT("servant possessed by an AServantAIController"), Controller)) { return nullptr; }
	if (!ReadSettings(Controller)) { return nullptr; }
	if (Task != EServantTask::Unassigned && !TestTrue(TEXT("harness: servant assigned"), SFT::Assign(World, OutServant, Task))) { return nullptr; }
	return Controller;
}

bool FServantReplantWorldSpec::ReadSettings(const AServantAIController* Controller)
{
	const bool bRead = ServantWanderTest::ReadNumber(Controller, TEXT("InitialDecisionDelay"), InitialDecisionDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("RetryDelay"), RetryDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MaxWanderPause"), MaxWanderPause)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("HarvestNextDelay"), HarvestNextDelay)
		&& ServantWanderTest::ReadNumber(Controller, TEXT("MaxWakeDelay"), MaxWakeDelay);
	if (!bRead)
	{
		AddError(TEXT("InitialDecisionDelay, RetryDelay, MaxWanderPause, HarvestNextDelay or MaxWakeDelay not found by reflection (names from the specs)."));
		return false;
	}
	if (RetryDelay < 10.0 * TickStep || HarvestNextDelay < 4.0 * TickStep)
	{
		AddError(FString::Printf(TEXT("RetryDelay (%f) or HarvestNextDelay (%f) too short for the tick step of these tests."), RetryDelay, HarvestNextDelay));
		return false;
	}
	return true;
}

bool FServantReplantWorldSpec::CheckWakeWindow(double ExtraSeconds)
{
	// The wake-up tests only mean something if a wake-up (<= MaxWakeDelay + slack) is clearly faster than the retry timer.
	return TestTrue(TEXT("harness: the wake window must stay well below RetryDelay (tunables changed?)"),
	                ExtraSeconds + MaxWakeDelay + TimerSlack + 4.0 * TickStep < RetryDelay);
}

bool FServantReplantWorldSpec::StartResting(AServantAIController* Controller)
{
	// The first decision is made InitialDecisionDelay after the spawn; with nothing to do it is a Wander decision, and the servant
	// then waits for its next decision (RetryDelay, no NavMesh): this is the "pause d'errance" of the spec.
	TickFor(InitialDecisionDelay + 2.0 * TickStep);
	const bool bWander = Controller->GetLastAction() == EServantWanderAction::Wander;
	return TestTrue(TEXT("precondition: the first decision was Wander (nothing to do yet): the servant rests"), bWander);
}

void FServantReplantWorldSpec::TickFor(double Seconds)
{
	SFT::TickFor(*Wrapper, Seconds);
}

bool FServantReplantWorldSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	return SFT::TickUntil(*Wrapper, Condition, MaxSeconds);
}

void FServantReplantWorldSpec::TickWatching(double Seconds, TFunctionRef<void()> Observer)
{
	SFT::TickWatching(*Wrapper, Seconds, Observer);
}

bool FServantReplantWorldSpec::IsReservedNow(const AFieldPlot* Field) const
{
	const UFieldPlotSubsystem* Subsystem = World->GetSubsystem<UFieldPlotSubsystem>();
	return Subsystem != nullptr && Subsystem->IsReserved(Field);
}

bool FServantReplantWorldSpec::IsInState(const AFieldPlot* Field, EFieldState State) const
{
	EFieldState Actual = EFieldState::Empty;
	return FieldPlotTest::GetState(Field, Actual) && Actual == State;
}

void FServantReplantWorldSpec::ExpectCounters(const FString& What, const UResourceCounterComponent* Counter, int32 Wheat, int32 Seed)
{
	// Wood, Stone and Bone must never move (checked by the shared helper).
	SFT::ExpectCounters(*this, What, Counter, Wheat, Seed);
}

void FServantReplantWorldSpec::RunBoundaryCase(const FBoundaryCase& Case)
{
	if (!RequireWorld()) { return; }
	SFT::SpawnZone(World, TEXT("ZoneBoundary"));
	UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, Case.Stock);
	AFieldPlot* Field = SpawnField(TEXT("FieldBoundary"), NearField, EFieldState::Grown);
	AServant* Servant = nullptr;
	AServantAIController* Controller = SetupServant(TEXT("ServantBoundary"), ServantLocation, Servant);
	if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
	if (!TestTrue(TEXT("harness: SeedCost written"), FieldPlotTest::SetSeedCost(Field, 3))) { return; }

	const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; }, InitialDecisionDelay + TimerSlack);
	if (!TestTrue(TEXT("the first decision is Harvest"), bHarvest)) { return; }
	TickFor(TickStep);

	// Stock after the credit = Case.Stock + YieldSeed (2). The replant needs 3.
	if (Case.bExpectPlanted)
	{
		ExpectCounters(TEXT("planted: the whole cost is spent"), Counter, BaseWheat + YieldWheat, Case.Stock + YieldSeed - 3);
		FieldPlotTest::ExpectState(*this, TEXT("planted"), Field, EFieldState::Growing);
	}
	else
	{
		ExpectCounters(TEXT("not planted: nothing is spent, not even a part"), Counter, BaseWheat + YieldWheat, Case.Stock + YieldSeed);
		FieldPlotTest::ExpectState(*this, TEXT("not planted"), Field, EFieldState::Empty);
	}
	TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
}

void FServantReplantWorldSpec::RunPlantIdleCase(const FPlantIdleCase& Case)
{
	if (!RequireWorld()) { return; }
	if (Case.bZone) { SFT::SpawnZone(World, TEXT("IdleZone")); }
	UResourceCounterComponent* Counter = Case.bPlayer ? SFT::SpawnPlayer(World, Case.bCounter, BaseWheat, Case.Seed) : nullptr;
	AFieldPlot* Field = SpawnField(TEXT("IdleField"), Case.FieldLocation, Case.FieldState);
	AActor* Holder = FieldPlotTest::SpawnPlainActor(World, TEXT("IdleHolder"));
	AActor* Other = FieldPlotTest::SpawnPlainActor(World, TEXT("IdleOther"));
	UFieldPlotSubsystem* Fields = World->GetSubsystem<UFieldPlotSubsystem>();
	if (Field == nullptr || Holder == nullptr || Other == nullptr || Fields == nullptr) { AddError(TEXT("harness: actors or subsystem missing")); return; }
	if (Case.bReservedByOther && !TestTrue(TEXT("harness: the holder reserves the field"), Fields->TryReserve(Field, Holder))) { return; }
	AServant* Servant = nullptr;
	AServantAIController* Controller = SetupServant(TEXT("IdleServant"), ServantLocation, Servant, Case.Task);
	if (Controller == nullptr) { return; }
	if (Case.bPlayer && !TestNotNull(TEXT("harness: GetPlayerPawn(0) resolves"), UGameplayStatics::GetPlayerPawn(World, 0))) { return; }
	if (Case.bPlayer && Case.bCounter && !TestNotNull(TEXT("harness: player counter"), Counter)) { return; }

	bool bEverReserved = false;
	bool bEverActive = false;
	TickWatching(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack, [&]()
	{
		// When another actor holds the reservation the field is of course "reserved": only the servant's own would be a fault, checked below.
		bEverReserved |= (!Case.bReservedByOther && IsReservedNow(Field));
		const EServantWanderAction Action = Controller->GetLastAction();
		bEverActive |= (Action == EServantWanderAction::Plant || Action == EServantWanderAction::Harvest
			|| Controller->IsPlanting() || Controller->IsHarvesting());
	});

	TestFalse(TEXT("the servant never reserved the field, not even transiently"), bEverReserved);
	TestFalse(TEXT("the servant was never planting nor harvesting"), bEverActive);
	ServantFieldTest::ExpectAction(*this, TEXT("after several decisions"), Controller->GetLastAction(), EServantWanderAction::Wander);
	TestNull(TEXT("GetPlantTarget() is null"), Controller->GetPlantTarget());
	TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
	FieldPlotTest::ExpectState(*this, TEXT("the field is untouched"), Field, Case.FieldState);
	if (Counter != nullptr)
	{
		ExpectCounters(TEXT("counter unchanged"), Counter, BaseWheat, Case.Seed);
	}
	if (Case.bReservedByOther)
	{
		TestTrue(TEXT("the foreign reservation is still held by its owner"), Fields->TryReserve(Field, Holder));
		TestFalse(TEXT("nobody else, the servant included, took the field"), Fields->TryReserve(Field, Other));
	}
}

void FServantReplantWorldSpec::RunPromiseCase(const FPromiseCase& Case)
{
	if (!RequireWorld()) { return; }
	SFT::SpawnZone(World, TEXT("ZonePromise"));
	UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, Case.Stock);
	// Two Empty fields at the same distance from the servant: the "promised" one is held by a plain actor (stand-in for a servant on its way).
	AFieldPlot* Promised = SpawnField(TEXT("FieldPromised"), FVector(-50.0, 0.0, 0.0), EFieldState::Empty);
	AFieldPlot* Free = SpawnField(TEXT("FieldFree"), FVector(50.0, 0.0, 0.0), EFieldState::Empty);
	AActor* Holder = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseHolder"));
	AActor* Other = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseOther"));
	UFieldPlotSubsystem* Fields = World->GetSubsystem<UFieldPlotSubsystem>();
	AServant* Servant = nullptr;
	AServantAIController* Controller = SetupServant(TEXT("ServantPromise"), ServantLocation, Servant);
	if (Controller == nullptr || !Promised || !Free || !Holder || !Other || !Fields || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
	if (!TestTrue(TEXT("harness: the holder reserves the promised field"), Fields->TryReserve(Promised, Holder))) { return; }
	TestEqual(TEXT("harness: that reservation promises SeedCost seeds to another actor"), Fields->GetPromisedSeeds(Other), PlantCost);

	if (Case.bExpectImmediatePlant)
	{
		// Stock 2, 1 promised: 1 is available, which is the cost of the free field.
		const bool bPlant = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Plant; }, InitialDecisionDelay + TimerSlack);
		TestTrue(TEXT("the servant plants the free field: the stock beyond the promise is enough"), bPlant);
		TickFor(TickStep);
		FieldPlotTest::ExpectState(*this, TEXT("the free field"), Free, EFieldState::Growing);
		FieldPlotTest::ExpectState(*this, TEXT("the promised field is left alone"), Promised, EFieldState::Empty);
		TestEqual(TEXT("exactly one SeedCost spent"), Counter->GetResourceCount(EResourceType::Seed), Case.Stock - PlantCost);
		TestTrue(TEXT("the promise of the holder is intact"), Fields->TryReserve(Promised, Holder));
		TestFalse(TEXT("nobody took the promised field"), Fields->TryReserve(Promised, Other));
		return;
	}

	// Stock 1, 1 promised: nothing is available, the free field must stay untouched for several decisions...
	bool bEverActive = false;
	TickWatching(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack, [&]()
	{
		const EServantWanderAction Action = Controller->GetLastAction();
		bEverActive |= (Action == EServantWanderAction::Plant || Action == EServantWanderAction::Harvest || Controller->IsPlanting());
		bEverActive |= IsReservedNow(Free);
	});
	TestFalse(TEXT("the servant neither reserved nor planted: the only seed is promised to another"), bEverActive);
	FieldPlotTest::ExpectState(*this, TEXT("the free field"), Free, EFieldState::Empty);
	FieldPlotTest::ExpectState(*this, TEXT("the promised field"), Promised, EFieldState::Empty);
	ExpectCounters(TEXT("stock untouched"), Counter, BaseWheat, Case.Stock);

	// ...and positive control: once the promise is released the very same seed is available and the servant plants.
	Fields->Release(Promised, Holder);
	const bool bPlanted = TickUntil([this, Free, Promised]() { return IsInState(Free, EFieldState::Growing) || IsInState(Promised, EFieldState::Growing); },
	                                2.0 * RetryDelay + MaxWanderPause + TimerSlack);
	TestTrue(TEXT("control: once the promise is released the servant plants (at a following decision)"), bPlanted);
	TestEqual(TEXT("control: one SeedCost spent"), Counter->GetResourceCount(EResourceType::Seed), Case.Stock - PlantCost);
}

void FServantReplantWorldSpec::Define()
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

	Describe("Harvest then replant on the spot (criterion 4)", [this]()
	{
		// Mutation: the replant is not done (field left Empty) -> the Growing line and the Seed amount (one SeedCost too many) turn red;
		//           it spends something other than SeedCost, or on another counter -> the Seed amount turns red;
		//           a drop is spawned instead of crediting -> drop count turns red;
		//           the replant sets LastAction = Plant (spec: "garde LastAction == Harvest") -> the action line turns red;
		//           the replant is treated as a trip (IsPlanting stays true / target kept) -> those lines turn red;
		//           the next decision does not wait HarvestNextDelay -> the "still Harvest" line turns red;
		//           the reservation is kept after the replant -> the IsReserved line turns red.
		It("credits the yield, spends SeedCost to replant the same field, keeps the action Harvest, then goes back to Wander", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneReplant"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, BaseSeed);
			AFieldPlot* Field = SpawnField(TEXT("FieldReplant"), NearField, EFieldState::Grown);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantReplant"), ServantLocation, Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }

			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; }, InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is Harvest"), bHarvest)) { return; }
			TickFor(TickStep);

			ExpectCounters(TEXT("credited then replanted"), Counter, BaseWheat + YieldWheat, BaseSeed + YieldSeed - PlantCost);
			FieldPlotTest::ExpectState(*this, TEXT("after the harvest"), Field, EFieldState::Growing);
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
			ServantFieldTest::ExpectAction(*this, TEXT("the replant keeps the action Harvest"), Controller->GetLastAction(), EServantWanderAction::Harvest);
			TestFalse(TEXT("the replant on the spot is not a trip: IsPlanting() is false"), Controller->IsPlanting());
			TestNull(TEXT("GetPlantTarget() is null"), Controller->GetPlantTarget());

			TickFor(0.5 * HarvestNextDelay);
			ServantFieldTest::ExpectAction(*this, TEXT("before HarvestNextDelay the action is still Harvest"), Controller->GetLastAction(), EServantWanderAction::Harvest);
			const bool bNext = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::Harvest; }, 0.5 * HarvestNextDelay + TimerSlack);
			TestTrue(TEXT("a decision happens after HarvestNextDelay"), bNext);
			ServantFieldTest::ExpectAction(*this, TEXT("the field is Growing, nothing left to do"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestFalse(TEXT("the reservation is released"), IsReservedNow(Field));

			TickFor(3.0 * RetryDelay);
			ExpectCounters(TEXT("still one harvest and one replant"), Counter, BaseWheat + YieldWheat, BaseSeed + YieldSeed - PlantCost);
			FieldPlotTest::ExpectState(*this, TEXT("still Growing"), Field, EFieldState::Growing);
		});

		// Mutation: the replant checks / spends the stock BEFORE the yield is credited -> with a stock of 0 the field stays Empty and
		//           Seed ends at 2: the Growing line and the Seed amount (1) turn red.
		It("replants with the seed that comes from the very yield when the stock was 0", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneSeedFromYield"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
			AFieldPlot* Field = SpawnField(TEXT("FieldSeedFromYield"), NearField, EFieldState::Grown);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantSeedFromYield"), ServantLocation, Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			TestEqual(TEXT("precondition: no seed in stock"), Counter->GetResourceCount(EResourceType::Seed), 0);

			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; }, InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is Harvest (a Grown field is harvested whatever the stock)"), bHarvest)) { return; }
			TickFor(TickStep);

			ExpectCounters(TEXT("the yield's seed was spent on the replant"), Counter, BaseWheat + YieldWheat, YieldSeed - PlantCost);
			FieldPlotTest::ExpectState(*this, TEXT("replanted"), Field, EFieldState::Growing);
		});

		// Mutation: the replant is attempted without enough seeds and leaves the field in a wrong state, or the stock goes negative -> red;
		//           the reservation is not released after a harvest without replant -> the IsReserved line turns red;
		//           the servant keeps trying to plant (Plant action) with a stock of 0 -> the Wander line turns red.
		It("without a seed in stock nor in the yield: the harvest succeeds, the field is left Empty, the reservation is released, then Wander", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneNoSeed"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
			AFieldPlot* Field = SpawnField(TEXT("FieldNoSeed"), NearField, EFieldState::Grown);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantNoSeed"), ServantLocation, Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			// Yield: Wheat only (4), no seed at all.
			TArray<FResourceDropEntry> Table;
			Table.Add(FieldPlotTest::MakeEntry(nullptr, EResourceType::Wheat, 4, 1));
			if (!TestTrue(TEXT("harness: drops written"), FieldPlotTest::SetDrops(Field, Table))) { return; }

			const bool bHarvest = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Harvest; }, InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is Harvest"), bHarvest)) { return; }
			TickFor(TickStep);
			ExpectCounters(TEXT("harvest credited, nothing replanted"), Counter, BaseWheat + 4, 0);
			FieldPlotTest::ExpectState(*this, TEXT("left Empty"), Field, EFieldState::Empty);

			const bool bNext = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::Harvest; }, HarvestNextDelay + TimerSlack);
			TestTrue(TEXT("the next decision comes after HarvestNextDelay"), bNext);
			ServantFieldTest::ExpectAction(*this, TEXT("then the servant wanders (Empty field, no seed)"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestFalse(TEXT("the reservation is released"), IsReservedNow(Field));
			TickFor(2.0 * RetryDelay);
			FieldPlotTest::ExpectState(*this, TEXT("still Empty"), Field, EFieldState::Empty);
			ExpectCounters(TEXT("counter unchanged since the harvest"), Counter, BaseWheat + 4, 0);
		});

		// Boundary of "stock >= SeedCost" with a cost of 3 and a yield of 2 seeds.
		// Mutation: the comparison is strict (stock > cost) -> the "stock 1 + yield 2 = cost 3" row stays Empty: red;
		//           the replant spends what is available when it is not enough (partial spend), or plants anyway -> the "stock 0" row turns red.
		{
			TArray<FBoundaryCase> Cases;
			{
				FBoundaryCase Row;
				Row.Name = TEXT("stock 1 + yield 2 reaches SeedCost 3: planted");
				Row.Stock = 1;
				Row.bExpectPlanted = true;
				Cases.Add(Row);
			}
			{
				FBoundaryCase Row;
				Row.Name = TEXT("stock 0 + yield 2 is below SeedCost 3: left Empty, nothing spent");
				Row.Stock = 0;
				Row.bExpectPlanted = false;
				Cases.Add(Row);
			}
			for (const FBoundaryCase& Case : Cases)
			{
				It(*Case.Name, [this, Case]()
				{
					RunBoundaryCase(Case);
				});
			}
		}
	});

	Describe("Planting without a Grown field and priority (criterion 5)", [this]()
	{
		// Mutation: the controller never plants (Empty fields ignored) -> the Plant decision is not seen: red;
		//           it spends something other than SeedCost, or credits a drop -> the counters / drop count turn red;
		//           the field is left Empty (plant not applied) -> the Growing line turns red;
		//           a standalone plant does not set LastAction = Plant (inferred from "Plant ajoute a EServantWanderAction") -> red;
		//           the reservation is kept -> the IsReserved line turns red; the next decision ignores HarvestNextDelay -> "still Plant" red.
		It("plants an Empty field in reach with the stock, spending SeedCost, then waits HarvestNextDelay", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZonePlant"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, BaseSeed);
			AFieldPlot* Field = SpawnField(TEXT("FieldPlant"), NearField, EFieldState::Empty);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantPlant"), ServantLocation, Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: field"), Field) || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }

			const bool bPlant = TickUntil([Controller]() { return Controller->GetLastAction() == EServantWanderAction::Plant; }, InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is Plant (no Grown field, stock available)"), bPlant)) { return; }
			TickFor(TickStep);

			ExpectCounters(TEXT("one SeedCost spent"), Counter, BaseWheat, BaseSeed - PlantCost);
			FieldPlotTest::ExpectState(*this, TEXT("after the plant"), Field, EFieldState::Growing);
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
			TestFalse(TEXT("IsPlanting() is false: the plant was done on the spot"), Controller->IsPlanting());
			TestNull(TEXT("GetPlantTarget() is null"), Controller->GetPlantTarget());

			TickFor(0.5 * HarvestNextDelay);
			ServantFieldTest::ExpectAction(*this, TEXT("before HarvestNextDelay the action is still Plant"), Controller->GetLastAction(), EServantWanderAction::Plant);
			const bool bNext = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::Plant; }, 0.5 * HarvestNextDelay + TimerSlack);
			TestTrue(TEXT("a decision happens after HarvestNextDelay"), bNext);
			ServantFieldTest::ExpectAction(*this, TEXT("then nothing left to do: Wander"), Controller->GetLastAction(), EServantWanderAction::Wander);
			TestFalse(TEXT("the reservation is released"), IsReservedNow(Field));
			TickFor(3.0 * RetryDelay);
			ExpectCounters(TEXT("planted exactly once"), Counter, BaseWheat, BaseSeed - PlantCost);
		});

		// Mutation: the controller picks the closest field whatever its state (the Empty one, 40 cm, before the Grown one) -> after the
		//           first decision the Grown field is still Grown and the stock is 4: the state / Seed lines turn red;
		//           it plants Empty fields first, then harvests -> same lines turn red;
		//           the Empty field is never planted afterwards (loop stops after the harvest) -> the final Growing line turns red.
		It("handles a Grown field before an Empty one, even if the Empty one is closer, then plants the Empty one", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZonePriority"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, BaseSeed);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantPriority"), ServantLocation, Servant);
			if (Controller == nullptr || !TestNotNull(TEXT("harness: counter"), Counter) || !TestNotNull(TEXT("harness: capsule"), Servant->GetCapsuleComponent())) { return; }
			// Both fields are within immediate reach (no NavMesh): the Empty one at 40 cm, the Grown one just inside HarvestReach + capsule
			// radius (the largest immediate reach, read by the existing harvest spec from the same tunable), on the opposite side.
			double HarvestReach = 0.0;
			if (!TestTrue(TEXT("harness: HarvestReach read"), ServantWanderTest::ReadNumber(Controller, TEXT("HarvestReach"), HarvestReach))) { return; }
			const double GrownDistance = HarvestReach + Servant->GetCapsuleComponent()->GetScaledCapsuleRadius() - 5.0;
			AFieldPlot* EmptyField = SpawnField(TEXT("FieldPriorityEmpty"), FVector(40.0, 0.0, 0.0), EFieldState::Empty);
			AFieldPlot* GrownField = SpawnField(TEXT("FieldPriorityGrown"), FVector(-GrownDistance, 0.0, 0.0), EFieldState::Grown);
			if (!TestTrue(TEXT("harness: fields"), EmptyField && GrownField)) { return; }

			const bool bFirst = TickUntil([Controller]() { return Controller->GetLastAction() != EServantWanderAction::None && Controller->GetLastAction() != EServantWanderAction::Wander; },
			                              InitialDecisionDelay + TimerSlack);
			if (!TestTrue(TEXT("the first decision is an action"), bFirst)) { return; }
			TickFor(TickStep);
			ServantFieldTest::ExpectAction(*this, TEXT("first decision"), Controller->GetLastAction(), EServantWanderAction::Harvest);
			FieldPlotTest::ExpectState(*this, TEXT("the Grown field was harvested and replanted first"), GrownField, EFieldState::Growing);
			FieldPlotTest::ExpectState(*this, TEXT("the closer Empty field waits"), EmptyField, EFieldState::Empty);
			TestFalse(TEXT("the Empty field is not reserved"), IsReservedNow(EmptyField));
			ExpectCounters(TEXT("after the first decision: one harvest, one replant"), Counter, BaseWheat + YieldWheat, BaseSeed + YieldSeed - PlantCost);

			const bool bSecond = TickUntil([this, EmptyField]() { return IsInState(EmptyField, EFieldState::Growing); }, HarvestNextDelay + TimerSlack);
			TestTrue(TEXT("the Empty field is planted at the next decision"), bSecond);
			ExpectCounters(TEXT("after the second decision: two plantations"), Counter, BaseWheat + YieldWheat, BaseSeed + YieldSeed - 2 * PlantCost);
		});
	});

	Describe("Nothing to plant (criterion 6)", [this]()
	{
		// Mutation (one per row, all of the form "the controller reserves / plants although the rule says not to"):
		//   Unassigned / AttackDefense -> the task is ignored for planting (or the reservation is taken before the task check);
		//   stock 0 -> the stock is not checked before the reservation (a transient reservation or a Plant decision is seen);
		//   no player / no counter -> the storage is not checked (crash, or a Plant decision with no seeds);
		//   field outside the zone -> IsPointInZone(field) is not checked for Empty fields;
		//   Growing field -> the eligibility test ignores the state; reserved by another -> the reservation of others is ignored.
		// Positive control: the same setup with FieldHarvest, stock 5 and a free Empty field in reach is the "plants an Empty field" test
		// above, so a row cannot pass because the setup is unable to plant at all.
		TArray<FPlantIdleCase> Cases;
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("Unassigned servant");
			Row.Task = EServantTask::Unassigned;
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("AttackDefense servant");
			Row.Task = EServantTask::AttackDefense;
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("stock insufficient (0 seed)");
			Row.Seed = 0;
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("no player at all");
			Row.bPlayer = false;
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("a player pawn without a resource counter");
			Row.bCounter = false;
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("an Empty field outside the zone");
			Row.FieldLocation = FVector(3000.0, 0.0, 0.0);
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("a Growing field in reach");
			Row.FieldState = EFieldState::Growing;
			Cases.Add(Row);
		}
		{
			FPlantIdleCase Row;
			Row.Name = TEXT("an Empty field reserved by another actor");
			Row.bReservedByOther = true;
			Cases.Add(Row);
		}
		for (const FPlantIdleCase& Case : Cases)
		{
			It(*FString::Printf(TEXT("%s: no reservation, field and stock intact, action Wander"), *Case.Name), [this, Case]()
			{
				RunPlantIdleCase(Case);
			});
		}
	});

	Describe("Seed budget between servants (criterion 7)", [this]()
	{
		// The spec scenario, with the two servants in reach (see the header: here the first one resolves before the second decides).
		// Mutation: the controller does not check the available stock before choosing (reserves and "goes" with no seed) -> the second servant
		//           also reaches the Plant action: "exactly one servant" turns red;
		//           the stock is spent twice, or both fields are planted -> the Seed amount / the field states turn red;
		//           a reservation is left behind -> the IsReserved lines turn red.
		It("two Empty fields, stock for one plantation, two servants: one plants, the other stays idle, one SeedCost spent", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneBudget"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, PlantCost);
			AFieldPlot* FieldA = SpawnField(TEXT("BudgetFieldA"), FVector(50.0, 0.0, 0.0), EFieldState::Empty);
			AFieldPlot* FieldB = SpawnField(TEXT("BudgetFieldB"), FVector(-70.0, 0.0, 0.0), EFieldState::Empty);
			AServant* ServantA = nullptr;
			AServant* ServantB = nullptr;
			AServantAIController* ControllerA = SetupServant(TEXT("BudgetServantA"), ServantLocation, ServantA);
			AServantAIController* ControllerB = SetupServant(TEXT("BudgetServantB"), ServantLocation + FVector(0.0, 40.0, 0.0), ServantB);
			if (ControllerA == nullptr || ControllerB == nullptr || !FieldA || !FieldB || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }

			bool bPlantA = false;
			bool bPlantB = false;
			bool bEverReservedA = false;
			bool bEverReservedB = false;
			TickWatching(InitialDecisionDelay + 3.0 * RetryDelay + TimerSlack, [&]()
			{
				bPlantA |= (ControllerA->GetLastAction() == EServantWanderAction::Plant);
				bPlantB |= (ControllerB->GetLastAction() == EServantWanderAction::Plant);
				bEverReservedA |= IsReservedNow(FieldA);
				bEverReservedB |= IsReservedNow(FieldB);
			});

			TestTrue(TEXT("exactly one of the two servants was sent to plant"), bPlantA != bPlantB);
			TestEqual(TEXT("exactly one field is Growing"), (IsInState(FieldA, EFieldState::Growing) ? 1 : 0) + (IsInState(FieldB, EFieldState::Growing) ? 1 : 0), 1);
			TestEqual(TEXT("exactly one field is still Empty"), (IsInState(FieldA, EFieldState::Empty) ? 1 : 0) + (IsInState(FieldB, EFieldState::Empty) ? 1 : 0), 1);
			ExpectCounters(TEXT("exactly one SeedCost spent"), Counter, BaseWheat, 0);
			const AFieldPlot* StillEmpty = IsInState(FieldA, EFieldState::Empty) ? FieldA : FieldB;
			TestFalse(TEXT("the field left Empty was never reserved"), StillEmpty == FieldA ? bEverReservedA : bEverReservedB);
			TestFalse(TEXT("no reservation left on A"), IsReservedNow(FieldA));
			TestFalse(TEXT("no reservation left on B"), IsReservedNow(FieldB));
		});

		// The real exercise of the "promised seeds" rule: a plain actor holds a reservation on an Empty field (= a servant on its way),
		// a real servant decides with a stock that only covers ONE plantation.
		// Mutation: the available stock ignores the promises of others (Seed alone) -> stock 1: the servant plants the free field: red;
		//           the promises are counted twice or the check is strict (available > cost) -> stock 2: no plant: red;
		//           GetPromisedSeeds is not subtracted for the servant's decision but only for the replant on the spot -> stock 1 red.
		{
			TArray<FPromiseCase> Cases;
			{
				FPromiseCase Row;
				Row.Name = TEXT("stock 1, 1 seed promised to another reserver: not available, then planted once the promise is released");
				Row.Stock = 1;
				Row.bExpectImmediatePlant = false;
				Cases.Add(Row);
			}
			{
				FPromiseCase Row;
				Row.Name = TEXT("stock 2, 1 seed promised: the seed beyond the promise is available, the free field is planted");
				Row.Stock = 2;
				Row.bExpectImmediatePlant = true;
				Cases.Add(Row);
			}
			for (const FPromiseCase& Case : Cases)
			{
				It(*Case.Name, [this, Case]()
				{
					RunPromiseCase(Case);
				});
			}
		}
	});

	Describe("Wake-up by field change (criterion 8)", [this]()
	{
		// A reflection write of the state does not notify: the test of the polling safety net below relies on it.
		// Mutation: AFieldPlot does not notify when the growth ends, or the controller does not subscribe / ignores the event
		//           -> the field is only seen at the next normal decision (about RetryDelay later): the TickUntil window
		//           (growth + MaxWakeDelay + slack) expires first: red;
		//           the wake timer ignores MaxWakeDelay (set to RetryDelay or left untouched) -> same window: red;
		//           the wake does not require "Grown" and wakes for Growing too -> not detected here (harmless, see the report).
		It("a field that becomes Grown while the servant rests is harvested within MaxWakeDelay, long before the retry timer", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneWakeGrown"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
			AFieldPlot* Field = SpawnField(TEXT("FieldWakeGrown"), NearField, EFieldState::Empty);
			AActor* Planter = FieldPlotTest::SpawnPlainActor(World, TEXT("WakeGrownPlanter"));
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantWakeGrown"), ServantLocation, Servant);
			if (Controller == nullptr || !Field || !Planter || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWakeWindow(WakeGrowth)) { return; }
			if (!TestTrue(TEXT("harness: GrowthDuration written"), FieldPlotTest::SetGrowthDuration(Field, WakeGrowth))) { return; }
			// The test plants with its own seed so that the player's stock stays 0 (the Empty field is NOT plantable by the servant).
			UResourceCounterComponent* PlanterStock = FieldPlotTest::MakeCounter(Planter, 0, PlantCost);

			if (!StartResting(Controller)) { return; }
			ExpectCounters(TEXT("nothing happened during the first decision"), Counter, BaseWheat, 0);

			TestTrue(TEXT("harness: the test plants the field"), Field->TryPlant(PlanterStock));
			const bool bHarvested = TickUntil([Counter]() { return Counter->GetResourceCount(EResourceType::Wheat) > BaseWheat; },
			                                  WakeGrowth + MaxWakeDelay + TimerSlack);
			TestTrue(TEXT("harvested within growth + MaxWakeDelay + slack (the normal timer would need about RetryDelay)"), bHarvested);
			TickFor(TickStep);
			ExpectCounters(TEXT("harvested then replanted with the seed of the yield"), Counter, BaseWheat + YieldWheat, YieldSeed - PlantCost);
			FieldPlotTest::ExpectState(*this, TEXT("replanted"), Field, EFieldState::Growing);
		});

		// Mutation: the controller is not woken by a field becoming Empty -> the field is planted at the next normal decision, after the window: red;
		//           the wake fires but the decision plants nothing (stock ignored) -> red.
		It("a field that becomes Empty (harvested by someone else) while the servant rests, with a stock, is planted within MaxWakeDelay", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneWakeEmpty"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 3);
			AFieldPlot* Field = SpawnField(TEXT("FieldWakeEmpty"), NearField, EFieldState::Grown);
			AActor* Holder = FieldPlotTest::SpawnPlainActor(World, TEXT("WakeEmptyHolder"));
			UFieldPlotSubsystem* Fields = World->GetSubsystem<UFieldPlotSubsystem>();
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantWakeEmpty"), ServantLocation, Servant);
			if (Controller == nullptr || !Field || !Holder || !Fields || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWakeWindow(0.0)) { return; }
			// The Grown field is held by another actor, so the servant has nothing to do and rests.
			if (!TestTrue(TEXT("harness: the holder reserves the Grown field"), Fields->TryReserve(Field, Holder))) { return; }

			if (!StartResting(Controller)) { return; }
			FieldPlotTest::ExpectState(*this, TEXT("the reserved Grown field was left alone"), Field, EFieldState::Grown);

			Fields->Release(Field, Holder); // silent: the servant is not told, it still rests
			TMap<EResourceType, int32> Yield;
			TestTrue(TEXT("harness: someone else harvests the field (Grown -> Empty, notified)"), Field->TryHarvestYield(Yield));
			const bool bPlanted = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, MaxWakeDelay + TimerSlack);
			TestTrue(TEXT("planted within MaxWakeDelay + slack"), bPlanted);
			TestEqual(TEXT("one SeedCost spent"), Counter->GetResourceCount(EResourceType::Seed), 3 - PlantCost);
			TestEqual(TEXT("the Wheat of the harvest by someone else is not the servant's"), Counter->GetResourceCount(EResourceType::Wheat), BaseWheat);
		});
	});

	Describe("Wake-up by seeds (criterion 9)", [this]()
	{
		// Mutation: the controller does not subscribe to the counter, or does not wake on Seed -> the field is planted at the next normal
		//           decision, after the window: red; the wake happens but the decision does not use the new seed -> red.
		It("seeds arriving in the stock while the servant rests with an Empty field: planted within MaxWakeDelay", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneWakeSeed"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 0);
			AFieldPlot* Field = SpawnField(TEXT("FieldWakeSeed"), NearField, EFieldState::Empty);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantWakeSeed"), ServantLocation, Servant);
			if (Controller == nullptr || !Field || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWakeWindow(0.0)) { return; }

			if (!StartResting(Controller)) { return; }
			FieldPlotTest::ExpectState(*this, TEXT("no seed: the Empty field was left alone"), Field, EFieldState::Empty);

			Counter->AddResource(EResourceType::Seed, 1);
			const bool bPlanted = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, MaxWakeDelay + TimerSlack);
			TestTrue(TEXT("planted within MaxWakeDelay + slack"), bPlanted);
			ExpectCounters(TEXT("the arrived seed was spent"), Counter, BaseWheat, 1 - PlantCost);
		});

		// The state of the field is changed by reflection (SILENT: no notification) and a Wood arrives (NOT a seed): nothing must wake the
		// servant. Then the polling safety net is checked: the normal timer does see the change at the next decision.
		// Mutation: the controller wakes on any resource type -> the field is planted within the window: red;
		//           the decision timer is removed / replaced by the wake-ups (no polling) -> the final TickUntil never sees the plant: red;
		//           this test also proves that the windows of the other wake-up tests are shorter than the normal cycle (the same silent
		//           change is NOT seen inside the window).
		It("a resource other than Seed does not wake the servant; the normal timer still picks the change up later", [this]()
		{
			if (!RequireWorld()) { return; }
			SFT::SpawnZone(World, TEXT("ZoneNoWake"));
			UResourceCounterComponent* Counter = SFT::SpawnPlayer(World, true, BaseWheat, 1);
			AFieldPlot* Field = SpawnField(TEXT("FieldNoWake"), NearField, EFieldState::Growing);
			AServant* Servant = nullptr;
			AServantAIController* Controller = SetupServant(TEXT("ServantNoWake"), ServantLocation, Servant);
			if (Controller == nullptr || !Field || !TestNotNull(TEXT("harness: counter"), Counter)) { return; }
			if (!CheckWakeWindow(0.0)) { return; }

			if (!StartResting(Controller)) { return; }
			TestTrue(TEXT("harness: silent state change to Empty"), FieldPlotTest::SetState(Field, EFieldState::Empty));
			Counter->AddResource(EResourceType::Wood, 1);

			TickFor(MaxWakeDelay + TimerSlack);
			FieldPlotTest::ExpectState(*this, TEXT("a Wood arrival did not wake the servant: the Empty field is still not planted"), Field, EFieldState::Empty);
			TestEqual(TEXT("Seed untouched"), Counter->GetResourceCount(EResourceType::Seed), 1);
			ServantFieldTest::ExpectAction(*this, TEXT("still resting"), Controller->GetLastAction(), EServantWanderAction::Wander);

			const bool bPlanted = TickUntil([this, Field]() { return IsInState(Field, EFieldState::Growing); }, 2.0 * RetryDelay + MaxWanderPause + TimerSlack);
			TestTrue(TEXT("control: the normal decision timer plants the field later (the polling remains a safety net)"), bPlanted);
			TestEqual(TEXT("control: the seed was spent"), Counter->GetResourceCount(EResourceType::Seed), 1 - PlantCost);
			TestEqual(TEXT("Wood untouched"), Counter->GetResourceCount(EResourceType::Wood), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
