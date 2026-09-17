// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "StoryTrigger.generated.h"

UCLASS()
class SPOOKY_SPORES_API AStoryTrigger : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AStoryTrigger();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "Story")
	void OnStoryTriggered();

public:
	UFUNCTION(BlueprintPure, Category = "Story")
	FText GetStoryText() const { return StoryText; }
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Story")
	UBoxComponent* TriggerVolume;

	UPROPERTY(EditAnywhere, Category = "Story")
	FText StoryText;

	UPROPERTY(EditAnywhere, Category = "Story")
	bool bTriggerOnce = true;

	bool bHasTriggered = false;

};
