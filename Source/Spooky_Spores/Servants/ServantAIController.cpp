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
#include "Templates/UnrealTemplate.h"
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
	// Also before StopMovement: the reservation must not outlive the field job, and the Aborted callback must not resolve it
	ReleaseFieldTarget();
	StopMovement();
	GetWorldTimerManager().ClearTimer(DecisionTimer);
}

AFieldPlot* AServantAIController::GetHarvestTarget() const
{
	return bIsHarvesting ? FieldTarget.Get() : nullptr;
}

bool AServantAIController::IsHarvesting() const
{
	return bIsHarvesting;
}

AFieldPlot* AServantAIController::GetPlantTarget() const
{
	return bIsPlanting ? FieldTarget.Get() : nullptr;
}

bool AServantAIController::IsPlanting() const
{
	return bIsPlanting;
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

	// A field job move ends here, success or not (cancellations clear the flags before StopMovement)
	if (bIsHarvesting || bIsPlanting)
	{
		ResolveFieldWork(Result.IsSuccess());
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

	if (UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>())
	{
		// Native delegate (no AddUnique): remove first so a second possession never registers twice
		FieldSubsystem->OnFieldChanged.RemoveAll(this);
		FieldSubsystem->OnFieldChanged.AddUObject(this, &AServantAIController::OnFieldChanged);
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

	if (UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>())
	{
		FieldSubsystem->OnFieldChanged.RemoveAll(this);
	}

	if (UResourceCounterComponent* Storage = WatchedStorage.Get())
	{
		Storage->OnResourceChanged.RemoveDynamic(this, &AServantAIController::OnStorageResourceChanged);
	}
	WatchedStorage.Reset();

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

	// Wandering stays the background state: field work only replaces a Wander decision (never a Return)
	if (LastAction == EServantWanderAction::Wander && !bSkipHarvest && TryStartFieldWork(*Zone))
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

void AServantAIController::WatchStorage(UResourceCounterComponent* Storage)
{
	UResourceCounterComponent* Previous = WatchedStorage.Get();
	if (Previous == Storage)
	{
		return;
	}

	if (Previous)
	{
		Previous->OnResourceChanged.RemoveDynamic(this, &AServantAIController::OnStorageResourceChanged);
	}
	Storage->OnResourceChanged.AddUniqueDynamic(this, &AServantAIController::OnStorageResourceChanged);
	WatchedStorage = Storage;
}

bool AServantAIController::TryStartFieldWork(const ABaseZone& Zone)
{
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();
	const APawn* ControlledPawn = GetPawn();
	UResourceCounterComponent* Storage = FindHarvestStorage();
	// Storage checked before reserving: without it neither the harvest nor the planting would be possible
	if (!FieldSubsystem || !ControlledPawn || GetAssignedTask() != EServantTask::FieldHarvest || !Storage)
	{
		return false;
	}

	// Seeds arriving later wake an idle servant (PROVISIONAL, see FindHarvestStorage)
	WatchStorage(Storage);

	// Harvest first (the seed may come from the yield), then plant if the seeds not yet promised to others pay for it
	double NearestDistanceSq = 0.0;
	AFieldPlot* TargetField = FindNearestFieldInZone(FieldSubsystem->GetUnreservedHarvestableFields(), Zone, NearestDistanceSq);
	const bool bHarvest = TargetField != nullptr;
	if (!bHarvest)
	{
		const int32 AvailableSeeds = GetAvailableSeeds(*Storage);
		TArray<AFieldPlot*> PlantableFields = FieldSubsystem->GetUnreservedPlantableFields();
		PlantableFields.RemoveAll([AvailableSeeds](const AFieldPlot* Field)
		{
			return Field->GetSeedCost() > AvailableSeeds;
		});
		TargetField = FindNearestFieldInZone(PlantableFields, Zone, NearestDistanceSq);
	}

	if (!TargetField || !FieldSubsystem->TryReserve(TargetField, this))
	{
		return false;
	}

	FieldTarget = TargetField;
	bIsHarvesting = bHarvest;
	bIsPlanting = !bHarvest;
	LastAction = bHarvest ? EServantWanderAction::Harvest : EServantWanderAction::Plant;

	ApproachField(*TargetField, NearestDistanceSq);
	return true;
}

int32 AServantAIController::GetAvailableSeeds(const UResourceCounterComponent& Storage) const
{
	const UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();
	return Storage.GetResourceCount(EResourceType::Seed) - (FieldSubsystem ? FieldSubsystem->GetPromisedSeeds(this) : 0);
}

AFieldPlot* AServantAIController::FindNearestFieldInZone(const TArray<AFieldPlot*>& Fields, const ABaseZone& Zone,
														double& OutDistanceSq) const
{
	const FVector PawnLocation = GetPawn()->GetActorLocation();
	AFieldPlot* NearestField = nullptr;
	OutDistanceSq = TNumericLimits<double>::Max();
	for (AFieldPlot* Field : Fields)
	{
		if (!Zone.IsPointInZone(Field->GetActorLocation()))
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared2D(PawnLocation, Field->GetActorLocation());
		if (DistanceSq < OutDistanceSq)
		{
			OutDistanceSq = DistanceSq;
			NearestField = Field;
		}
	}
	return NearestField;
}

void AServantAIController::ApproachField(AFieldPlot& Field, double DistanceSq)
{
	// 2D on purpose: the path following measures arrival in 2D (CLAUDE.md section 8)
	const ACharacter* ControlledCharacter = GetCharacter();
	const double CapsuleRadius = ControlledCharacter ? ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0;
	const double ReachDistance = HarvestReach + CapsuleRadius;
	if (DistanceSq <= ReachDistance * ReachDistance)
	{
		ResolveFieldWork(/*bReachedTarget*/ true);
		return;
	}

	FVector NavPoint;
	if (!TryProjectOnNavMesh(Field.GetActorLocation(), FVector(HarvestProjectionExtent), NavPoint))
	{
		ResolveFieldWork(/*bReachedTarget*/ false);
		return;
	}

	// Follow-up scheduled by OnMoveCompleted only, even when MoveToLocation completes synchronously
	// (Failed, AlreadyAtGoal). Not projected again (bProjectDestinationToNavigation false), the capsule radius
	// comes from bStopOnOverlap.
	MoveToLocation(NavPoint, HarvestReach, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true,
				/*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false, /*FilterClass*/ nullptr,
				/*bAllowPartialPath*/ false);
}

void AServantAIController::ResolveFieldWork(bool bReachedTarget)
{
	if (bIsPlanting)
	{
		ResolvePlant(bReachedTarget);
	}
	else
	{
		ResolveHarvest(bReachedTarget);
	}
}

void AServantAIController::ResolveHarvest(bool bReachedTarget)
{
	// The changes we cause on the field must not cancel this very job
	TGuardValue<bool> ResolvingGuard(bIsResolving, true);

	AFieldPlot* Field = FieldTarget.Get();
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();

	// The field may have been harvested by the player or destroyed during the walk. TryReserve by us succeeds
	// only if nobody else holds it (a destroyed field is not registered anymore).
	bool bHarvested = false;
	if (bReachedTarget && Field && FieldSubsystem && FieldSubsystem->TryReserve(Field, this))
	{
		bHarvested = HarvestAndDeposit(*Field);
		if (bHarvested)
		{
			// Replant on the spot, reservation kept. Without enough seeds the field stays Empty: the harvest is still a success.
			TryPlantOnField(*Field);
		}
	}

	ReleaseFieldTarget();

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

void AServantAIController::ResolvePlant(bool bReachedTarget)
{
	TGuardValue<bool> ResolvingGuard(bIsResolving, true);

	AFieldPlot* Field = FieldTarget.Get();
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();

	// The field may have been planted by the player or destroyed during the walk, or the seeds spent meanwhile
	bool bPlanted = false;
	if (bReachedTarget && Field && FieldSubsystem && FieldSubsystem->TryReserve(Field, this))
	{
		bPlanted = TryPlantOnField(*Field);
	}

	ReleaseFieldTarget();

	if (bPlanted)
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
	// FindHarvestStorage; see the list in the header.
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

bool AServantAIController::TryPlantOnField(AFieldPlot& Field)
{
	// PROVISIONAL (until the shared base inventory, TODO item 7): seeds come from FindHarvestStorage; see the list in the header.
	// The seeds promised to other servants stay theirs.
	UResourceCounterComponent* Storage = FindHarvestStorage();
	if (!Storage || GetAvailableSeeds(*Storage) < Field.GetSeedCost())
	{
		return false;
	}

	return Field.TryPlant(Storage);
}

void AServantAIController::ReleaseFieldTarget()
{
	AFieldPlot* Field = FieldTarget.Get();
	UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>();
	if (Field && FieldSubsystem)
	{
		FieldSubsystem->Release(Field, this);
	}

	FieldTarget.Reset();
	bIsHarvesting = false;
	bIsPlanting = false;
}

void AServantAIController::CancelFieldWork()
{
	// Released and cleared before StopMovement: the Aborted callback then reschedules a plain decision.
	// LastAction = Wander (resting state): WakeIfIdle accepts the servant right away instead of after RetryDelay.
	ReleaseFieldTarget();
	LastAction = EServantWanderAction::Wander;
	StopMovement();
}

void AServantAIController::WakeIfIdle()
{
	// Idle = wandering, paused between two moves (decision timer active, so not walking), not on a field job
	FTimerManager& TimerManager = GetWorldTimerManager();
	if (!bIsWandering || bIsHarvesting || bIsPlanting || LastAction != EServantWanderAction::Wander
		|| !TimerManager.IsTimerActive(DecisionTimer) || GetAssignedTask() != EServantTask::FieldHarvest)
	{
		return;
	}

	// SetTimer with a rate of 0 would clear the timer
	const float WakeDelay = FMath::Max(FMath::FRandRange(0.0f, MaxWakeDelay), UE_KINDA_SMALL_NUMBER);
	if (WakeDelay < TimerManager.GetTimerRemaining(DecisionTimer))
	{
		ScheduleDecision(WakeDelay);
	}
}

void AServantAIController::OnFieldChanged(AFieldPlot* Field, bool bRemoved)
{
	// Our own harvest or planting
	if (bIsResolving)
	{
		return;
	}

	if ((bIsHarvesting || bIsPlanting) && Field == FieldTarget.Get())
	{
		const bool bStillAsExpected = !bRemoved && (bIsHarvesting ? Field->IsReadyToHarvest() : Field->IsReadyToPlant());
		if (!bStillAsExpected)
		{
			CancelFieldWork();
		}
		return;
	}

	if (!bRemoved && (Field->IsReadyToHarvest() || Field->IsReadyToPlant()))
	{
		WakeIfIdle();
	}
}

void AServantAIController::OnStorageResourceChanged(EResourceType Type, int32 NewCount)
{
	if (Type == EResourceType::Seed && NewCount > 0)
	{
		WakeIfIdle();
	}
}

void AServantAIController::OnServantAssignmentsChanged()
{
	// Fired on every (un)registration and assignment: only a field job in progress whose task was removed is cancelled.
	// An unregistered servant (EndPlay) counts as Unassigned.
	if ((!bIsHarvesting && !bIsPlanting) || GetAssignedTask() == EServantTask::FieldHarvest)
	{
		return;
	}

	CancelFieldWork();
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
