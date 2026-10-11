// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpellCastingComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SPOOKY_SPORES_API USpellCastingComponent : public UActorComponent
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:	
	// Sets default values for this component's properties
	USpellCastingComponent();

	UFUNCTION(BlueprintCallable, Category = "Spell")
	bool TryCastSpell();

private:
	UFUNCTION()
	void ResolveHit();

	UFUNCTION()
	void EndSwing();

	
// -----------PROPERTIES-----------
private:
	UPROPERTY(EditAnywhere, Category = "Spell")
	float SpellDamage = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Spell")
	float SpellManaCost = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Spell")
	float HitDelay = 0.2f; // Needs to be set when there'll be an anim

	UPROPERTY(EditAnywhere, Category = "Spell")
	float SwingCooldown = 0.5f; // Time between two hit

	bool bIsSwinging = false;
	FTimerHandle HitDelayTimerHandle;
	FTimerHandle SwingCooldownTimerHandle;
};
