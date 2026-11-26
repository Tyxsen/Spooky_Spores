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

	NameText->SetText(RowDefinition.DisplayName);
	ValueSlider->SetMinValue(RowDefinition.Min);
	ValueSlider->SetMaxValue(RowDefinition.Max);
	ValueSlider->SetStepSize(RowDefinition.Step);
}

void USettingFloatRowWidget::RefreshValue(float NewValue)
{
	// May re-fire OnValueChanged (USlider without Slate widget): harmless, SetFloat ignores an unchanged value
	ValueSlider->SetValue(NewValue);
	ValueText->SetText(FText::AsNumber(NewValue, &FNumberFormattingOptions::DefaultNoGrouping()));
}

void USettingFloatRowWidget::HandleSliderValueChanged(float NewValue)
{
	if (USettingsSubsystem* CurrentSubsystem = GetSettingsSubsystem())
	{
		CurrentSubsystem->SetFloat(GetDefinition().Id, NewValue);
	}
}

void USettingFloatRowWidget::HandleSliderReleased()
{
	if (USettingsSubsystem* CurrentSubsystem = GetSettingsSubsystem())
	{
		CurrentSubsystem->SaveSettings();
	}
}
