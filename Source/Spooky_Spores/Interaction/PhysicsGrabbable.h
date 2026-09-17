// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/Grabbable.h"
#include "GameFramework/Actor.h"
#include "PhysicsGrabbable.generated.h"

UCLASS()
class SPOOKY_SPORES_API APhysicsGrabbable : public AActor, public IGrabbable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APhysicsGrabbable();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
	UStaticMeshComponent* MeshComponent;
	
public:
	virtual UPrimitiveComponent* GetGrabbableComponent_Implementation() const override;
};
