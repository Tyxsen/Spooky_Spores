// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/Cauldron.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Resources/ResourceCounterComponent.h"
#include "DrawDebugHelpers.h"
#include "UObject/Class.h"

// Sets default values
ACauldron::ACauldron()
{
	// Tick is only used to draw the debug state.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsInteractable"));
}

void ACauldron::OnInteract_Implementation(AActor* InteractingActor)
{
	if (!InteractingActor) return;

	switch (CurrentState)
	{
		case ECauldronState::Idle:
			TryStartBrewing(InteractingActor);
			break;
		case ECauldronState::Brewing:
			break;
		case ECauldronState::Ready:
			CollectOutput(InteractingActor);
			break;
	}
}

void ACauldron::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebug)
	{
		DrawDebugState();
	}
}

void ACauldron::DrawDebugState() const
{
	FColor DebugColor = FColor(128, 128, 128); // Idle by default
	switch (CurrentState)
	{
		case ECauldronState::Idle:
			break;
		case ECauldronState::Brewing:
			DebugColor = FColor::Red;
			break;
		case ECauldronState::Ready:
			DebugColor = FColor::Green;
			break;
	}

	const FVector SphereLocation = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	DrawDebugSphere(GetWorld(), SphereLocation, 20.f, 12, DebugColor, false, -1.f, 0, 2.f);

	if (!Recipes.IsValidIndex(ActiveRecipeIndex)) return;

	const FString Label = FString::Printf(TEXT("%s %d%%"),
		*UEnum::GetDisplayValueAsText(Recipes[ActiveRecipeIndex].OutputType).ToString(),
		FMath::RoundToInt(GetBrewProgress() * 100.f));

	// Duration 0 = drawn for a single frame. -1 would make the text permanent and stack up every tick.
	DrawDebugString(GetWorld(), SphereLocation + FVector(0.f, 0.f, 40.f), Label, nullptr, DebugColor, 0.f, true);
}

void ACauldron::OnBrewComplete()
{
	CurrentState = ECauldronState::Ready;
}

bool ACauldron::CanAfford(const FCauldronRecipe& Recipe, const UResourceCounterComponent& Counter) const
{
	for (const TPair<EResourceType, int32>& Ingredient : Recipe.Ingredients)
	{
		if (Counter.GetResourceCount(Ingredient.Key) < Ingredient.Value) return false;
	}
	return true;
}

int32 ACauldron::FindFirstAffordableRecipe(const UResourceCounterComponent& Counter) const
{
	for (int32 Index = 0; Index < Recipes.Num(); ++Index)
	{
		// An empty recipe would be free: treat it as a configuration mistake, not a valid recipe.
		if (Recipes[Index].Ingredients.Num() == 0) continue;

		if (CanAfford(Recipes[Index], Counter)) return Index;
	}
	return INDEX_NONE;
}

void ACauldron::TryStartBrewing(AActor* InteractingActor)
{
	UResourceCounterComponent* Counter = InteractingActor->FindComponentByClass<UResourceCounterComponent>();
	if (!Counter) return;

	const int32 RecipeIndex = FindFirstAffordableRecipe(*Counter);
	if (RecipeIndex == INDEX_NONE) return;

	const FCauldronRecipe& Recipe = Recipes[RecipeIndex];
	for (const TPair<EResourceType, int32>& Ingredient : Recipe.Ingredients)
	{
		Counter->TrySpendResource(Ingredient.Key, Ingredient.Value);
	}

	ActiveRecipeIndex = RecipeIndex;
	CurrentState = ECauldronState::Brewing;
	BrewStartTime = GetWorld()->GetTimeSeconds();

	// A zero-duration timer never fires: complete immediately instead of staying stuck in Brewing.
	if (Recipe.BrewDuration <= 0.0f)
	{
		OnBrewComplete();
		return;
	}
	GetWorldTimerManager().SetTimer(BrewTimerHandle, this, &ACauldron::OnBrewComplete, Recipe.BrewDuration, false);
}

void ACauldron::CollectOutput(AActor* InteractingActor)
{
	UResourceCounterComponent* Counter = InteractingActor->FindComponentByClass<UResourceCounterComponent>();
	if (!Counter) return;

	if (Recipes.IsValidIndex(ActiveRecipeIndex))
	{
		const FCauldronRecipe& Recipe = Recipes[ActiveRecipeIndex];
		Counter->AddResource(Recipe.OutputType, Recipe.OutputAmount);
	}

	ActiveRecipeIndex = INDEX_NONE;
	CurrentState = ECauldronState::Idle;
}

float ACauldron::GetBrewProgress() const
{
	switch (CurrentState)
	{
		case ECauldronState::Idle: return 0.0f; // By default, empty
		case ECauldronState::Ready: return 1.0f; // Ready to collect the output
		default: break; // Brewing
	}

	if (!Recipes.IsValidIndex(ActiveRecipeIndex)) return 0.0f;

	const float Duration = Recipes[ActiveRecipeIndex].BrewDuration;
	if (Duration <= 0.0f) return 1.0f;

	const float Elapsed = GetWorld()->GetTimeSeconds() - BrewStartTime;
	return FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
}
