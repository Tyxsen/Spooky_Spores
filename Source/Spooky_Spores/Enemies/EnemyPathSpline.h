// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyPathSpline.generated.h"

class USplineComponent;

UCLASS()
class SPOOKY_SPORES_API AEnemyPathSpline : public AActor
{
	GENERATED_BODY()
	
public:
	AEnemyPathSpline();

	UFUNCTION(BlueprintPure, Category = "Path")
	USplineComponent* GetSplineComponent() const { return PathSplineComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Path", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> PathSplineComponent;
};
