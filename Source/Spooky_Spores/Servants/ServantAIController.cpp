// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/ServantAIController.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Base/BaseZone.h"
#include "Base/BaseZoneSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Spooky_Spores.h"
#include "TimerManager.h"

void AServantAIController::StartWandering()
{
	// Already wandering: restarting would interrupt the current move (Aborted) and count fake failed returns
	if (bIsWandering)
	{
		return;
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
	StopMovement();
	GetWorldTimerManager().ClearTimer(DecisionTimer);
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

	// Possession happens in the servant's PostInitializeComponents, before its BeginPlay and possibly
	// before the zone's BeginPlay (registration): the first decision is delayed, the zone is read then.
	StartWandering();
}

void AServantAIController::OnUnPossess()
{
	// Before Super: StopMovement still needs the pawn
	StopWandering();

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
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	const FNavAgentProperties& AgentProps = GetNavAgentPropertiesRef();
	const FVector QueryExtent(ProjectionHorizontalExtent, ProjectionHorizontalExtent,
							Zone.GetZoneExtent().Z + ProjectionVerticalMargin);

	for (int32 Attempt = 0; Attempt < MaxProjectionAttempts; ++Attempt)
	{
		FVector RandomPoint;
		if (!Zone.GetRandomPointInZone(RandomPoint))
		{
			return false;
		}

		FNavLocation ProjectedLocation;
		if (NavSys->ProjectPointToNavigation(RandomPoint, ProjectedLocation, QueryExtent, &AgentProps)
			&& Zone.IsPointInZone(ProjectedLocation.Location))
		{
			OutPoint = ProjectedLocation.Location;
			return true;
		}
	}
	return false;
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
