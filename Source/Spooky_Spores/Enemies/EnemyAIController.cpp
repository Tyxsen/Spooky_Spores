#include "Enemies/EnemyAIController.h"
#include "EnemyPathSpline.h"
#include "EnemySoldier.h"
#include "EngineUtils.h"
#include "Components/SplineComponent.h"
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

	for (TActorIterator<AEnemyPathSpline> Spline(GetWorld()); Spline; ++Spline)
	{
		PathSplineActor = *Spline;
		break;
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

	const float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), CurrentTarget->GetActorLocation());

	if (Distance > Soldier->GetAttackRange())
	{
		MoveToLocation(CurrentTarget->GetActorLocation(), Soldier->GetAttackRange() * 0.9f);

		EPathFollowingRequestResult::Type MoveResult = MoveToLocation(CurrentTarget->GetActorLocation(), Soldier->GetAttackRange() * 0.9f);
		UE_LOG(LogTemp, Warning, TEXT("MoveToActor result: %d"), (int32)MoveResult);
		
		/** UE_LOG(LogTemp, Warning, TEXT("Pawn: %s | Target: %s | Distance: %f | AcceptRadius: %f"),
		*ControlledPawn->GetActorLocation().ToString(),
		*CurrentTarget->GetActorLocation().ToString(),
		Distance,
		Soldier->GetAttackRange() * 0.9f); */
	}
	else
	{
		StopMovement();

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

		UE_LOG(LogTemp, Warning, TEXT("Destructible candidate found: %s"), *Candidate->GetName());

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
