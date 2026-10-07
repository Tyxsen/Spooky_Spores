// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Resources/ResourceTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Cauldron.generated.h"

UENUM()
enum class ECauldronState : uint8
{
	Idle,
	Brewing,
	Ready
};

UCLASS()
class SPOOKY_SPORES_API ACauldron : public AActor, public IInteractable
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:	
	// Sets default values for this actor's properties
	ACauldron();
	
	virtual void OnInteract_Implementation(AActor* InteractingActor) override;

	UFUNCTION(BlueprintPure, Category = "Cauldron")
	float GetBrewProgress() const;

private:
	void OnBrewComplete();

// -----------PROPERTIES-----------
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Cauldron")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Cauldron")
	ECauldronState CurrentState = ECauldronState::Idle;

	UPROPERTY(EditAnywhere, Category = "Cauldron")
	TMap<EResourceType, int32> RequiredIngredients;

	UPROPERTY(EditAnywhere, Category = "Cauldron")
	EResourceType OutputPotionType = EResourceType::Potion;
	
	UPROPERTY(EditAnywhere, Category = "Cauldron")
	int32 OutputAmount = 1;

	UPROPERTY(EditAnywhere, Category = "Cauldron")
	float BrewDuration = 30.0f;

	float BrewStartTime = 0.0f;
	FTimerHandle BrewTimerHandle;
};
