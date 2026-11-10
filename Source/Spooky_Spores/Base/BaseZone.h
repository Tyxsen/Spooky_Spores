// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseZone.generated.h"

class UBoxComponent;

/**
 * Base territory: an oriented box that answers "is this point in the base?" and
 * "give me a random point in the base". Pure geometry, no collision, no tick, no debug drawing.
 * Only one zone per world is supported (see UBaseZoneSubsystem).
 */
UCLASS()
class SPOOKY_SPORES_API ABaseZone : public AActor
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	ABaseZone();

	// True if WorldPoint is inside the zone. False for an invalid zone or a non-finite point.
	UFUNCTION(BlueprintPure, Category = "BaseZone")
	bool IsPointInZone(const FVector& WorldPoint) const;

	// Picks a uniformly distributed random point in the zone (drawn in the box's local space,
	// then transformed to world space). Returns false and leaves OutPoint at ZeroVector
	// when the zone is invalid. Never use OutPoint when the return value is false.
	UFUNCTION(BlueprintCallable, Category = "BaseZone")
	bool GetRandomPointInZone(FVector& OutPoint) const;

	// False if any extent or scale component is <= KINDA_SMALL_NUMBER.
	UFUNCTION(BlueprintPure, Category = "BaseZone")
	bool IsZoneValid() const;

	// Half-sizes of the zone along its local axes, scale included, always positive (absolute value of the scale).
	// 2.2c will need Z to size the vertical extent of its NavMesh projection.
	UFUNCTION(BlueprintPure, Category = "BaseZone")
	FVector GetZoneExtent() const;

	UBoxComponent* GetZoneBox() const { return ZoneBox; }

	// Pure maths, no UObject needed: used by the member functions and callable from Automation Specs.
	// UnscaledExtent is the UNSCALED local half-extent. BoxTransform is the component's world transform (scale included).
	static bool IsOrientedBoxValid(const FTransform& BoxTransform, const FVector& UnscaledExtent);
	static bool IsPointInOrientedBox(const FTransform& BoxTransform, const FVector& UnscaledExtent,
									const FVector& WorldPoint, bool bInIgnoreHeight);
	static bool TryGetRandomPointInOrientedBox(const FTransform& BoxTransform, const FVector& UnscaledExtent,
											bool bInIgnoreHeight, FVector& OutPoint);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

// -----------PROPERTIES-----------
protected:
	// When true, only X and Y (local) are tested/drawn; random points are picked on the box's mid plane (local Z = 0).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseZone")
	bool bIgnoreHeight = true;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BaseZone", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> ZoneBox;

	// Invalid-zone warning is logged once per zone (queried at every servant decision).
	mutable bool bHasWarnedInvalid = false;
};
