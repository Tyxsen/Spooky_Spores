// Automation Spec for AFieldPlot (harvest yield) and UFieldPlotSubsystem (registry and reservation).
// Source of truth: the servant field harvest spec (docs/specs/servant-field-harvest.md, removed at merge), acceptance criteria 1 to 5
// and the edge case "fields without DropClass are credited anyway".
//
// Item 2.2b (docs/specs/servant-replant.md, removed at merge) adds: TryPlant / IsReadyToPlant / GetSeedCost, GetUnreservedPlantableFields,
// GetPromisedSeeds and the UFieldPlotSubsystem::OnFieldChanged notification (acceptance criteria 1 to 3 of that spec, plus the notification
// rules of its "Notification" section and the edge case "a reserved Grown that becomes Empty is counted as promised").
//
// World: FTestWorldWrapper (Engine, Tests/AutomationCommon.h), Game world with BeginPlay done, so a spawned AFieldPlot registers in
// BeginPlay and Destroy() runs EndPlay (unregistration) synchronously. The world is only ticked by the growth tests (GrowthDuration reduced
// by reflection): everywhere else only the calls of the test change state.
// The state of a field is written through reflection (Tests/FieldPlotTestHelpers.h), the API has no setter. A reflection write does NOT
// notify: the notification tests therefore only count events caused by real API calls.
// Every test lists its mutation: the change of behaviour that must turn it red.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/FieldPlotTestHelpers.h"
#include "Resources/FieldPlot.h"
#include "Resources/FieldPlotSubsystem.h"
#include "Resources/ResourceCounterComponent.h"
#include "Resources/ResourceDrop.h"
#include "Resources/ResourceTypes.h"
#include "Interaction/Interactable.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "UObject/Class.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FFieldPlotSpec, "SpookySpores.Resources.FieldPlot",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	UFieldPlotSubsystem* Subsystem = nullptr;
	bool bWorldReady = false;

	// Events recorded from UFieldPlotSubsystem::OnFieldChanged (the state is read in the callback: "notified after the change").
	struct FRecordedEvent
	{
		AFieldPlot* Field = nullptr;
		bool bRemoved = false;
		bool bStateReadable = false;
		EFieldState StateAtCall = EFieldState::Empty;
	};
	TArray<FRecordedEvent> Events;
	FDelegateHandle EventsHandle;

	// Growth duration written on the fields of the growth tests (seconds of test-world time) and tick used to advance it.
	static constexpr double ShortGrowth = 0.4;
	static constexpr float GrowthTick = 0.1f;

	void StartRecording();
	int32 CountEvents(const AFieldPlot* Field, bool bRemoved) const;
	void TickGrowth(int32 Steps);
	bool RequireWorld();
	AFieldPlot* SpawnField(const TCHAR* Name, EFieldState State);
	void ExpectState(const FString& What, const AFieldPlot* Field, EFieldState Expected);
	void ExpectYield(const FString& What, const TMap<EResourceType, int32>& Actual, const TMap<EResourceType, int32>& Expected);
	bool ListContains(const AFieldPlot* Field) const;

END_DEFINE_SPEC(FFieldPlotSpec)

bool FFieldPlotSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world or UFieldPlotSubsystem unavailable: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

AFieldPlot* FFieldPlotSpec::SpawnField(const TCHAR* Name, EFieldState State)
{
	return FieldPlotTest::SpawnField(World, Name, FVector::ZeroVector, State);
}

void FFieldPlotSpec::ExpectState(const FString& What, const AFieldPlot* Field, EFieldState Expected)
{
	FieldPlotTest::ExpectState(*this, What, Field, Expected);
}

void FFieldPlotSpec::StartRecording()
{
	EventsHandle = Subsystem->OnFieldChanged.AddLambda([this](AFieldPlot* Field, bool bRemoved)
	{
		FRecordedEvent Event;
		Event.Field = Field;
		Event.bRemoved = bRemoved;
		if (!bRemoved)
		{
			// A field being removed is not safe to read: only live fields have their state captured.
			Event.bStateReadable = FieldPlotTest::GetState(Field, Event.StateAtCall);
		}
		Events.Add(Event);
	});
}

int32 FFieldPlotSpec::CountEvents(const AFieldPlot* Field, bool bRemoved) const
{
	int32 Count = 0;
	for (const FRecordedEvent& Event : Events)
	{
		if (Event.Field == Field && Event.bRemoved == bRemoved)
		{
			++Count;
		}
	}
	return Count;
}

void FFieldPlotSpec::TickGrowth(int32 Steps)
{
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(GrowthTick);
	}
}

void FFieldPlotSpec::ExpectYield(const FString& What, const TMap<EResourceType, int32>& Actual, const TMap<EResourceType, int32>& Expected)
{
	TestEqual(FString::Printf(TEXT("%s: number of resource types in the yield"), *What), Actual.Num(), Expected.Num());
	for (const TPair<EResourceType, int32>& Pair : Expected)
	{
		const int32* Found = Actual.Find(Pair.Key);
		TestTrue(FString::Printf(TEXT("%s: %s present"), *What, *UEnum::GetValueAsString(Pair.Key)), Found != nullptr);
		if (Found != nullptr)
		{
			TestEqual(FString::Printf(TEXT("%s: amount of %s"), *What, *UEnum::GetValueAsString(Pair.Key)), *Found, Pair.Value);
		}
	}
}

bool FFieldPlotSpec::ListContains(const AFieldPlot* Field) const
{
	return Subsystem->GetUnreservedHarvestableFields().Contains(Field);
}

void FFieldPlotSpec::Define()
{
	BeforeEach([this]()
	{
		Wrapper = MakeUnique<FTestWorldWrapper>();
		bWorldReady = Wrapper->CreateTestWorld(EWorldType::Game) && Wrapper->BeginPlayInTestWorld();
		World = Wrapper->GetTestWorld();
		Subsystem = bWorldReady ? World->GetSubsystem<UFieldPlotSubsystem>() : nullptr;
		bWorldReady = bWorldReady && Subsystem != nullptr;
	});

	AfterEach([this]()
	{
		// Unbind before the world is destroyed: destroying the fields broadcasts "removed" to a lambda that uses the members of this spec.
		if (Subsystem != nullptr && EventsHandle.IsValid())
		{
			Subsystem->OnFieldChanged.Remove(EventsHandle);
		}
		EventsHandle.Reset();
		Events.Reset();
		Wrapper.Reset();
		World = nullptr;
		Subsystem = nullptr;
		bWorldReady = false;
	});

	Describe("IsReadyToHarvest (criterion 1)", [this]()
	{
		// Mutation: IsReadyToHarvest returns "state != Empty" -> the Growing line turns red;
		//           returns "state == Empty" or always true -> the Empty or Growing line turns red.
		It("is true for Grown only", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("FieldReady"), EFieldState::Empty);
			if (!TestNotNull(TEXT("field spawned"), Field)) { return; }

			TestFalse(TEXT("Empty is not ready"), Field->IsReadyToHarvest());
			if (!TestTrue(TEXT("harness: state written"), FieldPlotTest::SetState(Field, EFieldState::Growing))) { return; }
			TestFalse(TEXT("Growing is not ready"), Field->IsReadyToHarvest());
			FieldPlotTest::SetState(Field, EFieldState::Grown);
			TestTrue(TEXT("Grown is ready"), Field->IsReadyToHarvest());
		});
	});

	Describe("TryHarvestYield (criterion 2)", [this]()
	{
		// Mutation: the yield counts Amount only (not Amount * SpawnCount), or overwrites instead of summing entries of the same type,
		//           or skips entries without DropClass -> the exact-amount assertions turn red.
		//           The field is left Grown, or a drop is spawned -> the state / drop-count assertions turn red.
		//           A second harvest succeeds on an Empty field -> the "second call" line turns red.
		It("on Grown returns the exact yield, empties the field and spawns no drop", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("FieldYield"), EFieldState::Grown);
			if (!TestNotNull(TEXT("field spawned"), Field)) { return; }
			const TArray<FResourceDropEntry> Table = FieldPlotTest::MakeYieldTable();
			if (!TestTrue(TEXT("harness: drops written"), FieldPlotTest::SetDrops(Field, Table))) { return; }
			const TMap<EResourceType, int32> Expected = FieldPlotTest::ExpectedYield(Table);
			// Guard against a vacuous table: Wheat 7 and Seed 2.
			TestEqual(TEXT("harness: expected Wheat"), Expected.FindRef(EResourceType::Wheat), 7);
			TestEqual(TEXT("harness: expected Seed"), Expected.FindRef(EResourceType::Seed), 2);

			TMap<EResourceType, int32> Yield;
			TestTrue(TEXT("returns true on a Grown field"), Field->TryHarvestYield(Yield));
			ExpectYield(TEXT("yield"), Yield, Expected);
			ExpectState(TEXT("after the harvest"), Field, EFieldState::Empty);
			TestFalse(TEXT("IsReadyToHarvest is false afterwards"), Field->IsReadyToHarvest());
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);

			TMap<EResourceType, int32> SecondYield;
			TestFalse(TEXT("a second harvest of the now Empty field fails"), Field->TryHarvestYield(SecondYield));
			TestEqual(TEXT("and yields nothing"), SecondYield.Num(), 0);
		});

		// Mutation: TryHarvestYield ignores the state (yields from Empty/Growing) -> returns true / state changes / yield filled: red.
		//           It resets a Growing field to Empty -> the state line turns red.
		It("on Empty or Growing returns false, keeps the state and leaves OutYield empty", [this]()
		{
			if (!RequireWorld()) { return; }
			const TArray<FResourceDropEntry> Table = FieldPlotTest::MakeYieldTable();
			for (const EFieldState State : { EFieldState::Empty, EFieldState::Growing })
			{
				const FString Name = UEnum::GetValueAsString(State);
				// Names must be unique in the world: the enum text is part of them.
				AFieldPlot* Field = SpawnField(*FString::Printf(TEXT("FieldNoYield_%s"), *Name.Replace(TEXT("::"), TEXT("_"))), State);
				if (!TestNotNull(TEXT("field spawned"), Field)) { return; }
				FieldPlotTest::SetDrops(Field, Table);

				TMap<EResourceType, int32> Yield;
				TestFalse(FString::Printf(TEXT("%s: returns false"), *Name), Field->TryHarvestYield(Yield));
				TestEqual(FString::Printf(TEXT("%s: OutYield stays empty"), *Name), Yield.Num(), 0);
				ExpectState(Name, Field, State);
			}
			TestEqual(TEXT("no AResourceDrop spawned"), FieldPlotTest::CountDrops(World), 0);
		});
	});

	Describe("Player path unchanged (criterion 3)", [this]()
	{
		// Mutation: OnInteract on Grown routes through TryHarvestYield (no drops) -> the drop count turns red;
		//           OnInteract credits the interacting actor's counter -> the counter assertions turn red;
		//           the field is not emptied -> the state line turns red;
		//           one drop per entry instead of per SpawnCount -> 2 instead of 5, red.
		It("OnInteract on Grown spawns one drop per SpawnCount, empties the field and credits no counter", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("FieldPlayerPath"), EFieldState::Grown);
			if (!TestNotNull(TEXT("field spawned"), Field)) { return; }
			// All entries have a DropClass here: SpawnCount 3 + 2 = 5 drops expected.
			TArray<FResourceDropEntry> Table;
			Table.Add(FieldPlotTest::MakeEntry(AResourceDrop::StaticClass(), EResourceType::Wheat, 2, 3));
			Table.Add(FieldPlotTest::MakeEntry(AResourceDrop::StaticClass(), EResourceType::Seed, 1, 2));
			FieldPlotTest::SetDrops(Field, Table);

			// The interacting actor carries a counter: the player path must leave it untouched (drops are picked up later, on the ground).
			AActor* Interactor = FieldPlotTest::SpawnPlainActor(World, TEXT("InteractorWithCounter"));
			if (!TestNotNull(TEXT("interactor spawned"), Interactor)) { return; }
			UResourceCounterComponent* Counter = NewObject<UResourceCounterComponent>(Interactor);
			Counter->RegisterComponent();
			Counter->AddResource(EResourceType::Wheat, 10);

			TestTrue(TEXT("precondition: the field implements IInteractable"), Field->Implements<UInteractable>());
			IInteractable::Execute_OnInteract(Field, Interactor);

			TestEqual(TEXT("one AResourceDrop per SpawnCount (3 + 2)"), FieldPlotTest::CountDrops(World), 5);
			ExpectState(TEXT("after the player harvest"), Field, EFieldState::Empty);
			TestEqual(TEXT("Wheat counter untouched"), Counter->GetResourceCount(EResourceType::Wheat), 10);
			TestEqual(TEXT("Seed counter untouched"), Counter->GetResourceCount(EResourceType::Seed), 0);
		});
	});

	Describe("UFieldPlotSubsystem registry (criterion 4)", [this]()
	{
		// Mutation: the field does not register in BeginPlay -> the Grown field is never listed (red);
		//           the list ignores the state (lists Empty/Growing) -> the exact-list assertion turns red;
		//           the list is a snapshot taken at registration (state read once) -> the "became Grown later" line turns red.
		It("lists only the Grown fields, whatever their state at spawn time", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* EmptyField = SpawnField(TEXT("RegEmpty"), EFieldState::Empty);
			AFieldPlot* GrowingField = SpawnField(TEXT("RegGrowing"), EFieldState::Growing);
			AFieldPlot* GrownField = SpawnField(TEXT("RegGrown"), EFieldState::Grown);
			AFieldPlot* LateField = SpawnField(TEXT("RegLate"), EFieldState::Empty); // registered Empty, becomes Grown afterwards
			if (!TestTrue(TEXT("fields spawned"), EmptyField && GrowingField && GrownField && LateField)) { return; }

			TArray<AFieldPlot*> List = Subsystem->GetUnreservedHarvestableFields();
			TestEqual(TEXT("only the Grown field is listed"), List.Num(), 1);
			TestTrue(TEXT("the Grown field is the listed one"), List.Contains(GrownField));

			FieldPlotTest::SetState(LateField, EFieldState::Grown);
			List = Subsystem->GetUnreservedHarvestableFields();
			TestEqual(TEXT("a field registered while Empty is listed once it is Grown"), List.Num(), 2);
			TestTrue(TEXT("the late field is listed"), List.Contains(LateField));
			TestFalse(TEXT("the Growing field is still not listed"), List.Contains(GrowingField));
			TestFalse(TEXT("the Empty field is still not listed"), List.Contains(EmptyField));
		});

		// Mutation: unregistration removes the wrong field (e.g. the first one instead of the destroyed one) -> the survivor disappears: red.
		//           The list returns a destroyed field (validity not filtered) -> "one field left" / "no null entry" turn red.
		// NOT detected: AFieldPlot::EndPlay not calling UnregisterField. The registry holds weak pointers, so a destroyed field is
		// invisible with or without the explicit unregistration; that call is memory housekeeping, not observable through behaviour.
		It("a destroyed field disappears from the list and the survivors stay", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Doomed = SpawnField(TEXT("RegDoomed"), EFieldState::Grown);
			AFieldPlot* Survivor = SpawnField(TEXT("RegSurvivor"), EFieldState::Grown);
			if (!TestTrue(TEXT("fields spawned"), Doomed && Survivor)) { return; }
			TestEqual(TEXT("precondition: both listed"), Subsystem->GetUnreservedHarvestableFields().Num(), 2);

			Doomed->Destroy();

			const TArray<AFieldPlot*> List = Subsystem->GetUnreservedHarvestableFields();
			TestEqual(TEXT("one field left"), List.Num(), 1);
			TestTrue(TEXT("the survivor is the one left"), List.Contains(Survivor));
			TestFalse(TEXT("no null entry"), List.Contains(nullptr));
		});

		// Mutation: the list ignores reservations -> the reserved field is still listed: red.
		//           Release does not make the field listable again -> the last line turns red.
		It("a reserved field is not listed until it is released", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("RegReserved"), EFieldState::Grown);
			AActor* Reserver = FieldPlotTest::SpawnPlainActor(World, TEXT("RegReserver"));
			if (!TestTrue(TEXT("spawned"), Field && Reserver)) { return; }

			TestTrue(TEXT("precondition: reserved"), Subsystem->TryReserve(Field, Reserver));
			TestFalse(TEXT("a reserved Grown field is not listed"), ListContains(Field));
			Subsystem->Release(Field, Reserver);
			TestTrue(TEXT("a released Grown field is listed again"), ListContains(Field));
		});
	});

	Describe("UFieldPlotSubsystem reservation (criterion 5)", [this]()
	{
		// Mutation: TryReserve always succeeds (no exclusivity) -> "B fails" turns red;
		//           TryReserve fails when the same owner replays it (not idempotent) -> "A replays" turns red;
		//           IsReserved is not updated -> the IsReserved lines turn red.
		It("TryReserve is exclusive, idempotent for the holder, and reflected by IsReserved", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("ResField"), EFieldState::Grown);
			AActor* A = FieldPlotTest::SpawnPlainActor(World, TEXT("ReserverA"));
			AActor* B = FieldPlotTest::SpawnPlainActor(World, TEXT("ReserverB"));
			if (!TestTrue(TEXT("spawned"), Field && A && B)) { return; }

			TestFalse(TEXT("precondition: not reserved"), Subsystem->IsReserved(Field));
			TestTrue(TEXT("A reserves"), Subsystem->TryReserve(Field, A));
			TestTrue(TEXT("IsReserved after A"), Subsystem->IsReserved(Field));
			TestFalse(TEXT("B is refused while A holds"), Subsystem->TryReserve(Field, B));
			TestTrue(TEXT("A replays and still succeeds"), Subsystem->TryReserve(Field, A));
			TestTrue(TEXT("still reserved after B's failed attempt"), Subsystem->IsReserved(Field));
		});

		// Mutation: Release ignores the reserver (anyone can free) -> "B's Release has no effect" turns red;
		//           Release by the holder does not free -> the next lines turn red.
		It("Release by a non-holder has no effect, by the holder frees", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("ResRelease"), EFieldState::Grown);
			AActor* A = FieldPlotTest::SpawnPlainActor(World, TEXT("ReleaseA"));
			AActor* B = FieldPlotTest::SpawnPlainActor(World, TEXT("ReleaseB"));
			if (!TestTrue(TEXT("spawned"), Field && A && B)) { return; }
			Subsystem->TryReserve(Field, A);

			Subsystem->Release(Field, B);
			TestTrue(TEXT("B's Release leaves the reservation"), Subsystem->IsReserved(Field));
			TestFalse(TEXT("B still cannot reserve"), Subsystem->TryReserve(Field, B));

			Subsystem->Release(Field, A);
			TestFalse(TEXT("A's Release frees the field"), Subsystem->IsReserved(Field));
			TestTrue(TEXT("B can reserve once A released"), Subsystem->TryReserve(Field, B));
		});

		// Mutation: the reservation holds the reserver strongly or never checks its validity (a dead reserver keeps the field) -> red.
		It("a destroyed reserver no longer holds the field", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("ResDeadReserver"), EFieldState::Grown);
			AActor* A = FieldPlotTest::SpawnPlainActor(World, TEXT("DyingReserver"));
			AActor* B = FieldPlotTest::SpawnPlainActor(World, TEXT("SurvivingReserver"));
			if (!TestTrue(TEXT("spawned"), Field && A && B)) { return; }
			Subsystem->TryReserve(Field, A);
			TestTrue(TEXT("precondition: reserved"), Subsystem->IsReserved(Field));

			A->Destroy();

			TestFalse(TEXT("IsReserved is false once the reserver is destroyed"), Subsystem->IsReserved(Field));
			TestTrue(TEXT("the field is listed again"), ListContains(Field));
			TestTrue(TEXT("another reserver can take it"), Subsystem->TryReserve(Field, B));
		});

		// Mutation: IsReserved / the list report a destroyed field as still reserved or listed (weak-pointer validity not checked) -> red.
		// NOT detected: AFieldPlot::EndPlay not unregistering. A destroyed field is invisible through the weak pointer either way,
		// so the explicit unregistration (memory housekeeping) cannot be observed through behaviour; no counting API is added for it.
		It("a destroyed field is no longer reserved nor listed", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("ResDeadField"), EFieldState::Grown);
			AActor* A = FieldPlotTest::SpawnPlainActor(World, TEXT("HolderOfDeadField"));
			if (!TestTrue(TEXT("spawned"), Field && A)) { return; }
			Subsystem->TryReserve(Field, A);

			Field->Destroy();

			TestFalse(TEXT("IsReserved is false for a destroyed field"), Subsystem->IsReserved(Field));
			TestEqual(TEXT("the list is empty"), Subsystem->GetUnreservedHarvestableFields().Num(), 0);
		});
	});

	Describe("IsReadyToPlant and GetSeedCost (2.2b)", [this]()
	{
		// Mutation: IsReadyToPlant returns "state != Grown" (true for Growing) or always true -> the Growing / Grown line turns red;
		//           returns "state == Grown" -> the Empty line turns red.
		It("IsReadyToPlant is true for Empty only", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("PlantReady"), EFieldState::Empty);
			if (!TestNotNull(TEXT("field spawned"), Field)) { return; }

			TestTrue(TEXT("Empty is ready to plant"), Field->IsReadyToPlant());
			if (!TestTrue(TEXT("harness: state written"), FieldPlotTest::SetState(Field, EFieldState::Growing))) { return; }
			TestFalse(TEXT("Growing is not ready to plant"), Field->IsReadyToPlant());
			FieldPlotTest::SetState(Field, EFieldState::Grown);
			TestFalse(TEXT("Grown is not ready to plant"), Field->IsReadyToPlant());
		});

		// Mutation: GetSeedCost returns a hard-coded value instead of the field's SeedCost -> the "3" line turns red;
		//           the default is not 1 (spec: "SeedCost du champ (1)") -> the first line turns red.
		It("GetSeedCost is 1 by default and follows the SeedCost setting", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("PlantCost"), EFieldState::Empty);
			if (!TestNotNull(TEXT("field spawned"), Field)) { return; }

			TestEqual(TEXT("default SeedCost"), Field->GetSeedCost(), 1);
			if (!TestTrue(TEXT("harness: SeedCost writable by reflection"), FieldPlotTest::SetSeedCost(Field, 3))) { return; }
			TestEqual(TEXT("GetSeedCost reads the setting"), Field->GetSeedCost(), 3);
		});
	});

	Describe("TryPlant (2.2b, criteria 1 and 2)", [this]()
	{
		// Mutation: TryPlant spends 1 (or everything, or nothing) instead of SeedCost -> the Seed amount (5 -> 2) turns red;
		//           it touches another type -> the Wheat line turns red;
		//           the field jumps straight to Grown, or never reaches Grown -> the "Growing" or the final "Grown" line turns red;
		//           the growth ignores GrowthDuration (Grown before it elapsed) -> the "still Growing after 0.2 s" line turns red.
		It("on Empty with enough seeds: true, spends exactly SeedCost, Growing, then Grown after GrowthDuration", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("PlantOk"), EFieldState::Empty);
			AActor* Owner = FieldPlotTest::SpawnPlainActor(World, TEXT("PlantOkOwner"));
			if (!TestTrue(TEXT("spawned"), Field && Owner)) { return; }
			if (!TestTrue(TEXT("harness: tunables written"), FieldPlotTest::SetSeedCost(Field, 3) && FieldPlotTest::SetGrowthDuration(Field, ShortGrowth))) { return; }
			UResourceCounterComponent* Counter = FieldPlotTest::MakeCounter(Owner, 7, 5);

			TestTrue(TEXT("TryPlant succeeds"), Field->TryPlant(Counter));
			TestEqual(TEXT("Seed spent: exactly SeedCost"), Counter->GetResourceCount(EResourceType::Seed), 5 - 3);
			TestEqual(TEXT("Wheat untouched"), Counter->GetResourceCount(EResourceType::Wheat), 7);
			ExpectState(TEXT("right after the plant"), Field, EFieldState::Growing);
			TestFalse(TEXT("a Growing field is not harvestable"), Field->IsReadyToHarvest());
			TestFalse(TEXT("a Growing field is not plantable"), Field->IsReadyToPlant());

			TickGrowth(2); // 0.2 s of 0.4 s
			ExpectState(TEXT("0.2 s into a 0.4 s growth"), Field, EFieldState::Growing);
			TickGrowth(4); // 0.6 s in total
			ExpectState(TEXT("after GrowthDuration"), Field, EFieldState::Grown);
			TestTrue(TEXT("a Grown field is harvestable again"), Field->IsReadyToHarvest());
			TestEqual(TEXT("growing did not spend more seeds"), Counter->GetResourceCount(EResourceType::Seed), 2);
		});

		// Mutation: the check is "Seed > SeedCost" (strict) -> the stock of exactly SeedCost is refused: red.
		//           The Seed is not removed when the stock equals the cost -> the 0 line turns red.
		It("a stock of exactly SeedCost is enough and is emptied", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("PlantExact"), EFieldState::Empty);
			AActor* Owner = FieldPlotTest::SpawnPlainActor(World, TEXT("PlantExactOwner"));
			if (!TestTrue(TEXT("spawned"), Field && Owner)) { return; }
			if (!TestTrue(TEXT("harness: SeedCost written"), FieldPlotTest::SetSeedCost(Field, 3))) { return; }
			UResourceCounterComponent* Counter = FieldPlotTest::MakeCounter(Owner, 0, 3);

			TestTrue(TEXT("TryPlant succeeds with Seed == SeedCost"), Field->TryPlant(Counter));
			TestEqual(TEXT("Seed is now 0"), Counter->GetResourceCount(EResourceType::Seed), 0);
			ExpectState(TEXT("after the plant"), Field, EFieldState::Growing);
		});

		// Mutation: the state check is missing (a Growing / Grown field is replanted: seeds spent, or state reset) -> the matching row turns red;
		//           the stock check is missing or partial (spends what is there) -> the "Seed 2 < cost 3" row turns red;
		//           a null source crashes or succeeds -> the null row turns red;
		//           a failed TryPlant still broadcasts a change -> the final "no notification" line turns red (inferred from "a chaque changement").
		It("fails without touching the field, the counter or the listeners: Growing, Grown, short stock, empty stock, null source", [this]()
		{
			if (!RequireWorld()) { return; }
			struct FFailRow { const TCHAR* Name; EFieldState State; int32 Seed; bool bNullSource; };
			const FFailRow Rows[] = {
				{ TEXT("Growing, stock 5"), EFieldState::Growing, 5, false },
				{ TEXT("Grown, stock 5"), EFieldState::Grown, 5, false },
				{ TEXT("Empty, stock 2 < cost 3"), EFieldState::Empty, 2, false },
				{ TEXT("Empty, stock 0"), EFieldState::Empty, 0, false },
				{ TEXT("Empty, null source"), EFieldState::Empty, 5, true },
			};
			StartRecording();
			int32 Index = 0;
			for (const FFailRow& Row : Rows)
			{
				AFieldPlot* Field = SpawnField(*FString::Printf(TEXT("PlantFail%d"), Index), Row.State);
				AActor* Owner = FieldPlotTest::SpawnPlainActor(World, *FString::Printf(TEXT("PlantFailOwner%d"), Index));
				++Index;
				if (!TestTrue(TEXT("spawned"), Field && Owner)) { return; }
				if (!TestTrue(TEXT("harness: SeedCost written"), FieldPlotTest::SetSeedCost(Field, 3))) { return; }
				UResourceCounterComponent* Counter = FieldPlotTest::MakeCounter(Owner, 7, Row.Seed);

				TestFalse(FString::Printf(TEXT("%s: TryPlant returns false"), Row.Name), Field->TryPlant(Row.bNullSource ? nullptr : Counter));
				ExpectState(Row.Name, Field, Row.State);
				TestEqual(FString::Printf(TEXT("%s: Seed untouched"), Row.Name), Counter->GetResourceCount(EResourceType::Seed), Row.Seed);
				TestEqual(FString::Printf(TEXT("%s: Wheat untouched"), Row.Name), Counter->GetResourceCount(EResourceType::Wheat), 7);
			}
			TestEqual(TEXT("no OnFieldChanged event for failed plants"), Events.Num(), 0);
		});

		// Mutation: OnInteract on an Empty field still does nothing (old behaviour) -> the Growing line turns red;
		//           the player's plant does not spend the seeds (or spends 1) -> the Seed amount turns red;
		//           it plants without checking the stock -> the "stock 2 < 3" part turns red.
		// ASSUMPTION (spec gap): the spec does not say where OnInteract takes the seeds from; the test assumes the
		// UResourceCounterComponent of the interacting actor (the player pawn), as for the servant path with the player's counter.
		It("OnInteract by the player on Empty gives the same result as TryPlant, and does nothing without enough seeds", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("PlayerPlantOk"), EFieldState::Empty);
			AFieldPlot* PoorField = SpawnField(TEXT("PlayerPlantPoor"), EFieldState::Empty);
			AActor* Rich = FieldPlotTest::SpawnPlainActor(World, TEXT("RichPlayer"));
			AActor* Poor = FieldPlotTest::SpawnPlainActor(World, TEXT("PoorPlayer"));
			if (!TestTrue(TEXT("spawned"), Field && PoorField && Rich && Poor)) { return; }
			if (!TestTrue(TEXT("harness: SeedCost written"), FieldPlotTest::SetSeedCost(Field, 3) && FieldPlotTest::SetSeedCost(PoorField, 3))) { return; }
			UResourceCounterComponent* RichCounter = FieldPlotTest::MakeCounter(Rich, 7, 5);
			UResourceCounterComponent* PoorCounter = FieldPlotTest::MakeCounter(Poor, 7, 2);
			TestTrue(TEXT("precondition: the field implements IInteractable"), Field->Implements<UInteractable>());

			IInteractable::Execute_OnInteract(Field, Rich);
			ExpectState(TEXT("player plant with enough seeds"), Field, EFieldState::Growing);
			TestEqual(TEXT("Seed spent: exactly SeedCost"), RichCounter->GetResourceCount(EResourceType::Seed), 5 - 3);
			TestEqual(TEXT("Wheat untouched"), RichCounter->GetResourceCount(EResourceType::Wheat), 7);
			TestEqual(TEXT("no drop is spawned by a plant"), FieldPlotTest::CountDrops(World), 0);

			IInteractable::Execute_OnInteract(PoorField, Poor);
			ExpectState(TEXT("player interaction with 2 seeds for a cost of 3"), PoorField, EFieldState::Empty);
			TestEqual(TEXT("Seed untouched"), PoorCounter->GetResourceCount(EResourceType::Seed), 2);
		});
	});

	Describe("GetUnreservedPlantableFields (2.2b, criterion 3)", [this]()
	{
		// Mutation: the list ignores the state (lists Growing / Grown) or lists only registered-while-Empty fields -> the exact-list lines turn red;
		//           it ignores reservations -> the reserved line turns red; Release does not free -> the last line turns red;
		//           a destroyed field stays listed -> the "no null / one left" lines turn red;
		//           the list is a snapshot (state read once) -> "became Empty later" turns red.
		It("lists only the Empty, unreserved, living fields", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* EmptyFree = SpawnField(TEXT("PlantableFree"), EFieldState::Empty);
			AFieldPlot* EmptyReserved = SpawnField(TEXT("PlantableReserved"), EFieldState::Empty);
			AFieldPlot* GrowingPlot = SpawnField(TEXT("PlantableGrowing"), EFieldState::Growing);
			AFieldPlot* GrownPlot = SpawnField(TEXT("PlantableGrown"), EFieldState::Grown);
			AFieldPlot* Doomed = SpawnField(TEXT("PlantableDoomed"), EFieldState::Empty);
			AActor* Reserver = FieldPlotTest::SpawnPlainActor(World, TEXT("PlantableReserver"));
			if (!TestTrue(TEXT("spawned"), EmptyFree && EmptyReserved && GrowingPlot && GrownPlot && Doomed && Reserver)) { return; }
			TestTrue(TEXT("precondition: reservation taken"), Subsystem->TryReserve(EmptyReserved, Reserver));
			Doomed->Destroy();

			TArray<AFieldPlot*> List = Subsystem->GetUnreservedPlantableFields();
			TestEqual(TEXT("only the free Empty field is listed"), List.Num(), 1);
			TestTrue(TEXT("the free Empty field is the listed one"), List.Contains(EmptyFree));
			TestFalse(TEXT("the reserved Empty field is not listed"), List.Contains(EmptyReserved));
			TestFalse(TEXT("no null entry (destroyed field)"), List.Contains(nullptr));

			Subsystem->Release(EmptyReserved, Reserver);
			TestTrue(TEXT("once released the field is listed"), Subsystem->GetUnreservedPlantableFields().Contains(EmptyReserved));

			FieldPlotTest::SetState(GrownPlot, EFieldState::Empty);
			TestTrue(TEXT("a field that became Empty later is listed"), Subsystem->GetUnreservedPlantableFields().Contains(GrownPlot));
			TestEqual(TEXT("three plantable fields now"), Subsystem->GetUnreservedPlantableFields().Num(), 3);
			// The harvest list is the mirror: the planted/Empty ones are not harvestable.
			TestEqual(TEXT("the harvestable list is empty"), Subsystem->GetUnreservedHarvestableFields().Num(), 0);
		});
	});

	Describe("GetPromisedSeeds (2.2b, criterion 3)", [this]()
	{
		// Mutation: the sum counts 1 per reservation instead of SeedCost -> the 3 + 1 lines turn red;
		//           the excluded reserver is not excluded (or everything is excluded) -> the A / B lines turn red;
		//           a reserved Grown field is counted -> the Grown line turns red;
		//           the promise is kept after Release -> the "after Release" line turns red;
		//           an unreserved Empty field is counted -> the unreserved line turns red.
		It("sums the SeedCost of the Empty fields reserved by someone other than the excluded actor", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* CostThree = SpawnField(TEXT("PromiseThree"), EFieldState::Empty);
			AFieldPlot* CostOne = SpawnField(TEXT("PromiseOne"), EFieldState::Empty);
			AFieldPlot* GrownPlot = SpawnField(TEXT("PromiseGrown"), EFieldState::Grown);
			AFieldPlot* Unreserved = SpawnField(TEXT("PromiseUnreserved"), EFieldState::Empty);
			AActor* A = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseA"));
			AActor* B = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseB"));
			AActor* Third = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseThird"));
			if (!TestTrue(TEXT("spawned"), CostThree && CostOne && GrownPlot && Unreserved && A && B && Third)) { return; }
			if (!TestTrue(TEXT("harness: SeedCost written"), FieldPlotTest::SetSeedCost(CostThree, 3))) { return; }

			TestEqual(TEXT("nothing reserved: nothing promised"), Subsystem->GetPromisedSeeds(Third), 0);
			Subsystem->TryReserve(CostThree, A);
			Subsystem->TryReserve(CostOne, B);
			Subsystem->TryReserve(GrownPlot, A);

			TestEqual(TEXT("a third party sees both promises (3 + 1), not the Grown one, not the unreserved one"), Subsystem->GetPromisedSeeds(Third), 4);
			TestEqual(TEXT("A is excluded: only B's promise (1)"), Subsystem->GetPromisedSeeds(A), 1);
			TestEqual(TEXT("B is excluded: only A's Empty promise (3), A's Grown reservation is worth 0"), Subsystem->GetPromisedSeeds(B), 3);

			Subsystem->Release(CostThree, A);
			TestEqual(TEXT("after Release the promise is gone"), Subsystem->GetPromisedSeeds(Third), 1);
			Subsystem->Release(CostOne, B);
			Subsystem->Release(GrownPlot, A);
			TestEqual(TEXT("all released: nothing promised"), Subsystem->GetPromisedSeeds(Third), 0);
		});

		// Mutation: the promise is recorded at reservation time (Grown = 0 forever, Empty = cost forever) instead of derived from the
		//           current state -> the "Grown became Empty" line turns red (spec edge case: counted until its reserver cancels);
		//           a destroyed reserver keeps promising seeds (would starve every other servant forever) -> the last line turns red.
		It("is derived from the current state and ignores destroyed reservers", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("PromiseDerived"), EFieldState::Grown);
			AActor* Reserver = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseReserver"));
			AActor* Other = FieldPlotTest::SpawnPlainActor(World, TEXT("PromiseOther"));
			if (!TestTrue(TEXT("spawned"), Field && Reserver && Other)) { return; }
			Subsystem->TryReserve(Field, Reserver);
			TestEqual(TEXT("a reserved Grown field promises nothing"), Subsystem->GetPromisedSeeds(Other), 0);

			FieldPlotTest::SetState(Field, EFieldState::Empty); // e.g. the player harvested it during the trip
			TestEqual(TEXT("the same reservation on a field that became Empty is now promised"), Subsystem->GetPromisedSeeds(Other), Field->GetSeedCost());

			Reserver->Destroy();
			TestEqual(TEXT("a destroyed reserver promises nothing"), Subsystem->GetPromisedSeeds(Other), 0);
		});
	});

	Describe("OnFieldChanged notification (2.2b)", [this]()
	{
		// Mutation: AFieldPlot does not notify on one of the four transitions (plant, growth end, servant harvest, player harvest)
		//           -> the exact sequence (6 events) turns red at that position;
		//           it notifies before writing the state (a listener would read the old state) -> the StateAtCall column turns red;
		//           it notifies twice for one change -> the count turns red; it passes bRemoved = true on a change -> the flag line turns red.
		// NOTE: "exactly one event per change" and "state already updated in the callback" are the natural reading of
		// "NotifyFieldChanged a chaque changement d'etat" + the cancellation rule that reads the state in the callback; the spec does not spell them out.
		It("notifies once per state change, after the change: plant, growth end, servant harvest, player harvest", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Field = SpawnField(TEXT("NotifyCycle"), EFieldState::Empty);
			AActor* Owner = FieldPlotTest::SpawnPlainActor(World, TEXT("NotifyOwner"));
			if (!TestTrue(TEXT("spawned"), Field && Owner)) { return; }
			if (!TestTrue(TEXT("harness: tunables written"), FieldPlotTest::SetGrowthDuration(Field, ShortGrowth) && FieldPlotTest::SetDrops(Field, FieldPlotTest::MakeYieldTable()))) { return; }
			UResourceCounterComponent* Counter = FieldPlotTest::MakeCounter(Owner, 0, 5);
			StartRecording();

			Field->TryPlant(Counter);                       // Empty -> Growing
			TickGrowth(6);                                  // Growing -> Grown (0.6 s > 0.4 s)
			TMap<EResourceType, int32> Yield;
			TestTrue(TEXT("harness: servant harvest succeeded"), Field->TryHarvestYield(Yield)); // Grown -> Empty (servant path)
			Field->TryPlant(Counter);                       // Empty -> Growing
			TickGrowth(6);                                  // Growing -> Grown
			IInteractable::Execute_OnInteract(Field, Owner); // Grown -> Empty (player path)

			const EFieldState Expected[] = { EFieldState::Growing, EFieldState::Grown, EFieldState::Empty, EFieldState::Growing, EFieldState::Grown, EFieldState::Empty };
			const int32 ExpectedCount = UE_ARRAY_COUNT(Expected);
			if (!TestEqual(TEXT("six changes, six events"), Events.Num(), ExpectedCount)) { return; }
			for (int32 Index = 0; Index < ExpectedCount; ++Index)
			{
				const FRecordedEvent& Event = Events[Index];
				TestTrue(FString::Printf(TEXT("event %d is about the field"), Index), Event.Field == Field);
				TestFalse(FString::Printf(TEXT("event %d is not a removal"), Index), Event.bRemoved);
				TestTrue(FString::Printf(TEXT("event %d: state readable in the callback"), Index), Event.bStateReadable);
				TestTrue(FString::Printf(TEXT("event %d: state at call is %s, got %s"), Index, *UEnum::GetValueAsString(Expected[Index]), *UEnum::GetValueAsString(Event.StateAtCall)),
				         Event.StateAtCall == Expected[Index]);
			}
		});

		// Mutation: UnregisterField does not broadcast -> the removed count is 0: red;
		//           it broadcasts with bRemoved = false -> the removed count is 0 and the plain count is 1: red;
		//           it broadcasts for every field instead of the destroyed one -> the survivor line turns red.
		It("destroying a field broadcasts bRemoved = true for that field only", [this]()
		{
			if (!RequireWorld()) { return; }
			AFieldPlot* Doomed = SpawnField(TEXT("NotifyDoomed"), EFieldState::Grown);
			AFieldPlot* Survivor = SpawnField(TEXT("NotifySurvivor"), EFieldState::Grown);
			if (!TestTrue(TEXT("spawned"), Doomed && Survivor)) { return; }
			StartRecording();

			Doomed->Destroy();

			TestEqual(TEXT("one removal event for the destroyed field"), CountEvents(Doomed, true), 1);
			TestEqual(TEXT("no plain change event for the destroyed field"), CountEvents(Doomed, false), 0);
			TestEqual(TEXT("no event at all for the survivor"), CountEvents(Survivor, true) + CountEvents(Survivor, false), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
