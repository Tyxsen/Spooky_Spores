#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemyPathSpline.h"
#include "Enemies/EnemySoldier.h"
#include "EngineUtils.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/Destructible.h"
#include "Health/HealthComponent.h"

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
	if (!CurrentTarget.IsValid())
		CurrentTarget = FindNearestDestructible();

	if (!CurrentTarget.IsValid()) return;

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

	const FVector Origin = ControlledPawn->GetActorLocation();
	AActor* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(TargetSearchRadius);
	
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate->Implements<UDestructible>()) continue;

		UHealthComponent* Health = IDestructible::Execute_GetHealthComponent(Candidate);
		if (!Health || Health->IsDead()) continue;

		const float DistanceSquared = FVector::DistSquared(Origin, Candidate->GetActorLocation());
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
	if (!CurrentTarget.IsValid()) return;

	Cast<AEnemySoldier>(GetPawn())->Attack(CurrentTarget.Get());
}
