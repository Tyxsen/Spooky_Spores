// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/ResourceCounterComponent.h"
#include "Spooky_Spores.h"

// Sets default values for this component's properties
UResourceCounterComponent::UResourceCounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UResourceCounterComponent::AddResource(EResourceType Type, int32 Amount)
{
	// A negative amount would silently remove resources without any check: spending goes through TrySpendResource
	if (Amount <= 0)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("AddResource called with a non-positive amount (%d), ignored"), Amount);
		return;
	}

	int32& ActualTypeAmount = ResourceCounts.FindOrAdd(Type);
	ActualTypeAmount += Amount;
	
	OnResourceChanged.Broadcast(Type, ActualTypeAmount);
}

int32 UResourceCounterComponent::GetResourceCount(EResourceType Type) const
{
	if (const int32* Found = ResourceCounts.Find(Type)) return *Found;	
	return 0;
}

bool UResourceCounterComponent::TrySpendResource(EResourceType Type, int32 Amount)
{
	// A negative amount would pass the check below and then add resources instead of spending them
	if (Amount <= 0)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("TrySpendResource called with a non-positive amount (%d), ignored"), Amount);
		return false;
	}

	int32* Found = ResourceCounts.Find(Type);
	if (!Found || *Found < Amount) return false;

	*Found -= Amount;
	OnResourceChanged.Broadcast(Type, *Found);
	return true;
}

