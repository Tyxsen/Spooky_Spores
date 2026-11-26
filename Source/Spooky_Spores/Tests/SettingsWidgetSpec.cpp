// Automation Specs for the settings UI and its first consumer: USettingFloatRowWidget (row), USettingsMenuWidget (menu) and the Character FOV.
// Source of truth: the settings menu spec (removed at merge), acceptance criteria 9 and 10 and its edge cases. Written from the spec only.
//
// Three specs in this file: "Row" (no world), "Menu" (test world + UGameInstance, see FSettingsTestGameInstance) and "CharacterFov" (same world).
// The abstract widgets are exercised through the concrete test subclasses of Tests/SettingsTestHelpers.h: real UMG objects (USlider, UTextBlock,
// UVerticalBox) are plugged into the BindWidget properties by reflection. Moving the slider = broadcasting USlider::OnValueChanged, releasing it =
// broadcasting OnMouseCaptureEnd (what Slate does). Slider positions are compared as a normalized alpha, so the tests hold whether the row maps
// the slider in actual units or in 0..1.
// Every test lists its mutation: the change of behaviour that must turn it red.
//
// Assumptions (spec gaps, see the report): SetupRow(USettingsSubsystem*, const FSettingDefinition&); NameText shows DisplayName; the row subscribes
// to OnSettingChanged in SetupRow and the slider handlers are bound there too; the menu creates its rows with a real CreateWidget that works
// without a LocalPlayer; the Character reads the subsystem through GetGameInstance(); GetFirstPersonCameraComponent() is public.
//
// Not automated: the real WBP_SettingsMenu / WBP_SettingRow_Float layout, the key O and the pause (editor-verifier / human PIE).
// The unsubscription of rows dropped by a rebuild IS tested: the menu calls ReleaseRow() explicitly (NativeDestruct never runs headless).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SettingsTestHelpers.h"
#include "Core/Character/Spooky_SporesCharacter.h"
#include "Settings/SettingsCatalog.h"
#include "Settings/SettingsMenuWidget.h"
#include "Settings/SettingRowWidget.h"
#include "Settings/SettingsSubsystem.h"
#include "UI/PlayerUIWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Tests/AutomationCommon.h"

namespace SettingsWidgetSpecPrivate
{
	const FName IdTestB(TEXT("Test.Volume"));

	int32 CountBound(const USettingsSubsystem* Subsystem, const UObject* Object)
	{
		int32 Count = 0;
		for (const UObject* Bound : Subsystem->OnSettingChanged.GetAllObjects())
		{
			Count += (Bound == Object) ? 1 : 0;
		}
		return Count;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Row
// ---------------------------------------------------------------------------------------------------------------------

BEGIN_DEFINE_SPEC(FSettingsRowSpec, "SpookySpores.Settings.Row",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	FString SlotName;
	SettingsTestPrivate::FRealSettingsFileGuard RealFileGuard;
	FSettingDefinition FovDef;
	bool bFovDefFound = false;
	FSettingDefinition DefB;
	TStrongObjectPtr<UGameInstance> BareGameInstance; // Outer of the subsystem (ClassWithin = UGameInstance), never Init()-ed
	TStrongObjectPtr<USettingsSubsystem> Subsystem;
	TStrongObjectPtr<USettingsTestListener> Listener;
	TStrongObjectPtr<UTestSettingFloatRow> Row;

	bool RequireHarness();
	UTestSettingFloatRow* MakeRow(const FSettingDefinition& Def);

END_DEFINE_SPEC(FSettingsRowSpec)

bool FSettingsRowSpec::RequireHarness()
{
	if (!bFovDefFound || !Subsystem.IsValid())
	{
		AddError(TEXT("Harness problem (FOV definition missing from the catalog or subsystem unavailable): the assertions of this test did not run."));
		return false;
	}
	return true;
}

UTestSettingFloatRow* FSettingsRowSpec::MakeRow(const FSettingDefinition& Def)
{
	FScopedAllowAbstractClassAllocation AllowAbstract;
	Row.Reset(NewObject<UTestSettingFloatRow>(GetTransientPackage(), UTestSettingFloatRow::StaticClass(), NAME_None, RF_Transient));
	if (Row.IsValid())
	{
		// Creates the bound widgets (CreateWidget would do it); before SetupRow, as in the game.
		Row->Initialize();
		TestTrue(TEXT("harness: the BindWidget properties ValueSlider / NameText / ValueText were found by name"), Row->bBindingsOk);
		Row->SetupRow(Subsystem.Get(), Def);
	}
	return Row.Get();
}

void FSettingsRowSpec::Define()
{
	using namespace SettingsWidgetSpecPrivate;

	BeforeEach([this]()
	{
		SlotName = SettingsTestPrivate::MakeUniqueSlotName(TEXT("SettingsRow"));
		RealFileGuard.Capture();
		bFovDefFound = SettingsTestPrivate::FindFovDefinition(FovDef);
		DefB = SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f);
		BareGameInstance.Reset(SettingsTestPrivate::NewBareGameInstance());
		Subsystem.Reset(NewObject<USettingsSubsystem>(BareGameInstance.Get()));
		Listener.Reset(NewObject<USettingsTestListener>());
		if (bFovDefFound)
		{
			Subsystem->InitializeFromDefinitions({ FovDef, DefB }, SlotName);
			Subsystem->OnSettingChanged.AddDynamic(Listener.Get(), &USettingsTestListener::HandleSettingChanged);
		}
	});

	AfterEach([this]()
	{
		Row.Reset();
		Listener.Reset();
		Subsystem.Reset();
		BareGameInstance.Reset();
		SettingsTestPrivate::DeleteSlot(SlotName);
		RealFileGuard.Verify(*this);
	});

	// Mutation: the slider is not synchronized at SetupRow (stays at its widget default 0) -> red; the slider bounds are not taken from the definition
	//           (the position is computed against a fixed 0..1 or 0..100 range) -> the alpha is wrong, red; NameText not filled -> red.
	It("Criterion 10: SetupRow shows the definition (name and slider position of the current value and a displayed value)", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }

		TestEqual(TEXT("NameText shows the DisplayName"), FovRow->TestNameText->GetText().ToString(), FovDef.DisplayName.ToString());
		TestNearlyEqual(TEXT("slider position is the current value inside [Min, Max]"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider),
		                SettingsTestPrivate::ExpectedAlpha(FovDef, Subsystem->GetFloat(SettingsCatalog::FOV)), 0.001f);
		TestFalse(TEXT("a value is displayed"), FovRow->TestValueText->GetText().IsEmpty());

		// A second row for a setting with other bounds: the mapping really comes from each definition.
		Subsystem->SetFloat(IdTestB, 0.75f);
		UTestSettingFloatRow* RowB = MakeRow(DefB);
		if (TestNotNull(TEXT("row B created"), RowB))
		{
			TestNearlyEqual(TEXT("row B slider position follows B's own bounds (0.75 in [0, 1])"), SettingsTestPrivate::SliderAlpha(RowB->TestSlider), 0.75f, 0.001f);
		}
	});

	// Mutation: the slider is not bound to SetFloat (GetFloat unchanged) -> red; the value goes through the subsystem with a wrong mapping (alpha ignored) -> red;
	//           the row also re-broadcasts when it resynchronizes (loop) -> 2 events instead of 1, red.
	It("Criterion 10: moving the slider changes GetFloat with exactly one broadcast (no feedback loop)", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }

		FovRow->TestSlider->OnValueChanged.Broadcast(SettingsTestPrivate::SliderValueAtAlpha(FovRow->TestSlider, 0.25f));
		const float Expected = FovDef.Min + 0.25f * (FovDef.Max - FovDef.Min);
		TestNearlyEqual(TEXT("GetFloat follows the slider (quarter of the range)"), Subsystem->GetFloat(SettingsCatalog::FOV), Expected, 0.01f);
		TestEqual(TEXT("exactly one OnSettingChanged for one slider move"), Listener->Num(), 1);

		// The same position reported again is not a change.
		FovRow->TestSlider->OnValueChanged.Broadcast(SettingsTestPrivate::SliderValueAtAlpha(FovRow->TestSlider, 0.25f));
		TestEqual(TEXT("same slider position again: no new event"), Listener->Num(), 1);
	});

	// Mutation: the row does not listen to OnSettingChanged -> the slider does not move, red; it listens without filtering on its Id ->
	//           the B change moves the FOV slider, red; it resyncs by calling SetFloat again with a new value (loop) -> extra event, red.
	It("Criterion 10: an outside SetFloat moves the slider and the displayed value only for its own Id and without a loop", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }
		const FString TextBefore = FovRow->TestValueText->GetText().ToString();
		const float AlphaBefore = SettingsTestPrivate::SliderAlpha(FovRow->TestSlider);

		Subsystem->SetFloat(SettingsCatalog::FOV, 105.f);
		TestNearlyEqual(TEXT("slider moved to the new value"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider), SettingsTestPrivate::ExpectedAlpha(FovDef, 105.f), 0.001f);
		TestTrue(TEXT("the slider really moved (the test can fail)"), !FMath::IsNearlyEqual(SettingsTestPrivate::SliderAlpha(FovRow->TestSlider), AlphaBefore));
		TestTrue(TEXT("displayed value changed"), FovRow->TestValueText->GetText().ToString() != TextBefore);
		TestEqual(TEXT("the row did not re-broadcast: exactly the one event of the outside change"), Listener->Num(), 1);

		const float AlphaAfter = SettingsTestPrivate::SliderAlpha(FovRow->TestSlider);
		const FString TextAfter = FovRow->TestValueText->GetText().ToString();
		Subsystem->SetFloat(IdTestB, 0.9f);
		TestNearlyEqual(TEXT("another setting's change does not move this slider"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider), AlphaAfter, 0.0001f);
		TestEqual(TEXT("nor change this displayed value"), FovRow->TestValueText->GetText().ToString(), TextAfter);
	});

	// Mutation: NativeDestruct does not unbind -> the row stays in the delegate (count 1) and keeps reacting, red.
	It("Criterion 10: the row unsubscribes from OnSettingChanged in NativeDestruct (and was subscribed once before)", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }
		TestEqual(TEXT("subscribed exactly once after SetupRow"), CountBound(Subsystem.Get(), FovRow), 1);

		FovRow->CallNativeDestruct();
		TestEqual(TEXT("no longer subscribed after NativeDestruct"), CountBound(Subsystem.Get(), FovRow), 0);

		const float AlphaBefore = SettingsTestPrivate::SliderAlpha(FovRow->TestSlider);
		Subsystem->SetFloat(SettingsCatalog::FOV, 105.f);
		TestNearlyEqual(TEXT("a destroyed row no longer reacts"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider), AlphaBefore, 0.0001f);
	});

	// Mutation: the row never saves on release -> no slot, red; it saves on every value change (spec: only at release) -> the slot exists before the release, red.
	It("Behaviour: the setting is written to disk when the slider is released and not while it moves", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }

		FovRow->TestSlider->OnValueChanged.Broadcast(SettingsTestPrivate::SliderValueAtAlpha(FovRow->TestSlider, 0.25f));
		TestFalse(TEXT("moving the slider writes no file"), SettingsTestPrivate::SlotExists(SlotName));

		FovRow->TestSlider->OnMouseCaptureEnd.Broadcast();
		if (TestTrue(TEXT("releasing the slider writes the file"), SettingsTestPrivate::SlotExists(SlotName)))
		{
			float Stored = 0.f;
			TestTrue(TEXT("the file holds an FOV line"), SettingsTestPrivate::ReadSlotValue(SlotName, SettingsCatalog::FOV, Stored));
			TestNearlyEqual(TEXT("with the released value"), Stored, FovDef.Min + 0.25f * (FovDef.Max - FovDef.Min), 0.01f);
		}
	});
	// ---- Slider snapping to the declared step (the mouse ignores the slider StepSize: the row snaps before storing) ----
	// (A SetValue equal to the current slider value broadcasts nothing: the case lists never repeat the value the handle is already on.)
	// The mouse is simulated by SetValue(raw) on the slider: without a Slate widget it stores the raw value and broadcasts OnValueChanged,
	// exactly the event chain of a drag. The slider therefore HOLDS the raw value until the row repositions it.

	// Mutation: the row stores the raw value (90.37) -> GetFloat is not 90, red; the text keeps decimals (90.37) -> red;
	//           the handle is left on the raw value (no reposition) -> alpha differs from the notch, red.
	It("Behaviour: a raw slider value 90.37 is stored as 90 and shown as 90 and the handle sits on the notch", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }

		FovRow->TestSlider->SetValue(90.37f);
		TestNearlyEqual(TEXT("stored value is the whole step"), Subsystem->GetFloat(SettingsCatalog::FOV), 90.f, 0.0001f);
		TestEqual(TEXT("displayed text has no decimals"), FovRow->TestValueText->GetText().ToString(), FString(TEXT("90")));
		TestNearlyEqual(TEXT("handle repositioned on the notch (normalized position)"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider),
		                SettingsTestPrivate::ExpectedAlpha(FovDef, 90.f), 0.0001f);
	});

	// Mutation: truncation instead of rounding (90.6 -> 90, 119.6 -> 119) -> red; rounding in the wrong direction -> red;
	//           the clamp is removed or the bounds are exclusive (120 or 60 moved inward) -> the exact-bound cases turn red.
	It("Behaviour: raw values round to the nearest notch and the bounds are reachable", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }
		struct FCase { float Raw; float Expected; };
		const FCase Cases[] = { { 90.6f, 91.f }, { 60.4f, 60.f }, { 119.6f, 120.f }, { 60.f, 60.f }, { 120.f, 120.f }, { 75.49f, 75.f } };
		for (const FCase& Case : Cases)
		{
			FovRow->TestSlider->SetValue(Case.Raw);
			TestNearlyEqual(*FString::Printf(TEXT("raw %f gives %f"), Case.Raw, Case.Expected), Subsystem->GetFloat(SettingsCatalog::FOV), Case.Expected, 0.0001f);
		}
	});

	// Mutation: the handle is repositioned only when the stored value changes (the SetValue after SetFloat is removed) -> the slider stays on 90.2, red;
	//           the row broadcasts although the snapped value equals the stored one -> the listener count moves, red.
	It("Behaviour: a raw value that snaps to the stored value broadcasts nothing but still repositions the handle", [this]()
	{
		if (!RequireHarness()) { return; }
		Subsystem->SetFloat(SettingsCatalog::FOV, 90.f);
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }
		Listener->Ids.Reset();
		Listener->Values.Reset();

		FovRow->TestSlider->SetValue(90.2f);
		TestNearlyEqual(TEXT("stored value unchanged"), Subsystem->GetFloat(SettingsCatalog::FOV), 90.f, 0.0001f);
		TestEqual(TEXT("no broadcast"), Listener->Num(), 0);
		TestNearlyEqual(TEXT("handle moved back onto the notch"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider),
		                SettingsTestPrivate::ExpectedAlpha(FovDef, 90.f), 0.0001f);
		TestEqual(TEXT("displayed text still 90"), FovRow->TestValueText->GetText().ToString(), FString(TEXT("90")));
	});

	// Mutation: the row broadcasts the raw value then the snapped one (two events) or loops through its own resync -> Num() != 1, red;
	//           the event carries 91.3 instead of 91 -> red.
	It("Behaviour: one real change gives exactly one broadcast carrying the snapped value", [this]()
	{
		if (!RequireHarness()) { return; }
		Subsystem->SetFloat(SettingsCatalog::FOV, 90.f);
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }
		Listener->Ids.Reset();
		Listener->Values.Reset();

		FovRow->TestSlider->SetValue(91.3f);
		if (TestEqual(TEXT("exactly one event"), Listener->Num(), 1))
		{
			TestTrue(TEXT("for the FOV"), Listener->Ids[0] == SettingsCatalog::FOV);
			TestNearlyEqual(TEXT("with the snapped value"), Listener->Values[0], 91.f, 0.0001f);
		}
	});

	// Mutation: the row snaps even when Step is 0 (rounds to whole numbers, or divides by zero) -> 0.37 is not kept, red;
	//           the text is forced to no decimals -> 0 or empty, red.
	It("Behaviour: a continuous setting (Step 0) keeps the raw value and its decimals", [this]()
	{
		if (!RequireHarness()) { return; }
		const FSettingDefinition Continuous = SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f, 0.f);
		Subsystem->InitializeFromDefinitions({ FovDef, Continuous }, SlotName);
		UTestSettingFloatRow* RowB = MakeRow(Continuous);
		if (!TestNotNull(TEXT("row created"), RowB)) { return; }

		RowB->TestSlider->SetValue(0.37f);
		TestNearlyEqual(TEXT("raw value stored as is"), Subsystem->GetFloat(IdTestB), 0.37f, 0.0001f);
		const FString Shown = RowB->TestValueText->GetText().ToString();
		TestFalse(TEXT("text not empty"), Shown.IsEmpty());
		TestTrue(TEXT("text keeps its decimals (not 0)"), Shown != TEXT("0") && Shown.StartsWith(TEXT("0")) && Shown.Len() > 1);
	});

	// Mutation: steps counted from 0 instead of Min (Min 2 Step 5: 4.6 -> 5 instead of 7) -> red; fixed step of 1 -> red.
	It("Behaviour: notches are counted from Min with a non unit step", [this]()
	{
		if (!RequireHarness()) { return; }
		const FSettingDefinition Step5 = SettingsTestPrivate::MakeDef(FName(TEXT("Test.Step5")), 60.f, 120.f, 90.f, 5.f);
		const FSettingDefinition Offset = SettingsTestPrivate::MakeDef(FName(TEXT("Test.Offset")), 2.f, 12.f, 2.f, 5.f);
		Subsystem->InitializeFromDefinitions({ FovDef, Step5, Offset }, SlotName);

		UTestSettingFloatRow* RowStep5 = MakeRow(Step5);
		if (!TestNotNull(TEXT("row Step5 created"), RowStep5)) { return; }
		RowStep5->TestSlider->SetValue(92.4f);
		TestNearlyEqual(TEXT("92.4 gives 90"), Subsystem->GetFloat(Step5.Id), 90.f, 0.0001f);
		RowStep5->TestSlider->SetValue(93.f);
		TestNearlyEqual(TEXT("93 gives 95"), Subsystem->GetFloat(Step5.Id), 95.f, 0.0001f);

		UTestSettingFloatRow* RowOffset = MakeRow(Offset);
		if (!TestNotNull(TEXT("row Offset created"), RowOffset)) { return; }
		RowOffset->TestSlider->SetValue(4.4f);
		TestNearlyEqual(TEXT("4.4 gives 2 (Min)"), Subsystem->GetFloat(Offset.Id), 2.f, 0.0001f);
		RowOffset->TestSlider->SetValue(4.6f);
		TestNearlyEqual(TEXT("4.6 gives 7 (Min plus one step)"), Subsystem->GetFloat(Offset.Id), 7.f, 0.0001f);
	});

	// Mutation: the clamp is removed -> a value above 10 or below 0 escapes, red;
	//           the notches are counted from 0 instead of Min or rounded wrongly -> not a multiple of the step from Min, red.
	// Max reachability is NOT asserted: a step that does not divide the range leaves the behaviour at Max unspecified.
	It("Behaviour: a step that does not divide the range always gives a value inside the bounds on a notch", [this]()
	{
		if (!RequireHarness()) { return; }
		const FSettingDefinition Odd = SettingsTestPrivate::MakeDef(FName(TEXT("Test.Odd")), 0.f, 10.f, 0.f, 3.f);
		Subsystem->InitializeFromDefinitions({ FovDef, Odd }, SlotName);
		UTestSettingFloatRow* RowOdd = MakeRow(Odd);
		if (!TestNotNull(TEXT("row created"), RowOdd)) { return; }

		const float Raws[] = { -4.f, 0.4f, 1.4f, 1.6f, 4.4f, 5.f, 7.4f, 8.9f, 9.9f, 10.f, 25.f };
		for (const float Raw : Raws)
		{
			RowOdd->TestSlider->SetValue(Raw);
			const float Stored = Subsystem->GetFloat(Odd.Id);
			TestTrue(*FString::Printf(TEXT("raw %f: stored %f is inside the bounds"), Raw, Stored), Stored >= 0.f && Stored <= 10.f);
			TestNearlyEqual(*FString::Printf(TEXT("raw %f: stored %f is a multiple of 3 from Min"), Raw, Stored), Stored, FMath::RoundToFloat(Stored / 3.f) * 3.f, 0.0001f);
		}
	});

	// Mutation: the text always uses the default formatting (75.4 shown with decimals) -> red; the text drops the decimals for a fractional step (Step 0.5) -> red.
	It("Behaviour: the displayed text has no decimals for a whole step and keeps them for a fractional step", [this]()
	{
		if (!RequireHarness()) { return; }
		UTestSettingFloatRow* FovRow = MakeRow(FovDef);
		if (!TestNotNull(TEXT("row created"), FovRow)) { return; }
		Subsystem->SetFloat(SettingsCatalog::FOV, 75.4f); // from outside: stored as is, only the display is formatted
		TestEqual(TEXT("whole step: 75.4 is displayed as 75"), FovRow->TestValueText->GetText().ToString(), FString(TEXT("75")));

		const FSettingDefinition Half = SettingsTestPrivate::MakeDef(FName(TEXT("Test.Half")), 60.f, 120.f, 90.f, 0.5f);
		Subsystem->InitializeFromDefinitions({ FovDef, Half }, SlotName);
		UTestSettingFloatRow* RowHalf = MakeRow(Half);
		if (!TestNotNull(TEXT("row Half created"), RowHalf)) { return; }
		Subsystem->SetFloat(Half.Id, 75.5f);
		const FString Shown = RowHalf->TestValueText->GetText().ToString();
		TestTrue(TEXT("fractional step: 75.5 keeps a decimal (culture neutral check)"), Shown.StartsWith(TEXT("75")) && Shown != TEXT("75") && Shown != TEXT("76"));
	});

}

// ---------------------------------------------------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------------------------------------------------

BEGIN_DEFINE_SPEC(FSettingsMenuSpec, "SpookySpores.Settings.Menu",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	FSettingsTestGameInstance GameInstance;
	FString SlotName;
	SettingsTestPrivate::FRealSettingsFileGuard RealFileGuard;
	FSettingDefinition FovDef;
	FSettingDefinition DefB;
	bool bReady = false;
	USettingsSubsystem* Settings = nullptr;
	APlayerController* PlayerController = nullptr;
	TStrongObjectPtr<UTestSettingsMenu> Menu;
	TStrongObjectPtr<USettingsTestListener> Listener;

	bool RequireHarness();
	// Replaces the definitions of the game instance's subsystem (the seam of the spec) and listens to it.
	void UseDefinitions(const TArray<FSettingDefinition>& Defs);
	void OpenMenu();

END_DEFINE_SPEC(FSettingsMenuSpec)

bool FSettingsMenuSpec::RequireHarness()
{
	if (!bReady || !Menu.IsValid())
	{
		AddError(TEXT("Harness problem (test world, game instance, subsystem, controller or menu unavailable): the assertions of this test did not run."));
		return false;
	}
	return true;
}

void FSettingsMenuSpec::UseDefinitions(const TArray<FSettingDefinition>& Defs)
{
	Settings->InitializeFromDefinitions(Defs, SlotName);
	Listener.Reset(NewObject<USettingsTestListener>());
	Settings->OnSettingChanged.AddDynamic(Listener.Get(), &USettingsTestListener::HandleSettingChanged);
}

void FSettingsMenuSpec::OpenMenu()
{
	IPlayerUIWidget::Execute_InitializeForPlayer(Menu.Get(), PlayerController, nullptr);
}

void FSettingsMenuSpec::Define()
{
	using namespace SettingsWidgetSpecPrivate;

	BeforeEach([this]()
	{
		bReady = false;
		SlotName = SettingsTestPrivate::MakeUniqueSlotName(TEXT("SettingsMenu"));
		RealFileGuard.Capture();
		DefB = SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f);
		Wrapper = MakeUnique<FTestWorldWrapper>();
		if (!Wrapper->CreateTestWorld(EWorldType::Game) || !Wrapper->BeginPlayInTestWorld())
		{
			return;
		}
		World = Wrapper->GetTestWorld();
		if (!SettingsTestPrivate::FindFovDefinition(FovDef) || !GameInstance.Setup(World))
		{
			return;
		}
		Settings = GameInstance.GetSettings();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlayerController = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
		if (Settings == nullptr || PlayerController == nullptr)
		{
			return;
		}
		UseDefinitions({ FovDef, DefB });
		{
			FScopedAllowAbstractClassAllocation AllowAbstract;
			Menu.Reset(NewObject<UTestSettingsMenu>(GameInstance.Instance.Get(), UTestSettingsMenu::StaticClass(), NAME_None, RF_Transient));
		}
		if (Menu.IsValid())
		{
			// CreateWidget(this, RowClass) requires the menu's WidgetTree, which only Initialize() creates (UserWidget.cpp, CreateWidgetInstance).
			// Initialize works without a LocalPlayer; the test menu also creates RowsContainer / RowClasses there.
			Menu->Initialize();
		}
		bReady = Menu.IsValid() && Menu->bBindingsOk;
	});

	AfterEach([this]()
	{
		Menu.Reset();
		Listener.Reset();
		Settings = nullptr;
		PlayerController = nullptr;
		GameInstance.Teardown(); // Deinitialize of the subsystem may write: only to the unique test slot, deleted below
		Wrapper.Reset();
		World = nullptr;
		bReady = false;
		SettingsTestPrivate::DeleteSlot(SlotName);
		RealFileGuard.Verify(*this);
	});

	// Mutation: the rows are appended without clearing RowsContainer -> 4 then 6 rows, red; one row per definition is not honoured (e.g. only the first) -> red;
	//           the subsystem is looked up in the world instead of the game instance -> no rows (the test subsystem is the game instance's), red.
	It("Criterion 10: InitializeForPlayer creates one row per valid definition and a second call leaves the same count", [this]()
	{
		if (!RequireHarness()) { return; }
		TestEqual(TEXT("no row before InitializeForPlayer"), Menu->GetRowCount(), 0);

		OpenMenu();
		TestEqual(TEXT("two valid definitions: two rows"), Menu->GetRowCount(), 2);
		TestNotNull(TEXT("a row shows the FOV"), Menu->FindRowByName(FovDef.DisplayName.ToString()));
		TestNotNull(TEXT("a row shows the second setting"), Menu->FindRowByName(DefB.DisplayName.ToString()));

		OpenMenu();
		TestEqual(TEXT("second call: still two rows (rebuild, idempotent)"), Menu->GetRowCount(), 2);
		OpenMenu();
		TestEqual(TEXT("third call: still two rows"), Menu->GetRowCount(), 2);
	});

	// Mutation: the invalid definitions are shown anyway (the menu iterates something else than the subsystem's valid definitions) -> 3 rows, red.
	It("Criterion 10: invalid definitions do not get a row", [this]()
	{
		if (!RequireHarness()) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		UseDefinitions({ FovDef, SettingsTestPrivate::MakeDef(FName(TEXT("Test.BadBounds")), 5.f, 5.f, 5.f), DefB });
		OpenMenu();
		TestEqual(TEXT("rows = valid definitions only"), Menu->GetRowCount(), 2);
	});

	// Mutation: a missing RowClass aborts the whole rebuild (return instead of continue) -> the row after it is missing, red;
	//           it is skipped silently -> the expected Error is not logged, red; a null row is added to the container -> the counts differ, red.
	It("Criterion 10: a type without RowClass skips only its own row with one error", [this]()
	{
		if (!RequireHarness()) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		const FSettingDefinition NoClass = SettingsTestPrivate::MakeDef(FName(TEXT("Test.UnknownType")), 0.f, 1.f, 0.5f, 0.f, static_cast<ESettingType>(200));
		UseDefinitions({ FovDef, NoClass, DefB }); // the type without a class sits BETWEEN two valid ones

		OpenMenu();
		TestEqual(TEXT("only the row of the unknown type is missing"), Menu->GetRowCount(), 2);
		TestNotNull(TEXT("the row before it exists"), Menu->FindRowByName(FovDef.DisplayName.ToString()));
		TestNotNull(TEXT("the row after it exists"), Menu->FindRowByName(DefB.DisplayName.ToString()));
		TestNull(TEXT("the unknown type has no row"), Menu->FindRowByName(NoClass.DisplayName.ToString()));
	});

	// Mutation: the mapping is ignored and a default row class is used -> 2 rows, red; no error per skipped row -> the expected count is not met, red.
	It("Criterion 10: with an empty RowClasses map no row is created and each definition logs an error", [this]()
	{
		if (!RequireHarness()) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
		if (!TestTrue(TEXT("harness: RowClasses cleared"), SettingsTestReflection::ClearRowClasses(Menu.Get()))) { return; }
		OpenMenu();
		TestEqual(TEXT("no row without a RowClass"), Menu->GetRowCount(), 0);
	});

	// Mutation: ClearRows does not call ReleaseRow (the old rows stay in the delegate: count 1) -> red;
	//           the new rows are not subscribed -> their count 0, red.
	It("Criterion 10: a rebuild unsubscribes the dropped rows and subscribes the new ones", [this]()
	{
		if (!RequireHarness()) { return; }
		OpenMenu();
		TArray<UTestSettingFloatRow*> OldRows;
		for (int32 Index = 0; Index < Menu->GetRowCount(); ++Index)
		{
			OldRows.Add(Menu->GetRowAt(Index));
		}
		if (!TestEqual(TEXT("first build: two rows"), OldRows.Num(), 2)) { return; }
		for (UTestSettingFloatRow* Old : OldRows)
		{
			if (!TestNotNull(TEXT("old row valid"), Old)) { return; }
			TestEqual(TEXT("first-build row is subscribed once"), CountBound(Settings, Old), 1);
		}

		OpenMenu(); // second build drops the first rows (no GC runs in between: the pointers stay valid)
		TestEqual(TEXT("second build: two rows"), Menu->GetRowCount(), 2);
		for (int32 Index = 0; Index < Menu->GetRowCount(); ++Index)
		{
			UTestSettingFloatRow* Current = Menu->GetRowAt(Index);
			TestFalse(TEXT("the second build made new rows"), OldRows.Contains(Current));
			TestEqual(TEXT("new row is subscribed once"), CountBound(Settings, Current), 1);
		}
		for (UTestSettingFloatRow* Old : OldRows)
		{
			TestEqual(TEXT("dropped row is no longer subscribed"), CountBound(Settings, Old), 0);
		}

		// Behavioural proof: a dropped row no longer follows the setting.
		UTestSettingFloatRow* DroppedFov = OldRows[0]->TestNameText->GetText().ToString() == FovDef.DisplayName.ToString() ? OldRows[0] : OldRows[1];
		const float AlphaBefore = SettingsTestPrivate::SliderAlpha(DroppedFov->TestSlider);
		Settings->SetFloat(SettingsCatalog::FOV, 62.f);
		TestNearlyEqual(TEXT("dropped FOV row's slider did not move"), SettingsTestPrivate::SliderAlpha(DroppedFov->TestSlider), AlphaBefore, 0.0001f);
	});

	// Mutation: rows share one handler / are not bound to their own Id (moving B's slider changes the FOV) -> red;
	//           the rebuilt rows are not wired (only the first build) -> red after the second OpenMenu;
	//           rows do not read the current value at creation -> the initial alpha line turns red.
	It("Criterion 10: after a rebuild each row drives its own setting and an outside SetFloat moves its slider", [this]()
	{
		if (!RequireHarness()) { return; }
		Settings->SetFloat(SettingsCatalog::FOV, 75.f);
		Listener->Ids.Reset();
		Listener->Values.Reset();

		OpenMenu();
		OpenMenu(); // rows used below come from the SECOND build
		UTestSettingFloatRow* FovRow = Menu->FindRowByName(FovDef.DisplayName.ToString());
		UTestSettingFloatRow* RowB = Menu->FindRowByName(DefB.DisplayName.ToString());
		if (!TestTrue(TEXT("both rows exist"), FovRow != nullptr && RowB != nullptr)) { return; }

		TestNearlyEqual(TEXT("the FOV row starts at the stored value (75), not the Default"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider),
		                SettingsTestPrivate::ExpectedAlpha(FovDef, 75.f), 0.001f);

		RowB->TestSlider->OnValueChanged.Broadcast(SettingsTestPrivate::SliderValueAtAlpha(RowB->TestSlider, 0.5f));
		TestNearlyEqual(TEXT("moving row B's slider sets B (middle of [0, 1])"), Settings->GetFloat(IdTestB), 0.5f, 0.01f);
		TestNearlyEqual(TEXT("and leaves the FOV alone"), Settings->GetFloat(SettingsCatalog::FOV), 75.f, 0.0001f);
		TestEqual(TEXT("one event, for B"), Listener->CountFor(IdTestB), 1);
		TestEqual(TEXT("no event for the FOV"), Listener->CountFor(SettingsCatalog::FOV), 0);

		FovRow->TestSlider->OnValueChanged.Broadcast(SettingsTestPrivate::SliderValueAtAlpha(FovRow->TestSlider, 0.75f));
		TestNearlyEqual(TEXT("moving the FOV row's slider sets the FOV"), Settings->GetFloat(SettingsCatalog::FOV), FovDef.Min + 0.75f * (FovDef.Max - FovDef.Min), 0.01f);

		Settings->SetFloat(SettingsCatalog::FOV, 62.f);
		TestNearlyEqual(TEXT("an outside SetFloat moves the FOV slider"), SettingsTestPrivate::SliderAlpha(FovRow->TestSlider), SettingsTestPrivate::ExpectedAlpha(FovDef, 62.f), 0.001f);
		TestNearlyEqual(TEXT("without moving row B's slider"), SettingsTestPrivate::SliderAlpha(RowB->TestSlider), 0.5f, 0.001f);
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// Character FOV
// ---------------------------------------------------------------------------------------------------------------------

BEGIN_DEFINE_SPEC(FSettingsCharacterFovSpec, "SpookySpores.Settings.CharacterFov",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	FSettingsTestGameInstance GameInstance;
	FString SlotName;
	SettingsTestPrivate::FRealSettingsFileGuard RealFileGuard;
	FSettingDefinition FovDef;
	bool bReady = false;
	USettingsSubsystem* Settings = nullptr;

	bool RequireHarness();
	// ASpooky_SporesCharacter is UCLASS(abstract): the concrete test subclass is spawned instead.
	ATestSettingsCharacter* SpawnCharacter(const TCHAR* Name);
	int32 CountBoundCharacters() const;

END_DEFINE_SPEC(FSettingsCharacterFovSpec)

bool FSettingsCharacterFovSpec::RequireHarness()
{
	if (!bReady)
	{
		AddError(TEXT("Harness problem (test world, game instance or subsystem unavailable): the assertions of this test did not run."));
	}
	return bReady;
}

ATestSettingsCharacter* FSettingsCharacterFovSpec::SpawnCharacter(const TCHAR* Name)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// SpawnActor runs BeginPlay before returning: what the Character does at BeginPlay is observable right after.
	return World->SpawnActor<ATestSettingsCharacter>(ATestSettingsCharacter::StaticClass(), FTransform(FVector(0.0, 0.0, 5000.0)), Params);
}

int32 FSettingsCharacterFovSpec::CountBoundCharacters() const
{
	int32 Count = 0;
	for (const UObject* Bound : Settings->OnSettingChanged.GetAllObjects())
	{
		Count += (Cast<ASpooky_SporesCharacter>(Bound) != nullptr) ? 1 : 0;
	}
	return Count;
}

void FSettingsCharacterFovSpec::Define()
{
	using namespace SettingsWidgetSpecPrivate;

	BeforeEach([this]()
	{
		bReady = false;
		SlotName = SettingsTestPrivate::MakeUniqueSlotName(TEXT("SettingsCharacter"));
		RealFileGuard.Capture();
		Wrapper = MakeUnique<FTestWorldWrapper>();
		if (!Wrapper->CreateTestWorld(EWorldType::Game) || !Wrapper->BeginPlayInTestWorld())
		{
			return;
		}
		World = Wrapper->GetTestWorld();
		if (!SettingsTestPrivate::FindFovDefinition(FovDef) || !GameInstance.Setup(World))
		{
			return;
		}
		Settings = GameInstance.GetSettings();
		if (Settings == nullptr)
		{
			return;
		}
		Settings->InitializeFromDefinitions({ FovDef, SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f) }, SlotName);
		bReady = true;
	});

	AfterEach([this]()
	{
		Settings = nullptr;
		GameInstance.Teardown();
		Wrapper.Reset();
		World = nullptr;
		bReady = false;
		SettingsTestPrivate::DeleteSlot(SlotName);
		RealFileGuard.Verify(*this);
	});

	// Mutation: the Character does not read the setting at BeginPlay (keeps the class default FOV) -> not 75, red;
	//           it does not listen to OnSettingChanged -> the 105 line stays at 75, red; it does not filter on the Id (B's change writes 0.8 as FOV) -> red;
	//           it applies the value to the arms' FOV or another component instead of the camera -> red.
	It("Criterion 9: at BeginPlay the camera FOV is the stored setting then follows SetFloat(FOV) and ignores other settings", [this]()
	{
		if (!RequireHarness()) { return; }
		Settings->SetFloat(SettingsCatalog::FOV, 75.f);
		ASpooky_SporesCharacter* Character = SpawnCharacter(TEXT("FovCharacter"));
		if (!TestNotNull(TEXT("character spawned"), Character)) { return; }
		const UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
		if (!TestNotNull(TEXT("character has a first person camera"), Camera)) { return; }

		TestNearlyEqual(TEXT("BeginPlay: camera FOV is the stored setting (75)"), Camera->FieldOfView, 75.f, 0.001f);

		Settings->SetFloat(SettingsCatalog::FOV, 105.f);
		TestNearlyEqual(TEXT("SetFloat(FOV, 105): camera FOV follows"), Camera->FieldOfView, 105.f, 0.001f);

		Settings->SetFloat(SettingsCatalog::FOV, 500.f);
		TestNearlyEqual(TEXT("a bounded value: the camera gets Max, not 500"), Camera->FieldOfView, FovDef.Max, 0.001f);

		Settings->SetFloat(IdTestB, 0.8f);
		TestNearlyEqual(TEXT("another setting changing does not touch the camera"), Camera->FieldOfView, FovDef.Max, 0.001f);
	});

	// Mutation: the Character only reads the setting in its constructor / never re-reads at BeginPlay -> the respawned pawn shows the class default, red.
	It("Criterion 9: a Character created after a change (respawn) takes the stored value", [this]()
	{
		if (!RequireHarness()) { return; }
		ASpooky_SporesCharacter* First = SpawnCharacter(TEXT("FovCharacterFirst"));
		if (!TestNotNull(TEXT("first character spawned"), First)) { return; }
		Settings->SetFloat(SettingsCatalog::FOV, 110.f);
		First->Destroy();

		ASpooky_SporesCharacter* Second = SpawnCharacter(TEXT("FovCharacterSecond"));
		if (!TestNotNull(TEXT("second character spawned"), Second)) { return; }
		TestNearlyEqual(TEXT("respawned camera FOV is the stored value (110)"), Second->GetFirstPersonCameraComponent()->FieldOfView, 110.f, 0.001f);
	});

	// Mutation: EndPlay does not unbind -> the character stays in the delegate (count 1) and its camera keeps following, red;
	//           EndPlay unbinds without calling Super (not observable here) -> not covered.
	It("Criterion 9: after EndPlay the Character no longer reacts to OnSettingChanged", [this]()
	{
		if (!RequireHarness()) { return; }
		Settings->SetFloat(SettingsCatalog::FOV, 75.f);
		ASpooky_SporesCharacter* Character = SpawnCharacter(TEXT("FovCharacterEnd"));
		if (!TestNotNull(TEXT("character spawned"), Character)) { return; }
		TStrongObjectPtr<UCameraComponent> Camera(Character->GetFirstPersonCameraComponent());
		if (!TestTrue(TEXT("harness: the character is subscribed while alive"), CountBoundCharacters() == 1)) { return; }

		Character->Destroy(); // runs EndPlay synchronously
		TestEqual(TEXT("unsubscribed after EndPlay"), CountBoundCharacters(), 0);

		Settings->SetFloat(SettingsCatalog::FOV, 105.f);
		TestNearlyEqual(TEXT("the ended character's camera keeps its last FOV"), Camera->FieldOfView, 75.f, 0.001f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
