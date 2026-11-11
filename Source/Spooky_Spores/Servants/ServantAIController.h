// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ServantAIController.generated.h"

class ABaseZone;
struct FPathFollowingResult;

// What the servant decided to do at its last decision.
UENUM(BlueprintType)
enum class EServantWanderAction : uint8
{
	None,
	Wait,
	Wander,
	Return,
	Teleport
};

/**
 * Makes an idle servant wander inside the base zone. Placed outside the zone, it walks back,
 * and is teleported in after MaxFailedReturnAttempts failed returns.
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

	// Stops the current move and cancels the next decision (task assignment will call this).
	UFUNCTION(BlueprintCallable, Category = "Servant|Wander")
	void StopWandering();

	UFUNCTION(BlueprintPure, Category = "Servant|Wander")
	bool IsWandering() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Wander")
	int32 GetFailedReturnAttempts() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Wander")
	EServantWanderAction GetLastAction() const;

	// Pure decision rule, no UObject needed: callable from Automation Specs.
	static EServantWanderAction DecideNextAction(bool bHasValidZone, bool bIsInZone, int32 FailedReturns, int32 MaxReturns);

	// Only place that schedules what follows a move. Also called synchronously by MoveToLocation
	// (Failed, AlreadyAtGoal) and by StopMovement (Aborted).
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void MakeDecision();
	void ScheduleDecision(float Delay);

	// Draws random points in the zone and projects them on the NavMesh, at most MaxProjectionAttempts times.
	// A projected point outside the zone counts as a failed attempt. Never use OutPoint when false.
	bool TryFindNavPointInZone(const ABaseZone& Zone, FVector& OutPoint) const;

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
	float WanderWalkSpeed = 200.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true"))
	bool bIsWandering = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true"))
	int32 FailedReturnAttempts = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Servant|Wander", meta = (AllowPrivateAccess = "true"))
	EServantWanderAction LastAction = EServantWanderAction::None;

	FTimerHandle DecisionTimer;
};
