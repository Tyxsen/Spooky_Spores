// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Servants/ServantTask.h"
#include "ServantTaskMenuWidget.generated.h"

class AServant;
class UServantSubsystem;

/**
 * Logic of the servant task menu (parent of WBP_ServantTaskMenu): selection, assignment, registry subscription.
 * Click to select a servant, then click a category (or the unassigned column) to assign it.
 * The Blueprint child only does the layout and wires the clicks.
 */
UCLASS(Abstract)
class SPOOKY_SPORES_API UServantTaskMenuWidget : public UUserWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Receives the registry (the widget never looks for it). Rebinds if called twice, then requests a refresh.
	UFUNCTION(BlueprintCallable, Category = "Servant|TaskMenu")
	void InitializeMenu(UServantSubsystem* InSubsystem);

	// Replaces the current selection (nullptr clears it).
	UFUNCTION(BlueprintCallable, Category = "Servant|TaskMenu")
	void SelectServant(AServant* Servant);

	// Assigns the selected servant to Task, then clears the selection. False without selection or if nothing changed.
	UFUNCTION(BlueprintCallable, Category = "Servant|TaskMenu")
	bool AssignSelectedServant(EServantTask Task);

	UFUNCTION(BlueprintPure, Category = "Servant|TaskMenu")
	AServant* GetSelectedServant() const;

	UFUNCTION(BlueprintPure, Category = "Servant|TaskMenu")
	UServantSubsystem* GetServantSubsystem() const;

protected:
	virtual void NativeDestruct() override;

	// Rebuild both columns from the registry.
	UFUNCTION(BlueprintImplementableEvent, Category = "Servant|TaskMenu")
	void OnMenuRefreshRequested();

	// Update the highlight. NewSelection is nullptr when the selection is cleared.
	UFUNCTION(BlueprintImplementableEvent, Category = "Servant|TaskMenu")
	void OnSelectionChanged(AServant* NewSelection);

private:
	UFUNCTION()
	void HandleAssignmentsChanged();

	// Fires OnSelectionChanged only if the selection actually changes.
	void SetSelection(AServant* NewSelection);

	void UnbindFromSubsystem();

// -----------PROPERTIES-----------
private:
	UPROPERTY(Transient)
	TObjectPtr<UServantSubsystem> ServantSubsystem = nullptr;

	// Weak: a destroyed servant must never be kept alive by the menu
	TWeakObjectPtr<AServant> SelectedServant;
};
