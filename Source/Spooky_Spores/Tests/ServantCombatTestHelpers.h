// Test-only actors and helpers for the servant combat Automation Specs (criteria of docs/specs/servant-combat.md, removed at merge).
// UHT does not accept a UCLASS inside #if WITH_DEV_AUTOMATION_TESTS, so the classes are compiled in every configuration;
// they are NotPlaceable / NotBlueprintable and never used by the game (same approach as EnemyTestActors.h).
// Test worlds have no NavMesh and no floor: every move goes through a test seam, and every character gets its movement tick disabled
// so that positions (and 2D distances) stay exactly what the test set.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "AIController.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "Enemies/EnemySoldier.h"
#include "Enemies/EnemyPathSpline.h"
#include "Components/CapsuleComponent.h"
#include "Health/HealthComponent.h"
#include "Engine/World.h"
#include "Tests/EnemyTestActors.h"
#include "Tests/EnemyTestHelpers.h"
#include "Tests/ServantWanderTestHelpers.h"
// Must stay the last include (UHT).
#include "ServantCombatTestHelpers.generated.h"

// Test seam for the chase of an enemy (MoveToActor needs a NavMesh): records every request instead of moving and answers
// RequestSuccessful (a move is then "in progress" forever, the test completes it by moving actors by hand).
// With bFailSynchronously it reproduces what the engine does when the move fails at once: the end of the move is reported SYNCHRONOUSLY,
// from inside RequestMoveToEnemy, before it returns. The production controller must have written its fight state before the call.
UCLASS(NotBlueprintable, NotPlaceable)
class ATestCombatServantAIController : public AServantAIController
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
protected:
	virtual EPathFollowingRequestResult::Type RequestMoveToEnemy(AActor* Enemy) override
	{
		RequestedEnemies.Add(Enemy);
		if (bFailSynchronously)
		{
			OnMoveCompleted(FAIRequestID::InvalidRequest, FPathFollowingResult(EPathFollowingResult::Blocked, FPathFollowingResultFlags::None));
		}
		return EPathFollowingRequestResult::RequestSuccessful;
	}

// -----------PROPERTIES-----------
public:
	// Not UPROPERTY on purpose: owned by the test, only compared by pointer.
	TArray<TWeakObjectPtr<AActor>> RequestedEnemies;
	bool bFailSynchronously = false;
};

// A servant wired to the test seam above.
UCLASS(NotBlueprintable, NotPlaceable)
class ATestCombatServant : public AServant
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	ATestCombatServant()
	{
		AIControllerClass = ATestCombatServantAIController::StaticClass();
	}
};

// An enemy soldier that does nothing: no controller (so no spline, no target search, no attack), only the real health component and the
// registration in UEnemyRegistrySubsystem done by AEnemySoldier itself. Lets a test observe what the servants do to it.
UCLASS(NotBlueprintable, NotPlaceable)
class ATestPassiveEnemySoldier : public AEnemySoldier
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	ATestPassiveEnemySoldier()
	{
		AutoPossessAI = EAutoPossessAI::Disabled;
	}
};

#if WITH_DEV_AUTOMATION_TESTS

namespace ServantCombatTest
{
	inline ATestCombatServant* SpawnServant(UWorld* World, const TCHAR* Name, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.Name = FName(Name);
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATestCombatServant* Servant = World->SpawnActor<ATestCombatServant>(ATestCombatServant::StaticClass(), FTransform(Location), Params);
		if (Servant != nullptr && Servant->GetCharacterMovement() != nullptr)
		{
			Servant->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
		return Servant;
	}

	inline ATestPassiveEnemySoldier* SpawnEnemy(UWorld* World, const TCHAR* Name, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.Name = FName(Name);
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATestPassiveEnemySoldier* Enemy = World->SpawnActor<ATestPassiveEnemySoldier>(ATestPassiveEnemySoldier::StaticClass(), FTransform(Location), Params);
		if (Enemy != nullptr && Enemy->GetCharacterMovement() != nullptr)
		{
			Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
		return Enemy;
	}

	// Soldier on the Engage test seam (same sequence as the wave spawner: the spline is set before FinishSpawning).
	inline ATestSeamEnemySoldier* SpawnSeamSoldier(UWorld* World, const FVector& Location)
	{
		AEnemyPathSpline* Spline = EnemyTest::SpawnSpline(World, Location, FVector(1.0, 0.0, 0.0), 60.0);
		if (Spline == nullptr) { return nullptr; }
		ATestSeamEnemySoldier* Soldier = World->SpawnActorDeferred<ATestSeamEnemySoldier>(ATestSeamEnemySoldier::StaticClass(),
			FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Soldier == nullptr) { return nullptr; }
		Soldier->SetPathSplineActor(Spline);
		Soldier->FinishSpawning(FTransform(Location));
		Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		return Soldier;
	}

	inline ATestCombatServantAIController* ControllerOf(const APawn* Servant)
	{
		return (Servant != nullptr) ? Cast<ATestCombatServantAIController>(Servant->GetController()) : nullptr;
	}

	inline UHealthComponent* HealthOf(const AActor* Actor)
	{
		return (Actor != nullptr) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
	}

	inline double CapsuleRadius(const ACharacter* Character)
	{
		return (Character != nullptr && Character->GetCapsuleComponent() != nullptr) ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0;
	}

	// Centre-to-centre 2D distance at which the range measure of the spec (closest point of the target collision minus the servant's
	// capsule radius, in 2D) equals Gap. In range when Gap <= AttackRange: pass 0.5 * range for "in range", range + 150 for "out of range".
	inline double CentreDistanceForGap(const AServant* Servant, const ACharacter* Enemy, double Gap)
	{
		return CapsuleRadius(Servant) + CapsuleRadius(Enemy) + Gap;
	}

	// Puts the controller in the "walking" state without a NavMesh: a hand-made two-point path is handed to its path following component,
	// so a REAL request is in progress (status Moving) until something calls StopMovement. The pawn never moves (movement tick disabled in
	// these worlds), so it can never "arrive": the only way this walk ends is the production code cancelling it. False if the component
	// did not accept the request (the caller must report a harness error, not skip the assertion silently).
	inline bool BeginInjectedWalk(AAIController* Controller, const FVector& From, const FVector& To)
	{
		UPathFollowingComponent* Component = (Controller != nullptr) ? Controller->GetPathFollowingComponent() : nullptr;
		if (Component == nullptr) { return false; }
		TArray<FVector> Points;
		Points.Add(From);
		Points.Add(To);
		FNavPathSharedPtr Path = MakeShareable(new FNavigationPath(Points));
		FAIMoveRequest Request(To);
		Request.SetUsePathfinding(false).SetAcceptanceRadius(5.0f);
		Component->RequestMove(Request, Path);
		return Controller->GetMoveStatus() == EPathFollowingStatus::Moving;
	}

	// Absolute angle in degrees between the actor's yaw and the 2D direction from it towards Target.
	inline double YawErrorTowards(const AActor* Actor, const AActor* Target)
	{
		FVector Direction = Target->GetActorLocation() - Actor->GetActorLocation();
		Direction.Z = 0.0;
		const double ExpectedYaw = Direction.Rotation().Yaw;
		return FMath::Abs(FRotator::NormalizeAxis(Actor->GetActorRotation().Yaw - ExpectedYaw));
	}

	// Detaches the soldier's own death reaction (HandleDeath) so that a lethal hit leaves a DEAD soldier that is still alive in the
	// world and still registered, the state described by the spec ("a dead soldier may still be registered until its EndPlay").
	inline bool MakeDeathLinger(AEnemySoldier* Enemy)
	{
		UHealthComponent* Health = HealthOf(Enemy);
		if (Health == nullptr) { return false; }
		Health->OnDeath.RemoveAll(Enemy);
		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
