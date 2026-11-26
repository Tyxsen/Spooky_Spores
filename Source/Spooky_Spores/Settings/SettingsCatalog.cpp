// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/SettingsCatalog.h"

namespace SettingsCatalog
{
	const FName FOV(TEXT("FOV"));

	const TArray<FSettingDefinition>& GetDefinitions()
	{
		static const TArray<FSettingDefinition> Definitions = []()
		{
			TArray<FSettingDefinition> Result;

			// Field of view of the first person camera (placeholder bounds: the GDD does not set any)
			FSettingDefinition FovDefinition;
			FovDefinition.Id = FOV;
			FovDefinition.DisplayName = NSLOCTEXT("Settings", "FOV", "Field of View");
			FovDefinition.Type = ESettingType::Float;
			FovDefinition.Min = 60.0f;
			FovDefinition.Max = 120.0f;
			FovDefinition.Default = 90.0f;
			FovDefinition.Step = 1.0f;
			Result.Add(FovDefinition);

			return Result;
		}();

		return Definitions;
	}
}
