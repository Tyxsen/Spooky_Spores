// Test-only listener for UServantSubsystem::OnAssignmentsChanged (servant task menu Automation Specs).
// A dynamic multicast delegate can only call a UFUNCTION of a UObject, hence this tiny UCLASS. UHT does not accept a UCLASS
// inside #if WITH_DEV_AUTOMATION_TESTS, so it is compiled in every configuration; it is Transient, hidden and never used by the game.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ServantTaskTestListener.generated.h"

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UServantTaskTestListener : public UObject
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	UFUNCTION()
	void HandleAssignmentsChanged() { ++BroadcastCount; }

// -----------PROPERTIES-----------
public:
	// Number of OnAssignmentsChanged broadcasts received since the listener was bound
	int32 BroadcastCount = 0;
};
