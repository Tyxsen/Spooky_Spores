// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Resources/ResourceTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Cauldron.generated.h"

class UResourceCounterComponent;

UENUM()
enum class ECauldronState : uint8
{
	Idle,
	Brewing,
	Ready
};

// One cauldron recipe: ingredients consumed at start, output delivered on collection.
USTRUCT(BlueprintType)
struct FCauldronRecipe
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cauldron")
	TMap<EResourceType, int32> Ingredients;

	UPROPERTY(EditAnywhere, Category = "Cauldron")
	EResourceType OutputType = EResourceType::Potion;

	UPROPERTY(EditAnywhere, Category = "Cauldron", meta = (ClampMin = "1"))
	int32 OutputAmount = 1;

	UPROPERTY(EditAnywhere, Category = "Cauldron", meta = (ClampMin = "0.0"))
	float BrewDuration = 30.0f;
};

UCLASS()
class SPOOKY_SPORES_API ACauldron : public AActor, public IInteractable
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Sets default values for this actor's properties
	ACauldron();

	virtual void OnInteract_Implementation(AActor* InteractingActor) override;

	UFUNCTION(BlueprintPure, Category = "Cauldron")
	float GetBrewProgress() const;

protected:
	virtual void Tick(float DeltaTime) override;

private:
	void DrawDebugState() const;
	void OnBrewComplete();
	void TryStartBrewing(AActor* InteractingActor);
	void CollectOutput(AActor* InteractingActor);

	bool CanAfford(const FCauldronRecipe& Recipe, const UResourceCounterComponent& Counter) const;

	// Returns the index of the first affordable recipe in array order, INDEX_NONE if none.
	int32 FindFirstAffordableRecipe(const UResourceCounterComponent& Counter) const;

// -----------PROPERTIES-----------
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Cauldron")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Cauldron")
	ECauldronState CurrentState = ECauldronState::Idle;

	// Tried in order: the first affordable recipe wins (temporary rule until a recipe selection UI exists).
	UPROPERTY(EditAnywhere, Category = "Cauldron")
	TArray<FCauldronRecipe> Recipes;

	// Index into Recipes of the brew in progress (or ready to collect), INDEX_NONE when idle.
	UPROPERTY(VisibleInstanceOnly, Category = "Cauldron")
	int32 ActiveRecipeIndex = INDEX_NONE;

	// Debug sphere above the cauldron (grey = idle, red = brewing, green = ready) with the active recipe and its progress.
	UPROPERTY(EditAnywhere, Category = "Cauldron|Debug")
	bool bShowDebug = true;

	float BrewStartTime = 0.0f;
	FTimerHandle BrewTimerHandle;
};
