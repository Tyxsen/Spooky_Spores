// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GrabComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API UGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGrabComponent();

protected:	
	UPROPERTY(VisibleAnywhere)
	UPhysicsHandleComponent* PhysicsHandle;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintPure, Category = "Grab")
	bool IsHolding() const;

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void Grab(UPrimitiveComponent* ComponentToGrab);

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void Release();

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void Launch();

	UPROPERTY(EditAnywhere, Category = "Grab")
	float HoldDistance = 200.0f; // Where the component is floating in front of the camera in cm

	UPROPERTY(EditAnywhere, Category = "Grab")
	float LaunchImpulseStrength = 1000.0f;
};
