#include "Enemies/EnemySoldier.h"
#include "Engine/World.h"
#include "Enemies/EnemyRegistrySubsystem.h"
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

	// NavMesh moves impose a velocity, not an acceleration: without this the Anim Blueprint sees no acceleration and slides without walking.
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}

// Called when the game starts or when spawned
void AEnemySoldier::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnDeath.AddDynamic(this, &AEnemySoldier::HandleDeath);

	// The subsystem may be missing (unsupported world type): the soldier simply stays out of the registry
	if (UEnemyRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UEnemyRegistrySubsystem>())
	{
		Registry->RegisterEnemy(this);
	}
}

void AEnemySoldier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		if (UEnemyRegistrySubsystem* Registry = World->GetSubsystem<UEnemyRegistrySubsystem>())
		{
			Registry->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AEnemySoldier::IsDead() const
{
	return HealthComponent->IsDead();
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

void AEnemySoldier::SetPathSplineActor(AEnemyPathSpline* NewPathSplineActor)
{
	PathSplineActor = NewPathSplineActor;
}

void AEnemySoldier::DebugKill()
{
	// Lethal damage instead of calling HandleDeath directly, so the whole death chain is exercised.
	UGameplayStatics::ApplyDamage(this, TNumericLimits<float>::Max(), nullptr, nullptr, UDamageType::StaticClass());
}

void AEnemySoldier::HandleDeath()
{
	AResourceDrop::SpawnDrops(GetWorld(), Drops, GetActorLocation(), DropSpawnRadius);
	Destroy();
}
