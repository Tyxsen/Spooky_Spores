// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/ResourceDrop.h"
#include "GameFramework/Pawn.h"
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
}

void AResourceDrop::OnMeshOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!PlayerPawn) return;
	
	UResourceCounterComponent* Resource = PlayerPawn->FindComponentByClass<UResourceCounterComponent>();
	if (!Resource) return;

	Resource->AddResource(DropType, Amount);
	Destroy();	
}

void AResourceDrop::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (Hit.ImpactNormal.Z < MinGroundNormalZ) return;
	MeshComponent->SetSimulatePhysics(false);
}

void AResourceDrop::InitializeDrop(EResourceType Type, int32 InAmount)
{
	DropType = Type;
	Amount = InAmount;
	
	FVector DropVector = FMath::VRandCone(FVector::UpVector, FMath::DegreesToRadians(PopConeHalfAngle));
	DropVector *= PopStrength;

	MeshComponent->AddImpulse(DropVector, NAME_None, true);
}

