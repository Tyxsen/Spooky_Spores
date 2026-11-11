// Automation Spec for ABaseZone and UBaseZoneSubsystem inside a minimal test world.
// Source of truth: the base zone spec (removed at merge, see commit a5a85da), criteria A8, A10 (member part), R7, D1-D2 (member part), C4, S1-S4,
// plus integration tests that go through the real actor so the static maths are not the only thing exercised.
//
// The world comes from FTestWorldWrapper (Engine module, Tests/AutomationCommon.h): a transient Game world with a
// GameInstance and a GameMode, begun play, so that actors spawned afterwards run BeginPlay/EndPlay immediately.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BaseZoneTestHelpers.h"
#include "Base/BaseZone.h"
#include "Base/BaseZoneSubsystem.h"
#include "CollisionChannels.h"
#include "Spooky_Spores.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Tests/AutomationCommon.h"
#include <limits>

BEGIN_DEFINE_SPEC(FBaseZoneWorldSpec, "SpookySpores.Base.Zone.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	ABaseZone* SpawnZone(const TCHAR* Name, const FTransform& Transform = FTransform::Identity) const;
	UBaseZoneSubsystem* GetSubsystem() const;
	bool RequireWorld();

END_DEFINE_SPEC(FBaseZoneWorldSpec)

ABaseZone* FBaseZoneWorldSpec::SpawnZone(const TCHAR* Name, const FTransform& Transform) const
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABaseZone* Zone = World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform::Identity, Params);
	if (Zone != nullptr)
	{
		// Applied after spawn so that rotation and scale are certainly part of the component transform.
		Zone->SetActorTransform(Transform);
	}
	return Zone;
}

UBaseZoneSubsystem* FBaseZoneWorldSpec::GetSubsystem() const
{
	return World != nullptr ? World->GetSubsystem<UBaseZoneSubsystem>() : nullptr;
}

bool FBaseZoneWorldSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

void FBaseZoneWorldSpec::Define()
{
	using namespace BaseZoneTest;

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
		// The wrapper destructor ends play and destroys the world (and every actor spawned in it).
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	Describe("Real actor integration", [this]()
	{
		It("A8/default: a spawned zone has the default extent, is valid and ignores height by default", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneDefault"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }

			TestTrue(TEXT("zone is valid"), Zone->IsZoneValid());
			TestTrue(TEXT("GetZoneExtent() == (500,500,200)"), Zone->GetZoneExtent().Equals(FVector(500, 500, 200), 0.001));
			TestTrue(TEXT("(0,0,5000) is inside with the default bIgnoreHeight=true"), Zone->IsPointInZone(FVector(0, 0, 5000)));
			TestFalse(TEXT("(501,0,0) is outside"), Zone->IsPointInZone(FVector(501, 0, 0)));

			TestTrue(TEXT("bIgnoreHeight can be flipped through reflection"), SetIgnoreHeight(Zone, false));
			TestFalse(TEXT("(0,0,5000) is outside once bIgnoreHeight=false: the member function honours the flag"),
			          Zone->IsPointInZone(FVector(0, 0, 5000)));
			TestTrue(TEXT("(0,0,199) is still inside"), Zone->IsPointInZone(FVector(0, 0, 199)));
		});

		It("A3 via the actor: the actor transform (location and yaw) is used", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneRotated"), FTransform(FRotator(0, 90, 0), FVector(1000, 0, 0)));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetBoxExtent(FVector(500, 200, 100));
			SetIgnoreHeight(Zone, false);

			TestTrue(TEXT("(1000,400,0) is inside"), Zone->IsPointInZone(FVector(1000, 400, 0)));
			TestFalse(TEXT("(1400,0,0) is outside"), Zone->IsPointInZone(FVector(1400, 0, 0)));
		});

		It("A8: SetBoxExtent at runtime is picked up by the next query", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneResize"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			SetIgnoreHeight(Zone, false);
			Zone->GetZoneBox()->SetBoxExtent(FVector(500, 300, 100));

			TestFalse(TEXT("(600,0,0) is outside before the resize"), Zone->IsPointInZone(FVector(600, 0, 0)));
			Zone->GetZoneBox()->SetBoxExtent(FVector(700, 300, 100));
			TestTrue(TEXT("(600,0,0) is inside after SetBoxExtent((700,300,100))"), Zone->IsPointInZone(FVector(600, 0, 0)));
			Zone->GetZoneBox()->SetBoxExtent(FVector(500, 300, 100));
			TestFalse(TEXT("(600,0,0) is outside again after shrinking back"), Zone->IsPointInZone(FVector(600, 0, 0)));
		});

		It("A8: SetWorldScale3D at runtime is picked up by the next query", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneRescale"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			SetIgnoreHeight(Zone, false);
			Zone->GetZoneBox()->SetBoxExtent(FVector(500, 300, 100));

			TestFalse(TEXT("(600,0,0) is outside at scale 1"), Zone->IsPointInZone(FVector(600, 0, 0)));
			Zone->GetZoneBox()->SetWorldScale3D(FVector(2, 1, 1));
			TestTrue(TEXT("(600,0,0) is inside at scale (2,1,1)"), Zone->IsPointInZone(FVector(600, 0, 0)));
			TestFalse(TEXT("(0,400,0) is still outside at scale (2,1,1): Y was not scaled"), Zone->IsPointInZone(FVector(0, 400, 0)));
		});

		It("A8: moving the actor at runtime is picked up by the next query", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneMove"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			SetIgnoreHeight(Zone, false);
			Zone->GetZoneBox()->SetBoxExtent(FVector(500, 300, 100));

			TestFalse(TEXT("(1100,0,0) is outside before the move"), Zone->IsPointInZone(FVector(1100, 0, 0)));
			Zone->SetActorLocation(FVector(1000, 0, 0));
			TestTrue(TEXT("(1100,0,0) is inside after moving the actor to x=1000"), Zone->IsPointInZone(FVector(1100, 0, 0)));
			TestFalse(TEXT("the old center (0,0,0) is outside after the move"), Zone->IsPointInZone(FVector(0, 0, 0)));
		});

		It("A10 via the actor: GetZoneExtent returns positive components under a negative scale", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneMirror"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			SetIgnoreHeight(Zone, false);
			Zone->GetZoneBox()->SetBoxExtent(FVector(100, 100, 100));
			Zone->GetZoneBox()->SetWorldScale3D(FVector(-3, 1, 1));

			TestTrue(TEXT("A10: GetZoneExtent() == (300,100,100)"), Zone->GetZoneExtent().Equals(FVector(300, 100, 100), 0.001));
			TestTrue(TEXT("A10: zone stays valid"), Zone->IsZoneValid());
			TestTrue(TEXT("A10: (250,0,0) inside"), Zone->IsPointInZone(FVector(250, 0, 0)));
			TestFalse(TEXT("A10: (350,0,0) outside"), Zone->IsPointInZone(FVector(350, 0, 0)));
			TestFalse(TEXT("A10: (0,250,0) outside"), Zone->IsPointInZone(FVector(0, 250, 0)));
		});

		It("GetZoneExtent includes the scale on every axis", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneExtentScale"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetWorldScale3D(FVector(2, 3, 4));
			// Default extent (500,500,200) scaled by (2,3,4). 2.2c sizes its NavMesh vertical extent with .Z.
			TestTrue(TEXT("GetZoneExtent() == (1000,1500,800)"), Zone->GetZoneExtent().Equals(FVector(1000, 1500, 800), 0.001));
		});

		It("A9 via the actor: non-finite points are rejected", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneNaN"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			TestTrue(TEXT("control: a finite point is inside"), Zone->IsPointInZone(FVector(1, 2, 3)));
			TestFalse(TEXT("NaN is rejected"), Zone->IsPointInZone(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0)));
			TestFalse(TEXT("infinity is rejected"), Zone->IsPointInZone(FVector(0, std::numeric_limits<double>::infinity(), 0)));
		});

		It("R1 via the actor: every random point satisfies IsPointInZone, in both height modes", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneDraw"), FTransform(FRotator(0, 45, 0), FVector(300, -200, 50)));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }

			for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
			{
				SetIgnoreHeight(Zone, HeightMode == 1);
				int32 Rejected = 0;
				bool bAllSucceeded = true;
				for (int32 Index = 0; Index < 500; ++Index)
				{
					FVector Point = FVector::ZeroVector;
					if (!Zone->GetRandomPointInZone(Point)) { bAllSucceeded = false; break; }
					if (!Zone->IsPointInZone(Point)) { ++Rejected; }
				}
				TestTrue(FString::Printf(TEXT("all draws succeed (ignoreHeight=%d)"), HeightMode), bAllSucceeded);
				TestEqual(*FString::Printf(TEXT("drawn points rejected by IsPointInZone (ignoreHeight=%d)"), HeightMode), Rejected, 0);
			}
		});

		It("R7: draws follow SetBoxExtent, SetWorldScale3D and actor moves (nothing is memorised)", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneRedraw"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			SetIgnoreHeight(Zone, false);
			UBoxComponent* Box = Zone->GetZoneBox();
			Box->SetBoxExtent(FVector(500, 300, 100));

			// Warm-up draws on the old extent, then enlarge.
			for (int32 Index = 0; Index < 200; ++Index)
			{
				FVector Ignored;
				Zone->GetRandomPointInZone(Ignored);
			}
			Box->SetBoxExtent(FVector(700, 300, 100));
			double MaxLocalX = 0.0;
			bool bWithinNewExtent = true;
			for (int32 Index = 0; Index < DrawCount; ++Index)
			{
				FVector P;
				if (!Zone->GetRandomPointInZone(P)) { AddError(TEXT("R7: draw failed on a valid zone")); return; }
				const FVector Local = Box->GetComponentTransform().InverseTransformPosition(P);
				MaxLocalX = FMath::Max(MaxLocalX, FMath::Abs(Local.X));
				bWithinNewExtent &= FMath::Abs(Local.X) <= 700.0 + IndependentTolerance;
			}
			TestTrue(TEXT("R7: after enlarging, draws go beyond the old extent (local |X| > 500)"), MaxLocalX > 500.0);
			TestTrue(TEXT("R7: after enlarging, draws stay within the new extent"), bWithinNewExtent);

			// Shrink: a cached larger extent would produce points outside.
			Box->SetBoxExtent(FVector(100, 100, 100));
			bool bWithinShrunk = true;
			for (int32 Index = 0; Index < 500; ++Index)
			{
				FVector P;
				Zone->GetRandomPointInZone(P);
				const FVector Local = Box->GetComponentTransform().InverseTransformPosition(P);
				bWithinShrunk &= FMath::Abs(Local.X) <= 100.0 + IndependentTolerance && FMath::Abs(Local.Y) <= 100.0 + IndependentTolerance;
			}
			TestTrue(TEXT("R7: after shrinking, draws stay within the new extent"), bWithinShrunk);

			// Move: draws follow the actor.
			Zone->SetActorLocation(FVector(5000, 0, 0));
			bool bFollowsActor = true;
			for (int32 Index = 0; Index < 200; ++Index)
			{
				FVector P;
				Zone->GetRandomPointInZone(P);
				bFollowsActor &= FMath::Abs(P.X - 5000.0) <= 100.0 + IndependentTolerance;
			}
			TestTrue(TEXT("R7: after moving the actor, draws are around the new center"), bFollowsActor);
		});
	});

	Describe("Degenerate zones through the actor (D1, D2)", [this]()
	{
		It("D1: zero extent makes the actor invalid and every query false, even at the exact center", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneZeroExtent"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetBoxExtent(FVector(0, 300, 100));
			for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
			{
				SetIgnoreHeight(Zone, HeightMode == 1);
				TestFalse(TEXT("IsZoneValid() is false"), Zone->IsZoneValid());
				TestFalse(TEXT("the exact center is not inside a zero-extent zone"), Zone->IsPointInZone(Zone->GetActorLocation()));
				FVector Out;
				TestFalse(TEXT("D2: GetRandomPointInZone returns false (the value of Out is not checked)"), Zone->GetRandomPointInZone(Out));
			}
		});

		It("D1: zero scale on one axis makes the actor invalid (an inverse-transform with a safe inverse would say 'inside')", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneZeroScale"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetWorldScale3D(FVector(1, 0, 1));
			TestFalse(TEXT("IsZoneValid() is false"), Zone->IsZoneValid());
			TestFalse(TEXT("the center is not inside"), Zone->IsPointInZone(Zone->GetActorLocation()));
			FVector Out;
			TestFalse(TEXT("GetRandomPointInZone returns false"), Zone->GetRandomPointInZone(Out));
		});

		It("D1: a hand-entered negative extent is an invalid zone, not a crash", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneNegativeExtent"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetBoxExtent(FVector(-500, 500, 200));
			TestFalse(TEXT("IsZoneValid() is false"), Zone->IsZoneValid());
			TestFalse(TEXT("the center is not inside"), Zone->IsPointInZone(Zone->GetActorLocation()));
			FVector Out;
			TestFalse(TEXT("GetRandomPointInZone returns false"), Zone->GetRandomPointInZone(Out));
		});

		It("Section 4: the invalid-zone Warning is logged once per zone, not at every query", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneWarnOnce"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			Zone->GetZoneBox()->SetBoxExtent(FVector(0, 300, 100));

			FCategoryLogCounter Counter(LogSpooky_Spores.GetCategoryName());
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Zone->IsPointInZone(FVector::ZeroVector);
				FVector Out;
				Zone->GetRandomPointInZone(Out);
				Zone->IsZoneValid();
			}
			Counter.FlushPendingLogs();
			TestEqual(TEXT("exactly one LogSpooky_Spores warning for 30 queries on one invalid zone"), Counter.WarningCount, 1);
			TestEqual(TEXT("no LogSpooky_Spores error for an invalid zone"), Counter.ErrorCount, 0);
		});

		It("Non-finite extent through the actor: invalid, every query false, no crash, one single warning", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneInfExtent"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }

			// The value is injected by reflection on purpose: SetBoxExtent would run UpdateBounds on an infinite extent,
			// which can raise an ensure in Debug builds. That is an engine infrastructure risk unrelated to the logic under test.
			FStructProperty* ExtentProperty = FindFProperty<FStructProperty>(UBoxComponent::StaticClass(), TEXT("BoxExtent"));
			if (!TestNotNull(TEXT("UBoxComponent::BoxExtent property found by reflection"), ExtentProperty)) { return; }
			FVector* ExtentValue = ExtentProperty->ContainerPtrToValuePtr<FVector>(Zone->GetZoneBox());
			if (!TestNotNull(TEXT("BoxExtent value pointer resolved"), ExtentValue)) { return; }
			*ExtentValue = FVector(std::numeric_limits<double>::infinity(), 300, 100);

			FCategoryLogCounter Counter(LogSpooky_Spores.GetCategoryName());
			for (int32 HeightMode = 0; HeightMode < 2; ++HeightMode)
			{
				SetIgnoreHeight(Zone, HeightMode == 1);
				for (int32 Index = 0; Index < 5; ++Index)
				{
					TestFalse(TEXT("IsZoneValid() is false"), Zone->IsZoneValid());
					TestFalse(TEXT("the center is not inside an infinite-extent zone"), Zone->IsPointInZone(Zone->GetActorLocation()));
					TestFalse(TEXT("a far point is not inside either"), Zone->IsPointInZone(FVector(1e9, 0, 0)));
					FVector Out(1, 2, 3);
					TestFalse(TEXT("GetRandomPointInZone returns false"), Zone->GetRandomPointInZone(Out));
				}
			}
			Counter.FlushPendingLogs();
			TestEqual(TEXT("exactly one LogSpooky_Spores warning for 40 queries on one non-finite zone"), Counter.WarningCount, 1);
			TestEqual(TEXT("no LogSpooky_Spores error"), Counter.ErrorCount, 0);
		});

		It("Section 4: a valid zone never logs a warning", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneNoWarn"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }

			FCategoryLogCounter Counter(LogSpooky_Spores.GetCategoryName());
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Zone->IsPointInZone(FVector::ZeroVector);
				FVector Out;
				Zone->GetRandomPointInZone(Out);
			}
			Counter.FlushPendingLogs();
			TestEqual(TEXT("no LogSpooky_Spores warning on a valid zone"), Counter.WarningCount, 0);
		});
	});

	Describe("C4: the zone is invisible to physics queries", [this]()
	{
		It("C4: interaction trace, visibility trace and a pawn-sized sweep pass through the zone", [this]()
		{
			if (!RequireWorld()) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneTrace"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			const FVector Start(-2000, 0, 0);
			const FVector End(2000, 0, 0);

			// Production state first.
			FHitResult Hit;
			const bool bObjectHit = World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(COLLISION_INTERACTABLE));
			TestFalse(TEXT("C4: the Interactable object trace does not hit the zone"), bObjectHit);

			const bool bVisibilityHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);
			TestFalse(TEXT("C4: the Visibility trace does not hit the zone"), bVisibilityHit);

			const bool bPawnSweepHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
			                                                       FCollisionShape::MakeCapsule(34.0f, 88.0f));
			TestFalse(TEXT("C4: a pawn-sized capsule sweep is not blocked by the zone"), bPawnSweepHit);

			// Positive controls: with a blocking preset the very same queries must hit, otherwise the negatives above prove nothing
			// (physics scene not ready in the test world => harness problem, reported as a failure rather than a silent pass).
			UBoxComponent* Box = Zone->GetZoneBox();
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			TestTrue(TEXT("C4 control: Visibility trace hits the box once it blocks (harness check)"),
			         World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility));
			TestTrue(TEXT("C4 control: the pawn sweep is blocked once the box blocks (harness check)"),
			         World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.0f, 88.0f)));

			Box->SetCollisionProfileName(TEXT("PhysicsInteractable"));
			TestTrue(TEXT("C4 control: the Interactable object trace hits the box with the PhysicsInteractable preset (harness check)"),
			         World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(COLLISION_INTERACTABLE)));
		});
	});

	Describe("UBaseZoneSubsystem registry", [this]()
	{
		It("S1: a single zone is returned by GetBaseZone() after BeginPlay", [this]()
		{
			if (!RequireWorld()) { return; }
			UBaseZoneSubsystem* Subsystem = GetSubsystem();
			if (!TestNotNull(TEXT("the world has a UBaseZoneSubsystem"), Subsystem)) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneS1"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			TestTrue(TEXT("S1: GetBaseZone() returns the spawned zone"), Subsystem->GetBaseZone() == Zone);
		});

		It("S2: with no zone, GetBaseZone() is nullptr and nothing is logged as an error", [this]()
		{
			if (!RequireWorld()) { return; }
			UBaseZoneSubsystem* Subsystem = GetSubsystem();
			if (!TestNotNull(TEXT("the world has a UBaseZoneSubsystem"), Subsystem)) { return; }
			FCategoryLogCounter Counter(LogSpooky_Spores.GetCategoryName());
			TestNull(TEXT("S2: GetBaseZone() is nullptr"), Subsystem->GetBaseZone());
			Counter.FlushPendingLogs();
			TestEqual(TEXT("S2: no LogSpooky_Spores error"), Counter.ErrorCount, 0);
		});

		It("S3: two zones - the first stays registered, the second is reported with an Error", [this]()
		{
			if (!RequireWorld()) { return; }
			UBaseZoneSubsystem* Subsystem = GetSubsystem();
			if (!TestNotNull(TEXT("the world has a UBaseZoneSubsystem"), Subsystem)) { return; }

			ABaseZone* First = SpawnZone(TEXT("BaseZoneFirst"));
			if (!TestNotNull(TEXT("first zone spawned"), First)) { return; }
			TestTrue(TEXT("S3: the first zone is registered"), Subsystem->GetBaseZone() == First);

			// The spec says the Error names both actors: matching the second one's name proves the log is emitted and is not generic.
			AddExpectedError(TEXT("BaseZoneSecond"), EAutomationExpectedErrorFlags::Contains, 1, false);
			ABaseZone* Second = SpawnZone(TEXT("BaseZoneSecond"));
			if (!TestNotNull(TEXT("second zone spawned"), Second)) { return; }
			TestTrue(TEXT("S3: the first zone is still the registered one"), Subsystem->GetBaseZone() == First);

			Second->Destroy();
			TestTrue(TEXT("S3: destroying the ignored second zone does not unregister the first"), Subsystem->GetBaseZone() == First);

			First->Destroy();
			TestNull(TEXT("S3: destroying the first zone leaves no zone (the second stays ignored)"), Subsystem->GetBaseZone());
		});

		It("S3: destroying the first zone while the second is alive leaves nullptr", [this]()
		{
			if (!RequireWorld()) { return; }
			UBaseZoneSubsystem* Subsystem = GetSubsystem();
			if (!TestNotNull(TEXT("the world has a UBaseZoneSubsystem"), Subsystem)) { return; }

			ABaseZone* First = SpawnZone(TEXT("BaseZoneFirstB"));
			AddExpectedError(TEXT("BaseZoneSecondB"), EAutomationExpectedErrorFlags::Contains, 1, false);
			ABaseZone* Second = SpawnZone(TEXT("BaseZoneSecondB"));
			if (!TestNotNull(TEXT("both zones spawned"), First) || !TestNotNull(TEXT("both zones spawned"), Second)) { return; }

			First->Destroy();
			TestNull(TEXT("S3: the second zone was never registered, so nothing is returned"), Subsystem->GetBaseZone());
		});

		It("S4: after the only zone is destroyed, GetBaseZone() is nullptr (no dangling pointer)", [this]()
		{
			if (!RequireWorld()) { return; }
			UBaseZoneSubsystem* Subsystem = GetSubsystem();
			if (!TestNotNull(TEXT("the world has a UBaseZoneSubsystem"), Subsystem)) { return; }
			ABaseZone* Zone = SpawnZone(TEXT("BaseZoneS4"));
			if (!TestNotNull(TEXT("zone spawned"), Zone)) { return; }
			TestTrue(TEXT("precondition: the zone is registered"), Subsystem->GetBaseZone() == Zone);

			Zone->Destroy();
			TestNull(TEXT("S4: GetBaseZone() is nullptr after Destroy()"), Subsystem->GetBaseZone());
		});

		It("S4 (derived): a new zone can register after the previous one was destroyed", [this]()
		{
			if (!RequireWorld()) { return; }
			UBaseZoneSubsystem* Subsystem = GetSubsystem();
			if (!TestNotNull(TEXT("the world has a UBaseZoneSubsystem"), Subsystem)) { return; }
			ABaseZone* Old = SpawnZone(TEXT("BaseZoneOld"));
			if (!TestNotNull(TEXT("zone spawned"), Old)) { return; }
			Old->Destroy();

			// No expected error here: a stale registration treated as "occupied" would log an Error and fail this test.
			ABaseZone* Replacement = SpawnZone(TEXT("BaseZoneReplacement"));
			if (!TestNotNull(TEXT("replacement spawned"), Replacement)) { return; }
			TestTrue(TEXT("the replacement is registered"), Subsystem->GetBaseZone() == Replacement);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
