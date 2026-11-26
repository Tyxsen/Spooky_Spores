// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/SettingsSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Settings/SettingsSaveGame.h"
#include "Spooky_Spores.h"

namespace
{
	const TCHAR* const SettingsSlotName = TEXT("Settings");

	// Single local player: the slot is not per user
	constexpr int32 SettingsUserIndex = 0;
}

void USettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InitializeFromDefinitions(SettingsCatalog::GetDefinitions(), SettingsSlotName);
}

void USettingsSubsystem::Deinitialize()
{
	if (bIsDirty)
	{
		SaveSettings();
	}

	Super::Deinitialize();
}

void USettingsSubsystem::InitializeFromDefinitions(const TArray<FSettingDefinition>& InDefinitions, const FString& InSlotName)
{
	Definitions.Reset();
	Values.Reset();
	SlotName = InSlotName;
	bIsDirty = false;

	for (const FSettingDefinition& Definition : InDefinitions)
	{
		if (Definition.Id.IsNone())
		{
			UE_LOG(LogSpooky_Spores, Error, TEXT("Setting definition ignored: empty Id"));
			continue;
		}
		if (FindDefinition(Definition.Id))
		{
			UE_LOG(LogSpooky_Spores, Error, TEXT("Setting definition '%s' ignored: duplicate Id"), *Definition.Id.ToString());
			continue;
		}
		if (Definition.Min >= Definition.Max)
		{
			UE_LOG(LogSpooky_Spores, Error, TEXT("Setting definition '%s' ignored: Min must be lower than Max"), *Definition.Id.ToString());
			continue;
		}
		if (Definition.Default < Definition.Min || Definition.Default > Definition.Max)
		{
			UE_LOG(LogSpooky_Spores, Error, TEXT("Setting definition '%s' ignored: Default is out of bounds"), *Definition.Id.ToString());
			continue;
		}

		Definitions.Add(Definition);
	}

	LoadSettings();
}

float USettingsSubsystem::GetFloat(FName Id) const
{
	const float* Value = Values.Find(Id);
	if (!Value)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("GetFloat: unknown setting '%s'"), *Id.ToString());
		return 0.0f;
	}
	return *Value;
}

bool USettingsSubsystem::SetFloat(FName Id, float Value)
{
	const FSettingDefinition* Definition = FindDefinition(Id);
	if (!Definition)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("SetFloat: unknown setting '%s'"), *Id.ToString());
		return false;
	}

	const float BoundedValue = FMath::Clamp(Value, Definition->Min, Definition->Max);

	// Every valid Id has a value (filled by LoadSettings)
	float& CurrentValue = Values.FindChecked(Id);
	if (CurrentValue != BoundedValue)
	{
		CurrentValue = BoundedValue;
		bIsDirty = true;
		OnSettingChanged.Broadcast(Id, BoundedValue);
	}
	return true;
}

void USettingsSubsystem::ResetToDefault(FName Id)
{
	if (const FSettingDefinition* Definition = FindDefinition(Id))
	{
		SetFloat(Id, Definition->Default);
	}
	else
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("ResetToDefault: unknown setting '%s'"), *Id.ToString());
	}
}

bool USettingsSubsystem::SaveSettings()
{
	USettingsSaveGame* SaveGame = Cast<USettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingsSaveGame::StaticClass()));
	if (!SaveGame)
	{
		UE_LOG(LogSpooky_Spores, Error, TEXT("SaveSettings: cannot create the save object"));
		return false;
	}

	SaveGame->Values = Values;
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, SettingsUserIndex))
	{
		UE_LOG(LogSpooky_Spores, Error, TEXT("SaveSettings: failed to write slot '%s'"), *SlotName);
		return false;
	}

	bIsDirty = false;
	return true;
}

TArray<FSettingDefinition> USettingsSubsystem::GetDefinitions() const
{
	return Definitions;
}

void USettingsSubsystem::LoadSettings()
{
	// Start from the defaults: a missing or unreadable slot leaves them
	for (const FSettingDefinition& Definition : Definitions)
	{
		Values.Add(Definition.Id, Definition.Default);
	}

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SettingsUserIndex))
	{
		UE_LOG(LogSpooky_Spores, Log, TEXT("No settings slot '%s' (first launch): defaults used"), *SlotName);
		return;
	}

	const USettingsSaveGame* SaveGame = Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, SettingsUserIndex));
	if (!SaveGame)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("Settings slot '%s' is unreadable: defaults used"), *SlotName);
		return;
	}

	// The file can be edited: absent, NaN or infinite falls back to the default, anything else is bounded.
	// Keys unknown to the definitions are never looked at.
	for (const FSettingDefinition& Definition : Definitions)
	{
		const float* Stored = SaveGame->Values.Find(Definition.Id);
		if (Stored && FMath::IsFinite(*Stored))
		{
			Values[Definition.Id] = FMath::Clamp(*Stored, Definition.Min, Definition.Max);
		}
	}
}

const FSettingDefinition* USettingsSubsystem::FindDefinition(FName Id) const
{
	return Definitions.FindByPredicate([Id](const FSettingDefinition& Definition)
	{
		return Definition.Id == Id;
	});
}
