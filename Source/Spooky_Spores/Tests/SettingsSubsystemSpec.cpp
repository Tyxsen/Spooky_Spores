// Automation Spec for USettingsSubsystem: bounding, broadcast, persistence, trust boundary on load, extensibility, invalid definitions.
// Source of truth: docs/specs/settings-menu.md (removed at merge), acceptance criteria 1 to 7 and its edge cases. Written from the spec only.
//
// Every instance is created with NewObject + InitializeFromDefinitions(Defs, UniqueSlot) (the seam of the spec): the real "Settings"
// slot of the player is never read or written. The slot name is unique per test and deleted in AfterEach.
// Every test lists its mutation: the change of behaviour that must turn it red.
//
// Assumptions (spec gaps, see the report): save user index 0; GetDefinitions() returns only the valid definitions; for a duplicate Id the
// first definition is kept; SetFloat on an unknown Id logs at most a Warning; a SetFloat that changes nothing does not mark the data modified.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SettingsTestHelpers.h"
#include "Settings/SettingsCatalog.h"
#include "Settings/SettingsSaveGame.h"
#include "Settings/SettingsSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace SettingsSubsystemSpecPrivate
{
	const FName IdTestB(TEXT("Test.Volume"));
	const FName IdUnknown(TEXT("Test.DoesNotExist"));
	const FName IdUnknownInSlot(TEXT("Test.GhostKeyInFile"));
}

BEGIN_DEFINE_SPEC(FSettingsSubsystemSpec, "SpookySpores.Settings.Subsystem",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	FString SlotName;
	FSettingDefinition FovDef;
	bool bFovDefFound = false;
	TArray<TStrongObjectPtr<USettingsSubsystem>> Instances;
	TStrongObjectPtr<USettingsTestListener> Listener;

	USettingsSubsystem* MakeSubsystem(const TArray<FSettingDefinition>& Defs);
	USettingsSubsystem* MakeCatalogSubsystem();
	void Listen(USettingsSubsystem* Subsystem);
	// Forges a slot with {FOV = Forged, GhostKey = 123}, loads it with a fresh subsystem and checks the FOV value read.
	void ExpectForgedFov(const FString& What, float Forged, float Expected);
	// Defs = [FOV, Bad, B]: the invalid entry must be ignored, the two valid ones usable, exactly one Error logged (declared by the caller).
	void ExpectInvalidIgnored(const FSettingDefinition& Bad);

END_DEFINE_SPEC(FSettingsSubsystemSpec)

USettingsSubsystem* FSettingsSubsystemSpec::MakeSubsystem(const TArray<FSettingDefinition>& Defs)
{
	USettingsSubsystem* Subsystem = NewObject<USettingsSubsystem>(GetTransientPackage());
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

void FSettingsSubsystemSpec::ExpectForgedFov(const FString& What, float Forged, float Expected)
{
	using namespace SettingsSubsystemSpecPrivate;
	TMap<FName, float> Values;
	Values.Add(SettingsCatalog::FOV, Forged);
	Values.Add(IdUnknownInSlot, 123.f);
	if (!TestTrue(FString::Printf(TEXT("%s: harness, the forged slot was written"), *What), SettingsTestPrivate::ForgeSlot(SlotName, Values)))
	{
		return;
	}
	USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
	TestNearlyEqual(FString::Printf(TEXT("%s: FOV read from the forged slot"), *What), Subsystem->GetFloat(SettingsCatalog::FOV), Expected, 0.0001f);
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
		bFovDefFound = SettingsTestPrivate::FindFovDefinition(FovDef);
		if (!bFovDefFound)
		{
			AddError(TEXT("SettingsCatalog::GetDefinitions() has no definition with Id SettingsCatalog::FOV: the spec requires one."));
		}
		Instances.Empty();
		Listener.Reset();
	});

	AfterEach([this]()
	{
		Listener.Reset();
		Instances.Empty();
		UGameplayStatics::DeleteGameInSlot(SlotName, SettingsTestPrivate::UserIndex);
	});

	// Mutation: Default is not applied when the slot is absent (value 0 or Min) -> red;
	//           the catalog Default of the FOV is changed away from the 90 of the spec/README -> the literal 90 line turns red.
	It("Criterion 1: without a slot, GetFloat(FOV) is the Default (90)", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("catalog FOV Default is 90 (spec)"), FovDef.Default, 90.f, 0.0001f);
		TestNearlyEqual(TEXT("first launch: GetFloat(FOV) is the Default"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);
		TestNearlyEqual(TEXT("first launch: GetFloat(FOV) is 90"), Subsystem->GetFloat(SettingsCatalog::FOV), 90.f, 0.0001f);
	});

	// Mutation: the clamp is removed (500 stored as is) -> red; the clamp uses only one bound (-5 stored as is) -> red;
	//           the clamp is applied at read time only with Max/Min swapped -> red; SetFloat returns the "changed" state instead of true -> not seen here (both calls change).
	It("Criterion 2: SetFloat(FOV, 500) gives Max, SetFloat(FOV, -5) gives Min, both return true", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();

		TestTrue(TEXT("SetFloat(500) returns true"), Subsystem->SetFloat(SettingsCatalog::FOV, 500.f));
		TestNearlyEqual(TEXT("500 is bounded to Max"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Max, 0.0001f);
		TestTrue(TEXT("SetFloat(-5) returns true"), Subsystem->SetFloat(SettingsCatalog::FOV, -5.f));
		TestNearlyEqual(TEXT("-5 is bounded to Min"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Min, 0.0001f);
	});

	// Mutation: the clamp is exclusive (Min or Max themselves moved inward) -> red; an in-range value is altered (rounded, clamped to Default...) -> red.
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

	// Mutation: an unknown Id is stored (map entry created, GetFloat returns 75 afterwards) -> red;
	//           it returns true, or broadcasts -> red; it writes into the known entry (e.g. clamps against the wrong definition) -> the FOV line turns red.
	It("Criterion 3: SetFloat on an unknown Id returns false, broadcasts nothing and creates no entry", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Listen(Subsystem);

		TestFalse(TEXT("SetFloat(unknown) returns false"), Subsystem->SetFloat(IdUnknown, 75.f));
		TestEqual(TEXT("nothing broadcast"), Listener->Num(), 0);
		TestNearlyEqual(TEXT("no entry created: GetFloat(unknown) is still 0 (spec)"), Subsystem->GetFloat(IdUnknown), 0.f, 0.0001f);
		TestNearlyEqual(TEXT("the known FOV is untouched"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);

		// The persisted data must not contain it either: SaveSettings writes only the declared settings.
		Subsystem->SaveSettings();
		const USettingsSaveGame* Saved = SettingsTestPrivate::LoadSlot(SlotName);
		if (TestNotNull(TEXT("harness: the slot exists after SaveSettings"), Saved))
		{
			TestFalse(TEXT("the unknown Id is not in the saved values"), Saved->Values.Contains(IdUnknown));
		}
		TestEqual(TEXT("still nothing broadcast after SaveSettings"), Listener->Num(), 0);
	});

	// Mutation: SetFloat always broadcasts -> the repeated-value lines turn red; the comparison is done on the raw value instead of the bounded one
	//           (9999 after Max) -> red; the event carries the raw value (500) instead of the bounded one -> red;
	//           the broadcast is skipped on a real change -> red.
	It("Criterion 4: OnSettingChanged is broadcast once per real change with the bounded value, never when the bounded value is unchanged", [this]()
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
	It("API: ResetToDefault restores the Default through SetFloat (one event), and is silent when already at Default", [this]()
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

	// Mutation: SetFloat writes to disk (the spec forbids it) -> the slot exists, red.
	It("Criterion 5 (edge): SetFloat alone writes nothing to disk", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		Subsystem->SetFloat(SettingsCatalog::FOV, 75.f);
		Subsystem->SetFloat(SettingsCatalog::FOV, 100.f);
		TestFalse(TEXT("no slot on disk after SetFloat"), UGameplayStatics::DoesSaveGameExist(SlotName, SettingsTestPrivate::UserIndex));
	});

	// Mutation: SaveSettings writes nothing / returns false -> red; the value written is the Default instead of the current one -> red;
	//           the load ignores the slot -> the new instance reads 90, red; SaveSettings persists the raw value instead of the bounded one -> not seen here (75 in range).
	It("Criterion 5: SetFloat + SaveSettings, then a new subsystem on the same slot reads the same value", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* First = MakeCatalogSubsystem();
		First->SetFloat(SettingsCatalog::FOV, 75.f);
		TestTrue(TEXT("SaveSettings returns true (written)"), First->SaveSettings());
		TestTrue(TEXT("the slot now exists"), UGameplayStatics::DoesSaveGameExist(SlotName, SettingsTestPrivate::UserIndex));

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

	// Mutation: Deinitialize always writes -> the slot exists after the first two lines, red;
	//           the modified flag is set before the equality check (SetFloat of the current value marks dirty) -> second part red.
	It("Edge case: Deinitialize writes nothing when nothing changed (untouched, or SetFloat of the current value)", [this]()
	{
		if (!bFovDefFound) { return; }
		USettingsSubsystem* Untouched = MakeCatalogSubsystem();
		Untouched->Deinitialize();
		TestFalse(TEXT("untouched: no slot written"), UGameplayStatics::DoesSaveGameExist(SlotName, SettingsTestPrivate::UserIndex));

		USettingsSubsystem* NoOp = MakeCatalogSubsystem();
		NoOp->SetFloat(SettingsCatalog::FOV, FovDef.Default); // current value: not a change
		NoOp->SetFloat(SettingsCatalog::FOV, FovDef.Default);
		NoOp->Deinitialize();
		TestFalse(TEXT("SetFloat(current value) then Deinitialize: still nothing written"), UGameplayStatics::DoesSaveGameExist(SlotName, SettingsTestPrivate::UserIndex));
	});

	// Mutation: the load trusts the file (NaN stored as is) -> GetFloat is NaN, the nearly-equal check fails, red;
	//           NaN is clamped (FMath::Clamp(NaN) is NaN or Min) instead of replaced by Default -> red.
	It("Criterion 6: a forged FOV = NaN is read as Default (unknown key ignored, no error)", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("NaN"), SettingsTestPrivate::NaNValue(), FovDef.Default);
	});

	// Mutation: infinity is clamped to Max (-inf to Min) instead of replaced by Default -> red.
	It("Criterion 6: a forged FOV = +inf or -inf is read as Default, not as Max or Min", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("+inf"), SettingsTestPrivate::InfValue(), FovDef.Default);
		UGameplayStatics::DeleteGameInSlot(SlotName, SettingsTestPrivate::UserIndex);
		ExpectForgedFov(TEXT("-inf"), -SettingsTestPrivate::InfValue(), FovDef.Default);
	});

	// Mutation: the load does not bound (9999 or -5 kept) -> red; out-of-range replaced by Default instead of bounded -> red.
	It("Criterion 6: a forged FOV out of bounds (9999, -5) is bounded to Max / Min, not replaced by Default", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("9999"), 9999.f, FovDef.Max);
		UGameplayStatics::DeleteGameInSlot(SlotName, SettingsTestPrivate::UserIndex);
		ExpectForgedFov(TEXT("-5"), -5.f, FovDef.Min);
	});

	// Mutation: the key lookup defaults to 0 / Min when absent (TMap::FindRef) instead of Default -> red.
	It("Criterion 6: a slot without the FOV key (only an unknown key) reads the Default", [this]()
	{
		if (!bFovDefFound) { return; }
		TMap<FName, float> Values;
		Values.Add(IdUnknownInSlot, 123.f);
		if (!TestTrue(TEXT("harness: forged slot written"), SettingsTestPrivate::ForgeSlot(SlotName, Values))) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("absent key: Default"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);
		TestNearlyEqual(TEXT("unknown key from the file is not adopted as a setting"), Subsystem->GetFloat(IdUnknownInSlot), 0.f, 0.0001f);
		TestFalse(TEXT("and it cannot be set either"), Subsystem->SetFloat(IdUnknownInSlot, 50.f));
	});

	// Control of the forged-slot tests: a valid forged value MUST be read, otherwise every "reads Default" assertion above could pass
	// because the forged slot is never found (wrong slot name or user index), not because the trust boundary works.
	// Mutation: the load never reads the slot -> 100 not read, red; the exact bounds are moved inward -> red.
	It("Criterion 6 (control): a forged in-range FOV, and the exact Min and Max, are read back as forged", [this]()
	{
		if (!bFovDefFound) { return; }
		ExpectForgedFov(TEXT("in-range 100"), 100.f, 100.f);
		UGameplayStatics::DeleteGameInSlot(SlotName, SettingsTestPrivate::UserIndex);
		ExpectForgedFov(TEXT("exact Min"), FovDef.Min, FovDef.Min);
		UGameplayStatics::DeleteGameInSlot(SlotName, SettingsTestPrivate::UserIndex);
		ExpectForgedFov(TEXT("exact Max"), FovDef.Max, FovDef.Max);
	});

	// Mutation: an unreadable slot is treated as an error that aborts the load / returns 0 / crashes -> red.
	// Caveat: the engine itself may log while reading garbage; the spec only says Warning + defaults.
	It("Edge case: an unreadable slot gives the defaults (no crash)", [this]()
	{
		if (!bFovDefFound) { return; }
		const TArray<uint8> Garbage = { 0x00, 0xFF, 0x13, 0x37, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03 };
		if (!TestTrue(TEXT("harness: garbage written to the slot"), UGameplayStatics::SaveDataToSlot(Garbage, SlotName, SettingsTestPrivate::UserIndex))) { return; }
		USettingsSubsystem* Subsystem = MakeCatalogSubsystem();
		TestNearlyEqual(TEXT("unreadable slot: Default"), Subsystem->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);
		TestTrue(TEXT("the subsystem is still usable"), Subsystem->SetFloat(SettingsCatalog::FOV, 75.f));
	});

	// Mutation: the bound is hardcoded to the FOV (60..120) -> B's value 5 stays 5 or becomes 120, red; the broadcast is filtered on the FOV Id -> no B event, red;
	//           one shared value instead of a per-Id value -> setting B moves the FOV, red.
	It("Criterion 7: a second setting (other Id, other bounds) is bounded, broadcast and isolated, without changing the subsystem", [this]()
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
	It("Criterion 7: a second setting survives a round trip and a forged slot, independently of the FOV", [this]()
	{
		if (!bFovDefFound) { return; }
		const TArray<FSettingDefinition> Defs = { FovDef, SettingsTestPrivate::MakeDef(IdTestB, 0.f, 1.f, 0.25f) };

		USettingsSubsystem* First = MakeSubsystem(Defs);
		First->SetFloat(IdTestB, 0.75f);
		TestTrue(TEXT("SaveSettings writes"), First->SaveSettings());
		const USettingsSaveGame* Saved = SettingsTestPrivate::LoadSlot(SlotName);
		if (TestNotNull(TEXT("slot readable as USettingsSaveGame"), Saved))
		{
			const float* Stored = Saved->Values.Find(IdTestB);
			TestTrue(TEXT("the save contains B's key, no per-setting code needed"), Stored != nullptr);
			if (Stored != nullptr)
			{
				TestNearlyEqual(TEXT("saved value of B"), *Stored, 0.75f, 0.0001f);
			}
		}
		USettingsSubsystem* Second = MakeSubsystem(Defs);
		TestNearlyEqual(TEXT("B read back by a new instance"), Second->GetFloat(IdTestB), 0.75f, 0.0001f);
		TestNearlyEqual(TEXT("FOV still the Default"), Second->GetFloat(SettingsCatalog::FOV), FovDef.Default, 0.0001f);

		UGameplayStatics::DeleteGameInSlot(SlotName, SettingsTestPrivate::UserIndex);
		TMap<FName, float> Values;
		Values.Add(SettingsCatalog::FOV, 100.f);
		Values.Add(IdTestB, SettingsTestPrivate::NaNValue());
		if (!TestTrue(TEXT("harness: forged slot written"), SettingsTestPrivate::ForgeSlot(SlotName, Values))) { return; }
		USettingsSubsystem* Third = MakeSubsystem(Defs);
		TestNearlyEqual(TEXT("valid FOV is kept"), Third->GetFloat(SettingsCatalog::FOV), 100.f, 0.0001f);
		TestNearlyEqual(TEXT("NaN on B is replaced by B's own Default, not the FOV's"), Third->GetFloat(IdTestB), 0.25f, 0.0001f);
	});

	// Invalid definitions: one test per rule, each logs exactly one Error (pattern ".*" at Error level: the text is not specified; a missing log AND an
	// extra error both turn the test red). Mutation (all six): the rule is not checked -> the entry is exposed (Num() == 3) or no Error is logged -> red.
	It("Criterion 8: Min == Max is ignored with one error, valid neighbours stay usable", [this]()
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
	It("Criterion 8: a duplicate Id is ignored with one error, the first definition stays in force", [this]()
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
