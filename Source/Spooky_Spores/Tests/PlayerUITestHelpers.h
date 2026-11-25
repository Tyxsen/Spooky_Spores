// Test-only classes for the PlayerController UI Automation Spec (docs/specs/ui-player-controller.md, removed at merge).
// UHT does not accept a UCLASS inside #if WITH_DEV_AUTOMATION_TESTS, so these classes are compiled in every configuration;
// they are NotPlaceable / NotBlueprintable and never used by the game (same approach as ServantTaskTestListener.h).
//
// - ATestUIPlayerController overrides the two protected test seams of ASpooky_SporesPlayerController
//   (CreateUIWidget: no CreateWidget, which needs a local player; ApplyModalState: records instead of pausing the world).
// - UTestPlayerUIWidget implements IPlayerUIWidget and records every InitializeForPlayer call.
// - UTestPlainUIWidget does not implement it.
// - UPlayerUITestListener records OnUIOpenChanged (a dynamic delegate can only call a UFUNCTION of a UObject).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "EnhancedInputComponent.h"
#include "Core/Controller/Spooky_SporesPlayerController.h"
#include "UI/PlayerUIWidget.h"
#include "UI/UIWidgetEntry.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "PlayerUITestHelpers.generated.h"

class APawn;
class APlayerController;

// One recorded call of InitializeForPlayer. Weak pointers: the record must not keep a pawn alive nor hide its destruction.
struct FPlayerUIInitCall
{
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<APawn> Pawn;
};

UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class UTestPlayerUIWidget : public UUserWidget, public IPlayerUIWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	virtual void InitializeForPlayer_Implementation(APlayerController* InController, APawn* InPawn) override
	{
		FPlayerUIInitCall Call;
		Call.Controller = InController;
		Call.Pawn = InPawn;
		Calls.Add(Call);
	}

// -----------PROPERTIES-----------
public:
	// Not UPROPERTY on purpose: owned by the test, only compared by pointer.
	TArray<FPlayerUIInitCall> Calls;
};

UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class UTestPlainUIWidget : public UUserWidget
{
	GENERATED_BODY()
};

UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class UPlayerUITestListener : public UObject
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	UFUNCTION()
	void HandleUIOpenChanged(FName Id, bool bIsOpen)
	{
		Ids.Add(Id);
		States.Add(bIsOpen);
	}

	int32 Num() const { return Ids.Num(); }

// -----------PROPERTIES-----------
public:
	// Parallel arrays, one element per received OnUIOpenChanged, in order.
	TArray<FName> Ids;
	TArray<bool> States;
};

UCLASS(NotBlueprintable, NotPlaceable, Transient)
class ATestUIPlayerController : public ASpooky_SporesPlayerController
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Must be called before FinishSpawning (BeginPlay creates the widgets).
	void SetEntries(const TArray<FUIWidgetEntry>& InEntries)
	{
		UIEntries = InEntries;
	}

	// A test world has no LocalPlayer: the controller only reports itself local (or not) through this flag.
	virtual bool IsLocalController() const override
	{
		return bReportLocal;
	}

	// Runs the production SetupInputComponent on a fresh Enhanced Input component (no LocalPlayer needed).
	// Returns nullptr if the controller ended up without an Enhanced Input component.
	UEnhancedInputComponent* RunInputSetup()
	{
		InputComponent = NewObject<UEnhancedInputComponent>(this, TEXT("TestUIInputComponent"));
		SetupInputComponent();
		return Cast<UEnhancedInputComponent>(InputComponent);
	}

protected:
	virtual UUserWidget* CreateUIWidget(const FUIWidgetEntry& Entry) override
	{
		// Recorded even for an entry the production code should have filtered out: the test asserts that none reaches the seam.
		CreatedIds.Add(Entry.Id);
		if (*Entry.WidgetClass == nullptr)
		{
			return nullptr;
		}
		FScopedAllowAbstractClassAllocation AllowAbstract;
		UUserWidget* Widget = NewObject<UUserWidget>(this, Entry.WidgetClass, NAME_None, RF_Transient);
		if (Widget != nullptr)
		{
			// Sentinel: neither Visible nor Collapsed, so only the controller can produce those two states.
			Widget->SetVisibility(ESlateVisibility::Hidden);
		}
		return Widget;
	}

	virtual void ApplyModalState(bool bModal) override
	{
		ModalCalls.Add(bModal);
		if (bCallRealModal)
		{
			Super::ApplyModalState(bModal);
		}
	}

// -----------PROPERTIES-----------
public:
	// Ids passed to CreateUIWidget, in order.
	TArray<FName> CreatedIds;

	// Every argument received by ApplyModalState, in order.
	TArray<bool> ModalCalls;

	bool bReportLocal = true;

	// When true, ApplyModalState also runs the production implementation (pause, cursor, input mode).
	bool bCallRealModal = false;
};
