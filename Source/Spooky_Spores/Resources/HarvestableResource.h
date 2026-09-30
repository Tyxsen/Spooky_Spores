// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Resources/ResourceDrop.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "HarvestableResource.generated.h"

USTRUCT(BlueprintType)
struct FResourceDropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<AResourceDrop> DropClass;

	UPROPERTY(EditAnywhere)
	EResourceType Type = EResourceType::Seed;

	UPROPERTY(EditAnywhere)
	int32 Amount = 1;

	UPROPERTY(EditAnywhere)
	int32 SpawnCount = 1;
};

UCLASS()
class SPOOKY_SPORES_API AHarvestableResource : public AActor, public IInteractable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHarvestableResource();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Resources")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere, Category = "Resources")
	TArray<FResourceDropEntry> Drops;
	
	UPROPERTY(EditAnywhere, Category = "Resources")
	float DropSpawnRadius = 30.0f;

public:
	virtual void OnInteract_Implementation(AActor* InteractingActor) override;
};
