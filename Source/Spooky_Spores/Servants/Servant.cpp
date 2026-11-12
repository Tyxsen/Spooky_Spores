// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/Servant.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Servants/ServantAIController.h"
#include "Servants/ServantSubsystem.h"

AServant::AServant()
{
	// Nothing to update every frame for now
	PrimaryActorTick.bCanEverTick = false;

	// The default (PlacedInWorld) would not possess a servant spawned by the player
	AIControllerClass = AServantAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Path following drives the movement through acceleration, like player input (walk cycle in the Anim Blueprint)
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;

	// Face movement direction instead of control rotation: with bCanStrafe = false the controller
	// focuses the next path point and the pawn would snap at every corner.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AServant::BeginPlay()
{
	Super::BeginPlay();

	// The subsystem may be missing (unsupported world type): the servant simply stays out of the registry
	if (UWorld* World = GetWorld())
	{
		if (UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>())
		{
			Subsystem->RegisterServant(this);
		}
	}
}

void AServant::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>())
		{
			Subsystem->UnregisterServant(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
