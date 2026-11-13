// Shared world helpers for the servant + field Automation Specs (harvest and replant): zone, servant and player spawning,
// task assignment, time advance and counter assertions. Free functions so that several specs reuse them.
// World recipe (same as ServantWanderWorldSpec): FTestWorldWrapper Game world, time advances only through TickTestWorld,
// servant movement tick disabled, NO NavMesh.

#pragma once

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
#include "Resources/ResourceCounterComponent.h"
#include "Resources/ResourceTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Templates/Function.h"
#include "Tests/AutomationCommon.h"

namespace ServantFieldTest
{
	inline constexpr float TickStep = 0.05f;
	inline constexpr double TimerSlack = 0.25;

	// Zone centred on the origin, default extent (500, 500, 200). Height is explicit so that it never decides "in zone".
	inline ABaseZone* SpawnZone(UWorld* World, const TCHAR* Name, bool bIgnoreHeight = true)
	{
		FActorSpawnParameters Params;
		Params.Name = FName(Name);
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABaseZone* Zone = World->SpawnActor<ABaseZone>(ABaseZone::StaticClass(), FTransform(FVector::ZeroVector), Params);
		if (Zone != nullptr)
		{
			BaseZoneTest::SetIgnoreHeight(Zone, bIgnoreHeight);
		}
		return Zone;
	}

	inline AServant* SpawnServant(UWorld* World, const TCHAR* Name, const FVector& Location)
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

	// Player 0: a player controller registered in the world with a pawn set through SetPawn (enough for GetPlayerPawn(World, 0)).
	// Returns the pawn's counter (stock Wheat / Seed, amounts <= 0 not added), or nullptr when bWithCounter is false
	// (the pawn still exists) or on a harness failure.
	inline UResourceCounterComponent* SpawnPlayer(UWorld* World, bool bWithCounter, int32 Wheat, int32 Seed)
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
		Controller->SetPawn(Pawn);
		return bWithCounter ? FieldPlotTest::MakeCounter(Pawn, Wheat, Seed) : nullptr;
	}

	inline bool Assign(UWorld* World, AServant* Servant, EServantTask Task)
	{
		UServantSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UServantSubsystem>() : nullptr;
		return Subsystem != nullptr && Servant != nullptr && Subsystem->IsServantRegistered(Servant)
			&& (Subsystem->AssignTask(Servant, Task) || Subsystem->GetServantTask(Servant) == Task);
	}

	inline void TickFor(FTestWorldWrapper& Wrapper, double Seconds)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			Wrapper.TickTestWorld(TickStep);
		}
	}

	inline bool TickUntil(FTestWorldWrapper& Wrapper, TFunctionRef<bool()> Condition, double MaxSeconds)
	{
		const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt32(MaxSeconds / TickStep));
		for (int32 Index = 0; Index < MaxSteps; ++Index)
		{
			if (Condition()) { return true; }
			Wrapper.TickTestWorld(TickStep);
		}
		return Condition();
	}

	inline void TickWatching(FTestWorldWrapper& Wrapper, double Seconds, TFunctionRef<void()> Observer)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			Wrapper.TickTestWorld(TickStep);
			Observer();
		}
	}

	inline void ExpectAction(FAutomationTestBase& Test, const FString& What, EServantWanderAction Actual, EServantWanderAction Expected)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: expected %s, got %s"), *What, *ServantWanderTest::ActionName(Expected), *ServantWanderTest::ActionName(Actual)),
		              Actual == Expected);
	}

	// Wheat and Seed as given; the types a field never yields must never move.
	inline void ExpectCounters(FAutomationTestBase& Test, const FString& What, const UResourceCounterComponent* Counter, int32 Wheat, int32 Seed)
	{
		Test.TestEqual(FString::Printf(TEXT("%s: Wheat"), *What), Counter->GetResourceCount(EResourceType::Wheat), Wheat);
		Test.TestEqual(FString::Printf(TEXT("%s: Seed"), *What), Counter->GetResourceCount(EResourceType::Seed), Seed);
		Test.TestEqual(FString::Printf(TEXT("%s: Wood"), *What), Counter->GetResourceCount(EResourceType::Wood), 0);
		Test.TestEqual(FString::Printf(TEXT("%s: Stone"), *What), Counter->GetResourceCount(EResourceType::Stone), 0);
		Test.TestEqual(FString::Printf(TEXT("%s: Bone"), *What), Counter->GetResourceCount(EResourceType::Bone), 0);
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
