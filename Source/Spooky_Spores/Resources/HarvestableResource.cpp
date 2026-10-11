// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/HarvestableResource.h"
#include "Health/HealthComponent.h"
#include "Resources/ResourceDrop.h"

// Sets default values
AHarvestableResource::AHarvestableResource()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsInteractable"));
}

void AHarvestableResource::BeginPlay()
{
	Super::BeginPlay();

	if (UHealthComponent* Health = FindComponentByClass<UHealthComponent>())
		Health->OnDeath.AddDynamic(this, &AHarvestableResource::HandleHealthDepleted);
}

void AHarvestableResource::OnInteract_Implementation(AActor* InteractingActor)
{
	if (FindComponentByClass<UHealthComponent>()) return;
	HarvestAndDestroy();
}

void AHarvestableResource::HandleHealthDepleted()
{
	HarvestAndDestroy();
}

void AHarvestableResource::HarvestAndDestroy()
{
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AResourceDrop::SpawnDrops(GetWorld(), Drops, GetActorLocation(), DropSpawnRadius);
	Destroy();
}
