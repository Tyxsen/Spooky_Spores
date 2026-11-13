// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/FieldPlot.h"
#include "Engine/World.h"
#include "Resources/FieldPlotSubsystem.h"
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
			TryPlant(InteractingActor->FindComponentByClass<UResourceCounterComponent>());
			break;
		case EFieldState::Growing:
			// Rien pour le moment
			break;
		case EFieldState::Grown:
			Harvest(InteractingActor);
			break;
	}
}

bool AFieldPlot::IsReadyToHarvest() const
{
	return CurrentState == EFieldState::Grown;
}

bool AFieldPlot::TryHarvestYield(TMap<EResourceType, int32>& OutYield)
{
	OutYield.Reset();
	if (!IsReadyToHarvest())
	{
		return false;
	}

	OutYield = ComputeHarvestYield();
	SetState(EFieldState::Empty);
	return true;
}

bool AFieldPlot::IsReadyToPlant() const
{
	return CurrentState == EFieldState::Empty;
}

int32 AFieldPlot::GetSeedCost() const
{
	return SeedCost;
}

bool AFieldPlot::TryPlant(UResourceCounterComponent* SeedSource)
{
	if (!IsReadyToPlant() || !SeedSource || !SeedSource->TrySpendResource(EResourceType::Seed, SeedCost))
	{
		return false;
	}

	GetWorldTimerManager().SetTimer(
		GrowthTimerHandle,
		this,
		&AFieldPlot::OnGrowthComplete,
		GrowthDuration,
		false
	);
	SetState(EFieldState::Growing);
	return true;
}

void AFieldPlot::BeginPlay()
{
	Super::BeginPlay();

	if (UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>())
	{
		FieldSubsystem->RegisterField(this);
	}
}

void AFieldPlot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The world can already be gone when the actor ends play with the world
	if (const UWorld* World = GetWorld())
	{
		if (UFieldPlotSubsystem* FieldSubsystem = World->GetSubsystem<UFieldPlotSubsystem>())
		{
			FieldSubsystem->UnregisterField(this);
		}
	}

	Super::EndPlay(EndPlayReason);
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
	SetState(EFieldState::Grown);
}

void AFieldPlot::Harvest(AActor* HarvestingActor)
{
	AResourceDrop::SpawnDrops(GetWorld(), HarvestDrops, GetActorLocation(), DropSpawnRadius);

	SetState(EFieldState::Empty);
}

void AFieldPlot::SetState(EFieldState NewState)
{
	CurrentState = NewState;

	if (UFieldPlotSubsystem* FieldSubsystem = GetWorld()->GetSubsystem<UFieldPlotSubsystem>())
	{
		FieldSubsystem->NotifyFieldChanged(this);
	}
}

TMap<EResourceType, int32> AFieldPlot::ComputeHarvestYield() const
{
	TMap<EResourceType, int32> Yield;
	for (const FResourceDropEntry& Entry : HarvestDrops)
	{
		// Same total as the drops of the player path (one drop of Amount per SpawnCount), whether or not DropClass is set
		Yield.FindOrAdd(Entry.Type) += Entry.Amount * Entry.SpawnCount;
	}
	return Yield;
}
