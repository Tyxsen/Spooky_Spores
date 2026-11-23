// Shared helpers for the enemy Automation Specs (wave spawner and Engage targeting; criteria of docs/specs/enemy-wave-spawner.md,
// removed at merge). Tunable values are set by reflection, by the property names given in the spec.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Enemies/EnemyPathSpline.h"
#include "Enemies/EnemyWaveSpawner.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace EnemyTest
{
	// Numeric type (float, double, int32) is not fixed by the spec: any numeric property is accepted.
	inline bool ReadNumber(const UObject* Object, const TCHAR* PropertyName, double& OutValue)
	{
		if (Object == nullptr) { return false; }
		const FNumericProperty* Property = FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr) { return false; }
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		OutValue = Property->IsFloatingPoint()
			? Property->GetFloatingPointPropertyValue(ValuePtr)
			: static_cast<double>(Property->GetSignedIntPropertyValue(ValuePtr));
		return true;
	}

	inline bool WriteNumber(UObject* Object, const TCHAR* PropertyName, double Value)
	{
		if (Object == nullptr) { return false; }
		const FNumericProperty* Property = FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr) { return false; }
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		if (Property->IsFloatingPoint()) { Property->SetFloatingPointPropertyValue(ValuePtr, Value); }
		else { Property->SetIntPropertyValue(ValuePtr, static_cast<int64>(FMath::RoundToDouble(Value))); }
		return true;
	}

	inline bool ReadBool(const UObject* Object, const TCHAR* PropertyName, bool& OutValue)
	{
		if (Object == nullptr) { return false; }
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr) { return false; }
		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	inline bool WriteBool(UObject* Object, const TCHAR* PropertyName, bool bValue)
	{
		if (Object == nullptr) { return false; }
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr) { return false; }
		Property->SetPropertyValue_InContainer(Object, bValue);
		return true;
	}

	inline bool WriteObject(UObject* Object, const TCHAR* PropertyName, UObject* Value)
	{
		if (Object == nullptr) { return false; }
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr) { return false; }
		Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}

	inline bool ReadObject(const UObject* Object, const TCHAR* PropertyName, UObject*& OutValue)
	{
		if (Object == nullptr) { return false; }
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr) { return false; }
		OutValue = Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Object));
		return true;
	}

	// Replaces the content of the "Waves" array property (an array of FEnemyWaveEntry).
	inline bool WriteWaves(UObject* Object, const TArray<FEnemyWaveEntry>& Entries)
	{
		if (Object == nullptr) { return false; }
		const FArrayProperty* Property = FindFProperty<FArrayProperty>(Object->GetClass(), TEXT("Waves"));
		const FStructProperty* Inner = (Property != nullptr) ? CastField<FStructProperty>(Property->Inner) : nullptr;
		if (Inner == nullptr || Inner->Struct != FEnemyWaveEntry::StaticStruct()) { return false; }
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Object));
		Helper.EmptyValues();
		Helper.AddValues(Entries.Num());
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			Inner->CopyCompleteValue(Helper.GetRawPtr(Index), &Entries[Index]);
		}
		return true;
	}

	inline int32 ReadWavesNum(const UObject* Object)
	{
		const FArrayProperty* Property = (Object != nullptr) ? FindFProperty<FArrayProperty>(Object->GetClass(), TEXT("Waves")) : nullptr;
		if (Property == nullptr) { return INDEX_NONE; }
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Object));
		return Helper.Num();
	}

	// Copies entry Index of the "Waves" array into OutEntry. False when the property or the index does not exist.
	inline bool ReadWaveAt(const UObject* Object, int32 Index, FEnemyWaveEntry& OutEntry)
	{
		const FArrayProperty* Property = (Object != nullptr) ? FindFProperty<FArrayProperty>(Object->GetClass(), TEXT("Waves")) : nullptr;
		const FStructProperty* Inner = (Property != nullptr) ? CastField<FStructProperty>(Property->Inner) : nullptr;
		if (Inner == nullptr || Inner->Struct != FEnemyWaveEntry::StaticStruct()) { return false; }
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Object));
		if (!Helper.IsValidIndex(Index)) { return false; }
		Inner->CopyCompleteValue(&OutEntry, Helper.GetRawPtr(Index));
		return true;
	}

	// Substrings of the expected configuration errors, declared with AddExpectedErrorPlain(..., Contains, <exact count>): the framework
	// fails the test if the count differs (Occurrences > 0 = exact, see AutomationTest.h). The spec fixes no message, so these name
	// the property or concept the error is about; if the implementation words them differently, adjust here (one place).
	inline const TCHAR* ErrorNoSpline = TEXT("has no PathSplineActor, cannot start waves");
	inline const TCHAR* ErrorNoWaves = TEXT("has an empty Waves array, cannot start waves");
	inline const TCHAR* ErrorNoEnemyClass = TEXT("has no EnemyClass, the wave is empty");
	// Long on purpose: FString::Contains is case-insensitive by default and AddError() of a failing assertion is matched against the
	// expected messages too (AutomationTest.cpp l.1717), so a short word like "destructible" also swallows (and counts) the assertion
	// labels of the test itself ("...: no destructible was attacked").
	inline const TCHAR* ErrorNoTarget = TEXT("found no living destructible");
	// Spec 3b: the distinct Error "no reachable target" (the spec fixes no wording; matched on the common stem). Keep the labels of the
	// assertions free of this stem, a failing assertion label is matched against the expected errors too.
	inline const TCHAR* ErrorNoReachableTarget = TEXT("reachable");

	struct FSpawnerConfig
	{
		TArray<FEnemyWaveEntry> Waves;
		double WaveInterval = 10.0;
		double SpawnStagger = 1.0;
		bool bAutoStart = false;
		double FirstWaveDelay = 5.0;
		AEnemyPathSpline* Spline = nullptr;
	};

	// A two-point straight spline starting at Start and running Length units along Direction.
	inline AEnemyPathSpline* SpawnSpline(UWorld* World, const FVector& Start, const FVector& Direction, double Length)
	{
		AEnemyPathSpline* Spline = World->SpawnActor<AEnemyPathSpline>(AEnemyPathSpline::StaticClass(), FTransform(Start));
		if (Spline == nullptr || Spline->GetSplineComponent() == nullptr) { return nullptr; }
		TArray<FVector> Points;
		Points.Add(Start);
		Points.Add(Start + Direction.GetSafeNormal() * Length);
		Spline->GetSplineComponent()->SetSplinePoints(Points, ESplineCoordinateSpace::World, true);
		return Spline;
	}

	// Lethal damage through the regular damage path (OnTakeAnyDamage -> health -> OnDeath), like the player would do.
	inline void KillActor(AActor* Actor)
	{
		UGameplayStatics::ApplyDamage(Actor, 1.0e9f, nullptr, nullptr, UDamageType::StaticClass());
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
