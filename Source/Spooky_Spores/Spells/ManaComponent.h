// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ManaComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API UManaComponent : public UActorComponent
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:	
	// Sets default values for this component's properties
	UManaComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Mana")
	bool TrySpendMana(float Cost);

	UFUNCTION(BlueprintPure, Category = "Mana")
	float GetManaRatio() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	
// -----------PROPERTIES-----------
private:
	UPROPERTY(EditAnywhere, Category = "Mana")
	float MaxMana = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Mana")
	float RegenRate = 2.0f; // mana per second - ~50 second for a complete refill

	UPROPERTY(VisibleInstanceOnly, Category = "Mana")
	float CurrentMana = 0.0f;
};
