// Test-only actors and listener for the enemy Automation Specs (wave spawner and Engage targeting; criteria of
// docs/specs/enemy-wave-spawner.md, removed at merge).
// UHT does not accept a UCLASS inside #if WITH_DEV_AUTOMATION_TESTS, so these classes are compiled in every configuration;
// they are NotPlaceable / NotBlueprintable and never used by the game (same approach as ServantTaskTestListener.h).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Enemies/Destructible.h"
#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemySoldier.h"
#include "Health/HealthComponent.h"
#include "EnemyTestActors.generated.h"

// A minimal destructible: a small box (queries only, overlaps everything so it never blocks a soldier) and a health component.
// The box gives the soldier a collision surface for its attack distance measurement.
UCLASS(NotBlueprintable, NotPlaceable)
class ATestDestructible : public AActor, public IDestructible
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	ATestDestructible()
	{
		Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
		Box->SetBoxExtent(FVector(50.0, 50.0, 50.0));
		Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		SetRootComponent(Box);
		Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	}

	virtual UHealthComponent* GetHealthComponent_Implementation() const override { return Health; }

	float GetHealthPercent() const { return Health->GetHealthPercent(); }
	bool IsDead() const { return Health->IsDead(); }

// -----------PROPERTIES-----------
private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY()
	TObjectPtr<UHealthComponent> Health;
};

// Stands in for BP_EnemySoldier, whose documented prerequisite is AutoPossessAI = PlacedInWorldOrSpawned (the C++ class cannot know it).
// Also flies (DefaultLandMovementMode) because the test worlds have neither a floor nor a NavMesh: the soldier then follows its
// AddMovementInput without falling.
// In flying mode BrakingDecelerationFlying defaults to 0 and only the weak fluid/braking friction slows the character down
// (CharacterMovementComponent.cpp: PhysFlying l.4444 uses 0.5 * FluidFriction, CalcVelocity l.3910-3915 and ApplyVelocityBraking l.4390 do
// nothing without braking beyond that friction), so a soldier with no input would drift hundreds of units on its momentum. A walking
// soldier is stopped by the floor friction; a high flying deceleration reproduces that in the floorless test worlds.
UCLASS(NotBlueprintable, NotPlaceable)
class ATestEnemySoldier : public AEnemySoldier
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	ATestEnemySoldier()
	{
		AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
		AIControllerClass = AEnemyAIController::StaticClass();
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
		// Only applied when there is no acceleration (CalcVelocity l.3910): it does not slow the spline following.
		GetCharacterMovement()->BrakingDecelerationFlying = 8000.0f;
	}
};

// Test seam for the NavMesh move of the Engage state (test worlds have no NavMesh): records every move request instead of
// moving, and answers with a configurable result. A soldier uses it through AIControllerClass.
UCLASS(NotBlueprintable, NotPlaceable)
class ATestEnemyAIController : public AEnemyAIController
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
protected:
	virtual EPathFollowingRequestResult::Type RequestMoveToTarget(AActor* Target) override
	{
		RequestedTargets.Add(Target);
		return NextResult;
	}

// -----------PROPERTIES-----------
public:
	// Not UPROPERTY on purpose: owned by the test, only compared by pointer.
	TArray<TWeakObjectPtr<AActor>> RequestedTargets;
	EPathFollowingRequestResult::Type NextResult = EPathFollowingRequestResult::RequestSuccessful;
};

namespace EnemyTest
{
	enum class EWaveEvent : uint8
	{
		Started,
		Cleared,
		AllCleared
	};

	struct FWaveEvent
	{
		EWaveEvent Type = EWaveEvent::Started;
		int32 Index = INDEX_NONE; // INDEX_NONE for AllCleared
		double Time = 0.0;        // world time at broadcast
	};
}

// Records, in order, every event of the wave spawner. A dynamic multicast delegate can only call a UFUNCTION of a UObject.
UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class UEnemyWaveTestListener : public UObject
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	UFUNCTION()
	void HandleWaveStarted(int32 WaveIndex)
	{
		Record(EnemyTest::EWaveEvent::Started, WaveIndex);
		if (OnStartedHook) { OnStartedHook(WaveIndex); }
	}

	UFUNCTION()
	void HandleWaveCleared(int32 WaveIndex)
	{
		Record(EnemyTest::EWaveEvent::Cleared, WaveIndex);
		if (OnClearedHook) { OnClearedHook(WaveIndex); }
	}

	UFUNCTION()
	void HandleAllWavesCleared()
	{
		Record(EnemyTest::EWaveEvent::AllCleared, INDEX_NONE);
	}

	int32 Count(EnemyTest::EWaveEvent Type) const
	{
		int32 Result = 0;
		for (const EnemyTest::FWaveEvent& Event : Events) { Result += (Event.Type == Type) ? 1 : 0; }
		return Result;
	}

	// Time of the first event of this type and index, or -1 when there is none.
	double TimeOf(EnemyTest::EWaveEvent Type, int32 Index) const
	{
		for (const EnemyTest::FWaveEvent& Event : Events)
		{
			if (Event.Type == Type && Event.Index == Index) { return Event.Time; }
		}
		return -1.0;
	}

// -----------PROPERTIES-----------
public:
	// Not UPROPERTY on purpose: owned by the test, valid for the whole lifetime of the test world.
	UWorld* World = nullptr;
	TArray<EnemyTest::FWaveEvent> Events;
	// Optional reactions run inside the broadcast (used to call StopWaves from a callback).
	TFunction<void(int32)> OnStartedHook;
	TFunction<void(int32)> OnClearedHook;

private:
	void Record(EnemyTest::EWaveEvent Type, int32 Index)
	{
		EnemyTest::FWaveEvent Event;
		Event.Type = Type;
		Event.Index = Index;
		Event.Time = (World != nullptr) ? World->GetTimeSeconds() : 0.0;
		Events.Add(Event);
	}
};
