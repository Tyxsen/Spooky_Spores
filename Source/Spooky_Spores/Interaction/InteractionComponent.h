// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class UPrimitiveComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractionComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGrabbableTarget(UPrimitiveComponent*& OutComponent) const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractableTarget() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsAttackableTarget(AActor*& OutTarget) const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	// On-demand trace against the world (not only interactables): returns a ground point in front of the player.
	// Fails if nothing is hit within PlacementRange or if the hit surface is too steep to stand on.
	UFUNCTION(BlueprintCallable, Category = "Interaction|Placement")
	bool GetPlacementPoint(FVector& OutLocation) const;

private:
	void UpdateInteractionTrace();
	
// -----------PROPERTIES-----------
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float GrabRange = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float AttackRange = 200.0f;
	
private:
	UPROPERTY(BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	float CurrentTargetDistance = TNumericLimits<float>::Max();
	
	// Actual target registered by the line trace
	TWeakObjectPtr<AActor> ActualTarget;

	UPROPERTY(EditAnywhere, Category = "Interaction|Placement")
	float PlacementRange = 300.0f;

	// Minimum Z of the hit normal to accept a placement (0.7 is about 45 degrees of slope, same rule as AResourceDrop)
	UPROPERTY(EditAnywhere, Category = "Interaction|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinPlacementNormalZ = 0.7f;
};
