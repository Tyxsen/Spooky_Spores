// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/Servant.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Health/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Servants/ServantAIController.h"
#include "Servants/ServantSubsystem.h"

AServant::AServant()
{
	// Nothing to update every frame for now
	PrimaryActorTick.bCanEverTick = false;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

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

UHealthComponent* AServant::GetHealthComponent_Implementation() const
{
	return HealthComponent;
}

void AServant::Attack(AActor* Target)
{
	if (!IsValid(Target)) return;

	UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
}

float AServant::GetAttackRange() const
{
	return AttackRange;
}

float AServant::GetAttackInterval() const
{
	return AttackInterval;
}

void AServant::HandleDeath()
{
	// No drop, no animation. EndPlay unregisters the servant (its assignment is lost); unpossession frees its field reservation.
	Destroy();
}

void AServant::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnDeath.AddDynamic(this, &AServant::HandleDeath);

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
