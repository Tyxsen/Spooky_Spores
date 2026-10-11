// Fill out your copyright notice in the Description page of Project Settings.


#include "Spells/ManaComponent.h"

// Sets default values for this component's properties
UManaComponent::UManaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UManaComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentMana = MaxMana;
}

// Called every frame
void UManaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CurrentMana = FMath::Clamp(CurrentMana + RegenRate * DeltaTime, 0.f, MaxMana);
}

bool UManaComponent::TrySpendMana(float Cost)
{
	if (CurrentMana < Cost) return false;
	
	CurrentMana -= Cost;
	return true;
}

float UManaComponent::GetManaRatio() const
{
	return CurrentMana / MaxMana;
}

