// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/ResourceCounterComponent.h"

// Sets default values for this component's properties
UResourceCounterComponent::UResourceCounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UResourceCounterComponent::AddResource(EResourceType Type, int32 Amount)
{
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
	int32* Found = ResourceCounts.Find(Type);
	if (!Found || *Found < Amount) return false;

	*Found -= Amount;
	OnResourceChanged.Broadcast(Type, *Found);
	return true;
}

