// Automation Spec for AFieldPlot (harvest yield) and UFieldPlotSubsystem (registry and reservation).
// Source of truth: the servant field harvest spec (docs/specs/servant-field-harvest.md, removed at merge), acceptance criteria 1 to 5
// and the edge case "fields without DropClass are credited anyway".
//
// World: FTestWorldWrapper (Engine, Tests/AutomationCommon.h), Game world with BeginPlay done, so a spawned AFieldPlot registers in
// BeginPlay and Destroy() runs EndPlay (unregistration) synchronously. The world is never ticked: only the calls of the test change state.
// The state of a field is written through reflection (Tests/FieldPlotTestHelpers.h), the API has no setter.
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
	EFieldState Actual = EFieldState::Empty;
	if (!TestTrue(FString::Printf(TEXT("%s: CurrentState readable by reflection"), *What), FieldPlotTest::GetState(Field, Actual))) { return; }
	TestTrue(FString::Printf(TEXT("%s: expected state %s, got %s"), *What, *UEnum::GetValueAsString(Expected), *UEnum::GetValueAsString(Actual)),
	         Actual == Expected);
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
