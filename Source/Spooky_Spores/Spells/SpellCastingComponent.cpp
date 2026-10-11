// Fill out your copyright notice in the Description page of Project Settings.


#include "Spells/SpellCastingComponent.h"
#include "Spells/ManaComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
USpellCastingComponent::USpellCastingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USpellCastingComponent::TryCastSpell()
{
	if (bIsSwinging) return false;
	
	UManaComponent* Mana = GetOwner()->FindComponentByClass<UManaComponent>();
	if (!Mana) return false;
	if (!Mana->TrySpendMana(SpellManaCost)) return false;

	bIsSwinging = true;
	GetWorld()->GetTimerManager().SetTimer(HitDelayTimerHandle, this, &USpellCastingComponent::ResolveHit, HitDelay, false);
	GetWorld()->GetTimerManager().SetTimer(SwingCooldownTimerHandle, this, &USpellCastingComponent::EndSwing, SwingCooldown, false);
	return true;
}

void USpellCastingComponent::ResolveHit()
{
	UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>();
	if (!Interaction) return;
	
	AActor* Target = nullptr;
	if (Interaction->IsAttackableTarget(Target))
	{
		UGameplayStatics::ApplyDamage(Target, SpellDamage, GetOwner()->GetInstigatorController(),
			GetOwner(), UDamageType::StaticClass());
		AController* InstigatorController = GetOwner()->GetInstigatorController();
		if (InstigatorController)
		{
			UE_LOG(LogTemp, Log, TEXT("Instigator controller: %s"), *InstigatorController->GetFName().ToString());
		}
		else
			UE_LOG(LogTemp, Log, TEXT("Instigator controller is null"));
	}
}

void USpellCastingComponent::EndSwing()
{
	bIsSwinging = false;
}
