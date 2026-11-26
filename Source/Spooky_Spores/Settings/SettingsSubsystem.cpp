// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/SettingsSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Spooky_Spores.h"

namespace
{
	const TCHAR* const SettingsSlotName = TEXT("Settings");
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

FString USettingsSubsystem::GetSettingsFilePath(const FString& InSlotName)
{
	// Not in Saved/SaveGames (game saves) nor Saved/Config (engine ini files): a machine preference of its own
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Settings"), InSlotName + TEXT(".txt"));
}

bool USettingsSubsystem::SaveSettings()
{
	// One "Id=Value" line per setting, in definition order
	FString Content;
	for (const FSettingDefinition& Definition : Definitions)
	{
		Content += FString::Printf(TEXT("%s=%s
"), *Definition.Id.ToString(), *FString::SanitizeFloat(Values.FindChecked(Definition.Id)));
	}

	const FString FilePath = GetSettingsFilePath(SlotName);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
	if (!FFileHelper::SaveStringToFile(Content, *FilePath))
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("SaveSettings: failed to write '%s'"), *FilePath);
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

	const FString FilePath = GetSettingsFilePath(SlotName);
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogSpooky_Spores, Log, TEXT("No settings file '%s' (first launch): defaults used"), *FilePath);
		return;
	}

	// LoadFileToString converts any byte sequence without asserting (invalid UTF-8 is replaced)
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath) || Content.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("Settings file '%s' is unreadable or empty: defaults used"), *FilePath);
		return;
	}

	// The file can be edited: keys are only compared as strings (never turned into FName), so unknown keys are ignored.
	// A missing key, a non numeric, NaN or infinite value falls back to the default, anything else is bounded.
	TMap<FString, FString> FileEntries;
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FString Key;
		FString Text;
		if (Line.Split(TEXT("="), &Key, &Text))
		{
			FileEntries.Add(Key.TrimStartAndEnd().ToLower(), Text.TrimStartAndEnd());
		}
	}

	for (const FSettingDefinition& Definition : Definitions)
	{
		const FString* Text = FileEntries.Find(Definition.Id.ToString().ToLower());
		float Parsed = 0.0f;
		// LexTryParseString reports failure ("abc"), unlike Atof which gives 0
		if (Text && LexTryParseString(Parsed, **Text) && FMath::IsFinite(Parsed))
		{
			Values[Definition.Id] = FMath::Clamp(Parsed, Definition.Min, Definition.Max);
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
