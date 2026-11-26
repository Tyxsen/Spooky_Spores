// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/SettingsMenuWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Settings/SettingRowWidget.h"
#include "Settings/SettingsSubsystem.h"
#include "Spooky_Spores.h"

void USettingsMenuWidget::InitializeForPlayer_Implementation(APlayerController* Controller, APawn*)
{
	// The menu only needs the settings, not the pawn
	const UGameInstance* GameInstance = Controller ? Controller->GetGameInstance() : nullptr;
	USettingsSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<USettingsSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' InitializeForPlayer: no settings subsystem"), *GetNameSafe(this));
		ClearRows();
		return;
	}

	RebuildRows(Subsystem);
}

void USettingsMenuWidget::RebuildRows(USettingsSubsystem* Subsystem)
{
	ClearRows();

	for (const FSettingDefinition& Definition : Subsystem->GetDefinitions())
	{
		const TSubclassOf<USettingRowWidget>* RowClass = RowClasses.Find(Definition.Type);
		if (!RowClass || !*RowClass)
		{
			UE_LOG(LogSpooky_Spores, Error, TEXT("'%s' has no row class for the type of setting '%s': row skipped"),
				*GetNameSafe(this), *Definition.Id.ToString());
			continue;
		}

		USettingRowWidget* Row = CreateWidget<USettingRowWidget>(this, *RowClass);
		if (!Row) continue;

		Row->SetupRow(Subsystem, Definition);
		RowsContainer->AddChild(Row);
	}
}

void USettingsMenuWidget::ClearRows()
{
	// Rows not yet constructed (menu not on screen) never get NativeDestruct: release them explicitly
	for (UWidget* Child : RowsContainer->GetAllChildren())
	{
		if (USettingRowWidget* Row = Cast<USettingRowWidget>(Child))
		{
			Row->ReleaseRow();
		}
	}
	RowsContainer->ClearChildren();
}
