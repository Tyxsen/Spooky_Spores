// Fill out your copyright notice in the Description page of Project Settings.


#include "Resources/FieldPlotSubsystem.h"

#include "GameFramework/Actor.h"
#include "Resources/FieldPlot.h"

void UFieldPlotSubsystem::RegisterField(AFieldPlot* Field)
{
	if (!Field || FindEntryIndex(Field) != INDEX_NONE) return;

	FFieldPlotEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Field = Field;
}

void UFieldPlotSubsystem::UnregisterField(AFieldPlot* Field)
{
	const int32 Index = FindEntryIndex(Field);
	if (Index == INDEX_NONE) return;

	// RemoveAt (not RemoveAtSwap) keeps the registration order; the reservation goes with the entry
	Entries.RemoveAt(Index);

	// After the removal: listeners that release their reservation find nothing, which is fine
	NotifyFieldChanged(Field, /*bRemoved*/ true);
}

TArray<AFieldPlot*> UFieldPlotSubsystem::GetUnreservedPlantableFields() const
{
	TArray<AFieldPlot*> Result;
	for (const FFieldPlotEntry& Entry : Entries)
	{
		AFieldPlot* EntryField = Entry.Field.Get();
		if (EntryField && !Entry.Reserver.IsValid() && EntryField->IsReadyToPlant())
		{
			Result.Add(EntryField);
		}
	}
	return Result;
}

int32 UFieldPlotSubsystem::GetPromisedSeeds(const AActor* ExcludedReserver) const
{
	int32 Promised = 0;
	for (const FFieldPlotEntry& Entry : Entries)
	{
		const AFieldPlot* EntryField = Entry.Field.Get();
		if (EntryField && Entry.Reserver.IsValid() && Entry.Reserver.Get() != ExcludedReserver && EntryField->IsReadyToPlant())
		{
			Promised += EntryField->GetSeedCost();
		}
	}
	return Promised;
}

void UFieldPlotSubsystem::NotifyFieldChanged(AFieldPlot* Field, bool bRemoved)
{
	OnFieldChanged.Broadcast(Field, bRemoved);
}

TArray<AFieldPlot*> UFieldPlotSubsystem::GetUnreservedHarvestableFields() const
{
	TArray<AFieldPlot*> Result;
	for (const FFieldPlotEntry& Entry : Entries)
	{
		AFieldPlot* EntryField = Entry.Field.Get();
		if (EntryField && !Entry.Reserver.IsValid() && EntryField->IsReadyToHarvest())
		{
			Result.Add(EntryField);
		}
	}
	return Result;
}

bool UFieldPlotSubsystem::TryReserve(AFieldPlot* Field, const AActor* Reserver)
{
	const int32 Index = FindEntryIndex(Field);
	if (Index == INDEX_NONE || !Reserver) return false;

	FFieldPlotEntry& Entry = Entries[Index];
	// A dead reserver reads as invalid: its reservation is free
	if (Entry.Reserver.IsValid() && Entry.Reserver.Get() != Reserver) return false;

	Entry.Reserver = Reserver;
	return true;
}

void UFieldPlotSubsystem::Release(AFieldPlot* Field, const AActor* Reserver)
{
	const int32 Index = FindEntryIndex(Field);
	if (Index == INDEX_NONE || !Reserver) return;

	FFieldPlotEntry& Entry = Entries[Index];
	if (Entry.Reserver.Get() == Reserver)
	{
		Entry.Reserver.Reset();
	}
}

bool UFieldPlotSubsystem::IsReserved(const AFieldPlot* Field) const
{
	const int32 Index = FindEntryIndex(Field);
	return Index != INDEX_NONE && Entries[Index].Reserver.IsValid();
}

int32 UFieldPlotSubsystem::FindEntryIndex(const AFieldPlot* Field) const
{
	if (!Field) return INDEX_NONE;

	return Entries.IndexOfByPredicate([Field](const FFieldPlotEntry& Entry)
	{
		return Entry.Field.Get() == Field;
	});
}
