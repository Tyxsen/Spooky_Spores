// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/FieldPlot.h"
#include "Resources/ResourceCounterComponent.h"
#include "Resources/ResourceDrop.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

// Sets default values
AFieldPlot::AFieldPlot()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsInteractable"));
}

void AFieldPlot::OnInteract_Implementation(AActor* InteractingActor)
{
	switch (CurrentState)
	{
		case EFieldState::Empty:
			if (UResourceCounterComponent* Counter = InteractingActor->FindComponentByClass<UResourceCounterComponent>())
			{
				if (Counter->TrySpendResource(EResourceType::Seed, SeedCost))
				{
					CurrentState = EFieldState::Growing;
					GetWorldTimerManager().SetTimer(
						GrowthTimerHandle,
						this,
						&AFieldPlot::OnGrowthComplete,
						GrowthDuration,
						false
					);
				}
			}
			break;
		case EFieldState::Growing:
			// Rien pour le moment
			break;
		case EFieldState::Grown:
			Harvest(InteractingActor);
			break;
	}
}

void AFieldPlot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FColor DebugColor;
	switch (CurrentState)
	{
		case EFieldState::Empty:
			DebugColor = FColor(128, 128, 128);
			break;
		case EFieldState::Growing:
			DebugColor = FColor::Red;
			break;
		case EFieldState::Grown:
			DebugColor = FColor::Green;
			break;
	}

	DrawDebugSphere(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 50.f),
		20.f, 12, DebugColor, false, -1.f, 0, 2.f);
}

void AFieldPlot::OnGrowthComplete()
{
	CurrentState = EFieldState::Grown;
}

void AFieldPlot::Harvest(AActor* HarvestingActor)
{
	AResourceDrop::SpawnDrops(GetWorld(), HarvestDrops, GetActorLocation(), DropSpawnRadius);

	CurrentState = EFieldState::Empty;
}

