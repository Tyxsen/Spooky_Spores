// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ResourceTypes.h"
#include "ResourceCounterComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResourceChanged, EResourceType, Type, int32, NewCount);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API UResourceCounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UResourceCounterComponent();
	
public:
	UFUNCTION(BlueprintCallable, Category = "Resources")
	void AddResource(EResourceType Type, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Resources")
	int32 GetResourceCount(EResourceType Type) const;

public:	
	UPROPERTY(BlueprintReadOnly, Category = "Resources")
	TMap<EResourceType, int32> ResourceCounts;

	UPROPERTY(BlueprintAssignable, Category = "Resources")
	FOnResourceChanged OnResourceChanged;
	
};
