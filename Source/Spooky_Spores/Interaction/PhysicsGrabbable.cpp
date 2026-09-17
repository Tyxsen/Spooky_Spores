// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/PhysicsGrabbable.h"

// Sets default values
APhysicsGrabbable::APhysicsGrabbable()
{
	PrimaryActorTick.bCanEverTick = false;
	
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComponent");
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsInteractable"));
	MeshComponent->SetSimulatePhysics(true);
}

UPrimitiveComponent* APhysicsGrabbable::GetGrabbableComponent_Implementation() const
{
	return MeshComponent;
}


