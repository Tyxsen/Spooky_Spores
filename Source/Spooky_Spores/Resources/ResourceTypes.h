// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ResourceTypes.generated.h"

class AResourceDrop;

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	Seed,
	Wheat,
	Wood,
	Stone
};

USTRUCT(BlueprintType)
struct FResourceDropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<AResourceDrop> DropClass;

	UPROPERTY(EditAnywhere)
	EResourceType Type = EResourceType::Seed;

	UPROPERTY(EditAnywhere)
	int32 Amount = 1;

	UPROPERTY(EditAnywhere)
	int32 SpawnCount = 1;
};
