// Shared helpers for the servant wandering Automation Specs (criteria A1-A9 of the servant wandering spec, docs/specs/2.2c-servant-wandering.md,
// removed at merge).

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Servants/ServantAIController.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/UnrealType.h"

namespace ServantWanderTest
{
	// Readable name of an action, for assertion messages.
	inline FString ActionName(EServantWanderAction Action)
	{
		return UEnum::GetValueAsString(Action);
	}

	// The tunable values are private UPROPERTYs (spec, "Valeurs reglables"): tests read them through reflection, by the names
	// given in the spec. The numeric type (float, double, int32) is not fixed by the spec, so any numeric property is accepted.
	inline bool ReadNumber(const UObject* Object, const TCHAR* PropertyName, double& OutValue)
	{
		if (Object == nullptr)
		{
			return false;
		}
		const FNumericProperty* Property = FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr)
		{
			return false;
		}
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		OutValue = Property->IsFloatingPoint()
			? Property->GetFloatingPointPropertyValue(ValuePtr)
			: static_cast<double>(Property->GetSignedIntPropertyValue(ValuePtr));
		return true;
	}

	// Writes a tunable value on an instance (test setup only), same name and type rules as ReadNumber.
	inline bool WriteNumber(UObject* Object, const TCHAR* PropertyName, double Value)
	{
		if (Object == nullptr)
		{
			return false;
		}
		const FNumericProperty* Property = FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName);
		if (Property == nullptr)
		{
			return false;
		}
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		if (Property->IsFloatingPoint())
		{
			Property->SetFloatingPointPropertyValue(ValuePtr, Value);
		}
		else
		{
			Property->SetIntPropertyValue(ValuePtr, static_cast<int64>(FMath::RoundToDouble(Value)));
		}
		return true;
	}

	// Counts every move request that reaches an end on a controller's path following component, including the synchronous
	// Failed / AlreadyAtGoal finishes that AAIController::MoveTo emits. Zero means "no MoveToLocation was ever issued".
	// Must live on the stack of the test (the lambda captures this).
	class FMoveRequestCounter
	{
	public:
		explicit FMoveRequestCounter(AAIController* Controller)
		{
			if (Controller != nullptr && Controller->GetPathFollowingComponent() != nullptr)
			{
				Component = Controller->GetPathFollowingComponent();
				Handle = Component->OnRequestFinished.AddLambda([this](FAIRequestID, const FPathFollowingResult&)
				{
					++Count;
				});
			}
		}

		~FMoveRequestCounter()
		{
			if (Component.IsValid())
			{
				Component->OnRequestFinished.Remove(Handle);
			}
		}

		FMoveRequestCounter(const FMoveRequestCounter&) = delete;
		FMoveRequestCounter& operator=(const FMoveRequestCounter&) = delete;

		bool IsBound() const { return Component.IsValid(); }

		int32 Count = 0;

	private:
		TWeakObjectPtr<UPathFollowingComponent> Component;
		FDelegateHandle Handle;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
