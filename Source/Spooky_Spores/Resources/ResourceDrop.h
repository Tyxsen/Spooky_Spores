// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Resources/ResourceTypes.h"
#include "ResourceDrop.generated.h"

UCLASS()
class SPOOKY_SPORES_API AResourceDrop : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AResourceDrop();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnMeshOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:	
	UFUNCTION(BlueprintCallable, Category = "Resources")
	void InitializeDrop(EResourceType Type, int32 InAmount);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere, Category = "Resources|Pop")
	float PopStrength = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Resources|Pop")
	float PopConeHalfAngle = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Resources|Pop")
	float MinGroundNormalZ = 0.7f;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resources", meta = (AllowPrivateAccess = "true"))
	EResourceType DropType = EResourceType::Seed;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resources", meta = (AllowPrivateAccess = "true"))
	int32 Amount = 1;
};
