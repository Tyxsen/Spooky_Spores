// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/UIWidgetEntry.h"
#include "Spooky_SporesPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIOpenChanged, FName, Id, bool, bIsOpen);

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 *  Owns and drives the UI widgets declared in UIEntries (it survives pawn changes).
 */
UCLASS(abstract, config="Game")
class SPOOKY_SPORES_API ASpooky_SporesPlayerController : public APlayerController
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:

	/** Constructor */
	ASpooky_SporesPlayerController();

	// Opens the UI (Visible). True if the state changed; false if already open or unknown Id (warning).
	UFUNCTION(BlueprintCallable, Category = "UI")
	bool OpenUI(FName Id);

	// Closes the UI (Collapsed). True if the state changed; false if already closed or unknown Id (warning).
	UFUNCTION(BlueprintCallable, Category = "UI")
	bool CloseUI(FName Id);

	// True if the state changed (false only for an unknown Id).
	UFUNCTION(BlueprintCallable, Category = "UI")
	bool ToggleUI(FName Id);

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsUIOpen(FName Id) const;

	// Nullptr for an unknown Id (warning).
	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetUIWidget(FName Id) const;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Notifies the widgets of the new pawn */
	virtual void OnPossess(APawn* InPawn) override;

	/** Notifies the widgets that there is no pawn anymore */
	virtual void OnUnPossess() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	// Creates the widget of an entry and adds it to the player screen. Test seam.
	virtual UUserWidget* CreateUIWidget(const FUIWidgetEntry& Entry);

	// Applies the engine side of the modal state (pause, cursor, input mode). Only called when the state changes. Test seam.
	virtual void ApplyModalState(bool bModal);

private:

	void CreateUIWidgets();

	// Calls IPlayerUIWidget::InitializeForPlayer on the widget if it implements the interface
	void InitializeWidgetForPlayer(UUserWidget* Widget, APawn* InPawn);

	void InitializeWidgetsForPawn(APawn* InPawn);

	// Single path of OpenUI / CloseUI / ToggleUI: visibility, modal state, event
	bool SetUIOpen(FName Id, bool bOpen);

	bool HasOpenPausingUI() const;

	// Handler bound to ToggleAction (void: payload delegate signature)
	void HandleToggleInput(FName Id);

	// Entries that are usable (non empty Id, WidgetClass set, first of their Id). Logs the ignored ones if asked.
	void GetValidEntries(TArray<const FUIWidgetEntry*>& OutEntries, bool bLogIgnored) const;

// -----------PROPERTIES-----------
public:

	// Fired at every real open/close change
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnUIOpenChanged OnUIOpenChanged;

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	// Widgets created at BeginPlay (local controllers only). Filled in the child Blueprint.
	UPROPERTY(EditAnywhere, Category = "UI")
	TArray<FUIWidgetEntry> UIEntries;

private:

	// Created widgets by Id (kept alive here, created once)
	UPROPERTY()
	TMap<FName, TObjectPtr<UUserWidget>> UIWidgets;

	// Ids of the widgets currently open
	TSet<FName> OpenUIIds;

	// Ids of the created widgets whose entry has bPausesGame
	TSet<FName> PausingUIIds;
};
