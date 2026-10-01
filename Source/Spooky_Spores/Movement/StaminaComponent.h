// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSprintStateChanged, bool, bNewIsSprinting);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UStaminaComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void OnRegenDelayElapsed();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	bool TryStartSprint();

	UFUNCTION(BlueprintCallable)
	void StopSprint();
	
	UFUNCTION(BlueprintPure)
	bool IsSprinting() const;

	UFUNCTION(BlueprintPure)
	float GetStaminaRatio() const;

public:
	UPROPERTY(BlueprintAssignable)
	FOnSprintStateChanged OnSprintStateChanged;

private:
	UPROPERTY(EditAnywhere, Category = "Stamina")
	float MaxStamina = 10.0f; // Max sprint duration, in seconds

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float RegenDelay = 4.0f; // Delay without sprinting before regeneration

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float RegenRate = 2.5f; // Stamina regenerated per second

	UPROPERTY(VisibleInstanceOnly, Category = "Stamina")
	float CurrentStamina = 0.0f; // Initialized to MaxStamina in BeginPlay

	bool bIsSprinting = false;
	bool bIsRegenerating = false;
	FTimerHandle RegenDelayTimerHandle;
};
