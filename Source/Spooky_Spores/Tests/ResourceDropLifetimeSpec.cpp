// Automation Spec for the despawn of resource drops (AResourceDrop::DropLifetime).
// Source of truth: docs/specs/drop-despawn.md (removed at merge), acceptance criteria 1 to 9 and its edge cases.
//
// World: FTestWorldWrapper Game world with BeginPlay done; time only advances through TickTestWorld (TickFor, 0.05 s steps).
// DropLifetime is a protected UPROPERTY with no setter: it is written by reflection on a deferred-spawned drop, BEFORE FinishSpawning
// (so before BeginPlay reads it). The spec suggests a test subclass, but UHT does not process a UCLASS declared in a .cpp, and the
// existing specs already write protected/private tunables by reflection (FieldPlotTestHelpers, ServantWanderTestHelpers): same approach here.
// The test drops have no mesh asset (no body), so they neither fall nor leave the world during the long time advances.
//
// Pickup: TryCollect is private and the spec does not say how it is triggered. The tests broadcast the mesh's OnComponentBeginOverlap
// with the player's pawn as OtherActor (what a real overlap does) once PickupDelay has elapsed; see the report for this assumption.
// Every test lists its mutation: the change of behaviour that must turn it red.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantFieldTestHelpers.h"
#include "Tests/FieldPlotTestHelpers.h"
#include "Tests/ServantWanderTestHelpers.h"
#include "Resources/ResourceDrop.h"
#include "Resources/ResourceCounterComponent.h"
#include "Resources/ResourceTypes.h"
#include "Interaction/Grabbable.h"
#include "CollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Tests/AutomationCommon.h"

namespace ResourceDropLifetimeSpecPrivate
{
	// Protected/private tunables are read through reflection (names from the specs / headers).
	bool ReadNumber(const UObject* Object, const TCHAR* PropertyName, double& OutValue)
	{
		const FNumericProperty* Property = Object != nullptr ? FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (Property == nullptr)
		{
			return false;
		}
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		OutValue = Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(ValuePtr)
		                                       : static_cast<double>(Property->GetSignedIntPropertyValue(ValuePtr));
		return true;
	}

	UStaticMeshComponent* ReadMesh(const AResourceDrop* Drop)
	{
		const FObjectProperty* Property = Drop != nullptr ? FindFProperty<FObjectProperty>(AResourceDrop::StaticClass(), TEXT("MeshComponent")) : nullptr;
		return Property != nullptr ? Cast<UStaticMeshComponent>(Property->GetObjectPropertyValue_InContainer(Drop)) : nullptr;
	}
}

BEGIN_DEFINE_SPEC(FResourceDropLifetimeSpec, "SpookySpores.Resources.DropLifetime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	bool RequireWorld();
	// Deferred spawn, InitializeDrop and DropLifetime written; BeginPlay has NOT run yet.
	AResourceDrop* BeginDrop(const TCHAR* Name, double Lifetime, EResourceType Type, int32 Amount);
	// Spawn + finish in one go.
	AResourceDrop* SpawnDrop(const TCHAR* Name, double Lifetime, EResourceType Type, int32 Amount);
	// Simulates the physical contact of the actor with the drop (what a real overlap broadcasts).
	void SimulateOverlap(AResourceDrop* Drop, AActor* Other);

END_DEFINE_SPEC(FResourceDropLifetimeSpec)

bool FResourceDropLifetimeSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world unavailable: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

AResourceDrop* FResourceDropLifetimeSpec::BeginDrop(const TCHAR* Name, double Lifetime, EResourceType Type, int32 Amount)
{
	AResourceDrop* Drop = World->SpawnActorDeferred<AResourceDrop>(AResourceDrop::StaticClass(), FTransform::Identity, nullptr, nullptr,
	                                                               ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Drop == nullptr)
	{
		return nullptr;
	}
	if (!ServantWanderTest::WriteNumber(Drop, TEXT("DropLifetime"), Lifetime))
	{
		AddError(FString::Printf(TEXT("Harness: DropLifetime property not found by reflection on %s."), Name));
	}
	Drop->InitializeDrop(Type, Amount);
	return Drop;
}

AResourceDrop* FResourceDropLifetimeSpec::SpawnDrop(const TCHAR* Name, double Lifetime, EResourceType Type, int32 Amount)
{
	AResourceDrop* Drop = BeginDrop(Name, Lifetime, Type, Amount);
	if (Drop != nullptr)
	{
		Drop->FinishSpawning(FTransform::Identity);
	}
	return Drop;
}

void FResourceDropLifetimeSpec::SimulateOverlap(AResourceDrop* Drop, AActor* Other)
{
	UStaticMeshComponent* Mesh = ResourceDropLifetimeSpecPrivate::ReadMesh(Drop);
	if (!TestNotNull(TEXT("harness: the drop's MeshComponent is readable"), Mesh) || Other == nullptr)
	{
		return;
	}
	Mesh->OnComponentBeginOverlap.Broadcast(Mesh, Other, Cast<UPrimitiveComponent>(Other->GetRootComponent()), 0, false, FHitResult());
}

void FResourceDropLifetimeSpec::Define()
{
	using namespace ResourceDropLifetimeSpecPrivate;

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

	// Mutation: the default changes (e.g. 30 or 0) -> the CDO line turns red;
	//           SetLifeSpan is not called, or called with a hard-coded value instead of DropLifetime -> the GetLifeSpan range turns red.
	It("Criterion 2: DropLifetime defaults to 300 s on the CDO and a default drop is armed with it", [this]()
	{
		if (!RequireWorld()) { return; }
		double CdoValue = -1.0;
		if (!TestTrue(TEXT("DropLifetime readable on the CDO"), ReadNumber(GetDefault<AResourceDrop>(), TEXT("DropLifetime"), CdoValue))) { return; }
		TestEqual(TEXT("CDO default is 300 s"), CdoValue, 300.0);

		AResourceDrop* Drop = World->SpawnActor<AResourceDrop>(AResourceDrop::StaticClass(), FTransform::Identity);
		if (!TestNotNull(TEXT("default drop spawned"), Drop)) { return; }
		TestTrue(FString::Printf(TEXT("a default drop has about 300 s left (got %f)"), Drop->GetLifeSpan()),
		         Drop->GetLifeSpan() > 299.0f && Drop->GetLifeSpan() <= 300.0f);
	});

	// Mutation: the despawn is never armed -> still valid after D, red; armed with another duration (2x, or D in ms) -> too early/late, red;
	//           the drop is destroyed immediately at BeginPlay (CLAUDE.md pitfall) -> invalid at 1.5 s, red;
	//           the lifetime counts real time or does not follow the world clock -> red.
	It("Criterion 1: a drop with DropLifetime D is still valid shortly before D and destroyed shortly after", [this]()
	{
		if (!RequireWorld()) { return; }
		AResourceDrop* Drop = SpawnDrop(TEXT("DropExpiring"), 2.0, EResourceType::Wheat, 1);
		if (!TestNotNull(TEXT("drop spawned"), Drop)) { return; }
		const TWeakObjectPtr<AResourceDrop> Weak(Drop);

		ServantFieldTest::TickFor(*Wrapper, 1.5);
		if (!TestTrue(TEXT("valid at D - 0.5 s"), Weak.IsValid())) { return; }
		TestTrue(FString::Printf(TEXT("the remaining life counts down (about 0.5 s expected, got %f)"), Drop->GetLifeSpan()),
		         Drop->GetLifeSpan() > 0.0f && Drop->GetLifeSpan() < 1.0f);

		ServantFieldTest::TickFor(*Wrapper, 1.0);
		TestFalse(TEXT("destroyed at D + 0.5 s"), Weak.IsValid());
	});

	// Mutation: the guard is inverted or removed with a clamp (SetLifeSpan(FMath::Max(DropLifetime, 1))) -> the drop expires, red;
	//           a negative value is treated as its absolute value -> the -5 drop expires, red.
	// (Removing "if DropLifetime > 0" alone is NOT detectable: the engine's SetLifeSpan(<= 0) already means "no despawn".)
	It("Criterion 3: DropLifetime <= 0 means no despawn (GetLifeSpan 0, survives a long time advance)", [this]()
	{
		if (!RequireWorld()) { return; }
		AResourceDrop* Zero = SpawnDrop(TEXT("DropLifetimeZero"), 0.0, EResourceType::Wheat, 1);
		AResourceDrop* Negative = SpawnDrop(TEXT("DropLifetimeNegative"), -5.0, EResourceType::Wheat, 1);
		if (!TestTrue(TEXT("drops spawned"), Zero != nullptr && Negative != nullptr)) { return; }
		const TWeakObjectPtr<AResourceDrop> WeakZero(Zero);
		const TWeakObjectPtr<AResourceDrop> WeakNegative(Negative);
		TestEqual(TEXT("DropLifetime 0: GetLifeSpan is 0 (infinite)"), Zero->GetLifeSpan(), 0.0f);
		TestEqual(TEXT("DropLifetime -5: GetLifeSpan is 0 (infinite)"), Negative->GetLifeSpan(), 0.0f);

		ServantFieldTest::TickFor(*Wrapper, 30.0);
		TestTrue(TEXT("DropLifetime 0: still valid after 30 s"), WeakZero.IsValid());
		TestTrue(TEXT("DropLifetime -5: still valid after 30 s"), WeakNegative.IsValid());
	});

	// Mutation: the countdown starts at SpawnActorDeferred (timer or SetLifeSpan taken before FinishSpawning) -> 7 s left instead of 10, red;
	//           SetLifeSpan is called in InitializeDrop or in the constructor from the CDO value -> same, or wrong duration, red.
	It("Criterion 4: the countdown starts at FinishSpawning, not at SpawnActorDeferred", [this]()
	{
		if (!RequireWorld()) { return; }
		AResourceDrop* Drop = BeginDrop(TEXT("DropDeferred"), 10.0, EResourceType::Wheat, 1);
		if (!TestNotNull(TEXT("drop spawned (deferred)"), Drop)) { return; }
		const TWeakObjectPtr<AResourceDrop> Weak(Drop);

		ServantFieldTest::TickFor(*Wrapper, 3.0); // the world advances while the drop is not finished
		Drop->FinishSpawning(FTransform::Identity);
		TestTrue(FString::Printf(TEXT("full duration left right after FinishSpawning (expected 10, got %f)"), Drop->GetLifeSpan()),
		         FMath::IsNearlyEqual(Drop->GetLifeSpan(), 10.0f, 0.05f));

		ServantFieldTest::TickFor(*Wrapper, 8.0); // 11 s since the deferred spawn, 8 s since FinishSpawning
		TestTrue(TEXT("still valid 8 s after FinishSpawning"), Weak.IsValid());
		ServantFieldTest::TickFor(*Wrapper, 2.5);
		TestFalse(TEXT("destroyed 10.5 s after FinishSpawning"), Weak.IsValid());
	});

	// Mutation: the destruction by expiry credits the nearest/first player (refund in EndPlay/Destroyed or in an expiry handler) -> the
	//           counter changes, red; the expiry is implemented as a forced pickup by the player pawn -> red.
	It("Criterion 5: an expired drop credits nothing, even with a counter-equipped pawn out of pickup range", [this]()
	{
		if (!RequireWorld()) { return; }
		UResourceCounterComponent* Counter = ServantFieldTest::SpawnPlayer(World, true, 3, 2);
		if (!TestNotNull(TEXT("harness: player with counter"), Counter)) { return; }
		AResourceDrop* Drop = SpawnDrop(TEXT("DropNoCredit"), 1.0, EResourceType::Wheat, 5);
		if (!TestNotNull(TEXT("drop spawned"), Drop)) { return; }
		const TWeakObjectPtr<AResourceDrop> Weak(Drop);

		ServantFieldTest::TickFor(*Wrapper, 0.6);
		TestTrue(TEXT("harness: the drop is still alive at 0.6 s"), Weak.IsValid());
		ServantFieldTest::ExpectCounters(*this, TEXT("while the drop is alive and out of range"), Counter, 3, 2);

		ServantFieldTest::TickFor(*Wrapper, 1.0);
		TestFalse(TEXT("the drop expired"), Weak.IsValid());
		ServantFieldTest::ExpectCounters(*this, TEXT("after the expiry"), Counter, 3, 2);
	});

	// Mutation: the pickup does not destroy the drop, or the credit is made twice (pickup + EndPlay/expiry credit) -> 11 instead of 7, red;
	//           the lifetime timer survives the pickup and credits or errors at D -> the later counters turn red;
	//           the pickup credits the wrong Type or Amount -> red.
	It("Criterion 6: a drop picked up before expiry credits exactly once, and nothing changes after D", [this]()
	{
		if (!RequireWorld()) { return; }
		UResourceCounterComponent* Counter = ServantFieldTest::SpawnPlayer(World, true, 3, 2);
		AActor* Pawn = Counter != nullptr ? Counter->GetOwner() : nullptr;
		if (!TestTrue(TEXT("harness: player pawn with counter"), Pawn != nullptr)) { return; }
		AResourceDrop* Drop = SpawnDrop(TEXT("DropPickedUp"), 5.0, EResourceType::Wheat, 4);
		if (!TestNotNull(TEXT("drop spawned"), Drop)) { return; }
		const TWeakObjectPtr<AResourceDrop> Weak(Drop);

		ServantFieldTest::TickFor(*Wrapper, 0.8); // PickupDelay (0.5 s) elapsed, lifetime (5 s) not
		SimulateOverlap(Drop, Pawn);
		ServantFieldTest::ExpectCounters(*this, TEXT("after the pickup (3 + 4 Wheat)"), Counter, 7, 2);
		TestFalse(TEXT("the picked up drop is destroyed"), Weak.IsValid());

		ServantFieldTest::TickFor(*Wrapper, 6.0); // well past D
		ServantFieldTest::ExpectCounters(*this, TEXT("after the world advanced past D"), Counter, 7, 2);
	});

	// Mutation: the expiry is delayed until the pickup is enabled (SetLifeSpan(Max(D, PickupDelay)), or armed from EnablePickup) -> still
	//           valid at 0.4 s, red; the pickup timer of the destroyed drop still credits the player at 0.5 s -> counter changes, red.
	It("Criterion 7: DropLifetime shorter than PickupDelay destroys the drop at expiry, with no late pickup", [this]()
	{
		if (!RequireWorld()) { return; }
		UResourceCounterComponent* Counter = ServantFieldTest::SpawnPlayer(World, true, 3, 2);
		if (!TestNotNull(TEXT("harness: player with counter"), Counter)) { return; }
		AResourceDrop* Drop = SpawnDrop(TEXT("DropShortLived"), 0.2, EResourceType::Wheat, 5); // PickupDelay default is 0.5 s
		if (!TestNotNull(TEXT("drop spawned"), Drop)) { return; }
		const TWeakObjectPtr<AResourceDrop> Weak(Drop);

		ServantFieldTest::TickFor(*Wrapper, 0.1);
		TestTrue(TEXT("valid at 0.1 s"), Weak.IsValid());
		ServantFieldTest::TickFor(*Wrapper, 0.3); // 0.4 s: after D, before PickupDelay
		TestFalse(TEXT("destroyed at 0.4 s, before PickupDelay elapsed"), Weak.IsValid());

		ServantFieldTest::TickFor(*Wrapper, 1.0); // the pickup timer would have fired by now
		ServantFieldTest::ExpectCounters(*this, TEXT("after the pickup timer would have fired"), Counter, 3, 2);
	});

	// Mutation: AResourceDrop implements IGrabbable, or its mesh gets the PhysicsInteractable preset / Interactable object type -> red.
	It("Criterion 8: a drop is not grabbable and its collision is not PhysicsInteractable / Interactable", [this]()
	{
		if (!RequireWorld()) { return; }
		TestFalse(TEXT("the CDO does not implement UGrabbable"), GetDefault<AResourceDrop>()->Implements<UGrabbable>());
		const UStaticMeshComponent* CdoMesh = ReadMesh(GetDefault<AResourceDrop>());
		if (TestNotNull(TEXT("CDO MeshComponent readable"), CdoMesh))
		{
			TestTrue(TEXT("CDO preset is not PhysicsInteractable"), CdoMesh->GetCollisionProfileName() != FName(TEXT("PhysicsInteractable")));
			TestTrue(TEXT("CDO object type is not Interactable"), CdoMesh->GetCollisionObjectType() != COLLISION_INTERACTABLE);
		}

		// Same checks on a live drop: nothing done at BeginPlay may turn it into an interactable.
		AResourceDrop* Drop = SpawnDrop(TEXT("DropNotGrabbable"), 300.0, EResourceType::Wheat, 1);
		if (!TestNotNull(TEXT("drop spawned"), Drop)) { return; }
		TestFalse(TEXT("a spawned drop does not implement UGrabbable"), Drop->Implements<UGrabbable>());
		const UStaticMeshComponent* Mesh = ReadMesh(Drop);
		if (TestNotNull(TEXT("MeshComponent readable on the spawned drop"), Mesh))
		{
			TestTrue(TEXT("spawned drop preset is not PhysicsInteractable"), Mesh->GetCollisionProfileName() != FName(TEXT("PhysicsInteractable")));
			TestTrue(TEXT("spawned drop object type is not Interactable"), Mesh->GetCollisionObjectType() != COLLISION_INTERACTABLE);
		}
	});

	// Mutation: SpawnDrops stops at an entry (return instead of continue) or ignores SpawnCount -> the count turns red;
	//           the lifetime is only armed on some spawn paths (not by the drop itself) -> a SpawnDrops drop without ~300 s left, red;
	//           InitializeDrop/Amount no longer applied by SpawnDrops -> the Amount sum turns red.
	It("Criterion 9: SpawnDrops still spawns one drop per SpawnCount, each one armed with the default lifetime", [this]()
	{
		if (!RequireWorld()) { return; }
		TArray<FResourceDropEntry> Table;
		Table.Add(FieldPlotTest::MakeEntry(AResourceDrop::StaticClass(), EResourceType::Wheat, 2, 3));
		Table.Add(FieldPlotTest::MakeEntry(AResourceDrop::StaticClass(), EResourceType::Seed, 7, 0)); // spawns nothing, must not stop the loop
		Table.Add(FieldPlotTest::MakeEntry(AResourceDrop::StaticClass(), EResourceType::Seed, 1, 2));
		AResourceDrop::SpawnDrops(World, Table, FVector::ZeroVector, 100.0f);

		TestEqual(TEXT("one drop per SpawnCount (3 + 0 + 2)"), FieldPlotTest::CountDrops(World), 5);
		double AmountSum = 0.0;
		for (TActorIterator<AResourceDrop> It(World); It; ++It)
		{
			TestTrue(FString::Printf(TEXT("%s has about 300 s left (got %f)"), *It->GetName(), It->GetLifeSpan()),
			         It->GetLifeSpan() > 299.0f && It->GetLifeSpan() <= 300.0f);
			double Amount = 0.0;
			if (TestTrue(TEXT("Amount readable by reflection"), ReadNumber(*It, TEXT("Amount"), Amount)))
			{
				AmountSum += Amount;
			}
		}
		TestEqual(TEXT("sum of Amount over the drops (2*3 + 1*2)"), AmountSum, 8.0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
