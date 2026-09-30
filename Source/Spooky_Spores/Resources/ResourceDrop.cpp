// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/ResourceDrop.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Resources/ResourceCounterComponent.h"

// Sets default values
AResourceDrop::AResourceDrop()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("ResourceDrop"));
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetGenerateOverlapEvents(true);
	MeshComponent->SetNotifyRigidBodyCollision(true);
}

// Called when the game starts or when spawned
void AResourceDrop::BeginPlay()
{
	Super::BeginPlay();
	
	MeshComponent->OnComponentBeginOverlap.AddDynamic(this, &AResourceDrop::OnMeshOverlap);
	MeshComponent->OnComponentHit.AddDynamic(this, &AResourceDrop::OnMeshHit);

	FVector DropVector = FMath::VRandCone(FVector::UpVector, FMath::DegreesToRadians(PopConeHalfAngle));
	DropVector *= PopStrength;
	MeshComponent->AddImpulse(DropVector, NAME_None, true);
	
	
	if (PickupDelay <= 0.0f)
	{
		EnablePickup();
	}
	else
	{
		GetWorldTimerManager().SetTimer(PickupTimerHandle, this, &AResourceDrop::EnablePickup, PickupDelay, false);
	}
}

void AResourceDrop::OnMeshOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bCanBePickedUp) return;

	TryCollect(OtherActor);
}

void AResourceDrop::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (Hit.ImpactNormal.Z < MinGroundNormalZ) return;
	if (bHasLanded) return;
	bHasLanded = true;
	
	MeshComponent->SetSimulatePhysics(false);
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));

	const FBoxSphereBounds& MeshBounds = MeshComponent->Bounds;
	const float BottomZ = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
	const float Difference = Hit.ImpactPoint.Z - BottomZ;
	AddActorWorldOffset(FVector(0.0f, 0.0f, Difference));
}

void AResourceDrop::InitializeDrop(EResourceType Type, int32 InAmount)
{
	DropType = Type;
	Amount = InAmount;
}

void AResourceDrop::EnablePickup()
{
	bCanBePickedUp = true;
	TArray<AActor*> OverlappedActors;
	MeshComponent->GetOverlappingActors(OverlappedActors, APawn::StaticClass());
	
	for (AActor* Actor : OverlappedActors)
	{
		if (TryCollect(Actor)) return;
	}
}

bool AResourceDrop::TryCollect(AActor* Actor)
{
	APawn* PlayerPawn = Cast<APawn>(Actor);
	if (!PlayerPawn) return false;
	
	UResourceCounterComponent* Resource = PlayerPawn->FindComponentByClass<UResourceCounterComponent>();
	if (!Resource) return false;

	Resource->AddResource(DropType, Amount);
	Destroy();
	return true;
}

