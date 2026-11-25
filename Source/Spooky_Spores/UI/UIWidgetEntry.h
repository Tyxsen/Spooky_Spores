// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UIWidgetEntry.generated.h"

class UInputAction;
class UUserWidget;

// One widget owned by the PlayerController. Adding a UI to the game = adding an entry to ASpooky_SporesPlayerController::UIEntries.
USTRUCT(BlueprintType)
struct SPOOKY_SPORES_API FUIWidgetEntry
{
	GENERATED_BODY()

	// Unique key used by OpenUI / CloseUI / ToggleUI / IsUIOpen / GetUIWidget. Empty or duplicated: entry ignored (error log).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> WidgetClass = nullptr;

	// Layer on screen (higher = in front). The only layering mechanism.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	int32 ZOrder = 0;

	// Opened (Visible) as soon as the controller begins play, instead of Collapsed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool bOpenAtStart = false;

	// While opened, the game is paused and the cursor is shown. The Input Action should have bTriggerWhenPaused.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool bPausesGame = false;

	// Optional: Started on this action toggles the entry
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TObjectPtr<UInputAction> ToggleAction = nullptr;
};
