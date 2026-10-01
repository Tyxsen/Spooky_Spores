// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/StaminaComponent.h"

// Sets default values for this component's properties
UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentStamina = MaxStamina;
}

void UStaminaComponent::OnRegenDelayElapsed()
{
	bIsRegenerating = true;
}


// Called every frame
void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsSprinting)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina - DeltaTime, 0.0f, MaxStamina);
		if (CurrentStamina <= 0) StopSprint();
	}

	if (bIsRegenerating)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina + RegenRate * DeltaTime, 0.0f, MaxStamina);
		if (CurrentStamina >= MaxStamina) bIsRegenerating = false;
	}
}

bool UStaminaComponent::TryStartSprint()
{
	if (CurrentStamina <= 0.0f) return false;

	GetWorld()->GetTimerManager().ClearTimer(RegenDelayTimerHandle);
	bIsSprinting = true;
	bIsRegenerating = false;
	OnSprintStateChanged.Broadcast(true);
	
	return true;
}

void UStaminaComponent::StopSprint()
{
	if (!bIsSprinting) return;
	
	bIsSprinting = false;
	GetWorld()->GetTimerManager().SetTimer(RegenDelayTimerHandle, this, &UStaminaComponent::OnRegenDelayElapsed, RegenDelay, false);
	OnSprintStateChanged.Broadcast(false);
}

bool UStaminaComponent::IsSprinting() const
{
	return bIsSprinting;
}

float UStaminaComponent::GetStaminaRatio() const
{
	return CurrentStamina / MaxStamina;
}
