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

private:	
	void UpdateInteractionTrace();
	
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGrabbableTarget(UPrimitiveComponent*& OutComponent) const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractableTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float GrabRange = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractRange = 200.0f;		
	
private:
	UPROPERTY(BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	float CurrentTargetDistance = TNumericLimits<float>::Max();
	
	// Actual target registered by the line trace
	TWeakObjectPtr<AActor> ActualTarget;
};
