// Copyright Epic Games, Inc. All Rights Reserved.


#include "Spooky_SporesPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Core/Character/Spooky_SporesCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "UI/PlayerUIWidget.h"
#include "Spooky_Spores.h"
#include "Widgets/Input/SVirtualJoystick.h"

ASpooky_SporesPlayerController::ASpooky_SporesPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ASpooky_SporesCameraManager::StaticClass();
}

void ASpooky_SporesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogSpooky_Spores, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	if (IsLocalPlayerController())
	{
		CreateUIWidgets();
	}
}

void ASpooky_SporesPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		// UI toggles: Started on an entry's ToggleAction toggles that entry
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			TArray<const FUIWidgetEntry*> ValidEntries;
			GetValidEntries(ValidEntries, false);
			for (const FUIWidgetEntry* Entry : ValidEntries)
			{
				if (Entry->ToggleAction)
				{
					EnhancedInputComponent->BindAction(Entry->ToggleAction, ETriggerEvent::Started, this, &ASpooky_SporesPlayerController::HandleToggleInput, Entry->Id);
				}
			}
		}
	}
}

void ASpooky_SporesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// GetPawn() rather than InPawn: the engine refuses some possessions (spectator)
	InitializeWidgetsForPawn(GetPawn());
}

void ASpooky_SporesPlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	InitializeWidgetsForPawn(nullptr);
}

bool ASpooky_SporesPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

bool ASpooky_SporesPlayerController::OpenUI(FName Id)
{
	return SetUIOpen(Id, true);
}

bool ASpooky_SporesPlayerController::CloseUI(FName Id)
{
	return SetUIOpen(Id, false);
}

bool ASpooky_SporesPlayerController::ToggleUI(FName Id)
{
	return SetUIOpen(Id, !OpenUIIds.Contains(Id));
}

bool ASpooky_SporesPlayerController::IsUIOpen(FName Id) const
{
	if (!UIWidgets.Contains(Id))
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' has no UI widget with Id '%s'"), *GetNameSafe(this), *Id.ToString());
		return false;
	}
	return OpenUIIds.Contains(Id);
}

UUserWidget* ASpooky_SporesPlayerController::GetUIWidget(FName Id) const
{
	const TObjectPtr<UUserWidget>* Widget = UIWidgets.Find(Id);
	if (!Widget)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' has no UI widget with Id '%s'"), *GetNameSafe(this), *Id.ToString());
		return nullptr;
	}
	return *Widget;
}

UUserWidget* ASpooky_SporesPlayerController::CreateUIWidget(const FUIWidgetEntry& Entry)
{
	UUserWidget* Widget = CreateWidget<UUserWidget>(this, Entry.WidgetClass);
	if (Widget)
	{
		Widget->AddToPlayerScreen(Entry.ZOrder);
	}
	return Widget;
}

void ASpooky_SporesPlayerController::ApplyModalState(bool bModal)
{
	// While paused, only Input Actions with bTriggerWhenPaused and UMG clicks stay active.
	SetPause(bModal);
	SetShowMouseCursor(bModal);

	if (bModal)
	{
		// Not UIOnly: it ignores game input, so the toggle key could not close the UI
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ASpooky_SporesPlayerController::CreateUIWidgets()
{
	TArray<const FUIWidgetEntry*> ValidEntries;
	GetValidEntries(ValidEntries, true);

	for (const FUIWidgetEntry* Entry : ValidEntries)
	{
		UUserWidget* Widget = CreateUIWidget(*Entry);
		if (!Widget)
		{
			UE_LOG(LogSpooky_Spores, Error, TEXT("'%s' could not create the UI widget '%s'"), *GetNameSafe(this), *Entry->Id.ToString());
			continue;
		}

		Widget->SetVisibility(ESlateVisibility::Collapsed);
		UIWidgets.Add(Entry->Id, Widget);
		if (Entry->bPausesGame)
		{
			PausingUIIds.Add(Entry->Id);
		}

		InitializeWidgetForPlayer(Widget, GetPawn());

		if (Entry->bOpenAtStart)
		{
			SetUIOpen(Entry->Id, true);
		}
	}
}

void ASpooky_SporesPlayerController::InitializeWidgetForPlayer(UUserWidget* Widget, APawn* InPawn)
{
	if (Widget && Widget->Implements<UPlayerUIWidget>())
	{
		IPlayerUIWidget::Execute_InitializeForPlayer(Widget, this, InPawn);
	}
}

void ASpooky_SporesPlayerController::InitializeWidgetsForPawn(APawn* InPawn)
{
	for (const TPair<FName, TObjectPtr<UUserWidget>>& Pair : UIWidgets)
	{
		InitializeWidgetForPlayer(Pair.Value, InPawn);
	}
}

bool ASpooky_SporesPlayerController::SetUIOpen(FName Id, bool bOpen)
{
	UUserWidget* Widget = GetUIWidget(Id);
	if (!Widget) return false;

	if (OpenUIIds.Contains(Id) == bOpen) return false;

	const bool bWasModal = HasOpenPausingUI();

	if (bOpen)
	{
		OpenUIIds.Add(Id);
	}
	else
	{
		OpenUIIds.Remove(Id);
	}
	Widget->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// Only on a modal state change: a second pausing UI must not reapply pause and input mode
	const bool bIsModal = HasOpenPausingUI();
	if (bIsModal != bWasModal)
	{
		ApplyModalState(bIsModal);
	}

	OnUIOpenChanged.Broadcast(Id, bOpen);
	return true;
}

bool ASpooky_SporesPlayerController::HasOpenPausingUI() const
{
	for (const FName& OpenId : OpenUIIds)
	{
		if (PausingUIIds.Contains(OpenId)) return true;
	}
	return false;
}

void ASpooky_SporesPlayerController::HandleToggleInput(FName Id)
{
	ToggleUI(Id);
}

void ASpooky_SporesPlayerController::GetValidEntries(TArray<const FUIWidgetEntry*>& OutEntries, bool bLogIgnored) const
{
	TSet<FName> SeenIds;
	for (const FUIWidgetEntry& Entry : UIEntries)
	{
		if (Entry.Id.IsNone() || !Entry.WidgetClass)
		{
			UE_CLOG(bLogIgnored, LogSpooky_Spores, Error, TEXT("'%s' ignores a UI entry '%s': empty Id or no WidgetClass"), *GetNameSafe(this), *Entry.Id.ToString());
			continue;
		}

		if (SeenIds.Contains(Entry.Id))
		{
			UE_CLOG(bLogIgnored, LogSpooky_Spores, Error, TEXT("'%s' ignores a UI entry: duplicate Id '%s'"), *GetNameSafe(this), *Entry.Id.ToString());
			continue;
		}

		SeenIds.Add(Entry.Id);
		OutEntries.Add(&Entry);
	}
}
