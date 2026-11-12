// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/ServantTaskMenuWidget.h"

#include "Servants/Servant.h"
#include "Servants/ServantSubsystem.h"
#include "Spooky_Spores.h"

void UServantTaskMenuWidget::InitializeMenu(UServantSubsystem* InSubsystem)
{
	UnbindFromSubsystem();
	ServantSubsystem = InSubsystem;

	if (!ServantSubsystem)
	{
		// No refresh: the Blueprint would read an empty registry reference
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' InitializeMenu called without a servant subsystem"), *GetNameSafe(this));
		SetSelection(nullptr);
		return;
	}

	ServantSubsystem->OnAssignmentsChanged.AddDynamic(this, &UServantTaskMenuWidget::HandleAssignmentsChanged);

	// Same path as a broadcast: drops a selection unknown to the new registry, then rebuilds
	HandleAssignmentsChanged();
}

void UServantTaskMenuWidget::SelectServant(AServant* Servant)
{
	SetSelection(Servant);
}

bool UServantTaskMenuWidget::AssignSelectedServant(EServantTask Task)
{
	AServant* Selected = SelectedServant.Get();
	if (!Selected) return false;

	// Broadcast (and so the refresh) happens inside AssignTask, before the selection is cleared
	const bool bHasChanged = ServantSubsystem && ServantSubsystem->AssignTask(Selected, Task);
	SetSelection(nullptr);
	return bHasChanged;
}

AServant* UServantTaskMenuWidget::GetSelectedServant() const
{
	return SelectedServant.Get();
}

UServantSubsystem* UServantTaskMenuWidget::GetServantSubsystem() const
{
	return ServantSubsystem;
}

void UServantTaskMenuWidget::NativeDestruct()
{
	UnbindFromSubsystem();

	Super::NativeDestruct();
}

void UServantTaskMenuWidget::HandleAssignmentsChanged()
{
	// A servant removed from the registry (destroyed) cannot stay selected
	AServant* Selected = SelectedServant.Get();
	if (Selected && (!ServantSubsystem || !ServantSubsystem->IsServantRegistered(Selected)))
	{
		SetSelection(nullptr);
	}

	OnMenuRefreshRequested();
}

void UServantTaskMenuWidget::SetSelection(AServant* NewSelection)
{
	if (SelectedServant.Get() == NewSelection) return;

	SelectedServant = NewSelection;
	OnSelectionChanged(NewSelection);
}

void UServantTaskMenuWidget::UnbindFromSubsystem()
{
	if (ServantSubsystem)
	{
		ServantSubsystem->OnAssignmentsChanged.RemoveDynamic(this, &UServantTaskMenuWidget::HandleAssignmentsChanged);
	}
}
