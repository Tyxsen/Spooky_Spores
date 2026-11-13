// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Resources/ResourceTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "FieldPlot.generated.h"

class UResourceCounterComponent;

UENUM()
enum class EFieldState : uint8
{
	Empty,
	Growing,
	Grown
};

UCLASS()
class SPOOKY_SPORES_API AFieldPlot : public AActor, public IInteractable
{
	GENERATED_BODY()
	
// -----------FUNCTIONS-----------
public:
	// Sets default values for this actor's properties
	AFieldPlot();

	virtual void OnInteract_Implementation(AActor* InteractingActor) override;

	UFUNCTION(BlueprintPure, Category = "Farming")
	bool IsReadyToHarvest() const;

	// Harvests without spawning drops: on a Grown field, fills OutYield (total amount per type) and empties the field.
	// Returns false (field untouched, OutYield empty) otherwise. The caller decides where the yield goes.
	bool TryHarvestYield(TMap<EResourceType, int32>& OutYield);

	UFUNCTION(BlueprintPure, Category = "Farming")
	bool IsReadyToPlant() const;

	UFUNCTION(BlueprintPure, Category = "Farming")
	int32 GetSeedCost() const;

	// Plants on an Empty field: spends SeedCost seeds from SeedSource and starts the growth.
	// Returns false (field and counter untouched) if the field is not Empty, SeedSource is null or holds too few seeds.
	// Shared by the player (OnInteract) and the servants.
	UFUNCTION(BlueprintCallable, Category = "Farming")
	bool TryPlant(UResourceCounterComponent* SeedSource);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	void OnGrowthComplete();
	void Harvest(AActor* HarvestingActor);

	// Only place that changes CurrentState: tells the field subsystem so that servants react without polling.
	void SetState(EFieldState NewState);

	// Single computation of what a harvest yields, from HarvestDrops: Amount * SpawnCount per type.
	TMap<EResourceType, int32> ComputeHarvestYield() const;

// -----------PROPERTIES-----------
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Farming")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Farming")
	EFieldState CurrentState = EFieldState::Empty;
	
	UPROPERTY(EditAnywhere, Category = "Farming")
	int32 SeedCost = 1;

	UPROPERTY(EditAnywhere, Category = "Farming")
	float GrowthDuration = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Farming")
	TArray<FResourceDropEntry> HarvestDrops;

	UPROPERTY(EditAnywhere, Category = "Farming")
	float DropSpawnRadius = 30.0f;

	FTimerHandle GrowthTimerHandle;
};
