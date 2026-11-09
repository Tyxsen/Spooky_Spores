// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Destructible.generated.h"

class UHealthComponent;

UINTERFACE(BlueprintType)
class SPOOKY_SPORES_API UDestructible : public UInterface
{
	GENERATED_BODY()
};

class SPOOKY_SPORES_API IDestructible
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	UHealthComponent* GetHealthComponent() const;
};
