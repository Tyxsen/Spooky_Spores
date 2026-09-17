// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Grabbable.generated.h"

class UPrimitiveComponent;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UGrabbable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class SPOOKY_SPORES_API IGrabbable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	UPrimitiveComponent* GetGrabbableComponent() const;
};
