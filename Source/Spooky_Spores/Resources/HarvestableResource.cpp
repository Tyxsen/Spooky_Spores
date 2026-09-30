// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/HarvestableResource.h"
#include "Engine/World.h"

// Sets default values
AHarvestableResource::AHarvestableResource()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsInteractable"));
}

void AHarvestableResource::OnInteract_Implementation(AActor* InteractingActor)
{
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	for (const FResourceDropEntry& Drop : Drops)
	{
		if (!Drop.DropClass) continue;
		
		for (int32 i = 0; i < Drop.SpawnCount; ++i)
		{
			FVector RandomOffset(
				FMath::FRandRange(-DropSpawnRadius, DropSpawnRadius),
				FMath::FRandRange(-DropSpawnRadius, DropSpawnRadius),
				0.0f
			);
			FVector SpawnLocation = GetActorLocation() + RandomOffset + FVector(0, 0, 50.0f);
			FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
			
			AResourceDrop* NewDrop = GetWorld()->SpawnActorDeferred<AResourceDrop>(Drop.DropClass, SpawnTransform);
			if (!NewDrop) continue;

			NewDrop->InitializeDrop(Drop.Type, Drop.Amount);
			NewDrop->FinishSpawning(SpawnTransform);
		}
	}
	Destroy();
}
