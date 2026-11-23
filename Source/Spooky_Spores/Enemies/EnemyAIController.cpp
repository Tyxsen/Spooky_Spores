#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemyPathSpline.h"
#include "Enemies/EnemySoldier.h"
#include "Enemies/EnemyTargetSubsystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/Destructible.h"
#include "Health/HealthComponent.h"
#include "Base/BaseZone.h"
#include "Base/BaseZoneSubsystem.h"
#include "Spooky_Spores.h"

namespace
{
	// A destructible is a valid target only while its health component exists and is not dead.
	bool IsAliveDestructible(AActor* Candidate)
	{
		if (!Candidate || !Candidate->Implements<UDestructible>()) return false;

		const UHealthComponent* Health = IDestructible::Execute_GetHealthComponent(Candidate);
		return Health && !Health->IsDead();
	}
}

AEnemyAIController::AEnemyAIController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

AActor* AEnemyAIController::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AEnemySoldier* Soldier = Cast<AEnemySoldier>(InPawn))
	{
		PathSplineActor = Soldier->GetPathSplineActor();
	}

	DistanceAlongSpline = 0.0f;
	CurrentState = EEnemyState::Transit;
}

void AEnemyAIController::OnUnPossess()
{
	if (UEnemyTargetSubsystem* TargetSubsystem = GetTargetSubsystem())
	{
		TargetSubsystem->ReleaseClaim(this);
	}

	Super::OnUnPossess();
}

void AEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEnemyTargetSubsystem* TargetSubsystem = GetTargetSubsystem())
	{
		TargetSubsystem->ReleaseClaim(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (CurrentState)
	{
		case EEnemyState::Transit:
			TickTransit(DeltaSeconds);
			break;
		case EEnemyState::Engage:
			TickEngage(DeltaSeconds);
			break;
	}
}

void AEnemyAIController::TickTransit(float DeltaSeconds)
{
	if (!PathSplineActor.IsValid()) return;

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	const float SplineLength = PathSplineActor->GetSplineComponent()->GetSplineLength();

	DistanceAlongSpline += MoveSpeed * DeltaSeconds;
	if (DistanceAlongSpline >= SplineLength)
	{
		CurrentState = EEnemyState::Engage;
		return;
	}
	else
	{
		const float LookAheadTarget = FMath::Min(DistanceAlongSpline + LookAheadDistance, SplineLength);
		const FVector TargetPoint = PathSplineActor->GetSplineComponent()->GetLocationAtDistanceAlongSpline(LookAheadTarget, ESplineCoordinateSpace::World);
		const FVector Direction = (TargetPoint - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(Direction);
	}
}

void AEnemyAIController::TickEngage(float DeltaSeconds)
{
	// A weak pointer stays valid while the destructible is at 0 HP but not yet destroyed, so liveness is checked too.
	if (!IsAliveDestructible(CurrentTarget.Get()))
	{
		ChooseTarget();
	}

	if (!CurrentTarget.IsValid()) return;

	const AEnemySoldier* Soldier = Cast<AEnemySoldier>(GetPawn());
	if (!Soldier) return;

	// The distance test stays the authority for "arrived", not the path following.
	if (IsInAttackRange(*Soldier, *CurrentTarget.Get()))
	{
		if (bIsMovingToTarget)
		{
			// Flag first: StopMovement calls OnMoveCompleted synchronously.
			bIsMovingToTarget = false;
			StopMovement();
		}

		if (!GetWorldTimerManager().IsTimerActive(AttackTimerHandle))
		{
			GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &AEnemyAIController::HandleAttackTimer, Soldier->GetAttackInterval(), true);
		}
		return;
	}

	if (!bHasRequestedMove)
	{
		RequestMoveToCurrentTarget();
	}
}

void AEnemyAIController::ChooseTarget()
{
	// Stop striking the old target; the timer restarts once the soldier is in range of the new one.
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	if (bIsMovingToTarget)
	{
		// Flag first: StopMovement calls OnMoveCompleted synchronously.
		bIsMovingToTarget = false;
		StopMovement();
	}

	UEnemyTargetSubsystem* TargetSubsystem = GetTargetSubsystem();
	if (TargetSubsystem)
	{
		TargetSubsystem->ReleaseClaim(this);
	}
	CurrentTarget.Reset();
	bHasRequestedMove = false;

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !TargetSubsystem) return;

	// Looked up at every search, never cached: the zone may register after this controller exists.
	const UBaseZoneSubsystem* ZoneSubsystem = GetWorld()->GetSubsystem<UBaseZoneSubsystem>();
	const ABaseZone* BaseZone = ZoneSubsystem ? ZoneSubsystem->GetBaseZone() : nullptr;

	TArray<AActor*> Candidates;
	bool bAnyAlive = false;
	if (BaseZone)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Candidate = *It;
			if (!IsAliveDestructible(Candidate)) continue;

			// Only the actor's location counts: a destructible straddling the limit is judged by its origin.
			if (!BaseZone->IsPointInZone(Candidate->GetActorLocation())) continue;

			bAnyAlive = true;

			const double* RetryTime = ExcludedTargets.Find(Candidate);
			if (RetryTime && *RetryTime > Now) continue;

			Candidates.Add(Candidate);
		}
	}

	if (!bAnyAlive)
	{
		// No zone, or no living destructible in it: configuration error, no fallback. Logged once per soldier.
		if (!bHasLoggedNoTarget)
		{
			bHasLoggedNoTarget = true;
			UE_LOG(LogSpooky_Spores, Error, TEXT("Enemy %s is engaging but found no living destructible in the base zone (zone missing or empty)."),
				*GetNameSafe(ControlledPawn));
		}
		return;
	}

	if (Candidates.IsEmpty())
	{
		// Every living destructible was recently unreachable: wait (the exclusions expire), never walk in a straight line.
		if (!bHasLoggedNoReachableTarget)
		{
			bHasLoggedNoReachableTarget = true;
			UE_LOG(LogSpooky_Spores, Error, TEXT("Enemy %s has no reachable target: every living destructible failed its NavMesh move. Waiting before retrying."),
				*GetNameSafe(ControlledPawn));
		}
		return;
	}

	AActor* Chosen = TargetSubsystem->PickTarget(this, ControlledPawn->GetActorLocation(), Candidates);
	CurrentTarget = Chosen;
	TargetSubsystem->SetClaim(this, Chosen);
}

void AEnemyAIController::RequestMoveToCurrentTarget()
{
	// All state is written before the call: MoveToLocation may complete synchronously.
	bHasRequestedMove = true;
	bIsMovingToTarget = true;

	const EPathFollowingRequestResult::Type Result = RequestMoveToTarget(CurrentTarget.Get());

	// OnMoveCompleted already handled the end of this request (synchronous completion).
	if (!bIsMovingToTarget) return;

	if (Result != EPathFollowingRequestResult::RequestSuccessful)
	{
		bIsMovingToTarget = false;
		HandleTargetUnreachable();
	}
}

EPathFollowingRequestResult::Type AEnemyAIController::RequestMoveToTarget(AActor* Target)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return EPathFollowingRequestResult::Failed;

	FNavLocation ProjectedLocation;
	if (!NavSys->ProjectPointToNavigation(Target->GetActorLocation(), ProjectedLocation, TargetProjectionExtent, &GetNavAgentPropertiesRef()))
	{
		return EPathFollowingRequestResult::Failed;
	}

	// Not projected again (bProjectDestinationToNavigation false); the capsule radius comes from bStopOnOverlap.
	return MoveToLocation(ProjectedLocation.Location, ApproachAcceptanceRadius, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true,
		/*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false);
}

void AEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// Not awaited: target changed, soldier already in range, or a move we stopped ourselves.
	if (!bIsMovingToTarget) return;
	bIsMovingToTarget = false;

	// In range: the next TickEngage starts the attack. Anything else (failure, or success out of range) means unreachable.
	const AEnemySoldier* Soldier = Cast<AEnemySoldier>(GetPawn());
	const AActor* Target = CurrentTarget.Get();
	if (Soldier && Target && IsInAttackRange(*Soldier, *Target)) return;

	HandleTargetUnreachable();
}

void AEnemyAIController::HandleTargetUnreachable()
{
	if (AActor* Target = CurrentTarget.Get())
	{
		ExcludedTargets.Add(Target, GetWorld()->GetTimeSeconds() + UnreachableRetryDelay);
	}

	if (UEnemyTargetSubsystem* TargetSubsystem = GetTargetSubsystem())
	{
		TargetSubsystem->ReleaseClaim(this);
	}
	CurrentTarget.Reset();
	bHasRequestedMove = false;
}

bool AEnemyAIController::IsInAttackRange(const AEnemySoldier& Soldier, const AActor& Target) const
{
	FVector ClosestPoint = Target.GetActorLocation();
	if (const UPrimitiveComponent* TargetCollision = Cast<UPrimitiveComponent>(Target.GetRootComponent()))
	{
		FVector CollisionPoint;
		if (TargetCollision->GetClosestPointOnCollision(Soldier.GetActorLocation(), CollisionPoint) >= 0.0f)
			ClosestPoint = CollisionPoint;
	}

	// Measured in 2D (ignoring height), consistent with how the player's own movement is driven.
	const float Distance = (Soldier.GetActorLocation() - ClosestPoint).Size2D() - Soldier.GetCapsuleComponent()->GetScaledCapsuleRadius();
	return Distance <= Soldier.GetAttackRange();
}

UEnemyTargetSubsystem* AEnemyAIController::GetTargetSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEnemyTargetSubsystem>() : nullptr;
}

void AEnemyAIController::HandleAttackTimer()
{
	// The target may have died since the last tick; TickEngage then clears this timer and picks a new target.
	if (!IsAliveDestructible(CurrentTarget.Get())) return;

	Cast<AEnemySoldier>(GetPawn())->Attack(CurrentTarget.Get());
}
