// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Servant.generated.h"

// Resurrected servant spawned by the player from a Necrospore.
// Step 2.2a: no behaviour yet, it only exists in the world. Wandering comes in step 2.2c.
UCLASS()
class SPOOKY_SPORES_API AServant : public ACharacter
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	AServant();
};
