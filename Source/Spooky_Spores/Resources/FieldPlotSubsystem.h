// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FieldPlotSubsystem.generated.h"

class AActor;
class AFieldPlot;

// Field, true if the field is being removed (EndPlay) rather than changing state.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFieldChanged, AFieldPlot*, bool);

// One registered field and who reserved it. Weak pointers: the registry never keeps anything alive and never dangles.
struct FFieldPlotEntry
{
	TWeakObjectPtr<AFieldPlot> Field;
	TWeakObjectPtr<const AActor> Reserver;
};

/**
 * Registry of the fields of a world, and reservation so that two servants never go for the same field.
 * A reservation whose reserver is destroyed counts as free. No TActorIterator: fields register themselves.
 */
UCLASS()
class SPOOKY_SPORES_API UFieldPlotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Called by AFieldPlot::BeginPlay / EndPlay only. Duplicates and unknown fields are ignored.
	void RegisterField(AFieldPlot* Field);
	void UnregisterField(AFieldPlot* Field);

	// Grown fields nobody holds a reservation on, in registration order.
	UFUNCTION(BlueprintPure, Category = "Farming")
	TArray<AFieldPlot*> GetUnreservedHarvestableFields() const;

	// Empty fields nobody holds a reservation on, in registration order.
	UFUNCTION(BlueprintPure, Category = "Farming")
	TArray<AFieldPlot*> GetUnreservedPlantableFields() const;

	// Seeds already promised to other servants: sum of the SeedCost of the Empty fields reserved by a live reserver
	// other than ExcludedReserver. Derived from the reservations, no state of its own.
	UFUNCTION(BlueprintPure, Category = "Farming")
	int32 GetPromisedSeeds(const AActor* ExcludedReserver) const;

	// Called by AFieldPlot only (state change). UnregisterField broadcasts it with bRemoved = true.
	void NotifyFieldChanged(AFieldPlot* Field, bool bRemoved = false);

	// True if the field is now reserved by Reserver (free, or already its own). False if another live reserver holds it,
	// or if the field is not registered.
	bool TryReserve(AFieldPlot* Field, const AActor* Reserver);

	// No effect unless Reserver holds the reservation.
	void Release(AFieldPlot* Field, const AActor* Reserver);

	UFUNCTION(BlueprintPure, Category = "Farming")
	bool IsReserved(const AFieldPlot* Field) const;

private:
	// INDEX_NONE if not registered
	int32 FindEntryIndex(const AFieldPlot* Field) const;

// -----------PROPERTIES-----------
public:
	FOnFieldChanged OnFieldChanged;

private:
	TArray<FFieldPlotEntry> Entries;
};
