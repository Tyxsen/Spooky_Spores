// Fill out your copyright notice in the Description page of Project Settings.


#include "Servants/ServantSubsystem.h"

#include "Servants/Servant.h"

bool UServantSubsystem::AssignTask(AServant* Servant, EServantTask NewTask)
{
	const int32 Index = FindEntryIndex(Servant);
	if (Index == INDEX_NONE) return false;

	FServantTaskEntry& Entry = Entries[Index];
	if (Entry.Task == NewTask) return false;

	// The AI controller is deliberately not touched: in 2.1 every servant keeps wandering whatever its task
	Entry.Task = NewTask;
	OnAssignmentsChanged.Broadcast();
	return true;
}

EServantTask UServantSubsystem::GetServantTask(const AServant* Servant) const
{
	const int32 Index = FindEntryIndex(Servant);
	return Index == INDEX_NONE ? EServantTask::Unassigned : Entries[Index].Task;
}

TArray<AServant*> UServantSubsystem::GetServantsWithTask(EServantTask Task) const
{
	TArray<AServant*> Result;
	for (const FServantTaskEntry& Entry : Entries)
	{
		AServant* EntryServant = Entry.Servant.Get();
		if (EntryServant && Entry.Task == Task)
		{
			Result.Add(EntryServant);
		}
	}
	return Result;
}

int32 UServantSubsystem::GetServantNumber(const AServant* Servant) const
{
	const int32 Index = FindEntryIndex(Servant);
	return Index == INDEX_NONE ? 0 : Entries[Index].Number;
}

int32 UServantSubsystem::GetServantCount() const
{
	return Entries.Num();
}

bool UServantSubsystem::IsServantRegistered(const AServant* Servant) const
{
	return FindEntryIndex(Servant) != INDEX_NONE;
}

void UServantSubsystem::RegisterServant(AServant* Servant)
{
	if (!Servant || FindEntryIndex(Servant) != INDEX_NONE) return;

	FServantTaskEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Servant = Servant;
	Entry.Task = EServantTask::Unassigned;
	Entry.Number = ++LastServantNumber;

	OnAssignmentsChanged.Broadcast();
}

void UServantSubsystem::UnregisterServant(AServant* Servant)
{
	const int32 Index = FindEntryIndex(Servant);
	if (Index == INDEX_NONE) return;

	// RemoveAt (not RemoveAtSwap) keeps the registration order. The entry is gone before the broadcast,
	// so listeners already see the servant as unregistered.
	Entries.RemoveAt(Index);
	OnAssignmentsChanged.Broadcast();
}

int32 UServantSubsystem::FindEntryIndex(const AServant* Servant) const
{
	if (!Servant) return INDEX_NONE;

	return Entries.IndexOfByPredicate([Servant](const FServantTaskEntry& Entry)
	{
		return Entry.Servant.Get() == Servant;
	});
}
