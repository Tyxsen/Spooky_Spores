// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/ServantTaskMenuWidget.h"

#include "Engine/World.h"
#include "Servants/Servant.h"
#include "Servants/ServantSubsystem.h"
#include "Spooky_Spores.h"

void UServantTaskMenuWidget::InitializeMenu(UServantSubsystem* InSubsystem)
{
	UnbindFromSubsystem();
	ServantSubsystem = InSubsystem;

	if (!InSubsystem)
	{
		// No refresh: the Blueprint would read an empty registry reference
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' InitializeMenu called without a servant subsystem"), *GetNameSafe(this));
		SetSelection(nullptr);
		return;
	}

	InSubsystem->OnAssignmentsChanged.AddUniqueDynamic(this, &UServantTaskMenuWidget::HandleAssignmentsChanged);

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
	UServantSubsystem* Subsystem = ServantSubsystem.Get();
	const bool bHasChanged = Subsystem && Subsystem->AssignTask(Selected, Task);
	SetSelection(nullptr);
	return bHasChanged;
}

AServant* UServantTaskMenuWidget::GetSelectedServant() const
{
	return SelectedServant.Get();
}

UServantSubsystem* UServantTaskMenuWidget::GetServantSubsystem() const
{
	return ServantSubsystem.Get();
}

void UServantTaskMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// First construction usually happens before InitializeMenu: nothing to rebind yet
	if (UServantSubsystem* Subsystem = ServantSubsystem.Get())
	{
		Subsystem->OnAssignmentsChanged.AddUniqueDynamic(this, &UServantTaskMenuWidget::HandleAssignmentsChanged);
		HandleAssignmentsChanged();
	}
}

void UServantTaskMenuWidget::NativeDestruct()
{
	UnbindFromSubsystem();

	Super::NativeDestruct();
}

void UServantTaskMenuWidget::HandleAssignmentsChanged()
{
	// On world teardown every servant's EndPlay broadcasts: rebuilding the columns then would create widgets
	// in a dying world. The registry's world is used, not the widget's: the widget's outer is the GameInstance.
	UServantSubsystem* Subsystem = ServantSubsystem.Get();
	const UWorld* SubsystemWorld = Subsystem ? Subsystem->GetWorld() : nullptr;
	if (!SubsystemWorld || SubsystemWorld->bIsTearingDown) return;

	// A servant removed from the registry (destroyed) cannot stay selected
	AServant* Selected = SelectedServant.Get();
	if (Selected && !Subsystem->IsServantRegistered(Selected))
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
	// The registry may already be destroyed (world gone): nothing left to unbind from
	if (UServantSubsystem* Subsystem = ServantSubsystem.Get())
	{
		Subsystem->OnAssignmentsChanged.RemoveDynamic(this, &UServantTaskMenuWidget::HandleAssignmentsChanged);
	}
}
