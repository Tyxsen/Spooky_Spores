// Automation Spec for the construction of ABaseZone, read on the class default object (no UWorld).
// Source of truth: docs/specs/base-zone.md, criteria C1-C3.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BaseZoneTestHelpers.h"
#include "Base/BaseZone.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"

BEGIN_DEFINE_SPEC(FBaseZoneCdoSpec, "SpookySpores.Base.Zone.Construction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FBaseZoneCdoSpec)

void FBaseZoneCdoSpec::Define()
{
	Describe("Class default object", [this]()
	{
		It("C1: ZoneBox exists and is a pure geometry volume (no collision, no overlaps, no navigation)", [this]()
		{
			const ABaseZone* Default = GetDefault<ABaseZone>();
			if (!TestNotNull(TEXT("CDO exists"), Default))
			{
				return;
			}
			const UBoxComponent* Box = Default->GetZoneBox();
			if (!TestNotNull(TEXT("C1: ZoneBox is created in the constructor"), Box))
			{
				return;
			}
			TestTrue(TEXT("C1: collision is NoCollision"), Box->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
			TestTrue(TEXT("C1: preset is the built-in NoCollision (spec 2.4)"), Box->GetCollisionProfileName() == FName(TEXT("NoCollision")));
			TestFalse(TEXT("C1: overlap events are disabled"), Box->GetGenerateOverlapEvents());
			TestFalse(TEXT("C1: the box can never affect navigation"), Box->CanEverAffectNavigation());
		});

		It("C2: ZoneBox is the root component", [this]()
		{
			const ABaseZone* Default = GetDefault<ABaseZone>();
			if (!TestNotNull(TEXT("CDO exists"), Default))
			{
				return;
			}
			TestTrue(TEXT("C2: GetRootComponent() == ZoneBox"),
			         Default->GetRootComponent() != nullptr && Default->GetRootComponent() == Default->GetZoneBox());
		});

		It("C3: default values (bIgnoreHeight, extent, no tick, hidden in game)", [this]()
		{
			const ABaseZone* Default = GetDefault<ABaseZone>();
			if (!TestNotNull(TEXT("CDO exists"), Default))
			{
				return;
			}

			// bIgnoreHeight is protected: read it through reflection, using the name given by the spec.
			const FBoolProperty* Property = FindFProperty<FBoolProperty>(ABaseZone::StaticClass(), TEXT("bIgnoreHeight"));
			if (TestNotNull(TEXT("C3: bIgnoreHeight is a reflected bool property"), Property))
			{
				TestTrue(TEXT("C3: bIgnoreHeight defaults to true"), Property->GetPropertyValue_InContainer(Default));
			}

			const UBoxComponent* Box = Default->GetZoneBox();
			if (TestNotNull(TEXT("C3: ZoneBox exists"), Box))
			{
				TestTrue(TEXT("C3: default extent is (500, 500, 200)"), Box->GetUnscaledBoxExtent().Equals(FVector(500.0, 500.0, 200.0), 0.001));
				TestTrue(TEXT("C3: the C++ keeps the engine default bHiddenInGame = true (BP_BaseZone unticks it)"), Box->bHiddenInGame);
			}

			TestFalse(TEXT("C3: the actor never ticks"), Default->PrimaryActorTick.bCanEverTick);
		});

		It("C3 (behaviour): the CDO answers membership queries without a world, with height ignored by default", [this]()
		{
			const ABaseZone* Default = GetDefault<ABaseZone>();
			if (!TestNotNull(TEXT("CDO exists"), Default))
			{
				return;
			}
			// Spec 4: "No World (CDO): the calculation functions need no UWorld". Default box: centered at origin, extent (500, 500, 200).
			TestTrue(TEXT("CDO: zone is valid with the default extent"), Default->IsZoneValid());
			TestTrue(TEXT("CDO: (0,0,5000) is inside because bIgnoreHeight defaults to true"), Default->IsPointInZone(FVector(0, 0, 5000)));
			TestFalse(TEXT("CDO: (501,0,0) is outside"), Default->IsPointInZone(FVector(501, 0, 0)));
			TestTrue(TEXT("CDO: (499,499,0) is inside"), Default->IsPointInZone(FVector(499, 499, 0)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
