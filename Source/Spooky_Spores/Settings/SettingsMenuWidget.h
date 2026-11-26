// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SettingsCatalog.h"
#include "UI/PlayerUIWidget.h"
#include "SettingsMenuWidget.generated.h"

class UPanelWidget;
class USettingRowWidget;
class USettingsSubsystem;

/**
 * Settings menu (parent of WBP_SettingsMenu): one row per valid definition of the settings subsystem.
 * Generic: a new setting appears here without any change. The Blueprint child only provides RowsContainer
 * and the row class of each setting type.
 */
UCLASS(Abstract)
class SPOOKY_SPORES_API USettingsMenuWidget : public UUserWidget, public IPlayerUIWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:

	// IPlayerUIWidget: gets the subsystem from the controller's game instance, then rebuilds the rows (idempotent)
	virtual void InitializeForPlayer_Implementation(APlayerController* Controller, APawn* Pawn) override;

private:

	// Empties RowsContainer and recreates one row per definition. A type without row class skips only its row.
	void RebuildRows(USettingsSubsystem* Subsystem);

	void ClearRows();

// -----------PROPERTIES-----------
protected:

	// Row widget class used for each setting type
	UPROPERTY(EditAnywhere, Category = "Settings")
	TMap<ESettingType, TSubclassOf<USettingRowWidget>> RowClasses;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowsContainer;
};
