// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SettingsCatalog.generated.h"

// How a setting is presented in the menu. Every value is stored as a float whatever the type.
// Append new values at the end only (a future Toggle = 0/1, Choice = index): one value + one row class.
UENUM(BlueprintType)
enum class ESettingType : uint8
{
	Float = 0
};

USTRUCT(BlueprintType)
struct SPOOKY_SPORES_API FSettingDefinition
{
	GENERATED_BODY()

// -----------PROPERTIES-----------
public:

	// Unique key of the setting (also the key in the save file)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ESettingType Type = ESettingType::Float;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float Max = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float Default = 0.0f;

	// Slider granularity. 0 = continuous.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float Step = 0.0f;
};

// The only place where a setting is declared. Adding a setting = one entry in GetDefinitions()
// + one line in the system that consumes it.
namespace SettingsCatalog
{
	SPOOKY_SPORES_API extern const FName FOV;

	SPOOKY_SPORES_API const TArray<FSettingDefinition>& GetDefinitions();
}
