// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Settings/SettingsCatalog.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SettingsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpookySettingChanged, FName, Id, float, Value);

/**
 * Stores the player settings (all as floats, bounded by their definition), broadcasts changes and persists them
 * in the "Settings" save slot. Lives as long as the process: survives levels and pawns.
 * Consumers read GetFloat at startup and listen to OnSettingChanged.
 */
UCLASS()
class SPOOKY_SPORES_API USettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Writes the settings if they changed since the last save (safety net)
	virtual void Deinitialize() override;

	// Test seam, called by Initialize with the catalog: resets the state, keeps the valid definitions, loads the slot.
	void InitializeFromDefinitions(const TArray<FSettingDefinition>& InDefinitions, const FString& InSlotName);

	// Current value, 0 (and a warning) for an unknown Id
	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetFloat(FName Id) const;

	// Clamps to [Min, Max], stores and broadcasts if the value really changed. No disk write.
	// False only for an unknown Id.
	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool SetFloat(FName Id, float Value);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ResetToDefault(FName Id);

	// Writes all values to the slot. True if written.
	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool SaveSettings();

	// The valid definitions only
	UFUNCTION(BlueprintPure, Category = "Settings")
	TArray<FSettingDefinition> GetDefinitions() const;

private:

	void LoadSettings();

	const FSettingDefinition* FindDefinition(FName Id) const;

// -----------PROPERTIES-----------
public:

	// Fired at every real change (never when the bounded value equals the current one)
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSpookySettingChanged OnSettingChanged;

private:

	// Valid definitions only
	UPROPERTY()
	TArray<FSettingDefinition> Definitions;

	UPROPERTY()
	TMap<FName, float> Values;

	FString SlotName;

	// True when a value changed since the last load or save
	bool bIsDirty = false;
};
