#include "Enemies/EnemySoldier.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Health/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Resources/ResourceDrop.h"

// Sets default values
AEnemySoldier::AEnemySoldier()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	// Face movement direction instead of control rotation, so it turns to match AddMovementInput (spline or target chase).
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

// Called when the game starts or when spawned
void AEnemySoldier::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnDeath.AddDynamic(this, &AEnemySoldier::HandleDeath);
}

void AEnemySoldier::Attack(AActor* Target)
{
	if (!IsValid(Target)) return;

	UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
}

float AEnemySoldier::GetAttackRange() const
{
	return AttackRange;
}

float AEnemySoldier::GetAttackInterval() const
{
	return AttackInterval;
}

AEnemyPathSpline* AEnemySoldier::GetPathSplineActor() const
{
	return PathSplineActor;
}

void AEnemySoldier::HandleDeath()
{
	AResourceDrop::SpawnDrops(GetWorld(), Drops, GetActorLocation(), DropSpawnRadius);
	Destroy();
}
