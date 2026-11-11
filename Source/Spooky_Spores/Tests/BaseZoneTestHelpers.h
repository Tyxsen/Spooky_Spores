// Shared helpers for the base zone Automation Specs (criteria from the base zone spec, removed at merge: see commit a5a85da).

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Base/BaseZone.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/UnrealType.h"

namespace BaseZoneTest
{
	// Number of random draws used by the statistical criteria R1-R5 (spec value).
	constexpr int32 DrawCount = 2000;

	// Tolerance used by the tests when they re-check bounds independently of the implementation.
	// The spec's internal edge tolerance is 0.1 cm and must stay below 0.5 cm (A2).
	constexpr double IndependentTolerance = 0.5;

	struct FBoxConfig
	{
		FString Name;
		FTransform Transform;
		FVector Extent; // UNSCALED local half-extent
	};

	inline FTransform MakeTransform(const FVector& Center, const FRotator& Rotation = FRotator::ZeroRotator, const FVector& Scale = FVector::OneVector)
	{
		return FTransform(Rotation, Center, Scale);
	}

	// The reference configurations named by the spec (A1, A3, A4, A5, A6).
	inline TArray<FBoxConfig> MakeReferenceConfigs()
	{
		TArray<FBoxConfig> Configs;
		Configs.Add({ TEXT("A1 axis-aligned"), MakeTransform(FVector::ZeroVector), FVector(500.0, 300.0, 100.0) });
		Configs.Add({ TEXT("A3 yaw 90"), MakeTransform(FVector(1000.0, 0.0, 0.0), FRotator(0.0, 90.0, 0.0)), FVector(500.0, 200.0, 100.0) });
		Configs.Add({ TEXT("A4 yaw 45"), MakeTransform(FVector(1000.0, 0.0, 0.0), FRotator(0.0, 45.0, 0.0)), FVector(500.0, 200.0, 100.0) });
		Configs.Add({ TEXT("A5 pitch 30"), MakeTransform(FVector::ZeroVector, FRotator(30.0, 0.0, 0.0)), FVector(500.0, 300.0, 100.0) });
		Configs.Add({ TEXT("A6 scale 3,1,1"), MakeTransform(FVector::ZeroVector, FRotator::ZeroRotator, FVector(3.0, 1.0, 1.0)), FVector(100.0, 100.0, 100.0) });
		return Configs;
	}

	// Draws Count points with the pure static function. Returns false as soon as one draw reports failure.
	inline bool DrawPoints(const FTransform& Transform, const FVector& Extent, bool bIgnoreHeight, int32 Count, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FVector Point = FVector::ZeroVector;
			if (!ABaseZone::TryGetRandomPointInOrientedBox(Transform, Extent, bIgnoreHeight, Point))
			{
				return false;
			}
			OutPoints.Add(Point);
		}
		return true;
	}

	// bIgnoreHeight is a protected UPROPERTY: tests flip it through reflection (property name given by the spec).
	inline bool SetIgnoreHeight(ABaseZone* Zone, bool bValue)
	{
		FBoolProperty* Property = FindFProperty<FBoolProperty>(ABaseZone::StaticClass(), TEXT("bIgnoreHeight"));
		if (Zone == nullptr || Property == nullptr)
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(Zone, bValue);
		return true;
	}

	// Counts log lines of one category, by verbosity. Independent of the message text (which the spec does not fix).
	class FCategoryLogCounter : public FOutputDevice
	{
	public:
		explicit FCategoryLogCounter(const FName& InCategory)
			: Category(InCategory)
		{
			GLog->AddOutputDevice(this);
		}

		virtual ~FCategoryLogCounter() override
		{
			GLog->RemoveOutputDevice(this);
		}

		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& InCategory) override
		{
			if (InCategory != Category)
			{
				return;
			}
			if (Verbosity == ELogVerbosity::Warning)
			{
				++WarningCount;
			}
			else if (Verbosity == ELogVerbosity::Error)
			{
				++ErrorCount;
			}
		}

		// Makes sure logs buffered from other threads have reached this device before counts are read.
		void FlushPendingLogs() { GLog->FlushThreadedLogs(); }

		int32 WarningCount = 0;
		int32 ErrorCount = 0;

	private:
		FName Category;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
