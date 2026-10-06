// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Resources/ResourceTypes.h"
#include "HarvestableResource.generated.h"

UCLASS()
class SPOOKY_SPORES_API AHarvestableResource : public AActor, public IInteractable
{
	GENERATED_BODY()
	
// -----------FUNCTIONS-----------
public:
	// Sets default values for this actor's properties
	AHarvestableResource();

	virtual void OnInteract_Implementation(AActor* InteractingActor) override;

// -----------PROPERTIES-----------
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Resources")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, Category = "Resources")
	TArray<FResourceDropEntry> Drops;

	UPROPERTY(EditAnywhere, Category = "Resources")
	float DropSpawnRadius = 30.0f;
};
