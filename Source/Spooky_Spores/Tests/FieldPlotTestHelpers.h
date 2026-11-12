// Shared helpers for the field plot / servant field harvest Automation Specs (spec docs/specs/servant-field-harvest.md, removed at merge).
// AFieldPlot::CurrentState and AFieldPlot::HarvestDrops are private UPROPERTYs with no setter: tests write them through
// reflection (names unchanged by the spec), the same way the other specs write the tunable values.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Resources/FieldPlot.h"
#include "Resources/ResourceDrop.h"
#include "Resources/ResourceTypes.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

namespace FieldPlotTest
{
	// Writes the private CurrentState (test setup only). Returns false if the property is not found.
	inline bool SetState(AFieldPlot* Field, EFieldState State)
	{
		if (Field == nullptr)
		{
			return false;
		}
		FProperty* Property = FindFProperty<FProperty>(AFieldPlot::StaticClass(), TEXT("CurrentState"));
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(Field);
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(State));
			return true;
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			ByteProperty->SetIntPropertyValue(ByteProperty->ContainerPtrToValuePtr<void>(Field), static_cast<int64>(State));
			return true;
		}
		return false;
	}

	inline bool GetState(const AFieldPlot* Field, EFieldState& OutState)
	{
		if (Field == nullptr)
		{
			return false;
		}
		FProperty* Property = FindFProperty<FProperty>(AFieldPlot::StaticClass(), TEXT("CurrentState"));
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			const void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(Field);
			OutState = static_cast<EFieldState>(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr));
			return true;
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			OutState = static_cast<EFieldState>(ByteProperty->GetSignedIntPropertyValue(ByteProperty->ContainerPtrToValuePtr<void>(Field)));
			return true;
		}
		return false;
	}

	// Writes the private HarvestDrops table (test setup only).
	inline bool SetDrops(AFieldPlot* Field, const TArray<FResourceDropEntry>& Entries)
	{
		const FArrayProperty* Property = FindFProperty<FArrayProperty>(AFieldPlot::StaticClass(), TEXT("HarvestDrops"));
		if (Field == nullptr || Property == nullptr)
		{
			return false;
		}
		*Property->ContainerPtrToValuePtr<TArray<FResourceDropEntry>>(Field) = Entries;
		return true;
	}

	inline FResourceDropEntry MakeEntry(TSubclassOf<AResourceDrop> DropClass, EResourceType Type, int32 Amount, int32 SpawnCount)
	{
		FResourceDropEntry Entry;
		Entry.DropClass = DropClass;
		Entry.Type = Type;
		Entry.Amount = Amount;
		Entry.SpawnCount = SpawnCount;
		return Entry;
	}

	// Table used by the yield tests: the same type appears twice (sums must add up), one entry has no DropClass
	// (spec edge case: credited anyway), the others have one (so a spawned drop would be seen).
	// Expected yield: Wheat 2*3 + 1*1 = 7, Seed 1*2 = 2.
	inline TArray<FResourceDropEntry> MakeYieldTable()
	{
		TArray<FResourceDropEntry> Table;
		Table.Add(MakeEntry(AResourceDrop::StaticClass(), EResourceType::Wheat, 2, 3));
		Table.Add(MakeEntry(AResourceDrop::StaticClass(), EResourceType::Wheat, 1, 1));
		Table.Add(MakeEntry(nullptr, EResourceType::Seed, 1, 2));
		return Table;
	}

	// Independent computation of the spec formula: per type, the sum of Amount * SpawnCount.
	inline TMap<EResourceType, int32> ExpectedYield(const TArray<FResourceDropEntry>& Table)
	{
		TMap<EResourceType, int32> Yield;
		for (const FResourceDropEntry& Entry : Table)
		{
			Yield.FindOrAdd(Entry.Type) += Entry.Amount * Entry.SpawnCount;
		}
		return Yield;
	}

	inline AFieldPlot* SpawnField(UWorld* World, const TCHAR* Name, const FVector& Location, EFieldState State)
	{
		FActorSpawnParameters Params;
		Params.Name = FName(Name);
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFieldPlot* Field = World->SpawnActor<AFieldPlot>(AFieldPlot::StaticClass(), FTransform(Location), Params);
		if (Field != nullptr)
		{
			SetState(Field, State);
		}
		return Field;
	}

	// Plain actor, used as a reserver or as an interacting actor.
	inline AActor* SpawnPlainActor(UWorld* World, const TCHAR* Name)
	{
		FActorSpawnParameters Params;
		Params.Name = FName(Name);
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	}

	inline int32 CountDrops(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AResourceDrop> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
