// Automation Spec for USettingsSubsystem: bounding, broadcast, persistence (text file "Id=Value"), trust boundary on load, extensibility,
// invalid definitions. Source of truth: docs/specs/settings-menu.md (removed at merge), acceptance criteria 1 to 7 and its edge cases, with the
// updated persistence contract: a text file USettingsSubsystem::GetSettingsFilePath(Slot), one "Id=Value" line per setting (SanitizeFloat),
// instead of the USaveGame described in the spec.
//
// Every instance is created with NewObject (Outer = a bare UGameInstance, ClassWithin) + InitializeFromDefinitions(Defs, UniqueSlot) (the seam of the
// spec): the real "Settings" file of the player is never read or written. The slot name is unique per test, its file is deleted in AfterEach, and
// FRealSettingsFileGuard fails any test that would have touched the real file.
// Every test lists its mutation: the change of behaviour that must turn it red.
//
// Assumptions: GetDefinitions() returns only the valid definitions; for a duplicate Id the first definition is kept; SetFloat on an unknown Id logs
// at most a Warning; a SetFloat that changes nothing does not mark the data modified; a numeric value read from a truncated line ("FOV=1") is
// indistinguishable from a real one and is bounded like any number (-> Min), only a non-numeric / empty value falls back to the Default.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SettingsTestHelpers.h"
#include "Settings/SettingsCatalog.h"
#include "Settings/SettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace SettingsSubsystemSpecPrivate
{
	const FName IdTestB(TEXT("Test.Volume"));
	const FName IdUnknown(TEXT("Test.DoesNotExist"));
	const FString GhostKeyInFile(TEXT("Test.GhostKeyInFile"));

	// The 11 aberrant bytes that made the engine assert when the data went through a USaveGame (kept: same input, new persistence).
	TArray<uint8> ShortGarbage()
	{
		return { 0x00, 0xFF, 0x13, 0x37, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03 };
	}

	// A longer deterministic binary blob that contains '=' (0x3D) at regular places, high bytes, NULs and a fake "FOV" key.
	TArray<uint8> LongBinaryWithEquals()
	{
		TArray<uint8> Bytes;
		for (int32 Index = 0; Index < 4096; ++Index)
		{
			Bytes.Add((Index % 17 == 0) ? static_cast<uint8>('=') : static_cast<uint8>((Index * 37 + 11) & 0xFF));
		}
		const FString Fake(TEXT("FOV="));
		for (const TCHAR Character : Fake)
		{
			Bytes.Add(static_cast<uint8>(Character));
		}
		Bytes.Add(0xFF);
		Bytes.Add(0xFE);
		return Bytes;
	}
}

BEGIN_DEFINE_SPEC(FSettingsSubsystemSpec, "SpookySpores.Settings.Subsystem",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	FString SlotName;
	FSettingDefinition FovDef;
	bool bFovDefFound = false;
	SettingsTestPrivate::FRealSettingsFileGuard RealFileGuard;
	TStrongObjectPtr<UGameInstance> BareGameInstance; // Outer of the subsystems (ClassWithin = UGameInstance), never Init()-ed
	TArray<TStrongObjectPtr<USettingsSubsystem>> Instances;
	TStrongObjectPtr<USettingsTestListener> Listener;

	USettingsSubsystem* MakeSubsystem(const TArray<FSettingDefinition>& Defs);
	USettingsSubsystem* MakeCatalogSubsystem();
	void Listen(USettingsSubsystem* Subsystem);
	// Writes {FOV=ForgedText, ghost key} as the settings file, loads it with a fresh subsystem and checks the FOV value read.
	void ExpectForgedFov(const FString& What, const FString& ForgedText, float Expected);
	// Writes the given bytes as the settings file: no crash, FOV = Expected, and the subsystem stays usable (Set + Save + a new instance rereads it).
	void ExpectCorruptFile(const FString& What, const TArray<uint8>& Bytes, float Expected);
	// Defs = [FOV, Bad, B]: the invalid entry must be ignored, the two valid ones usable (exactly one Error is declared by the caller).
	void ExpectInvalidIgnored(const FSettingDefinition& Bad);

END_DEFINE_SPEC(FSettingsSubsystemSpec)

USettingsSubsystem* FSettingsSubsystemSpec::MakeSubsystem(const TArray<FSettingDefinition>& Defs)
{
	USettingsSubsystem* Subsystem = NewObject<USettingsSubsystem>(BareGameInstance.Get());
	Instances.Add(TStrongObjectPtr<USettingsSubsystem>(Subsystem));
	Subsystem->InitializeFromDefinitions(Defs, SlotName);
	return Subsystem;
}

USettingsSubsystem* FSettingsSubsystemSpec::MakeCatalogSubsystem()
{
	return MakeSubsystem(SettingsCatalog::GetDefinitions());
}

void FSettingsSubsystemSpec::Listen(USettingsSubsystem* Subsystem)
{
	Listener.Reset(NewObject<USettingsTestListener>());
	Subsystem->OnSettingChanged.AddDynamic(Listener.Get(), &USettingsTestListener::HandleSettingChanged);
}

void FSettingsSubsystemSpec::ExpectForgedFov(const FString& What, const FString& ForgedText, float Expected)
{
	using namespace SettingsSubsystemSpecPrivate;
	const TArray<FString> Lines = { FString::Printf(TEXT("%s=%s"), *SettingsCatalog::FOV.ToString(), *ForgedText), GhostKeyInFile + TEXT("=123") };
	if (!TestTrue(FString::Printf(TEXT("%s: harness, the forged file was written"), *What), SettingsTestPrivate::ForgeLines(SlotName, Lines)))
	{
		return;
	}
	USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
	TestNearlyEqual(FString::Printf(TEXT("%s: FOV read from the forged file"), *What), Subsystem->GetFloat(SettingsCatalog::FOV), Expected, 0.0001f);
}

void FSettingsSubsystemSpec::ExpectCorruptFile(const FString& What, const TArray<uint8>& Bytes, float Expected)
{
	if (!TestTrue(FString::Printf(TEXT("%s: harness, the corrupt file was written"), *What), SettingsTestPrivate::ForgeBytes(SlotName, Bytes)))
	{
		return;
	}
	USettingsSubsystem* Subsystem = MakeCatalogSubsystem(); // must not crash
	TestNearlyEqual(FString::Printf(TEXT("%s: FOV read"), *What), Subsystem->GetFloat(SettingsCatalog::FOV), Expected, 0.0001f);

	// Still usable: the next save replaces the corrupt content by a valid file that a new instance rereads.
	TestTrue(FString::Printf(TEXT("%s: SetFloat still works"), *What), Subsystem->SetFloat(SettingsCatalog::FOV, 75.f));
	TestTrue(FString::Printf(TEXT("%s: SaveSettings still writes"), *What), Subsystem->SaveSettings());
	USettingsSubsystem* Reloaded = MakeCatalogSubsystem();
	TestNearlyEqual(FString::Printf(TEXT("%s: a new instance rereads the saved value"), *What), Reloaded->GetFloat(SettingsCatalog::FOV), 75.f, 0.0001f);
}

void FSettingsSubsystemSpec::ExpectInvalidIgnored(const FSettingDefinition& Bad)
{
	using namespace SettingsSubsystemSpecPrivate;
	const FSettingDefinition DefB = SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f);
	USettingsSubsystem* Subsystem = MakeSubsystem({ FovDef, Bad, DefB });

	const TArray<FSettingDefinition> Valid = Subsystem->GetDefinitions();
	TestEqual(TEXT("only the two valid definitions are exposed"), Valid.Num(), 2);
	int32 FovCount = 0;
	int32 BCount = 0;
	for (const FSettingDefinition& Def : Valid)
	{
		FovCount += (Def.Id == FovDef.Id) ? 1 : 0;
		BCount += (Def.Id == IdTestB) ? 1 : 0;
	}
	TestEqual(TEXT("the valid FOV definition stays (once)"), FovCount, 1);
	TestEqual(TEXT("the valid definition after the invalid one stays (once)"), BCount, 1);

	// The valid neighbours are fully usable.
	TestNearlyEqual(TEXT("valid B starts at its Default"), Subsystem->GetFloat(IdTestB), 0.25f, 0.0001f);
	TestTrue(TEXT("SetFloat on valid B returns true"), Subsystem->SetFloat(IdTestB, 0.5f));
	TestNearlyEqual(TEXT("valid B stored"), Subsystem->GetFloat(IdTestB), 0.5f, 0.0001f);
	TestTrue(TEXT("SetFloat on FOV returns true"), Subsystem->SetFloat(FovDef.Id, 500.f));
	TestNearlyEqual(TEXT("FOV still bounded by its own (first) definition"), Subsystem->GetFloat(FovDef.Id), FovDef.Max, 0.0001f);
}

void FSettingsSubsystemSpec::Define()
{
	using namespace SettingsSubsystemSpecPrivate;

	BeforeEach([this]()
	{
		SlotName = SettingsTestPrivate::MakeUniqueSlotName(TEXT("SettingsSubsystem"));
		RealFileGuard.Capture();
		bFovDefFound = SettingsTestPrivate::FindFovDefinition(FovDef);
		if (!bFovDefFound)
		{
			AddError(TEXT("SettingsCatalog::GetDefinitions() has no definition with Id SettingsCatalog::FOV: the spec requires one."));
		}
		BareGameInstance.Reset(SettingsTestPrivate::NewBareGameInstance());
		Instances.Empty();
		Listener.Reset();
	});

	AfterEach([this]()
	{
		Listener.Reset();
		Instances.Empty();
		BareGameInstance.Reset();
		SettingsTestPrivate::DeleteSlot(SlotName);
		RealFileGuard.Verify(*this);
	});

	// Mutation: Default is not applied when the file is absent (value 0 or Min) -> red;
	//           the catalog Default of the FOV is changed away from the 90 of the spec/README -> the literal 90 line turns red.
	It("Criterion 1: without a file GetFloat(FOV) is the Default (90)", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("catalog FOV Default is 90 (spec)"), FovDef.Default, 90.f, 0.0001f);
		TestNearlyEqual(TEXT("first launch: GetFloat(FOV) is the Default"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);
		TestNearlyEqual(TEXT("first launch: GetFloat(FOV) is 90"), Subsystem->GetFloat(SettingsCatalog::FOV), 90.f, 0.0001f);
		TestFalse(TEXT("first launch: reading creates no file"), SettingsTestPrivate::SlotExists(SlotName));
	});

	// Mutation: the clamp is removed (500 stored as is) -> red; the clamp uses only one bound (-5 stored as is) -> red.
	It("Criterion 2: SetFloat with 500 gives Max and SetFloat with -5 gives Min and both return true", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();

		TestTrue(TEXT("SetFloat(500) returns true"), Subsystem->SetFloat(SettingsCatalog::FOV, 500.f));
		TestNearlyEqual(TEXT("500 is bounded to Max"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Max, 0.0001f);
		TestTrue(TEXT("SetFloat(-5) returns true"), Subsystem->SetFloat(SettingsCatalog::FOV, -5.f));
		TestNearlyEqual(TEXT("-5 is bounded to Min"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Min, 0.0001f);
	});

	// Mutation: the clamp is exclusive (Min or Max moved inward) -> red; an in-range value is altered (rounded, clamped to Default...) -> red.
	It("Criterion 2 (edge): exact bounds and an in-range value are stored unchanged", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		const float InRange = 75.f;

		TestTrue(TEXT("in-range value is inside the bounds (harness)"), InRange > FovDef.Min && InRange < FovDef.Max);
		Subsystem->SetFloat(SettingsCatalog::FOV, InRange);
		TestNearlyEqual(TEXT("in-range value stored as is"), Subsystem->GetFloat(SettingsCatalog::FOV), InRange, 0.0001f);
		Subsystem->SetFloat(SettingsCatalog::FOV, FovDef.Max);
		TestNearlyEqual(TEXT("exactly Max stays Max"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Max, 0.0001f);
		Subsystem->SetFloat(SettingsCatalog::FOV, FovDef.Min);
		TestNearlyEqual(TEXT("exactly Min stays Min"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Min, 0.0001f);
	});

	// Mutation: an unknown Id is stored (GetFloat returns 75 afterwards) -> red; it returns true, or broadcasts -> red;
	//           it writes into the known entry -> the FOV line turns red; the unknown Id is written by SaveSettings -> the file line exists, red.
	It("Criterion 3: SetFloat on an unknown Id returns false and broadcasts nothing and creates no entry", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Listen(Subsystem);

		TestFalse(TEXT("SetFloat(unknown) returns false"), Subsystem->SetFloat(IdUnknown, 75.f));
		TestEqual(TEXT("nothing broadcast"), Listener->Num(), 0);
		TestNearlyEqual(TEXT("no entry created: GetFloat(unknown) is still 0 (spec)"), Subsystem->GetFloat(IdUnknown), 0.f, 0.0001f);
		TestNearlyEqual(TEXT("the known FOV is untouched"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);

		// The persisted data must not contain it either: SaveSettings writes only the declared settings.
		TestTrue(TEXT("SaveSettings writes"), Subsystem->SaveSettings());
		float Stored = 0.f;
		TestFalse(TEXT("the unknown Id is not in the saved file"), SettingsTestPrivate::ReadSlotValue(SlotName, IdUnknown, Stored));
		TestTrue(TEXT("the declared FOV is in the saved file"), SettingsTestPrivate::ReadSlotValue(SlotName, SettingsCatalog::FOV, Stored));
		TestEqual(TEXT("still nothing broadcast after SaveSettings"), Listener->Num(), 0);
	});

	// Mutation: SetFloat always broadcasts -> the repeated-value lines turn red; the comparison is done on the raw value instead of the bounded one
	//           (9999 after Max) -> red; the event carries the raw value (500) -> red; the broadcast is skipped on a real change -> red.
	It("Criterion 4: OnSettingChanged is broadcast once per real change with the bounded value and never when it is unchanged", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Listen(Subsystem);

		Subsystem->SetFloat(SettingsCatalog::FOV, FovDef.Default);
		TestEqual(TEXT("setting the current value (the Default at startup) does not broadcast"), Listener->Num(), 0);

		Subsystem->SetFloat(SettingsCatalog::FOV, 75.f);
		if (TestEqual(TEXT("one event for a real change"), Listener->Num(), 1))
		{
			TestTrue(TEXT("event Id is FOV"), Listener->Ids[0] == SettingsCatalog::FOV);
			TestNearlyEqual(TEXT("event value"), Listener->Values[0], 75.f, 0.0001f);
		}
		Subsystem->SetFloat(SettingsCatalog::FOV, 75.f);
		TestEqual(TEXT("same value again: no event"), Listener->Num(), 1);

		Subsystem->SetFloat(SettingsCatalog::FOV, 500.f);
		if (TestEqual(TEXT("clamped change: one more event"), Listener->Num(), 2))
		{
			TestNearlyEqual(TEXT("the event carries the BOUNDED value (Max), not 500"), Listener->Values[1], FovDef.Max, 0.0001f);
		}
		Subsystem->SetFloat(SettingsCatalog::FOV, 500.f);
		Subsystem->SetFloat(SettingsCatalog::FOV, 9999.f);
		TestEqual(TEXT("different raw values that bound to the current one: no event"), Listener->Num(), 2);

		Subsystem->SetFloat(SettingsCatalog::FOV, -5.f);
		if (TestEqual(TEXT("change to Min: one more event"), Listener->Num(), 3))
		{
			TestNearlyEqual(TEXT("the event carries Min"), Listener->Values[2], FovDef.Min, 0.0001f);
		}
		Subsystem->SetFloat(SettingsCatalog::FOV, -100.f);
		TestEqual(TEXT("below Min again: no event"), Listener->Num(), 3);
	});

	// Mutation: ResetToDefault writes the value without going through SetFloat (no broadcast, or broadcast even when already at Default) -> red;
	//           it resets to Min instead of Default -> red.
	It("API: ResetToDefault restores the Default through SetFloat (one event) and is silent when already at Default", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Subsystem->SetFloat(SettingsCatalog::FOV, 75.f);
		Listen(Subsystem);

		Subsystem->ResetToDefault(SettingsCatalog::FOV);
		TestNearlyEqual(TEXT("value is the Default again"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);
		if (TestEqual(TEXT("one event"), Listener->Num(), 1))
		{
			TestNearlyEqual(TEXT("event carries the Default"), Listener->Values[0], FovDef.Default, 0.0001f);
		}
		Subsystem->ResetToDefault(SettingsCatalog::FOV);
		TestEqual(TEXT("already at Default: no event"), Listener->Num(), 1);
	});

	// Mutation: SetFloat writes to disk (the spec forbids it) -> the file exists, red.
	It("Criterion 5 (edge): SetFloat alone writes no file", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Subsystem->SetFloat(SettingsCatalog::FOV, 75.f);
		Subsystem->SetFloat(SettingsCatalog::FOV, 100.f);
		TestFalse(TEXT("no file on disk after SetFloat"), SettingsTestPrivate::SlotExists(SlotName));
	});

	// Mutation: SaveSettings writes nothing / returns false -> red; the value written is the Default instead of the current one -> red;
	//           the load ignores the file -> the new instance reads 90, red; the line format is not "Id=Value" -> the ReadSlotValue line turns red.
	It("Criterion 5: SetFloat then SaveSettings then a new subsystem on the same slot reads the same value", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* First = MakeCatalogSubsystem();
		First->SetFloat(SettingsCatalog::FOV, 75.f);
		TestTrue(TEXT("SaveSettings returns true (written)"), First->SaveSettings());
		TestTrue(TEXT("the file now exists"), SettingsTestPrivate::SlotExists(SlotName));
		float OnDisk = 0.f;
		if (TestTrue(TEXT("the file holds an FOV=<value> line"), SettingsTestPrivate::ReadSlotValue(SlotName, SettingsCatalog::FOV, OnDisk)))
		{
			TestNearlyEqual(TEXT("with the saved value"), OnDisk, 75.f, 0.0001f);
		}

		USettingsSubsystem* Second = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("new instance reads the saved value"), Second->GetFloat(SettingsCatalog::FOV), 75.f, 0.0001f);
		TestTrue(TEXT("and it is not the Default (the test can fail)"), !FMath::IsNearlyEqual(Second->GetFloat(SettingsCatalog::FOV), FovDef.Default));
	});

	// Mutation: Deinitialize does not save (the safety net is missing) -> the new instance reads 90, red.
	It("Criterion 5: SetFloat alone then Deinitialize: the value is read back by a new subsystem", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* First = MakeCatalogSubsystem();
		First->SetFloat(SettingsCatalog::FOV, 105.f);
		First->Deinitialize();

		USettingsSubsystem* Second = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("value persisted by Deinitialize"), Second->GetFloat(SettingsCatalog::FOV), 105.f, 0.0001f);
	});

	// Mutation: Deinitialize always writes -> the file exists after part A, or the pre-existing file is rewritten (its extra line disappears) in part B, red;
	//           the modified flag is set before the equality check (SetFloat of the current value marks dirty) -> part C red.
	It("Edge case: Deinitialize writes nothing when nothing changed (no file created and existing file left byte for byte)", [this]()
	{
		if (!bFovDefFound) { return; }
		// A: untouched, no file before -> no file after.
		USettingsSubsystem* Untouched = MakeCatalogSubsystem();
		Untouched->Deinitialize();
		TestFalse(TEXT("untouched: no file written"), SettingsTestPrivate::SlotExists(SlotName));

		// B: an existing file with a line a rewrite would drop; loaded, nothing changed -> content identical after Deinitialize.
		const FString Original = FString::Printf(TEXT("%s=100\nTest.Custom=keep-me\n"), *SettingsCatalog::FOV.ToString());
		if (!TestTrue(TEXT("harness: file forged"), SettingsTestPrivate::ForgeText(SlotName, Original))) { return; }
		USettingsSubsystem* Loaded = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("harness: the forged value was loaded"), Loaded->GetFloat(SettingsCatalog::FOV), 100.f, 0.0001f);
		Loaded->Deinitialize();
		FString After;
		TestTrue(TEXT("the file still exists"), SettingsTestPrivate::ReadSlotText(SlotName, After));
		TestEqual(TEXT("and was not rewritten"), After, Original);

		// C: SetFloat of the current value is not a modification.
		SettingsTestPrivate::DeleteSlot(SlotName);
		USettingsSubsystem* NoOp = MakeCatalogSubsystem();
		NoOp->SetFloat(SettingsCatalog::FOV, FovDef.Default);
		NoOp->SetFloat(SettingsCatalog::FOV, FovDef.Default);
		NoOp->Deinitialize();
		TestFalse(TEXT("SetFloat(current value) then Deinitialize: still nothing written"), SettingsTestPrivate::SlotExists(SlotName));
	});

	// Mutation: the load trusts the file (NaN kept as is) -> GetFloat is NaN, red; NaN is clamped (stays NaN or becomes Min) instead of Default -> red.
	It("Criterion 6: a forged FOV = NaN is read as Default (unknown key ignored and no error)", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("nan"), TEXT("nan"), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("NaN (mixed case)"), TEXT("NaN"), FovDef.Default);
	});

	// Mutation: infinity is clamped to Max (-inf to Min) instead of replaced by Default -> red.
	It("Criterion 6: a forged FOV = inf or -inf is read as Default and not as Max or Min", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("inf"), TEXT("inf"), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("+inf"), TEXT("+inf"), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("-inf"), TEXT("-inf"), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("infinity"), TEXT("Infinity"), FovDef.Default);
	});

	// Mutation: the load does not bound (9999 or -5 kept) -> red; out-of-range replaced by Default instead of bounded -> red.
	It("Criterion 6: a forged FOV out of bounds (9999 or -5) is bounded to Max or Min and not replaced by Default", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("9999"), TEXT("9999"), FovDef.Max);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("-5"), TEXT("-5"), FovDef.Min);
	});

	// Mutation: a non-numeric value is parsed with Atof (0 -> clamped to Min) instead of rejected -> reads Min (60) instead of Default (90), red.
	//           (Needs Min != 0, which holds for the FOV: otherwise the Atof trap would be invisible.)
	It("Criterion 6: a non-numeric or empty FOV value is read as Default and not as Min (Atof trap)", [this]()
	{
		if (!bFovDefFound) { return; }
		TestTrue(TEXT("harness: Min is not 0, so a 0 from Atof would be visible"), !FMath::IsNearlyZero(FovDef.Min) && !FMath::IsNearlyEqual(FovDef.Min, FovDef.Default));
		ExpectForgedFov(TEXT("abc"), TEXT("abc"), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("empty value"), TEXT(""), FovDef.Default);
	});

	// Mutation: the key lookup defaults to 0 / Min when absent instead of Default -> red.
	It("Criterion 6: a file without the FOV key (only an unknown key) reads the Default", [this]()
	{
		if (!bFovDefFound) { return; }
		using namespace SettingsSubsystemSpecPrivate;
		if (!TestTrue(TEXT("harness: forged file written"), SettingsTestPrivate::ForgeLines(SlotName, { GhostKeyInFile + TEXT("=123") }))) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("absent key: Default"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);
		TestNearlyEqual(TEXT("unknown key from the file is not adopted as a setting"), Subsystem->GetFloat(FName(*GhostKeyInFile)), 0.f, 0.0001f);
		TestFalse(TEXT("and it cannot be set either"), Subsystem->SetFloat(FName(*GhostKeyInFile), 50.f));
		TestEqual(TEXT("loading created no extra definition"), Subsystem->GetDefinitions().Num(), SettingsCatalog::GetDefinitions().Num());
	});

	// Mutation: the key is turned into an FName (the original crash class: FName max length 1023) -> the engine asserts, or the unknown key is adopted -> red.
	It("Criterion 6: an unknown key of 3000 characters is ignored and the valid FOV line is still read", [this]()
	{
		if (!bFovDefFound) { return; }
		const FString LongKey = FString::ChrN(3000, TEXT('K'));
		if (!TestTrue(TEXT("harness: forged file written"), SettingsTestPrivate::ForgeLines(SlotName, { LongKey + TEXT("=5"), SettingsCatalog::FOV.ToString() + TEXT("=100") }))) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("FOV read next to the huge key"), Subsystem->GetFloat(SettingsCatalog::FOV), 100.f, 0.0001f);
	});

	// Control of the forged-file tests: a valid forged value MUST be read, otherwise every "reads Default" assertion could pass because the file is
	// never found (wrong path), not because the trust boundary works.
	// Mutation: the load never reads the file -> 100 not read, red; the exact bounds are moved inward -> red.
	It("Criterion 6 (control): a forged in-range FOV and the exact Min and Max are read back as forged", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("in-range 100"), TEXT("100"), 100.f);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("exact Min"), FString::SanitizeFloat(FovDef.Min), FovDef.Min);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("exact Max"), FString::SanitizeFloat(FovDef.Max), FovDef.Max);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectForgedFov(TEXT("fractional 99.5"), TEXT("99.5"), 99.5f);
	});

	// Corrupt files: no crash (the first version of this test found an engine assert on aberrant bytes), Default, and the subsystem stays usable.
	// Mutation (all): a corrupt file aborts / crashes the load, or the failure leaves the values at 0 -> red; a later SaveSettings refuses to
	// overwrite the corrupt file -> the reload line turns red.
	It("Edge case: aberrant bytes (11 bytes) give the defaults with no crash and the subsystem stays usable", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectCorruptFile(TEXT("11 aberrant bytes"), SettingsSubsystemSpecPrivate::ShortGarbage(), FovDef.Default);
	});

	It("Edge case: a long binary file full of equal signs gives the defaults with no crash and the subsystem stays usable", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectCorruptFile(TEXT("4 KB binary with '='"), SettingsSubsystemSpecPrivate::LongBinaryWithEquals(), FovDef.Default);
	});

	It("Edge case: an empty file and a whitespace-only file give the defaults and stay usable", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectCorruptFile(TEXT("empty file"), TArray<uint8>(), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		const FString Blank(TEXT("   \n\t\r\n  \n"));
		FTCHARToUTF8 Utf8(*Blank);
		TArray<uint8> BlankBytes;
		BlankBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		ExpectCorruptFile(TEXT("blank file"), BlankBytes, FovDef.Default);
	});

	// A file cut in the middle of a line. "FO" and "FOV" (no '=') and "FOV=" (no value) carry no number: Default.
	// "FOV=1" carries a valid number (1): per contract it is bounded like any value (-> Min), a truncation cannot be told from a real "1".
	// Mutation: a line without '=' crashes or is taken as a key -> red; an empty value read as 0 (Atof) -> "FOV=" gives Min, red;
	//           the numeric prefix is rejected or replaced by Default -> the "FOV=1" line turns red.
	It("Edge case: a file truncated mid-line (FO or FOV or FOV= or FOV=1) does not crash and is read per contract", [this]()
	{
		if (!bFovDefFound) { return; }
		const auto ToBytes = [](const FString& Text)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return Bytes;
		};
		ExpectCorruptFile(TEXT("FO"), ToBytes(TEXT("FO")), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectCorruptFile(TEXT("FOV"), ToBytes(SettingsCatalog::FOV.ToString()), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectCorruptFile(TEXT("FOV="), ToBytes(SettingsCatalog::FOV.ToString() + TEXT("=")), FovDef.Default);
		SettingsTestPrivate::DeleteSlot(SlotName);
		ExpectCorruptFile(TEXT("FOV=1 cut mid-number"), ToBytes(SettingsCatalog::FOV.ToString() + TEXT("=1")), FovDef.Min);
	});

	// Non-regression: the player's real "Settings" file is never touched. The guard of BeforeEach / AfterEach checks every test;
	// this one runs the whole life cycle (set, save, forge, corrupt, deinitialize) and checks the slot name is not the default one.
	// Mutation: a test helper or the subsystem falls back to the default slot name ("Settings") -> the real file changes, red.
	It("Non-regression: a full life cycle on a unique slot never touches the real Settings file", [this]()
	{
		if (!bFovDefFound) { return; }
		TestTrue(TEXT("the slot is not the default one"), SlotName != TEXT("Settings"));
		TestTrue(TEXT("the file path of the test slot is not the default file"), SettingsTestPrivate::SlotFile(SlotName) != SettingsTestPrivate::SlotFile(TEXT("Settings")));

		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Subsystem->SetFloat(SettingsCatalog::FOV, 75.f);
		Subsystem->SaveSettings();
		Subsystem->SetFloat(SettingsCatalog::FOV, 100.f);
		Subsystem->Deinitialize();
		TestTrue(TEXT("the unique slot file was written"), SettingsTestPrivate::SlotExists(SlotName));
		RealFileGuard.Verify(*this); // explicit check now, AfterEach checks again
	});

	// Mutation: the bound is hardcoded to the FOV (60..120) -> B's value 5 stays 5 or becomes 120, red; the broadcast is filtered on the FOV Id
	//           -> no B event, red; one shared value instead of a per-Id value -> setting B moves the FOV, red.
	It("Criterion 7: a second setting (other Id and other bounds) is bounded and broadcast and isolated without changing the subsystem", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeSubsystem({ FovDef, SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f) });
		Listen(Subsystem);

		TestNearlyEqual(TEXT("B starts at its own Default"), Subsystem->GetFloat(IdTestB), 0.25f, 0.0001f);
		TestNearlyEqual(TEXT("FOV starts at its own Default"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);

		TestTrue(TEXT("SetFloat(B, 5) returns true"), Subsystem->SetFloat(IdTestB, 5.f));
		TestNearlyEqual(TEXT("B bounded to its own Max (1), not the FOV one"), Subsystem->GetFloat(IdTestB), 1.f, 0.0001f);
		Subsystem->SetFloat(IdTestB, -3.f);
		TestNearlyEqual(TEXT("B bounded to its own Min (0)"), Subsystem->GetFloat(IdTestB), 0.f, 0.0001f);
		TestNearlyEqual(TEXT("FOV untouched by B"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);

		if (TestEqual(TEXT("two events for B"), Listener->Num(), 2))
		{
			TestTrue(TEXT("events carry the Id of B"), Listener->Ids[0] == IdTestB && Listener->Ids[1] == IdTestB);
			TestNearlyEqual(TEXT("first event value is the bounded 1"), Listener->Values[0], 1.f, 0.0001f);
			TestNearlyEqual(TEXT("second event value is the bounded 0"), Listener->Values[1], 0.f, 0.0001f);
		}
		Subsystem->SetFloat(IdTestB, -7.f);
		TestEqual(TEXT("B already at Min: no event"), Listener->Num(), 2);
	});

	// Mutation: the save/load code only handles the FOV key (hardcoded) -> B comes back as its Default, red;
	//           the NaN replacement uses the FOV Default (90) instead of the Default of the setting being loaded -> B reads 90, red.
	It("Criterion 7: a second setting survives a round trip and a forged file independently of the FOV", [this]()
	{
		if (!bFovDefFound) { return; }
		const TArray<FSettingDefinition> Defs = { FovDef, SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f) };

		USettingsSubsystem* First = MakeSubsystem(Defs);
		First->SetFloat(IdTestB, 0.75f);
		TestTrue(TEXT("SaveSettings writes"), First->SaveSettings());
		float OnDisk = 0.f;
		if (TestTrue(TEXT("the file contains B's line, no per-setting code needed"), SettingsTestPrivate::ReadSlotValue(SlotName, IdTestB, OnDisk)))
		{
			TestNearlyEqual(TEXT("saved value of B"), OnDisk, 0.75f, 0.0001f);
		}
		USettingsSubsystem* Second = MakeSubsystem(Defs);
		TestNearlyEqual(TEXT("B read back by a new instance"), Second->GetFloat(IdTestB), 0.75f, 0.0001f);
		TestNearlyEqual(TEXT("FOV still the Default"), Second->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);

		SettingsTestPrivate::DeleteSlot(SlotName);
		if (!TestTrue(TEXT("harness: forged file written"), SettingsTestPrivate::ForgeLines(SlotName, { SettingsCatalog::FOV.ToString() + TEXT("=100"), IdTestB.ToString() + TEXT("=nan") }))) { return; }
		USettingsSubsystem* Third = MakeSubsystem(Defs);
		TestNearlyEqual(TEXT("valid FOV is kept"), Third->GetFloat(SettingsCatalog::FOV), 100.f, 0.0001f);
		TestNearlyEqual(TEXT("NaN on B is replaced by B's own Default, not the FOV's"), Third->GetFloat(IdTestB), 0.25f, 0.0001f);
	});

	// Invalid definitions: one test per rule, each logs exactly one Error (pattern ".*" at Error level: the text is not specified; a missing log AND an
	// extra error both turn the test red). Mutation (all six): the rule is not checked -> the entry is exposed (Num() == 3) or no Error is logged -> red.
	It("Criterion 8: Min == Max is ignored with one error and valid neighbours stay usable", [this]()
	{
		if (!bFovDefFound) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		ExpectInvalidIgnored(SettingsTestPrivate::MakeDef(FName(TEXT("Test.MinEqMax")), 5.f, 5.f, 5.f));
	});

	It("Criterion 8: Min > Max is ignored with one error", [this]()
	{
		if (!bFovDefFound) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		ExpectInvalidIgnored(SettingsTestPrivate::MakeDef(FName(TEXT("Test.MinOverMax")), 10.f, 0.f, 5.f));
	});

	It("Criterion 8: Default above Max is ignored with one error", [this]()
	{
		if (!bFovDefFound) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		ExpectInvalidIgnored(SettingsTestPrivate::MakeDef(FName(TEXT("Test.DefaultHigh")), 0.f, 10.f, 11.f));
	});

	It("Criterion 8: Default below Min is ignored with one error", [this]()
	{
		if (!bFovDefFound) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		ExpectInvalidIgnored(SettingsTestPrivate::MakeDef(FName(TEXT("Test.DefaultLow")), 0.f, 10.f, -1.f));
	});

	It("Criterion 8: an empty Id is ignored with one error", [this]()
	{
		if (!bFovDefFound) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		ExpectInvalidIgnored(SettingsTestPrivate::MakeDef(NAME_None, 0.f, 10.f, 5.f));
	});

	// Assumption: the first definition with an Id wins (same rule as the UIEntries duplicates of the PlayerController).
	It("Criterion 8: a duplicate Id is ignored with one error and the first definition stays in force", [this]()
	{
		if (!bFovDefFound) { return; }
		AddExpectedMessage(TEXT(".*"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		// Same Id as the FOV, other bounds: if it replaced the first one, the FOV would be bounded to 0..10 and the Max check of the helper fails.
		ExpectInvalidIgnored(SettingsTestPrivate::MakeDef(SettingsCatalog::FOV, 0.f, 10.f, 5.f));
	});

	// Mutation: the Default check uses >= / <= (strict) -> a definition whose Default sits exactly on a bound is wrongly rejected, red
	//           (an Error is then logged, which also fails the test by itself).
	It("Criterion 8 (edge): Default exactly equal to Min or to Max is valid", [this]()
	{
		if (!bFovDefFound) { return; }
		const FName IdAtMin(TEXT("Test.DefaultAtMin"));
		const FName IdAtMax(TEXT("Test.DefaultAtMax"));
		USettingsSubsystem* Subsystem = MakeSubsystem({ SettingsTestPrivate::MakeDef(IdAtMin, 0.f, 10.f, 0.f), SettingsTestPrivate::MakeDef(IdAtMax, 0.f, 10.f, 10.f) });
		TestEqual(TEXT("both definitions are kept"), Subsystem->GetDefinitions().Num(), 2);
		TestNearlyEqual(TEXT("Default == Min"), Subsystem->GetFloat(IdAtMin), 0.f, 0.0001f);
		TestNearlyEqual(TEXT("Default == Max"), Subsystem->GetFloat(IdAtMax), 10.f, 0.0001f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
