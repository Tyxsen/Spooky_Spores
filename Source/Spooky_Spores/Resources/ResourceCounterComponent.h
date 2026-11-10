// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Resources/ResourceTypes.h"
#include "ResourceCounterComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResourceChanged, EResourceType, Type, int32, NewCount);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API UResourceCounterComponent : public UActorComponent
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Sets default values for this component's properties
	UResourceCounterComponent();

	UFUNCTION(BlueprintCallable, Category = "Resources")
	void AddResource(EResourceType Type, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Resources")
	int32 GetResourceCount(EResourceType Type) const;

	UFUNCTION(BlueprintCallable, Category = "Resources")
	bool TrySpendResource(EResourceType Type, int32 Amount);

// -----------PROPERTIES-----------
public:
	UPROPERTY(BlueprintAssignable, Category = "Resources")
	FOnResourceChanged OnResourceChanged;

private:
	// Private so every change goes through AddResource/TrySpendResource and broadcasts OnResourceChanged
	UPROPERTY(BlueprintReadOnly, Category = "Resources", meta = (AllowPrivateAccess = "true"))
	TMap<EResourceType, int32> ResourceCounts;

};
