// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Resources/ResourceTypes.h"
#include "Servants/ServantTask.h"
#include "ServantAIController.generated.h"

class ABaseZone;
class AEnemySoldier;
class AFieldPlot;
class AServant;
class UResourceCounterComponent;
struct FPathFollowingResult;

// What the servant decided to do at its last decision.
UENUM(BlueprintType)
enum class EServantWanderAction : uint8
{
	None,
	Wait,
	Wander,
	Return,
	Teleport,
	Harvest,
	Plant,
	Attack
};

/**
 * Makes an idle servant wander inside the base zone. Placed outside the zone, it walks back,
 * and is teleported in after MaxFailedReturnAttempts failed returns.
 * An assigned FieldHarvest servant leaves the wander for a Grown field of the zone (harvest), then wanders again
 * when none is left. It also replants (on the spot after a harvest, or an Empty field) while the player's seeds allow it.
 * An assigned AttackDefense servant leaves the wander for the nearest (2D) living enemy whose origin is in the zone, chases it and
 * strikes it every AttackInterval until it dies or leaves the zone, then wanders again (no reservation: several servants may share an enemy).
 * Field and seed changes wake an idle servant early (OnFieldChanged, OnResourceChanged); the decision timer stays the safety net.
 * A signal received while the servant walks does not turn it around: it is remembered (bWakePending) and shortens the pause that follows the move.
 * No Tick, no Behavior Tree: one decision timer, rescheduled after each move by OnMoveCompleted.
 */
UCLASS()
class SPOOKY_SPORES_API AServantAIController : public AAIController
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Resets the failed-return counter and schedules the first decision. No effect if already wandering.
	UFUNCTION(BlueprintCallable, Category = "Servant|Wander")
	void StartWandering();

	// Stops the current move and cancels the next decision. Also releases the harvest reservation.
	// Called by OnUnPossess; a task assignment change does not call it (see OnServantAssignmentsChanged).
	UFUNCTION(BlueprintCallable, Category = "Servant|Wander")
	void StopWandering();

	UFUNCTION(BlueprintPure, Category = "Servant|Wander")
	bool IsWandering() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Wander")
	int32 GetFailedReturnAttempts() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Wander")
	EServantWanderAction GetLastAction() const;

	// The field this servant is going to harvest, nullptr when not harvesting (or destroyed meanwhile).
	UFUNCTION(BlueprintPure, Category = "Servant|Harvest")
	AFieldPlot* GetHarvestTarget() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Harvest")
	bool IsHarvesting() const;

	// The field this servant is going to plant, nullptr when not planting (or destroyed meanwhile).
	UFUNCTION(BlueprintPure, Category = "Servant|Plant")
	AFieldPlot* GetPlantTarget() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Plant")
	bool IsPlanting() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Combat")
	bool IsFighting() const;

	// The enemy this servant is fighting, nullptr outside a fight (or destroyed meanwhile).
	UFUNCTION(BlueprintPure, Category = "Servant|Combat")
	AActor* GetFightTarget() const;

	// Pure decision rule, no UObject needed: callable from Automation Specs.
	static EServantWanderAction DecideNextAction(bool bHasValidZone, bool bIsInZone, int32 FailedReturns, int32 MaxReturns);

	// Only place that schedules what follows a move. Also called synchronously by MoveToLocation
	// (Failed, AlreadyAtGoal) and by StopMovement (Aborted).
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Starts the chase of Enemy (MoveToActor: the target moves). Virtual so tests can replace it (test worlds have no NavMesh).
	virtual EPathFollowingRequestResult::Type RequestMoveToEnemy(AActor* Enemy);

private:
	void MakeDecision();
	void ScheduleDecision(float Delay);

	// Draws random points in the zone and projects them on the NavMesh, at most MaxProjectionAttempts times.
	// A projected point outside the zone counts as a failed attempt. Never use OutPoint when false.
	bool TryFindNavPointInZone(const ABaseZone& Zone, FVector& OutPoint) const;

	// Projects Point on the NavMesh with the given extent. Never use OutPoint when false.
	bool TryProjectOnNavMesh(const FVector& Point, const FVector& Extent, FVector& OutPoint) const;

	// Task of the controlled servant, Unassigned if none or not registered.
	EServantTask GetAssignedTask() const;

	// PROVISIONAL (until the shared base inventory, TODO item 7): storage is resolved in FindHarvestStorage, credited
	// in HarvestAndDeposit, spent in TryPlantOnField and watched in WatchStorage; these functions are the only ones to change.
	// Here: player 0's counter. nullptr when there is no player or no counter.
	UResourceCounterComponent* FindHarvestStorage() const;

	// Subscribes to the seed changes of Storage (once; leaves the previous storage if it changed).
	void WatchStorage(UResourceCounterComponent* Storage);

	// Starts a field job if the servant is assigned and an eligible field exists: the nearest Grown field first (harvest),
	// else the nearest Empty one the available seeds can pay (plant). True if the decision is a Harvest or a Plant
	// (the follow-up is then scheduled by the job itself).
	bool TryStartFieldWork(const ABaseZone& Zone);

	// Seeds of Storage not already promised to another servant.
	int32 GetAvailableSeeds(const UResourceCounterComponent& Storage) const;

	// Nearest (2D) field of Fields inside the zone, nullptr if none. OutDistanceSq is valid only when non null.
	AFieldPlot* FindNearestFieldInZone(const TArray<AFieldPlot*>& Fields, const ABaseZone& Zone, double& OutDistanceSq) const;

	// Walks to the field, or resolves at once if already in reach (or if no nav point: failure).
	void ApproachField(AFieldPlot& Field, double DistanceSq);

	// Runs when the servant is at the field (immediately or from OnMoveCompleted): harvests or plants, or fails.
	void ResolveFieldWork(bool bReachedTarget);
	void ResolveHarvest(bool bReachedTarget);
	void ResolvePlant(bool bReachedTarget);

	// PROVISIONAL (until the shared base inventory, TODO item 7): credits the harvest (storage comes from
	// FindHarvestStorage); see the list above.
	// False if the field was not harvested (no storage: the field stays Grown).
	bool HarvestAndDeposit(AFieldPlot& Field);

	// PROVISIONAL (until the shared base inventory, TODO item 7): plants Field if the available seeds pay for it.
	bool TryPlantOnField(AFieldPlot& Field);

	// Releases the reservation and forgets the target. Safe when no field job is running.
	void ReleaseFieldTarget();

	// Aborts the field job in progress (reservation released, state cleared BEFORE StopMovement: its callback is synchronous).
	// Leaves LastAction == Wander so that WakeIfIdle can wake the servant during the RetryDelay scheduled by the callback.
	void CancelFieldWork();

	// Brings the next decision forward (random delay up to MaxWakeDelay) if the servant is idle: wandering, paused.
	// If it is walking (wander or return move) the move is not interrupted: bWakePending is raised and
	// OnMoveCompleted uses the wake delay instead of the normal pause (or of the RetryDelay if the move fails). Ignored if not assigned to FieldHarvest.
	void WakeIfIdle();

	// Cancels our job if its field changed (no longer Grown / Empty as expected, or removed), else wakes the servant.
	void OnFieldChanged(AFieldPlot* Field, bool bRemoved);

	UFUNCTION()
	void OnStorageResourceChanged(EResourceType Type, int32 NewCount);

	UFUNCTION()
	void OnServantAssignmentsChanged();

	// Starts a fight if the servant is assigned to AttackDefense and a living enemy of the zone exists: the nearest one (2D).
	// True if the decision is an Attack (the follow-up is then scheduled by EndCombat).
	bool TryStartCombat(const ABaseZone& Zone);

	// The eligibility and choice rule of every fight: living enemy whose origin is in the zone, nearest to the servant (2D). nullptr if none.
	AEnemySoldier* FindNearestEnemyInZone(const ABaseZone& Zone) const;

	// Writes the fight state (target, flags, speed). Nothing is started: no timer, no step, no movement call.
	void BeginCombat(AEnemySoldier& Enemy);

	// Arms the combat timer and runs the first step at once (it may end the fight synchronously).
	void RunCombat();

	// Enemy scan timer: wakes a wandering AttackDefense servant (walking or paused) as soon as an eligible enemy exists.
	// The walk is cancelled with the fight state already written (StopMovement calls OnMoveCompleted synchronously).
	UFUNCTION()
	void ScanForEnemies();

	// One fight step (timer, and once at the start): ends the fight, strikes if in range, or (re)starts the chase.
	UFUNCTION()
	void CombatStep();

	// Same measure as the enemy soldier's attack test: closest point of the target collision, minus the capsule radius, in 2D.
	bool IsInAttackRange(const AServant& Servant, const AActor& Target) const;

	// Clears the fight state and its timer. Does not touch the movement nor the decision timer.
	void ClearCombatState();

	// Ends the fight: state cleared BEFORE StopMovement (its callback is synchronous), then the next decision is
	// scheduled LAST (CombatNextDelay, or RetryDelay and no field work / fight for one decision when bFailed).
	void EndCombat(bool bFailed);

	void MoveToZonePoint(const ABaseZone& Zone);
	void TeleportIntoZone(const ABaseZone& Zone);

// -----------PROPERTIES-----------
private:
	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float InitialDecisionDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float MinWanderPause = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float MaxWanderPause = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float RetryDelay = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxProjectionAttempts = 10;

	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProjectionHorizontalExtent = 100.0f;

	// Added to the zone's half-height to get the vertical projection extent.
	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProjectionVerticalMargin = 100.0f;

	// The capsule radius is already added by bStopOnOverlap: do not add it here.
	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AcceptanceRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxFailedReturnAttempts = 3;

	// Applied to the pawn's MaxWalkSpeed when wandering starts (wander and return moves): a walk, not a run.
	UPROPERTY(EditAnywhere, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true", ClampMin = "10.0"))
	float WanderWalkSpeed = 275.0f;

	// Reach to the field, from the servant's capsule (the capsule radius is added by bStopOnOverlap: do not add it here).
	UPROPERTY(EditAnywhere, Category = "Servant|Harvest", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HarvestReach = 100.0f;

	// Extent to project the field on the NavMesh. Keep it <= HarvestReach, or the servant stops too far to harvest.
	UPROPERTY(EditAnywhere, Category = "Servant|Harvest", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HarvestProjectionExtent = 100.0f;

	// Pause after a harvest before the next decision (the next field, or the wander).
	UPROPERTY(EditAnywhere, Category = "Servant|Harvest", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float HarvestNextDelay = 0.5f;

	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true"))
	bool bIsWandering = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true"))
	int32 FailedReturnAttempts = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true"))
	EServantWanderAction LastAction = EServantWanderAction::None;

	// True from the reservation of a field to the end of the harvest (success, failure or cancellation)
	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Harvest", meta = (AllowPrivateAccess = "true"))
	bool bIsHarvesting = false;

	// True from the reservation of an Empty field to the end of the planting (success, failure or cancellation)
	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Plant", meta = (AllowPrivateAccess = "true"))
	bool bIsPlanting = false;

	// Set after a failed harvest or planting: the next decision does no field work, so an unreachable field cannot make us loop
	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Harvest", meta = (AllowPrivateAccess = "true"))
	bool bSkipNextHarvest = false;

	// Speed while chasing and fighting; WanderWalkSpeed is restored when the fight ends.
	UPROPERTY(EditAnywhere, Category = "Servant|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "10.0"))
	float CombatWalkSpeed = 400.0f;

	// Pause after a fight before the next decision (the next enemy, or the wander).
	UPROPERTY(EditAnywhere, Category = "Servant|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float CombatNextDelay = 0.5f;

	// Period of the enemy scan that wakes a wandering AttackDefense servant (cancels its walk, starts the fight)
	UPROPERTY(EditAnywhere, Category = "Servant|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float EnemyScanInterval = 0.5f;

	// True from the choice of an enemy to the end of the fight (kill, enemy out of the zone, unassignment, failed move, death)
	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Combat", meta = (AllowPrivateAccess = "true"))
	bool bIsFighting = false;

	// A chase request is in progress: no new one is sent while true; OnMoveCompleted ignores any end while this is false.
	bool bIsMovingToEnemy = false;

	// The enemy being fought (bIsFighting)
	TWeakObjectPtr<AEnemySoldier> FightTarget;

	// Upper bound of the random delay before an idle servant reacts to a field or seed change (spreads the rush).
	UPROPERTY(EditAnywhere, Category = "Servant|Wake", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float MaxWakeDelay = 0.5f;

	// A wake signal arrived while the servant was walking: the pause after this move is a wake delay. Raised only while walking (wander or
	// return move), never during a field job. Cleared when OnMoveCompleted consumes it and in StopWandering. After a failed
	// Return move the wake delay replaces the RetryDelay (accepted deviation).
	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wake", meta = (AllowPrivateAccess = "true"))
	bool bWakePending = false;

	// True while ResolveHarvest / ResolvePlant run: the field changes we cause ourselves must not cancel our own job
	bool bIsResolving = false;

	// Target of the current field job (bIsHarvesting or bIsPlanting)
	TWeakObjectPtr<AFieldPlot> FieldTarget;

	TWeakObjectPtr<UResourceCounterComponent> WatchedStorage;

	FTimerHandle DecisionTimer;

	// Repeating, period AttackInterval, runs CombatStep during a fight
	FTimerHandle CombatTimer;

	// Repeating, period EnemyScanInterval, runs ScanForEnemies. Armed by StartWandering, cleared by StopWandering.
	FTimerHandle EnemyScanTimer;
};
