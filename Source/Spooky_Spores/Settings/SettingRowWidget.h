// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SettingsCatalog.h"
#include "SettingRowWidget.generated.h"

class USettingsSubsystem;
class USlider;
class UTextBlock;

/**
 * One line of the settings menu: edits one setting of the subsystem.
 * Base class: handles the subsystem subscription and keeps the row in sync with the stored value.
 * A presentation (slider, toggle...) is a child class that fills ConfigureRow and RefreshValue.
 */
UCLASS(Abstract)
class SPOOKY_SPORES_API USettingRowWidget : public UUserWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:

	// Receives its dependencies. Idempotent: rebinds if called twice.
	void SetupRow(USettingsSubsystem* InSubsystem, const FSettingDefinition& InDefinition);

	// Stops listening to the subsystem (also done by NativeDestruct). Called by the menu before dropping a row.
	void ReleaseRow();

protected:

	// Rebinds after RemoveFromParent + AddToViewport (NativeDestruct unbinds)
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Called once the subsystem and the definition are stored, before the first RefreshValue
	virtual void ConfigureRow() {}

	// Shows NewValue (the stored value, already bounded). Must not write back to the subsystem.
	virtual void RefreshValue(float NewValue) {}

	const FSettingDefinition& GetDefinition() const { return Definition; }

	USettingsSubsystem* GetSettingsSubsystem() const { return Subsystem.Get(); }

private:

	UFUNCTION()
	void HandleSettingChanged(FName Id, float Value);

// -----------PROPERTIES-----------
private:

	// Weak: the row must not keep the subsystem alive
	TWeakObjectPtr<USettingsSubsystem> Subsystem;

	FSettingDefinition Definition;
};

/**
 * Float presentation: a slider bounded by the definition, the name and the current value.
 * The Blueprint child (WBP_SettingRow_Float) only does the layout, widgets named NameText, ValueText, ValueSlider.
 */
UCLASS(Abstract)
class SPOOKY_SPORES_API USettingFloatRowWidget : public USettingRowWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
protected:

	virtual void ConfigureRow() override;
	virtual void RefreshValue(float NewValue) override;

private:

	// Rounds to the declared Step (counted from Min), bounded to [Min, Max]. Unchanged when Step <= 0.
	float SnapToStep(float RawValue) const;

	// Live change while dragging: stores the value (no disk write)
	UFUNCTION()
	void HandleSliderValueChanged(float NewValue);

	// Slider released (mouse or controller/keyboard): persists
	UFUNCTION()
	void HandleSliderReleased();

// -----------PROPERTIES-----------
private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> ValueSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;
};
