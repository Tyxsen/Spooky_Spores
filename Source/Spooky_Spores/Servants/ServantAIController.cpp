// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/ServantAIController.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Base/BaseZone.h"
#include "Base/BaseZoneSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Resources/FieldPlot.h"
#include "Resources/FieldPlotSubsystem.h"
#include "Resources/ResourceCounterComponent.h"
#include "Servants/Servant.h"
#include "Servants/ServantSubsystem.h"
#include "Spooky_Spores.h"
#include "TimerManager.h"

void AServantAIController::StartWandering()
{
	// Already wandering: restarting would interrupt the current move (Aborted) and count fake failed returns
	if (bIsWandering)
	{
		return;
	}

	// The speed belongs to the behaviour, not to the servant's defaults. No pawn: the first decision stops wandering.
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->GetCharacterMovement()->MaxWalkSpeed = WanderWalkSpeed;
	}

	bIsWandering = true;
	FailedReturnAttempts = 0;
	// A Return from a previous cycle must not count as a failed return in the new one
	LastAction = EServantWanderAction::None;
	ScheduleDecision(InitialDecisionDelay);
}

void AServantAIController::StopWandering()
{
	// Must be cleared before StopMovement: the synchronous Aborted callback would otherwise reschedule a decision
	bIsWandering = false;
	// Also before StopMovement: the reservation must not outlive the harvest, and the Aborted callback must not resolve it
	ReleaseHarvest();
	StopMovement();
	GetWorldTimerManager().ClearTimer(DecisionTimer);
}

AFieldPlot* AServantAIController::GetHarvestTarget() const
{
	return HarvestTarget.Get();
}

bool AServantAIController::IsHarvesting() const
{
	return bIsHarvesting;
}

bool AServantAIController::IsWandering() const
{
	return bIsWandering;
}

int32 AServantAIController::GetFailedReturnAttempts() const
{
	return FailedReturnAttempts;
}

EServantWanderAction AServantAIController::GetLastAction() const
{
	return LastAction;
}

EServantWanderAction AServantAIController::DecideNextAction(bool bHasValidZone, bool bIsInZone, int32 FailedReturns, int32 MaxReturns)
{
	if (!bHasValidZone)
	{
		return EServantWanderAction::Wait;
	}
	if (bIsInZone)
	{
		return EServantWanderAction::Wander;
	}
	return FailedReturns >= MaxReturns ? EServantWanderAction::Teleport : EServantWanderAction::Return;
}

void AServantAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (!bIsWandering)
	{
		return;
	}

	// A harvest move ends here, success or not (cancellations clear bIsHarvesting before StopMovement)
	if (bIsHarvesting)
	{
		ResolveHarvest(Result.IsSuccess());
		return;
	}

	if (Result.IsSuccess())
	{
		ScheduleDecision(FMath::FRandRange(MinWanderPause, MaxWanderPause));
	}
	else
	{
		ScheduleDecision(RetryDelay);
	}
}

void AServantAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (UServantSubsystem* ServantSubsystem = GetWorld()->GetSubsystem<UServantSubsystem>())
	{
		ServantSubsystem->OnAssignmentsChanged.AddUniqueDynamic(this, &AServantAIController::OnServantAssignmentsChanged);
	}

	// Possession happens in the servant's PostInitializeComponents, before its BeginPlay and possibly
	// before the zone's BeginPlay (registration): the first decision is delayed, the zone is read then.
	StartWandering();
}

void AServantAIController::OnUnPossess()
{
	// Before Super: StopMovement still needs the pawn
	StopWandering();

	if (UServantSubsystem* ServantSubsystem = GetWorld()->GetSubsystem<UServantSubsystem>())
	{
		ServantSubsystem->OnAssignmentsChanged.RemoveDynamic(this, &AServantAIController::OnServantAssignmentsChanged);
	}

	Super::OnUnPossess();
}

void AServantAIController::MakeDecision()
{
	if (!bIsWandering)
	{
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		// Nothing to move: stop the cycle so that IsWandering() tells the truth
		bIsWandering = false;
		return;
	}

	// Read at every decision, never cached: the zone may register after us, move or disappear
	const UBaseZoneSubsystem* ZoneSubsystem = GetWorld()->GetSubsystem<UBaseZoneSubsystem>();
	ABaseZone* Zone = ZoneSubsystem ? ZoneSubsystem->GetBaseZone() : nullptr;

	// Consumed by this decision whatever it ends up being
	const bool bSkipHarvest = bSkipNextHarvest;
	bSkipNextHarvest = false;

	const bool bHasValidZone = Zone && Zone->IsZoneValid();
	const bool bIsInZone = bHasValidZone && Zone->IsPointInZone(ControlledPawn->GetActorLocation());

	if (bHasValidZone)
	{
		if (bIsInZone)
		{
			FailedReturnAttempts = 0;
		}
		else if (LastAction == EServantWanderAction::Return)
		{
			// Whatever the cause (no nav point, Failed, Aborted, Blocked, stopped on the border), the return failed
			++FailedReturnAttempts;
		}
	}

	LastAction = DecideNextAction(bHasValidZone, bIsInZone, FailedReturnAttempts, MaxFailedReturnAttempts);

	// Wandering stays the background state: the harvest only replaces a Wander decision (never a Return)
	if (LastAction == EServantWanderAction::Wander && !bSkipHarvest && TryStartHarvest(*Zone))
	{
		return;
	}

	switch (LastAction)
	{
	case EServantWanderAction::Wander:
	case EServantWanderAction::Return:
		MoveToZonePoint(*Zone);
		break;
	case EServantWanderAction::Teleport:
		TeleportIntoZone(*Zone);
		break;
	default:
		// Wait: no zone yet, or an invalid one
		ScheduleDecision(RetryDelay);
		break;
	}
}

void AServantAIController::ScheduleDecision(float Delay)
{
	// Non-looping: each decision schedules the next one (directly or through OnMoveCompleted)
	GetWorldTimerManager().SetTimer(DecisionTimer, this, &AServantAIController::MakeDecision, Delay, false);
}

bool AServantAIController::TryFindNavPointInZone(const ABaseZone& Zone, FVector& OutPoint) const
{
	const FVector QueryExtent(ProjectionHorizontalExtent, ProjectionHorizontalExtent,
							Zone.GetZoneExtent().Z + ProjectionVerticalMargin);

	for (int32 Attempt = 0; Attempt < MaxProjectionAttempts; ++Attempt)
	{
		FVector RandomPoint;
		if (!Zone.GetRandomPointInZone(RandomPoint))
		{
			return false;
		}

		FVector ProjectedPoint;
		if (TryProjectOnNavMesh(RandomPoint, QueryExtent, ProjectedPoint) && Zone.IsPointInZone(ProjectedPoint))
		{
			OutPoint = ProjectedPoint;
			return true;
		}
	}
	return false;
}

bool AServantAIController::TryProjectOnNavMesh(const FVector& Point, const FVector& Extent, FVector& OutPoint) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	if (!NavSys->ProjectPointToNavigation(Point, ProjectedLocation, Extent, &GetNavAgentPropertiesRef()))
	{
		return false;
	}

	OutPoint = ProjectedLocation.Location;
	return true;
}

EServantTask AServantAIController::GetAssignedTask() const
{
	const UServantSubsystem* ServantSubsystem = GetWorld()->GetSubsystem<UServantSubsystem>();
	return ServantSubsystem ? ServantSubsystem->GetServantTask(Cast<AServant>(GetPawn())) : EServantTask::Unassigned;
}

UResourceCounterComponent* AServantAIController::FindHarvestStorage() const
{
	// PROVISIONAL: player 0 until the shared base inventory exists (TODO item 7)
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return PlayerPawn ? PlayerPawn->FindComponentByClass<UResourceCounterComponent>() : nullptr;
}

bool AServantAIController::TryStartHarvest(const ABaseZone& Zone)
{
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();
	const APawn* ControlledPawn = GetPawn();
	// Storage checked before reserving: without it the harvest would be refused anyway
	if (!FieldSubsystem || !ControlledPawn || GetAssignedTask() != EServantTask::FieldHarvest || !FindHarvestStorage())
	{
		return false;
	}

	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	AFieldPlot* NearestField = nullptr;
	double NearestDistanceSq = TNumericLimits<double>::Max();
	for (AFieldPlot* Field : FieldSubsystem->GetUnreservedHarvestableFields())
	{
		if (!Zone.IsPointInZone(Field->GetActorLocation()))
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared2D(PawnLocation, Field->GetActorLocation());
		if (DistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			NearestField = Field;
		}
	}

	if (!NearestField || !FieldSubsystem->TryReserve(NearestField, this))
	{
		return false;
	}

	HarvestTarget = NearestField;
	bIsHarvesting = true;
	LastAction = EServantWanderAction::Harvest;

	// 2D on purpose: the path following measures arrival in 2D (CLAUDE.md section 8)
	const ACharacter* ControlledCharacter = GetCharacter();
	const double CapsuleRadius = ControlledCharacter ? ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0;
	const double ReachDistance = HarvestReach + CapsuleRadius;
	if (NearestDistanceSq <= ReachDistance * ReachDistance)
	{
		ResolveHarvest(/*bReachedTarget*/ true);
		return true;
	}

	FVector NavPoint;
	if (!TryProjectOnNavMesh(NearestField->GetActorLocation(), FVector(HarvestProjectionExtent), NavPoint))
	{
		ResolveHarvest(/*bReachedTarget*/ false);
		return true;
	}

	// Follow-up scheduled by OnMoveCompleted only, even when MoveToLocation completes synchronously
	// (Failed, AlreadyAtGoal). Not projected again (bProjectDestinationToNavigation false), the capsule radius
	// comes from bStopOnOverlap.
	MoveToLocation(NavPoint, HarvestReach, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true,
				/*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false, /*FilterClass*/ nullptr,
				/*bAllowPartialPath*/ false);
	return true;
}

void AServantAIController::ResolveHarvest(bool bReachedTarget)
{
	AFieldPlot* Field = HarvestTarget.Get();
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();

	// The field may have been harvested by the player or destroyed during the walk. TryReserve by us succeeds
	// only if nobody else holds it (a destroyed field is not registered anymore).
	bool bHarvested = false;
	if (bReachedTarget && Field && FieldSubsystem && FieldSubsystem->TryReserve(Field, this))
	{
		bHarvested = HarvestAndDeposit(*Field);
	}

	ReleaseHarvest();

	if (bHarvested)
	{
		ScheduleDecision(HarvestNextDelay);
	}
	else
	{
		bSkipNextHarvest = true;
		ScheduleDecision(RetryDelay);
	}
}

bool AServantAIController::HarvestAndDeposit(AFieldPlot& Field)
{
	// PROVISIONAL (until the shared base inventory, TODO item 7): credit happens here, storage is resolved in
	// FindHarvestStorage; these two functions are the only ones to change.
	UResourceCounterComponent* Storage = FindHarvestStorage();
	if (!Storage)
	{
		return false;
	}

	TMap<EResourceType, int32> Yield;
	if (!Field.TryHarvestYield(Yield))
	{
		return false;
	}

	for (const TPair<EResourceType, int32>& Pair : Yield)
	{
		Storage->AddResource(Pair.Key, Pair.Value);
	}
	return true;
}

void AServantAIController::ReleaseHarvest()
{
	AFieldPlot* Field = HarvestTarget.Get();
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();
	if (Field && FieldSubsystem)
	{
		FieldSubsystem->Release(Field, this);
	}

	HarvestTarget.Reset();
	bIsHarvesting = false;
}

void AServantAIController::OnServantAssignmentsChanged()
{
	// Fired on every (un)registration and assignment: only a harvest in progress whose task was removed is cancelled.
	// An unregistered servant (EndPlay) counts as Unassigned.
	if (!bIsHarvesting || GetAssignedTask() == EServantTask::FieldHarvest)
	{
		return;
	}

	// Released and cleared before StopMovement: the Aborted callback then reschedules a plain decision
	ReleaseHarvest();
	LastAction = EServantWanderAction::None;
	StopMovement();
}

void AServantAIController::MoveToZonePoint(const ABaseZone& Zone)
{
	FVector TargetPoint;
	if (!TryFindNavPointInZone(Zone, TargetPoint))
	{
		ScheduleDecision(RetryDelay);
		return;
	}

	// The follow-up is scheduled by OnMoveCompleted only, including when MoveToLocation fails
	// or is already at goal (both call OnMoveCompleted synchronously).
	MoveToLocation(TargetPoint, AcceptanceRadius, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true,
				/*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false, /*FilterClass*/ nullptr,
				/*bAllowPartialPath*/ false);
}

void AServantAIController::TeleportIntoZone(const ABaseZone& Zone)
{
	ACharacter* ControlledCharacter = GetCharacter();

	FVector TargetPoint;
	if (!TryFindNavPointInZone(Zone, TargetPoint))
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("Servant %s could not be teleported into the base zone: no navigable point found."),
			*GetNameSafe(ControlledCharacter));
		ScheduleDecision(RetryDelay);
		return;
	}

	if (!ControlledCharacter || !ControlledCharacter->TeleportTo(
			TargetPoint + FVector(0.0, 0.0, ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
			ControlledCharacter->GetActorRotation()))
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("Servant %s could not be teleported into the base zone: TeleportTo failed."),
			*GetNameSafe(ControlledCharacter));
		ScheduleDecision(RetryDelay);
		return;
	}

	UE_LOG(LogSpooky_Spores, Warning, TEXT("Servant %s teleported into the base zone after %d failed returns."),
		*GetNameSafe(ControlledCharacter), FailedReturnAttempts);
	FailedReturnAttempts = 0;
	ScheduleDecision(FMath::FRandRange(MinWanderPause, MaxWanderPause));
}
