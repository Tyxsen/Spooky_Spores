// Fill out your copyright notice in the Description page of Project Settings.


#include "Narrative/StoryTrigger.h"

// Sets default values
AStoryTrigger::AStoryTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(FName("TriggerVolume"));
	RootComponent = TriggerVolume;
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
}

// Called when the game starts or when spawned
void AStoryTrigger::BeginPlay()
{
	Super::BeginPlay();
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AStoryTrigger::OnBoxOverlap);
}

void AStoryTrigger::OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
								UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
								bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasTriggered) return;
	if (bTriggerOnce) return;
	
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;
	
	bHasTriggered = true;
	OnStoryTriggered();
}
