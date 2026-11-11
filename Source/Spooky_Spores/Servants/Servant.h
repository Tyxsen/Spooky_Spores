// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Servant.generated.h"

// Resurrected servant spawned by the player from a Necrospore.
// Possessed by an AServantAIController as soon as it is spawned or placed: it wanders in the base zone.
UCLASS()
class SPOOKY_SPORES_API AServant : public ACharacter
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	AServant();
};
