// Automation Spec for the servant wandering, parts that need no UWorld: the pure decision function and the class default objects.
// Source of truth: the servant wandering spec (docs/specs/2.2c-servant-wandering.md, removed at merge), criteria A1, A2,
// plus the default values listed in its "Valeurs reglables" and "API exposee" sections.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantWanderTestHelpers.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

BEGIN_DEFINE_SPEC(FServantWanderLogicSpec, "SpookySpores.Servants.Wander.Logic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	void ExpectAction(const TCHAR* What, EServantWanderAction Actual, EServantWanderAction Expected);

END_DEFINE_SPEC(FServantWanderLogicSpec)

void FServantWanderLogicSpec::ExpectAction(const TCHAR* What, EServantWanderAction Actual, EServantWanderAction Expected)
{
	TestTrue(FString::Printf(TEXT("%s: expected %s, got %s"), What, *ServantWanderTest::ActionName(Expected), *ServantWanderTest::ActionName(Actual)),
	         Actual == Expected);
}

void FServantWanderLogicSpec::Define()
{
	Describe("DecideNextAction (A1)", [this]()
	{
		It("A1: no valid zone gives Wait, whatever the position and the counter", [this]()
		{
			ExpectAction(TEXT("no zone, in zone, counter 0"), AServantAIController::DecideNextAction(false, true, 0, 3), EServantWanderAction::Wait);
			ExpectAction(TEXT("no zone, out of zone, counter 0"), AServantAIController::DecideNextAction(false, false, 0, 3), EServantWanderAction::Wait);
			// Counter at and beyond the max: an invalid zone must never lead to a teleport into nowhere.
			ExpectAction(TEXT("no zone, out of zone, counter == max"), AServantAIController::DecideNextAction(false, false, 3, 3), EServantWanderAction::Wait);
			ExpectAction(TEXT("no zone, out of zone, counter > max"), AServantAIController::DecideNextAction(false, false, 7, 3), EServantWanderAction::Wait);
		});

		It("A1: inside the zone gives Wander, even with a counter at or above the max", [this]()
		{
			ExpectAction(TEXT("in zone, counter 0"), AServantAIController::DecideNextAction(true, true, 0, 3), EServantWanderAction::Wander);
			ExpectAction(TEXT("in zone, counter == max"), AServantAIController::DecideNextAction(true, true, 3, 3), EServantWanderAction::Wander);
			ExpectAction(TEXT("in zone, counter > max"), AServantAIController::DecideNextAction(true, true, 9, 3), EServantWanderAction::Wander);
		});

		It("A1: out of the zone gives Return below the max and Teleport from the max on (boundary included)", [this]()
		{
			ExpectAction(TEXT("out, counter 0"), AServantAIController::DecideNextAction(true, false, 0, 3), EServantWanderAction::Return);
			ExpectAction(TEXT("out, counter == max - 1"), AServantAIController::DecideNextAction(true, false, 2, 3), EServantWanderAction::Return);
			ExpectAction(TEXT("out, counter == max"), AServantAIController::DecideNextAction(true, false, 3, 3), EServantWanderAction::Teleport);
			ExpectAction(TEXT("out, counter > max"), AServantAIController::DecideNextAction(true, false, 4, 3), EServantWanderAction::Teleport);
			// The max is a parameter, not a hard-coded 3.
			ExpectAction(TEXT("out, max 1, counter 0"), AServantAIController::DecideNextAction(true, false, 0, 1), EServantWanderAction::Return);
			ExpectAction(TEXT("out, max 1, counter 1"), AServantAIController::DecideNextAction(true, false, 1, 1), EServantWanderAction::Teleport);
		});
	});

	Describe("Class default objects (A2 and spec defaults)", [this]()
	{
		It("A2: AServant spawns its own AServantAIController, also when spawned, and paths are acceleration driven", [this]()
		{
			const AServant* Default = GetDefault<AServant>();
			if (!TestNotNull(TEXT("AServant CDO exists"), Default)) { return; }

			TestTrue(TEXT("A2: AutoPossessAI == PlacedInWorldOrSpawned (the engine default PlacedInWorld never possesses a spawned servant)"),
			         Default->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned);
			TestTrue(TEXT("A2: AIControllerClass == AServantAIController"),
			         Default->AIControllerClass.Get() == AServantAIController::StaticClass());
			// The controller must not drive the yaw, otherwise bOrientRotationToMovement is overridden (servant walks sideways).
			TestFalse(TEXT("A2: bUseControllerRotationYaw == false"), Default->bUseControllerRotationYaw);

			const UCharacterMovementComponent* Movement = Default->GetCharacterMovement();
			if (TestNotNull(TEXT("A2: the CDO has a CharacterMovement component"), Movement))
			{
				TestTrue(TEXT("A2: NavMovementProperties.bUseAccelerationForPaths == true"),
				         Movement->GetNavMovementProperties().bUseAccelerationForPaths);
				TestTrue(TEXT("A2: bOrientRotationToMovement == true"), Movement->bOrientRotationToMovement);
			}
		});

		It("Spec defaults: runtime state starts idle and the tunable values match the spec", [this]()
		{
			const AServantAIController* Default = GetDefault<AServantAIController>();
			if (!TestNotNull(TEXT("AServantAIController CDO exists"), Default)) { return; }

			TestFalse(TEXT("bIsWandering defaults to false"), Default->IsWandering());
			TestEqual(TEXT("FailedReturnAttempts defaults to 0"), Default->GetFailedReturnAttempts(), 0);
			ExpectAction(TEXT("LastAction defaults to None"), Default->GetLastAction(), EServantWanderAction::None);

			struct FExpectedValue { const TCHAR* Name; double Value; };
			const FExpectedValue Expected[] =
			{
				{ TEXT("InitialDecisionDelay"), 0.5 },
				{ TEXT("MinWanderPause"), 2.0 },
				{ TEXT("MaxWanderPause"), 5.0 },
				{ TEXT("RetryDelay"), 2.0 },
				{ TEXT("MaxProjectionAttempts"), 10.0 },
				{ TEXT("ProjectionHorizontalExtent"), 100.0 },
				{ TEXT("ProjectionVerticalMargin"), 100.0 },
				{ TEXT("AcceptanceRadius"), 50.0 },
				{ TEXT("MaxFailedReturnAttempts"), 3.0 },
				{ TEXT("WanderWalkSpeed"), 275.0 },
			};
			for (const FExpectedValue& Entry : Expected)
			{
				double Actual = 0.0;
				if (TestTrue(FString::Printf(TEXT("%s is a reflected numeric property (name from the spec)"), Entry.Name),
				             ServantWanderTest::ReadNumber(Default, Entry.Name, Actual)))
				{
					TestEqual(FString::Printf(TEXT("%s default"), Entry.Name), Actual, Entry.Value, 0.0001);
				}
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
