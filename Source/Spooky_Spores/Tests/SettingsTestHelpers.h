// Test-only helpers for the settings Automation Specs (docs/specs/settings-menu.md, removed at merge).
// UHT does not accept a UCLASS inside #if WITH_DEV_AUTOMATION_TESTS, so the classes below are compiled in every configuration;
// they are NotBlueprintable / HideDropdown and never used by the game (same approach as PlayerUITestHelpers.h).
//
// Nothing here reads the implementation: everything uses the public API of the spec, plus reflection (property name only) to fill the
// BindWidget / RowClasses properties of the abstract widgets, because a C++ test cannot build a Blueprint widget tree.
//
// - USettingsTestListener records OnSettingChanged (a dynamic delegate can only call a UFUNCTION of a UObject).
// - UTestSettingFloatRow is a concrete USettingFloatRowWidget whose bound widgets (ValueSlider, NameText, ValueText) are real UMG objects.
// - UTestSettingsMenu is a concrete USettingsMenuWidget with a real RowsContainer and RowClasses[Float] = UTestSettingFloatRow.
// - FSettingsTestGameInstance gives a test world a UGameInstance (so Controller->GetGameInstance() works) with its subsystem collection.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/SettingRowWidget.h"
#include "Settings/SettingsCatalog.h"
#include "Settings/SettingsMenuWidget.h"
#include "Settings/SettingsSaveGame.h"
#include "Settings/SettingsSubsystem.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>
#include "SettingsTestHelpers.generated.h"

namespace SettingsTestPrivate
{
	// Save user index: the spec does not state it (spec gap), index 0 is the single-player convention.
	constexpr int32 UserIndex = 0;

	inline float NaNValue() { return std::numeric_limits<float>::quiet_NaN(); }
	inline float InfValue() { return std::numeric_limits<float>::infinity(); }

	inline FSettingDefinition MakeDef(FName Id, float Min, float Max, float Default, float Step = 0.f, ESettingType Type = ESettingType::Float)
	{
		FSettingDefinition Def;
		Def.Id = Id;
		Def.DisplayName = FText::FromString(Id.ToString());
		Def.Type = Type;
		Def.Min = Min;
		Def.Max = Max;
		Def.Default = Default;
		Def.Step = Step;
		return Def;
	}

	// The FOV definition of the real catalog (bounds come from the catalog: the spec calls them placeholders).
	inline bool FindFovDefinition(FSettingDefinition& OutDef)
	{
		for (const FSettingDefinition& Def : SettingsCatalog::GetDefinitions())
		{
			if (Def.Id == SettingsCatalog::FOV)
			{
				OutDef = Def;
				return true;
			}
		}
		return false;
	}

	// Writes a slot the way a hand-edited file would look to the subsystem: through the engine save API, with the SaveGame type of the spec.
	inline bool ForgeSlot(const FString& SlotName, const TMap<FName, float>& Values)
	{
		USettingsSaveGame* Save = Cast<USettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingsSaveGame::StaticClass()));
		if (Save == nullptr)
		{
			return false;
		}
		Save->Values = Values;
		return UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex);
	}

	inline USettingsSaveGame* LoadSlot(const FString& SlotName)
	{
		return Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	}

	inline FString MakeUniqueSlotName(const TCHAR* Prefix)
	{
		return FString::Printf(TEXT("SpookyTest_%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	inline float SliderAlpha(const USlider* Slider)
	{
		const float Range = Slider->GetMaxValue() - Slider->GetMinValue();
		return Range > 0.f ? (Slider->GetValue() - Slider->GetMinValue()) / Range : 0.f;
	}

	// A value expressed in the slider's own units, whatever mapping the row uses (actual units or normalized 0..1).
	inline float SliderValueAtAlpha(const USlider* Slider, float Alpha)
	{
		return Slider->GetMinValue() + Alpha * (Slider->GetMaxValue() - Slider->GetMinValue());
	}

	inline float ExpectedAlpha(const FSettingDefinition& Def, float Value)
	{
		return (Value - Def.Min) / (Def.Max - Def.Min);
	}
}

namespace SettingsTestReflection
{
	// Only property names from the spec are used (RowsContainer, ValueSlider, NameText, ValueText, RowClasses): access specifiers do not matter.
	inline bool SetObjectProperty(UObject* Target, const TCHAR* PropertyName, UObject* Value)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(Target->GetClass(), PropertyName);
		if (Property == nullptr)
		{
			return false;
		}
		Property->SetObjectPropertyValue_InContainer(Target, Value);
		return true;
	}

	inline FMapProperty* FindRowClasses(UObject* Menu)
	{
		return FindFProperty<FMapProperty>(Menu->GetClass(), TEXT("RowClasses"));
	}

	inline bool ClearRowClasses(UObject* Menu)
	{
		FMapProperty* Property = FindRowClasses(Menu);
		if (Property == nullptr)
		{
			return false;
		}
		FScriptMapHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Menu));
		Helper.EmptyValues();
		return true;
	}

	inline bool AddRowClass(UObject* Menu, ESettingType Type, UClass* RowClass)
	{
		FMapProperty* Property = FindRowClasses(Menu);
		if (Property == nullptr)
		{
			return false;
		}
		FScriptMapHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Menu));
		const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
		uint8* KeyPtr = Helper.GetKeyPtr(Index);
		if (const FEnumProperty* EnumKey = CastField<FEnumProperty>(Property->KeyProp))
		{
			EnumKey->GetUnderlyingProperty()->SetIntPropertyValue(KeyPtr, static_cast<int64>(Type));
		}
		else if (const FNumericProperty* NumericKey = CastField<FNumericProperty>(Property->KeyProp))
		{
			NumericKey->SetIntPropertyValue(KeyPtr, static_cast<int64>(Type));
		}
		else
		{
			return false;
		}
		const FObjectPropertyBase* ValueProperty = CastField<FObjectPropertyBase>(Property->ValueProp);
		if (ValueProperty == nullptr)
		{
			return false;
		}
		ValueProperty->SetObjectPropertyValue(Helper.GetValuePtr(Index), RowClass);
		Helper.Rehash();
		return true;
	}
}

UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class USettingsTestListener : public UObject
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	UFUNCTION()
	void HandleSettingChanged(FName Id, float Value)
	{
		Ids.Add(Id);
		Values.Add(Value);
	}

	int32 Num() const { return Ids.Num(); }

	int32 CountFor(FName Id) const
	{
		int32 Count = 0;
		for (const FName& Candidate : Ids)
		{
			Count += (Candidate == Id) ? 1 : 0;
		}
		return Count;
	}

// -----------PROPERTIES-----------
public:
	// Parallel arrays, one element per received OnSettingChanged, in order.
	TArray<FName> Ids;
	TArray<float> Values;
};

UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class UTestSettingFloatRow : public USettingFloatRowWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	UTestSettingFloatRow(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		TestSlider = ObjectInitializer.CreateDefaultSubobject<USlider>(this, TEXT("TestSlider"));
		TestNameText = ObjectInitializer.CreateDefaultSubobject<UTextBlock>(this, TEXT("TestNameText"));
		TestValueText = ObjectInitializer.CreateDefaultSubobject<UTextBlock>(this, TEXT("TestValueText"));
		bBindingsOk = SettingsTestReflection::SetObjectProperty(this, TEXT("ValueSlider"), TestSlider)
			&& SettingsTestReflection::SetObjectProperty(this, TEXT("NameText"), TestNameText)
			&& SettingsTestReflection::SetObjectProperty(this, TEXT("ValueText"), TestValueText);
	}

	// What the engine does when the widget leaves the screen (a C++ test has no viewport to remove it from).
	void CallNativeDestruct() { NativeDestruct(); }

// -----------PROPERTIES-----------
public:
	UPROPERTY()
	TObjectPtr<USlider> TestSlider = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> TestNameText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> TestValueText = nullptr;

	// False if one of the three BindWidget property names of the spec was not found by reflection.
	bool bBindingsOk = false;
};

UCLASS(NotBlueprintable, NotBlueprintType, HideDropdown)
class UTestSettingsMenu : public USettingsMenuWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	UTestSettingsMenu(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		TestRows = ObjectInitializer.CreateDefaultSubobject<UVerticalBox>(this, TEXT("TestRows"));
		bBindingsOk = SettingsTestReflection::SetObjectProperty(this, TEXT("RowsContainer"), TestRows)
			&& SettingsTestReflection::AddRowClass(this, ESettingType::Float, UTestSettingFloatRow::StaticClass());
	}

	int32 GetRowCount() const { return TestRows != nullptr ? TestRows->GetChildrenCount() : 0; }

	UTestSettingFloatRow* GetRowAt(int32 Index) const
	{
		return TestRows != nullptr ? Cast<UTestSettingFloatRow>(TestRows->GetChildAt(Index)) : nullptr;
	}

	// The row whose NameText shows the given text (null if none): rows are identified by what the player reads.
	UTestSettingFloatRow* FindRowByName(const FString& DisplayName) const
	{
		for (int32 Index = 0; Index < GetRowCount(); ++Index)
		{
			UTestSettingFloatRow* Row = GetRowAt(Index);
			if (Row != nullptr && Row->TestNameText != nullptr && Row->TestNameText->GetText().ToString() == DisplayName)
			{
				return Row;
			}
		}
		return nullptr;
	}

// -----------PROPERTIES-----------
public:
	UPROPERTY()
	TObjectPtr<UVerticalBox> TestRows = nullptr;

	bool bBindingsOk = false;
};

// Owns a UGameInstance with initialized subsystems and plugs it into a test world. The real USettingsSubsystem created by the collection
// is then re-initialized by the test with InitializeFromDefinitions(Defs, UniqueSlot): the real "Settings" slot is read once at creation
// (never written: nothing is modified) and never touched afterwards.
struct FSettingsTestGameInstance
{
	TStrongObjectPtr<UGameInstance> Instance;
	UWorld* World = nullptr;

	bool Setup(UWorld* InWorld)
	{
		if (InWorld == nullptr || GEngine == nullptr)
		{
			return false;
		}
		World = InWorld;
		Instance.Reset(NewObject<UGameInstance>(GEngine));
		Instance->Init();
		World->SetGameInstance(Instance.Get());
		return true;
	}

	USettingsSubsystem* GetSettings() const
	{
		return Instance.IsValid() ? Instance->GetSubsystem<USettingsSubsystem>() : nullptr;
	}

	void Teardown()
	{
		if (World != nullptr)
		{
			World->SetGameInstance(nullptr);
		}
		if (Instance.IsValid())
		{
			Instance->Shutdown();
		}
		Instance.Reset();
		World = nullptr;
	}
};
