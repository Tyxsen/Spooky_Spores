// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/Cauldron.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Resources/ResourceCounterComponent.h"

// Sets default values
ACauldron::ACauldron()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsInteractable"));
}

void ACauldron::OnInteract_Implementation(AActor* InteractingActor)
{
	switch (CurrentState)
	{
		case ECauldronState::Idle:
			TryStartBrewing(InteractingActor);
			break;
		case ECauldronState::Brewing:
			break;
		case ECauldronState::Ready:
			CollectPotion(InteractingActor);
			break;
	}
}

void ACauldron::OnBrewComplete()
{
	CurrentState = ECauldronState::Ready;
}

void ACauldron::TryStartBrewing(AActor* InteractingActor)
{
	UResourceCounterComponent* Counter = InteractingActor->FindComponentByClass<UResourceCounterComponent>();
	if (!Counter) return;

	for (const auto& Ingredient : RequiredIngredients)
	{
		if (Counter->GetResourceCount(Ingredient.Key) < Ingredient.Value) return;
	}
	for (const auto& Ingredient : RequiredIngredients)
	{
		Counter->TrySpendResource(Ingredient.Key, Ingredient.Value);
	}

	CurrentState = ECauldronState::Brewing;
	BrewStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(BrewTimerHandle, this, &ACauldron::OnBrewComplete, BrewDuration, false);
}

void ACauldron::CollectPotion(AActor* InteractingActor)
{
	if (UResourceCounterComponent* Counter = InteractingActor->FindComponentByClass<UResourceCounterComponent>())
	{
		Counter->AddResource(OutputPotionType, OutputAmount);
		CurrentState = ECauldronState::Idle;
	}
}

float ACauldron::GetBrewProgress() const
{
	switch (CurrentState)
	{
		case ECauldronState::Idle: return 0.0f; // By default, empty
		case ECauldronState::Ready: return 1.0f; // Ready to pick up the potion
		default: break; // Brewing
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - BrewStartTime;
	return FMath::Clamp(Elapsed / BrewDuration, 0.0f, 1.0f);
}
