// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/SettingRowWidget.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Settings/SettingsSubsystem.h"

void USettingRowWidget::SetupRow(USettingsSubsystem* InSubsystem, const FSettingDefinition& InDefinition)
{
	ReleaseRow();

	Subsystem = InSubsystem;
	Definition = InDefinition;

	if (!InSubsystem) return;

	InSubsystem->OnSettingChanged.AddUniqueDynamic(this, &USettingRowWidget::HandleSettingChanged);

	ConfigureRow();
	RefreshValue(InSubsystem->GetFloat(Definition.Id));
}

void USettingRowWidget::ReleaseRow()
{
	// The subsystem may already be destroyed: nothing left to unbind from
	if (USettingsSubsystem* CurrentSubsystem = Subsystem.Get())
	{
		CurrentSubsystem->OnSettingChanged.RemoveDynamic(this, &USettingRowWidget::HandleSettingChanged);
	}
}

void USettingRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// First construction usually happens before SetupRow: nothing to rebind yet
	if (USettingsSubsystem* CurrentSubsystem = Subsystem.Get())
	{
		CurrentSubsystem->OnSettingChanged.AddUniqueDynamic(this, &USettingRowWidget::HandleSettingChanged);
		RefreshValue(CurrentSubsystem->GetFloat(Definition.Id));
	}
}

void USettingRowWidget::NativeDestruct()
{
	ReleaseRow();

	Super::NativeDestruct();
}

void USettingRowWidget::HandleSettingChanged(FName Id, float Value)
{
	// The subsystem broadcasts every setting: only react to ours
	if (Id == Definition.Id)
	{
		RefreshValue(Value);
	}
}

void USettingFloatRowWidget::ConfigureRow()
{
	const FSettingDefinition& RowDefinition = GetDefinition();

	// AddUnique: ConfigureRow runs at every SetupRow. The slider lives and dies with the row, no unbind needed.
	ValueSlider->OnValueChanged.AddUniqueDynamic(this, &USettingFloatRowWidget::HandleSliderValueChanged);
	ValueSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &USettingFloatRowWidget::HandleSliderReleased);
	ValueSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &USettingFloatRowWidget::HandleSliderReleased);

	NameText->SetText(RowDefinition.DisplayName);
	ValueSlider->SetMinValue(RowDefinition.Min);
	ValueSlider->SetMaxValue(RowDefinition.Max);
	ValueSlider->SetStepSize(RowDefinition.Step);
}

void USettingFloatRowWidget::RefreshValue(float NewValue)
{
	// May re-fire OnValueChanged (USlider without Slate widget): harmless, SetFloat ignores an unchanged value
	ValueSlider->SetValue(NewValue);

	// A whole step shows no decimals ("90", never "90.0" or "90.37")
	const float Step = GetDefinition().Step;
	FNumberFormattingOptions Options = FNumberFormattingOptions::DefaultNoGrouping();
	if (Step >= 1.0f && FMath::IsNearlyEqual(Step, FMath::RoundToFloat(Step)))
	{
		Options.MaximumFractionalDigits = 0;
	}
	ValueText->SetText(FText::AsNumber(NewValue, &Options));
}

float USettingFloatRowWidget::SnapToStep(float RawValue) const
{
	const FSettingDefinition& RowDefinition = GetDefinition();

	// Continuous setting: nothing to snap
	if (RowDefinition.Step <= 0.0f) return RawValue;

	// Steps are counted from Min. The rounding can overshoot Max by an epsilon (or a step that does not divide the range).
	const float Snapped = RowDefinition.Min + FMath::RoundToFloat((RawValue - RowDefinition.Min) / RowDefinition.Step) * RowDefinition.Step;
	return FMath::Clamp(Snapped, RowDefinition.Min, RowDefinition.Max);
}

void USettingFloatRowWidget::HandleSliderValueChanged(float NewValue)
{
	USettingsSubsystem* CurrentSubsystem = GetSettingsSubsystem();
	if (!CurrentSubsystem) return;

	// The mouse ignores the slider step (SSlider MouseUsesStep is false): snap here, before storing
	const float SnappedValue = SnapToStep(NewValue);
	CurrentSubsystem->SetFloat(GetDefinition().Id, SnappedValue);

	// Put the handle on the notch even when the stored value did not change (no broadcast, no refresh).
	// Terminates: SetValue only re-fires when the value differs, and snapping an already snapped value is idempotent.
	if (SnappedValue != NewValue)
	{
		ValueSlider->SetValue(SnappedValue);
	}
}

void USettingFloatRowWidget::HandleSliderReleased()
{
	if (USettingsSubsystem* CurrentSubsystem = GetSettingsSubsystem())
	{
		CurrentSubsystem->SaveSettings();
	}
}
