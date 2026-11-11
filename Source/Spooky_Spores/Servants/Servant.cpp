// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/Servant.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Servants/ServantAIController.h"

AServant::AServant()
{
	// Nothing to update every frame for now
	PrimaryActorTick.bCanEverTick = false;

	// The default (PlacedInWorld) would not possess a servant spawned by the player
	AIControllerClass = AServantAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Path following drives the movement through acceleration, like player input (walk cycle in the Anim Blueprint)
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}
