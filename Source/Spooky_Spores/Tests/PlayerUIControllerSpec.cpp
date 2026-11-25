// Automation Spec for the UI system owned by ASpooky_SporesPlayerController (UIEntries, OpenUI/CloseUI/ToggleUI, modal state,
// IPlayerUIWidget dependency injection, ToggleAction binding) and for UServantTaskMenuWidget::InitializeForPlayer.
// Source of truth: docs/specs/ui-player-controller.md (removed at merge), acceptance criteria 1 to 9 and its edge cases.
//
// The controller under test is ATestUIPlayerController (Tests/PlayerUITestHelpers.h): it overrides the two protected seams
// CreateUIWidget (no CreateWidget: it needs a local player) and ApplyModalState (records the call instead of pausing the world).
// Widgets created by the seam start with a Hidden visibility sentinel, so Visible / Collapsed can only come from the controller.
// Every test lists its mutation: the change of behaviour that must turn it red.
//
// Not automated (to list in TODO.md, "Tests non automatisés"):
// - Real CreateUIWidget (CreateWidget + AddToPlayerScreen(ZOrder), added once, never RemoveFromParent): needs a LocalPlayer and a viewport.
// - Real pause (UGameplayStatics::IsGamePaused) and FInputModeGameAndUI / FInputModeGameOnly: the test world has no GameMode (SetPause is
//   refused) and no LocalPlayer. Only the cursor part of criterion 7 is asserted here.
// - OnMenuRefreshRequested is a BlueprintImplementableEvent: criterion 9 observes the registry subscriptions instead of the event.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/PlayerUITestHelpers.h"
#include "Core/Controller/Spooky_SporesPlayerController.h"
#include "UI/PlayerUIWidget.h"
#include "UI/UIWidgetEntry.h"
#include "Servants/ServantSubsystem.h"
#include "Servants/ServantTaskMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Tests/AutomationCommon.h"

namespace PlayerUISpecPrivate
{
	const FName IdA(TEXT("UI_A"));
	const FName IdB(TEXT("UI_B"));
	const FName IdC(TEXT("UI_C"));
	const FName IdD(TEXT("UI_D"));
	const FName IdUnknown(TEXT("UI_Unknown"));

	FUIWidgetEntry MakeEntry(FName Id, TSubclassOf<UUserWidget> WidgetClass, bool bOpenAtStart = false, bool bPausesGame = false,
	                         UInputAction* ToggleAction = nullptr)
	{
		FUIWidgetEntry Entry;
		Entry.Id = Id;
		Entry.WidgetClass = WidgetClass;
		Entry.bOpenAtStart = bOpenAtStart;
		Entry.bPausesGame = bPausesGame;
		Entry.ToggleAction = ToggleAction;
		return Entry;
	}

	FString ToString(const TArray<bool>& Values)
	{
		FString Result;
		for (const bool bValue : Values)
		{
			Result += bValue ? TEXT("T") : TEXT("F");
		}
		return Result;
	}

	int32 CountId(const TArray<FName>& Ids, FName Id)
	{
		int32 Count = 0;
		for (const FName& Candidate : Ids)
		{
			Count += (Candidate == Id) ? 1 : 0;
		}
		return Count;
	}

	// Executes every Started binding of the action (what the input system does when the key goes down). Returns how many ran.
	int32 FireStarted(const UEnhancedInputComponent* Input, const UInputAction* Action)
	{
		int32 Fired = 0;
		for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input->GetActionEventBindings())
		{
			if (Binding->GetAction() == Action && Binding->GetTriggerEvent() == ETriggerEvent::Started)
			{
				Binding->Execute(FInputActionInstance(Action));
				++Fired;
			}
		}
		return Fired;
	}

	// Number of bindings of the action on any trigger event other than Started.
	int32 CountNonStarted(const UEnhancedInputComponent* Input, const UInputAction* Action)
	{
		int32 Count = 0;
		for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input->GetActionEventBindings())
		{
			if (Binding->GetAction() == Action && Binding->GetTriggerEvent() != ETriggerEvent::Started)
			{
				++Count;
			}
		}
		return Count;
	}
}

BEGIN_DEFINE_SPEC(FPlayerUIControllerSpec, "SpookySpores.UI.PlayerController",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;
	ATestUIPlayerController* Controller = nullptr;
	TStrongObjectPtr<UPlayerUITestListener> Listener;

	bool RequireWorld();
	ADefaultPawn* SpawnPawn(const TCHAR* Name);
	// Deferred spawn: entries, flags and optional pawn are set BEFORE BeginPlay (which creates the widgets).
	ATestUIPlayerController* SpawnController(const TArray<FUIWidgetEntry>& Entries, APawn* StartPawn = nullptr, bool bLocal = true, bool bRealModal = false);
	// Binds a fresh listener on the controller's OnUIOpenChanged (after BeginPlay: events of the start are not counted).
	void Listen();
	void ExpectModalCalls(const FString& What, const TArray<bool>& Expected);

END_DEFINE_SPEC(FPlayerUIControllerSpec)

bool FPlayerUIControllerSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world unavailable: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

ADefaultPawn* FPlayerUIControllerSpec::SpawnPawn(const TCHAR* Name)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ADefaultPawn>(ADefaultPawn::StaticClass(), FTransform(FVector(0.0, 0.0, 5000.0)), Params);
}

ATestUIPlayerController* FPlayerUIControllerSpec::SpawnController(const TArray<FUIWidgetEntry>& Entries, APawn* StartPawn, bool bLocal, bool bRealModal)
{
	ATestUIPlayerController* Spawned = World->SpawnActorDeferred<ATestUIPlayerController>(
		ATestUIPlayerController::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Spawned == nullptr)
	{
		return nullptr;
	}
	Spawned->SetEntries(Entries);
	Spawned->bReportLocal = bLocal;
	Spawned->bCallRealModal = bRealModal;
	if (StartPawn != nullptr)
	{
		// Same trick as the other specs: SetPawn is enough for "the controller currently has this pawn" without running the possession chain.
		Spawned->SetPawn(StartPawn);
	}
	Spawned->FinishSpawning(FTransform::Identity);
	Controller = Spawned;
	return Spawned;
}

void FPlayerUIControllerSpec::Listen()
{
	Listener.Reset(NewObject<UPlayerUITestListener>());
	Controller->OnUIOpenChanged.AddDynamic(Listener.Get(), &UPlayerUITestListener::HandleUIOpenChanged);
}

void FPlayerUIControllerSpec::ExpectModalCalls(const FString& What, const TArray<bool>& Expected)
{
	TestTrue(FString::Printf(TEXT("%s: ApplyModalState calls expected [%s], got [%s]"), *What,
	                         *PlayerUISpecPrivate::ToString(Expected), *PlayerUISpecPrivate::ToString(Controller->ModalCalls)),
	         Controller->ModalCalls == Expected);
}

void FPlayerUIControllerSpec::Define()
{
	using namespace PlayerUISpecPrivate;

	BeforeEach([this]()
	{
		Wrapper = MakeUnique<FTestWorldWrapper>();
		bWorldReady = Wrapper->CreateTestWorld(EWorldType::Game) && Wrapper->BeginPlayInTestWorld();
		World = Wrapper->GetTestWorld();
		Controller = nullptr;
		if (!bWorldReady)
		{
			TArray<FString> Messages;
			Wrapper->AppendErrorMessages(Messages);
			for (const FString& Message : Messages)
			{
				AddError(FString::Printf(TEXT("FTestWorldWrapper: %s"), *Message));
			}
		}
	});

	AfterEach([this]()
	{
		Listener.Reset();
		Controller = nullptr;
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	// Mutation: the widget is created at each OpenUI (CreatedIds grows, the pointer changes) -> red;
	//           a valid entry is skipped, or created only when opened -> GetUIWidget null at start, red.
	It("Criterion 1: a valid entry produces one widget, created once even after several open/close cycles", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass()), MakeEntry(IdB, UTestPlainUIWidget::StaticClass()) }))) { return; }

		UUserWidget* WidgetA = Controller->GetUIWidget(IdA);
		UUserWidget* WidgetB = Controller->GetUIWidget(IdB);
		TestNotNull(TEXT("widget A exists right after BeginPlay"), WidgetA);
		TestNotNull(TEXT("widget B exists right after BeginPlay"), WidgetB);
		TestTrue(TEXT("widget A is an instance of the entry's WidgetClass"), WidgetA != nullptr && WidgetA->IsA<UTestPlayerUIWidget>());
		TestTrue(TEXT("widget B is an instance of the entry's WidgetClass"), WidgetB != nullptr && WidgetB->IsA<UTestPlainUIWidget>());
		TestTrue(TEXT("one widget per entry"), WidgetA != WidgetB);
		TestEqual(TEXT("the seam was called once per entry at start"), Controller->CreatedIds.Num(), 2);

		Controller->OpenUI(IdA);
		Controller->CloseUI(IdA);
		Controller->OpenUI(IdA);
		Controller->ToggleUI(IdA);
		Controller->ToggleUI(IdA);
		TestEqual(TEXT("no widget creation after open/close/toggle cycles"), Controller->CreatedIds.Num(), 2);
		TestTrue(TEXT("widget A is the same instance after the cycles"), Controller->GetUIWidget(IdA) == WidgetA);
	});

	// Mutation: bOpenAtStart ignored (all Collapsed) or all entries opened -> the IsUIOpen lines turn red;
	//           the visibility is not set (stays on the Hidden sentinel) or not kept in sync with IsUIOpen -> the visibility lines turn red.
	It("Criterion 2: bOpenAtStart entries are open and Visible after BeginPlay, the others closed and Collapsed", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), true), MakeEntry(IdB, UTestPlayerUIWidget::StaticClass()) }))) { return; }
		UUserWidget* WidgetA = Controller->GetUIWidget(IdA);
		UUserWidget* WidgetB = Controller->GetUIWidget(IdB);
		if (!TestTrue(TEXT("widgets created"), WidgetA != nullptr && WidgetB != nullptr)) { return; }

		TestTrue(TEXT("A (bOpenAtStart) is open right after BeginPlay"), Controller->IsUIOpen(IdA));
		TestFalse(TEXT("B is closed right after BeginPlay"), Controller->IsUIOpen(IdB));
		TestTrue(TEXT("A is Visible"), WidgetA->GetVisibility() == ESlateVisibility::Visible);
		TestTrue(TEXT("B is Collapsed"), WidgetB->GetVisibility() == ESlateVisibility::Collapsed);

		// Visibility follows later changes.
		Controller->OpenUI(IdB);
		Controller->CloseUI(IdA);
		TestTrue(TEXT("B Visible after OpenUI"), WidgetB->GetVisibility() == ESlateVisibility::Visible);
		TestTrue(TEXT("A Collapsed after CloseUI"), WidgetA->GetVisibility() == ESlateVisibility::Collapsed);
	});

	// Mutation: the validity check returns instead of continuing -> the entry after the first invalid one (C) is not created, red;
	//           the check is removed -> the invalid entries reach the seam (CreatedIds), or the duplicate creates a second widget, red;
	//           the error log is removed -> the expected-Error message is never seen, red.
	It("Criterion 3: invalid entries (no class, empty Id, duplicate) are ignored with an error, valid neighbours stay created", [this]()
	{
		if (!RequireWorld()) { return; }
		const FName IdNoClass(TEXT("UI_NoClass"));
		// The spec asks for UE_LOG(Error) per ignored entry; the text is not specified, so any error is expected, at least once.
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 0);

		TArray<FUIWidgetEntry> Entries;
		Entries.Add(MakeEntry(IdA, UTestPlayerUIWidget::StaticClass()));
		Entries.Add(MakeEntry(IdNoClass, nullptr));
		Entries.Add(MakeEntry(NAME_None, UTestPlayerUIWidget::StaticClass()));
		Entries.Add(MakeEntry(IdA, UTestPlainUIWidget::StaticClass())); // duplicate Id
		Entries.Add(MakeEntry(IdC, UTestPlayerUIWidget::StaticClass()));
		if (!TestNotNull(TEXT("controller spawned"), SpawnController(Entries))) { return; }

		TestNull(TEXT("entry without WidgetClass: no widget"), Controller->GetUIWidget(IdNoClass));
		TestNotNull(TEXT("the valid entry before the invalid ones is created"), Controller->GetUIWidget(IdA));
		TestNotNull(TEXT("the valid entry after the invalid ones is created"), Controller->GetUIWidget(IdC));
		TestEqual(TEXT("Id A reached the seam exactly once (duplicate ignored)"), CountId(Controller->CreatedIds, IdA), 1);
		TestEqual(TEXT("entry with an empty Id never reached the seam"), CountId(Controller->CreatedIds, NAME_None), 0);
		TestEqual(TEXT("entry without WidgetClass never reached the seam"), CountId(Controller->CreatedIds, IdNoClass), 0);
		TestEqual(TEXT("only the two valid entries were created"), Controller->CreatedIds.Num(), 2);
		TestFalse(TEXT("an ignored entry cannot be opened"), Controller->OpenUI(IdNoClass));
		TestFalse(TEXT("an ignored entry is not open"), Controller->IsUIOpen(IdNoClass));
	});

	// Mutation: the event is emitted on a no-op (second OpenUI, CloseUI of a closed UI) -> the counts turn red;
	//           ToggleUI returns false, or does not emit -> red; the event carries the wrong Id or the opposite state -> red;
	//           OpenUI on a bOpenAtStart UI returns true (state compared to Collapsed instead of IsUIOpen) -> red.
	It("Criterion 4: Open/Close/Toggle change IsUIOpen and emit OnUIOpenChanged once per real change, never on a no-op", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass()), MakeEntry(IdB, UTestPlayerUIWidget::StaticClass(), true) }))) { return; }
		Listen();

		TestTrue(TEXT("OpenUI on a closed UI returns true"), Controller->OpenUI(IdA));
		TestTrue(TEXT("A is open"), Controller->IsUIOpen(IdA));
		TestEqual(TEXT("one event after the first OpenUI"), Listener->Num(), 1);
		TestTrue(TEXT("event (A, true)"), Listener->Ids.Last() == IdA && Listener->States.Last() == true);

		TestFalse(TEXT("OpenUI on an open UI returns false"), Controller->OpenUI(IdA));
		TestEqual(TEXT("no event on the no-op OpenUI"), Listener->Num(), 1);

		TestTrue(TEXT("CloseUI on an open UI returns true"), Controller->CloseUI(IdA));
		TestFalse(TEXT("A is closed"), Controller->IsUIOpen(IdA));
		TestEqual(TEXT("two events after the CloseUI"), Listener->Num(), 2);
		TestTrue(TEXT("event (A, false)"), Listener->Ids.Last() == IdA && Listener->States.Last() == false);

		TestFalse(TEXT("CloseUI on a closed UI returns false"), Controller->CloseUI(IdA));
		TestEqual(TEXT("no event on the no-op CloseUI"), Listener->Num(), 2);

		TestTrue(TEXT("ToggleUI on a closed UI returns true"), Controller->ToggleUI(IdA));
		TestTrue(TEXT("A is open after ToggleUI"), Controller->IsUIOpen(IdA));
		TestEqual(TEXT("three events after the first ToggleUI"), Listener->Num(), 3);
		TestTrue(TEXT("event (A, true) from ToggleUI"), Listener->Ids.Last() == IdA && Listener->States.Last() == true);
		TestTrue(TEXT("ToggleUI on an open UI returns true"), Controller->ToggleUI(IdA));
		TestFalse(TEXT("A is closed after the second ToggleUI"), Controller->IsUIOpen(IdA));
		TestEqual(TEXT("four events after the second ToggleUI"), Listener->Num(), 4);
		TestTrue(TEXT("event (A, false) from ToggleUI"), Listener->Ids.Last() == IdA && Listener->States.Last() == false);

		// B starts open (bOpenAtStart): opening it is a no-op, closing it is a real change.
		TestFalse(TEXT("OpenUI on a bOpenAtStart UI returns false"), Controller->OpenUI(IdB));
		TestEqual(TEXT("no event on OpenUI of an already open bOpenAtStart UI"), Listener->Num(), 4);
		TestTrue(TEXT("CloseUI on a bOpenAtStart UI returns true"), Controller->CloseUI(IdB));
		TestEqual(TEXT("one more event"), Listener->Num(), 5);
		TestTrue(TEXT("event (B, false)"), Listener->Ids.Last() == IdB && Listener->States.Last() == false);
	});

	// Mutation: an unknown Id creates an entry / emits an event / returns true -> red;
	//           an unknown Id touches the state of another UI (e.g. toggles the first entry) -> the A lines turn red.
	It("Criterion 4 (unknown Id): negative result, no event, no side effect", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), false, true) }))) { return; }
		Listen();

		TestFalse(TEXT("OpenUI(unknown) returns false"), Controller->OpenUI(IdUnknown));
		TestFalse(TEXT("CloseUI(unknown) returns false"), Controller->CloseUI(IdUnknown));
		TestFalse(TEXT("ToggleUI(unknown) returns false"), Controller->ToggleUI(IdUnknown));
		TestFalse(TEXT("IsUIOpen(unknown) is false"), Controller->IsUIOpen(IdUnknown));
		TestNull(TEXT("GetUIWidget(unknown) is null"), Controller->GetUIWidget(IdUnknown));
		TestEqual(TEXT("no event emitted"), Listener->Num(), 0);
		TestFalse(TEXT("the known UI was not touched"), Controller->IsUIOpen(IdA));
		TestEqual(TEXT("no modal change"), Controller->ModalCalls.Num(), 0);
	});

	// Mutation: ApplyModalState is called at every open/close -> the sequence has extra entries, red;
	//           "last UI closed" is tracked as "last opened UI closed" (single id instead of a set) -> the reversed-order round turns red;
	//           the pause is never released, or released when the FIRST one closes -> red.
	It("Criterion 5: ApplyModalState(true) on the first pausing UI, (false) only when the last one closes", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), false, true), MakeEntry(IdB, UTestPlayerUIWidget::StaticClass(), false, true) }))) { return; }
		ExpectModalCalls(TEXT("nothing opened yet"), {});

		Controller->OpenUI(IdA);
		ExpectModalCalls(TEXT("first pausing UI opened"), { true });
		Controller->OpenUI(IdB);
		ExpectModalCalls(TEXT("second pausing UI opened"), { true });
		Controller->CloseUI(IdA);
		ExpectModalCalls(TEXT("first one closed, second still open"), { true });
		Controller->CloseUI(IdB);
		ExpectModalCalls(TEXT("last one closed"), { true, false });
		Controller->CloseUI(IdB);
		ExpectModalCalls(TEXT("no-op close does not call again"), { true, false });

		// Reversed order: the one opened first is the one still open at the end.
		Controller->OpenUI(IdA);
		Controller->OpenUI(IdB);
		Controller->CloseUI(IdB);
		ExpectModalCalls(TEXT("reversed round: last opened closed, first still open"), { true, false, true });
		Controller->CloseUI(IdA);
		ExpectModalCalls(TEXT("reversed round: last one closed"), { true, false, true, false });
	});

	// Mutation: any open UI counts as modal (the non-pausing one calls ApplyModalState(true)) -> red;
	//           ApplyModalState(HasPausingUI) is recomputed and called at every change -> the call list grows, red;
	//           closing the non-pausing UI releases the pause held by the pausing one -> red.
	It("Criterion 6: a non-pausing UI never calls ApplyModalState and does not disturb a pausing one", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), false, true), MakeEntry(IdC, UTestPlayerUIWidget::StaticClass()) }))) { return; }

		Controller->OpenUI(IdC);
		Controller->CloseUI(IdC);
		Controller->ToggleUI(IdC);
		Controller->ToggleUI(IdC);
		ExpectModalCalls(TEXT("non-pausing UI opened/closed/toggled alone"), {});

		Controller->OpenUI(IdA);
		Controller->OpenUI(IdC);
		Controller->CloseUI(IdC);
		ExpectModalCalls(TEXT("non-pausing UI opened and closed while a pausing one is open"), { true });
		Controller->CloseUI(IdA);
		ExpectModalCalls(TEXT("pausing UI closed"), { true, false });
	});

	// Mutation: the modal state is only evaluated by OpenUI/CloseUI, not at BeginPlay -> no call for the bOpenAtStart pausing UI, red;
	//           bOpenAtStart on a non-pausing UI applies a modal state -> the second controller line turns red.
	It("Edge case: a bOpenAtStart + bPausesGame UI applies the modal state at BeginPlay, a non-pausing one does not", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), true, true) }))) { return; }
		ExpectModalCalls(TEXT("open-at-start pausing UI"), { true });
		TestTrue(TEXT("it is open"), Controller->IsUIOpen(IdA));
		Controller->CloseUI(IdA);
		ExpectModalCalls(TEXT("closing it releases the modal state"), { true, false });

		if (!TestNotNull(TEXT("second controller spawned"), SpawnController({ MakeEntry(IdC, UTestPlayerUIWidget::StaticClass(), true, false) }))) { return; }
		TestTrue(TEXT("non-pausing open-at-start UI is open"), Controller->IsUIOpen(IdC));
		ExpectModalCalls(TEXT("open-at-start non-pausing UI"), {});
	});

	// Mutation: ApplyModalState(true) does not show the cursor (or (false) does not hide it) -> red.
	// Pause and input mode are NOT asserted: see the "Not automated" list at the top of the file.
	It("Criterion 7 (partial): the real ApplyModalState shows the cursor while a pausing UI is open and hides it after", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), false, true) }, nullptr, true, true))) { return; }
		TestFalse(TEXT("precondition: cursor hidden"), Controller->bShowMouseCursor);

		Controller->OpenUI(IdA);
		TestTrue(TEXT("cursor visible after opening a pausing UI"), Controller->bShowMouseCursor);
		Controller->CloseUI(IdA);
		TestFalse(TEXT("cursor hidden after closing the last pausing UI"), Controller->bShowMouseCursor);
	});

	// Mutation: the creation call is removed -> Num()==0, red; it is made twice -> Num()==2, red;
	//           the pawn given at creation is always nullptr (or a world search) -> the first-call lines turn red;
	//           OnPossess does not notify, or notifies with the previous pawn -> red; OnUnPossess does not notify, or passes the old pawn -> red;
	//           the widget that does not implement the interface is called without the Implements check -> check() fires on Execute_ (crash = red).
	It("Criterion 8: InitializeForPlayer at creation, at OnPossess and at OnUnPossess (nullptr); a plain widget is left alone", [this]()
	{
		if (!RequireWorld()) { return; }
		ADefaultPawn* PawnA = SpawnPawn(TEXT("UIPawnA"));
		ADefaultPawn* PawnB = SpawnPawn(TEXT("UIPawnB"));
		if (!TestTrue(TEXT("pawns spawned"), PawnA != nullptr && PawnB != nullptr)) { return; }
		const TArray<FUIWidgetEntry> Entries = { MakeEntry(IdA, UTestPlayerUIWidget::StaticClass()), MakeEntry(IdB, UTestPlainUIWidget::StaticClass()) };

		// Creation with a pawn already owned.
		if (!TestNotNull(TEXT("first controller spawned"), SpawnController(Entries, PawnA))) { return; }
		UTestPlayerUIWidget* WithPawn = Cast<UTestPlayerUIWidget>(Controller->GetUIWidget(IdA));
		if (!TestNotNull(TEXT("interface widget created"), WithPawn)) { return; }
		if (TestEqual(TEXT("one InitializeForPlayer call at creation"), WithPawn->Calls.Num(), 1))
		{
			TestTrue(TEXT("creation call: Controller is the owning controller"), WithPawn->Calls[0].Controller.Get() == Controller);
			TestTrue(TEXT("creation call: Pawn is the current pawn"), WithPawn->Calls[0].Pawn.Get() == PawnA);
		}

		// Creation before any possession, then possession change and loss.
		if (!TestNotNull(TEXT("second controller spawned"), SpawnController(Entries))) { return; }
		UTestPlayerUIWidget* Widget = Cast<UTestPlayerUIWidget>(Controller->GetUIWidget(IdA));
		if (!TestNotNull(TEXT("interface widget created"), Widget)) { return; }
		if (!TestEqual(TEXT("one InitializeForPlayer call at creation (no pawn yet)"), Widget->Calls.Num(), 1)) { return; }
		TestTrue(TEXT("creation call without pawn: Controller valid"), Widget->Calls[0].Controller.Get() == Controller);
		TestTrue(TEXT("creation call without pawn: Pawn is nullptr"), Widget->Calls[0].Pawn.Get() == nullptr);

		const int32 CallsBeforePossess = Widget->Calls.Num();
		Controller->Possess(PawnB);
		TestTrue(TEXT("Possess did give the pawn to the controller (harness check)"), Controller->GetPawn() == PawnB);
		TestTrue(TEXT("OnPossess notified the widget"), Widget->Calls.Num() > CallsBeforePossess);
		TestTrue(TEXT("OnPossess: last call carries the new pawn"), Widget->Calls.Last().Pawn.Get() == PawnB);
		TestTrue(TEXT("OnPossess: Controller still valid"), Widget->Calls.Last().Controller.Get() == Controller);

		const int32 CallsBeforeUnPossess = Widget->Calls.Num();
		Controller->UnPossess();
		TestTrue(TEXT("UnPossess released the pawn (harness check)"), Controller->GetPawn() == nullptr);
		TestTrue(TEXT("OnUnPossess notified the widget"), Widget->Calls.Num() > CallsBeforeUnPossess);
		TestTrue(TEXT("OnUnPossess: last call carries nullptr"), Widget->Calls.Last().Pawn.Get() == nullptr);
		TestTrue(TEXT("OnUnPossess: Controller still valid"), Widget->Calls.Last().Controller.Get() == Controller);

		TestNotNull(TEXT("the plain widget exists and survived all notifications"), Controller->GetUIWidget(IdB));
	});

	// Mutation: the non-local guard is removed (widgets created by any controller) -> CreatedIds not empty, red.
	// Depends on the controller using IsLocalController() (virtual) as its "local" test; a LocalPlayer-based test would make criterion tests red instead.
	It("Technical choice: a non-local controller creates no widget", [this]()
	{
		if (!RequireWorld()) { return; }
		if (!TestNotNull(TEXT("controller spawned"), SpawnController({ MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), true) }, nullptr, false))) { return; }
		TestEqual(TEXT("seam never called"), Controller->CreatedIds.Num(), 0);
		TestNull(TEXT("no widget"), Controller->GetUIWidget(IdA));
		TestFalse(TEXT("nothing open"), Controller->IsUIOpen(IdA));
	});

	// Mutation: the action is bound to Triggered/Completed instead of Started -> the "no other trigger event" lines turn red (and a Triggered
	//           toggle would flicker every frame); the handler captures a wrong Id (e.g. the last entry's) -> the A/B/C states turn red;
	//           only the first entry of a shared action is bound -> B stays closed, red; an entry without ToggleAction gets bound -> D opens, red.
	It("Input: ToggleAction is bound on Started to ToggleUI(Id), a shared action toggles every entry that uses it", [this]()
	{
		if (!RequireWorld()) { return; }
		TStrongObjectPtr<UInputAction> ActionX(NewObject<UInputAction>(GetTransientPackage(), NAME_None, RF_Transient));
		TStrongObjectPtr<UInputAction> ActionY(NewObject<UInputAction>(GetTransientPackage(), NAME_None, RF_Transient));
		TArray<FUIWidgetEntry> Entries;
		Entries.Add(MakeEntry(IdA, UTestPlayerUIWidget::StaticClass(), false, false, ActionX.Get()));
		Entries.Add(MakeEntry(IdB, UTestPlayerUIWidget::StaticClass(), false, false, ActionX.Get())); // shared action
		Entries.Add(MakeEntry(IdC, UTestPlayerUIWidget::StaticClass(), false, false, ActionY.Get()));
		Entries.Add(MakeEntry(IdD, UTestPlayerUIWidget::StaticClass())); // no action
		if (!TestNotNull(TEXT("controller spawned"), SpawnController(Entries))) { return; }
		UEnhancedInputComponent* Input = Controller->RunInputSetup();
		if (!TestNotNull(TEXT("harness: Enhanced Input component available after SetupInputComponent"), Input)) { return; }

		TestEqual(TEXT("ActionX has no binding other than Started"), CountNonStarted(Input, ActionX.Get()), 0);
		TestEqual(TEXT("ActionY has no binding other than Started"), CountNonStarted(Input, ActionY.Get()), 0);

		TestTrue(TEXT("ActionX has Started bindings"), FireStarted(Input, ActionX.Get()) > 0);
		TestTrue(TEXT("shared action: A toggled open"), Controller->IsUIOpen(IdA));
		TestTrue(TEXT("shared action: B toggled open"), Controller->IsUIOpen(IdB));
		TestFalse(TEXT("C (other action) untouched"), Controller->IsUIOpen(IdC));
		TestFalse(TEXT("D (no action) untouched"), Controller->IsUIOpen(IdD));

		TestTrue(TEXT("ActionY has Started bindings"), FireStarted(Input, ActionY.Get()) > 0);
		TestTrue(TEXT("C toggled open by ActionY"), Controller->IsUIOpen(IdC));
		TestTrue(TEXT("A still open (ActionY does not concern it)"), Controller->IsUIOpen(IdA));

		FireStarted(Input, ActionX.Get());
		TestFalse(TEXT("second press: A toggled closed"), Controller->IsUIOpen(IdA));
		TestFalse(TEXT("second press: B toggled closed"), Controller->IsUIOpen(IdB));
		TestTrue(TEXT("C still open"), Controller->IsUIOpen(IdC));
	});

	// Mutation: InitializeForPlayer_Implementation is not overridden (no registry) -> GetServantSubsystem stays null, red;
	//           the menu does not implement IPlayerUIWidget -> the Implements line turns red;
	//           the subscription is added without unbinding the previous one (no idempotence) -> two bindings, red
	//           (each binding triggers one OnMenuRefreshRequested per OnAssignmentsChanged broadcast).
	It("Criterion 9: UServantTaskMenuWidget::InitializeForPlayer plugs the menu on the world registry, idempotently", [this]()
	{
		if (!RequireWorld()) { return; }
		UServantSubsystem* Subsystem = World->GetSubsystem<UServantSubsystem>();
		if (!TestNotNull(TEXT("harness: registry exists"), Subsystem)) { return; }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APlayerController* PlayerController = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
		ADefaultPawn* Pawn = SpawnPawn(TEXT("MenuPawn"));
		if (!TestTrue(TEXT("harness: controller and pawn spawned"), PlayerController != nullptr && Pawn != nullptr)) { return; }

		// UServantTaskMenuWidget is Abstract (its Blueprint child does the layout): same allocation as ServantTaskMenuWidgetSpec.
		TStrongObjectPtr<UServantTaskMenuWidget> Menu;
		{
			FScopedAllowAbstractClassAllocation AllowAbstract;
			Menu.Reset(NewObject<UServantTaskMenuWidget>(World, UServantTaskMenuWidget::StaticClass(), NAME_None, RF_Transient));
		}
		if (!TestTrue(TEXT("harness: menu created"), Menu.IsValid())) { return; }

		TestTrue(TEXT("the menu implements IPlayerUIWidget"), Menu->Implements<UPlayerUIWidget>());
		TestNull(TEXT("precondition: no registry before InitializeForPlayer"), Menu->GetServantSubsystem());

		IPlayerUIWidget::Execute_InitializeForPlayer(Menu.Get(), PlayerController, nullptr);
		TestTrue(TEXT("registry plugged after the creation-time call"), Menu->GetServantSubsystem() == Subsystem);

		// Called again, as the controller does at OnPossess then OnUnPossess.
		IPlayerUIWidget::Execute_InitializeForPlayer(Menu.Get(), PlayerController, Pawn);
		IPlayerUIWidget::Execute_InitializeForPlayer(Menu.Get(), PlayerController, nullptr);
		TestTrue(TEXT("registry still the world's registry"), Menu->GetServantSubsystem() == Subsystem);

		int32 Bindings = 0;
		for (const UObject* Bound : Subsystem->OnAssignmentsChanged.GetAllObjects())
		{
			Bindings += (Bound == Menu.Get()) ? 1 : 0;
		}
		TestEqual(TEXT("after three InitializeForPlayer calls the menu listens to OnAssignmentsChanged exactly once"), Bindings, 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
