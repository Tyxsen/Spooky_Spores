#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemyPathSpline.h"
#include "Enemies/EnemySoldier.h"
#include "EngineUtils.h"
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
		// Stop striking the old target; the timer restarts below once the soldier is in range of the new one.
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		CurrentTarget = FindNearestDestructible();
	}

	if (!CurrentTarget.IsValid())
	{
		// No zone, or no living destructible in it: configuration error, no fallback. Logged once per soldier.
		if (!bHasLoggedNoTarget)
		{
			bHasLoggedNoTarget = true;
			UE_LOG(LogSpooky_Spores, Error, TEXT("Enemy %s is engaging but found no living destructible in the base zone (zone missing or empty)."),
				*GetNameSafe(GetPawn()));
		}
		return;
	}

	APawn* ControlledPawn = GetPawn();
	AEnemySoldier* Soldier = Cast<AEnemySoldier>(ControlledPawn);
	if (!ControlledPawn || !Soldier) return;

	const FVector TargetLocation = CurrentTarget->GetActorLocation();

	FVector ClosestPoint = TargetLocation;
	if (UPrimitiveComponent* TargetCollision = Cast<UPrimitiveComponent>(CurrentTarget->GetRootComponent()))
	{
		FVector CollisionPoint;
		if (TargetCollision->GetClosestPointOnCollision(ControlledPawn->GetActorLocation(), CollisionPoint) >= 0.0f)
			ClosestPoint = CollisionPoint;
	}

	// Measured in 2D (ignoring height), consistent with how the player's own movement is driven.
	float Distance = (ControlledPawn->GetActorLocation() - ClosestPoint).Size2D();

	const float SoldierRadius = Soldier->GetCapsuleComponent()->GetScaledCapsuleRadius();
	Distance -= SoldierRadius;

	if (Distance > Soldier->GetAttackRange())
	{
		const FVector Direction = (TargetLocation - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(Direction);
	}
	else
	{
		if (!GetWorldTimerManager().IsTimerActive(AttackTimerHandle))
		{
			GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &AEnemyAIController::HandleAttackTimer, Soldier->GetAttackInterval(), true);
		}
	}
}

AActor* AEnemyAIController::FindNearestDestructible() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return nullptr;

	// Looked up at every search, never cached: the zone may register after this controller exists.
	const UBaseZoneSubsystem* ZoneSubsystem = GetWorld()->GetSubsystem<UBaseZoneSubsystem>();
	const ABaseZone* BaseZone = ZoneSubsystem ? ZoneSubsystem->GetBaseZone() : nullptr;
	if (!BaseZone) return nullptr;

	const FVector Origin = ControlledPawn->GetActorLocation();
	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsAliveDestructible(Candidate)) continue;

		// Only the actor's location counts: a destructible straddling the limit is judged by its origin.
		if (!BaseZone->IsPointInZone(Candidate->GetActorLocation())) continue;

		const float DistanceSquared = FVector::DistSquared2D(Origin, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}
	
	return BestTarget;
}

void AEnemyAIController::HandleAttackTimer()
{
	// The target may have died since the last tick; TickEngage then clears this timer and picks a new target.
	if (!IsAliveDestructible(CurrentTarget.Get())) return;

	Cast<AEnemySoldier>(GetPawn())->Attack(CurrentTarget.Get());
}
