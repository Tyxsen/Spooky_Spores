// Automation Spec for the logic of the servant task menu (UServantTaskMenuWidget).
// Source of truth: the servant task menu spec (docs/specs/servant-task-menu.md, removed at merge), acceptance criterion 8
// and the edge cases "servant destroyed while selected" and "click on a category without selection".
//
// UServantTaskMenuWidget is UCLASS(Abstract) (its Blueprint child does the layout). The tests instantiate it with NewObject inside
// an FScopedAllowAbstractClassAllocation scope (CoreUObject), without CreateWidget, Slate or a player: only the C++ logic
// (selection, assignment, subscription to the registry) is exercised, no test subclass is added to the module.
// Not covered here: OnMenuRefreshRequested / OnSelectionChanged are BlueprintImplementableEvents, a C++ test cannot observe
// their calls without a Blueprint child; that part of criterion 8 is a PIE scenario.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantTaskTestListener.h"
#include "Servants/Servant.h"
#include "Servants/ServantSubsystem.h"
#include "Servants/ServantTask.h"
#include "Servants/ServantTaskMenuWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FServantTaskMenuWidgetSpec, "SpookySpores.Servants.Task.Menu",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	UServantSubsystem* Subsystem = nullptr;
	TStrongObjectPtr<UServantTaskMenuWidget> Menu;
	bool bWorldReady = false;

	bool RequireWorld();
	AServant* SpawnServant(UWorld* InWorld, const TCHAR* Name);
	UServantTaskMenuWidget* CreateMenu(UWorld* InWorld);

END_DEFINE_SPEC(FServantTaskMenuWidgetSpec)

bool FServantTaskMenuWidgetSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world, UServantSubsystem or menu widget unavailable: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

AServant* FServantTaskMenuWidgetSpec::SpawnServant(UWorld* InWorld, const TCHAR* Name)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return InWorld->SpawnActor<AServant>(AServant::StaticClass(), FTransform::Identity, Params);
}

UServantTaskMenuWidget* FServantTaskMenuWidgetSpec::CreateMenu(UWorld* InWorld)
{
	// Same outer as CreateWidget would use; the abstract flag is lifted only for this allocation.
	UObject* Outer = InWorld->GetGameInstance() != nullptr ? static_cast<UObject*>(InWorld->GetGameInstance()) : static_cast<UObject*>(InWorld);
	FScopedAllowAbstractClassAllocation AllowAbstract;
	return NewObject<UServantTaskMenuWidget>(Outer, UServantTaskMenuWidget::StaticClass(), NAME_None, RF_Transient);
}

void FServantTaskMenuWidgetSpec::Define()
{
	BeforeEach([this]()
	{
		Wrapper = MakeUnique<FTestWorldWrapper>();
		bWorldReady = Wrapper->CreateTestWorld(EWorldType::Game) && Wrapper->BeginPlayInTestWorld();
		World = Wrapper->GetTestWorld();
		if (!bWorldReady)
		{
			TArray<FString> Messages;
			Wrapper->AppendErrorMessages(Messages);
			for (const FString& Message : Messages)
			{
				AddError(FString::Printf(TEXT("FTestWorldWrapper: %s"), *Message));
			}
			return;
		}
		Subsystem = World->GetSubsystem<UServantSubsystem>();
		Menu.Reset(Subsystem != nullptr ? CreateMenu(World) : nullptr);
		bWorldReady = Subsystem != nullptr && Menu.IsValid();
		if (bWorldReady)
		{
			Menu->InitializeMenu(Subsystem);
		}
	});

	AfterEach([this]()
	{
		Menu.Reset();
		Wrapper.Reset();
		World = nullptr;
		Subsystem = nullptr;
		bWorldReady = false;
	});

	It("InitializeMenu stores the received registry", [this]()
	{
		if (!RequireWorld()) { return; }
		TestTrue(TEXT("GetServantSubsystem returns the registry given to InitializeMenu"), Menu->GetServantSubsystem() == Subsystem);
	});

	It("Criterion 8 / edge case: AssignSelectedServant without selection returns false and changes nothing (empty and populated registry)", [this]()
	{
		if (!RequireWorld()) { return; }
		TStrongObjectPtr<UServantTaskTestListener> Listener(NewObject<UServantTaskTestListener>());
		Subsystem->OnAssignmentsChanged.AddDynamic(Listener.Get(), &UServantTaskTestListener::HandleAssignmentsChanged);

		// Menu opened without any servant
		TestNull(TEXT("no selection at start"), Menu->GetSelectedServant());
		TestFalse(TEXT("empty registry: AssignSelectedServant returns false"), Menu->AssignSelectedServant(EServantTask::FieldHarvest));

		AServant* Servant = SpawnServant(World, TEXT("ServantNoSelection"));
		if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }
		const int32 Before = Listener->BroadcastCount;
		TestFalse(TEXT("populated registry, no selection: AssignSelectedServant returns false"), Menu->AssignSelectedServant(EServantTask::FieldHarvest));
		TestEqual(TEXT("no broadcast"), Listener->BroadcastCount, Before);
		TestTrue(TEXT("servant still Unassigned"), Subsystem->GetServantTask(Servant) == EServantTask::Unassigned);

		// Explicit deselection behaves like no selection
		Menu->SelectServant(Servant);
		Menu->SelectServant(nullptr);
		TestNull(TEXT("SelectServant(nullptr) clears the selection"), Menu->GetSelectedServant());
		TestFalse(TEXT("after SelectServant(nullptr): returns false"), Menu->AssignSelectedServant(EServantTask::FieldHarvest));
		TestTrue(TEXT("servant still Unassigned after deselection"), Subsystem->GetServantTask(Servant) == EServantTask::Unassigned);
	});

	It("Criterion 8: a click on a servant replaces the previous selection", [this]()
	{
		if (!RequireWorld()) { return; }
		AServant* A = SpawnServant(World, TEXT("ServantSelectA"));
		AServant* B = SpawnServant(World, TEXT("ServantSelectB"));
		if (!TestTrue(TEXT("servants spawned"), A != nullptr && B != nullptr)) { return; }

		Menu->SelectServant(A);
		TestTrue(TEXT("A selected"), Menu->GetSelectedServant() == A);
		Menu->SelectServant(B);
		TestTrue(TEXT("B replaces A"), Menu->GetSelectedServant() == B);
	});

	It("Criterion 8: with a selection, AssignSelectedServant assigns the selected servant, then clears the selection", [this]()
	{
		if (!RequireWorld()) { return; }
		AServant* A = SpawnServant(World, TEXT("ServantAssignA"));
		AServant* B = SpawnServant(World, TEXT("ServantAssignB"));
		if (!TestTrue(TEXT("servants spawned"), A != nullptr && B != nullptr)) { return; }

		Menu->SelectServant(B);
		TestTrue(TEXT("AssignSelectedServant returns true when the task changes"), Menu->AssignSelectedServant(EServantTask::FieldHarvest));
		TestTrue(TEXT("the selected servant (B) got the task"), Subsystem->GetServantTask(B) == EServantTask::FieldHarvest);
		TestTrue(TEXT("the other servant (A) is untouched"), Subsystem->GetServantTask(A) == EServantTask::Unassigned);
		TestNull(TEXT("selection cleared after the assignment"), Menu->GetSelectedServant());

		// Click on the "unassigned" column moves it back (AssignSelectedServant(Unassigned))
		Menu->SelectServant(B);
		TestTrue(TEXT("back to Unassigned returns true"), Menu->AssignSelectedServant(EServantTask::Unassigned));
		TestTrue(TEXT("B is Unassigned again"), Subsystem->GetServantTask(B) == EServantTask::Unassigned);
		TestNull(TEXT("selection cleared again"), Menu->GetSelectedServant());
	});

	It("Criterion 8: a click on the category the servant already has returns false and still clears the selection", [this]()
	{
		if (!RequireWorld()) { return; }
		AServant* Servant = SpawnServant(World, TEXT("ServantSameTask"));
		if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }

		Menu->SelectServant(Servant);
		TestFalse(TEXT("same task: AssignSelectedServant returns false"), Menu->AssignSelectedServant(EServantTask::Unassigned));
		TestNull(TEXT("spec: a click on a category is followed by an empty selection"), Menu->GetSelectedServant());
	});

	It("Criterion 8: the selection is cleared when the selected servant leaves the registry, and only then", [this]()
	{
		if (!RequireWorld()) { return; }
		AServant* Selected = SpawnServant(World, TEXT("ServantSelected"));
		AServant* Other = SpawnServant(World, TEXT("ServantOther"));
		if (!TestTrue(TEXT("servants spawned"), Selected != nullptr && Other != nullptr)) { return; }
		Menu->SelectServant(Selected);

		// Broadcasts that do not concern the selected servant must keep the selection (one per event kind).
		Subsystem->AssignTask(Other, EServantTask::AttackDefense);
		TestTrue(TEXT("another servant's assignment keeps the selection"), Menu->GetSelectedServant() == Selected);
		AServant* Late = SpawnServant(World, TEXT("ServantLate"));
		TestNotNull(TEXT("late servant spawned"), Late);
		TestTrue(TEXT("another servant's registration keeps the selection"), Menu->GetSelectedServant() == Selected);
		Subsystem->UnregisterServant(Other);
		TestTrue(TEXT("another servant's removal keeps the selection"), Menu->GetSelectedServant() == Selected);

		// Removal while the actor is still alive: only the registry subscription can clear the selection here.
		Subsystem->UnregisterServant(Selected);
		TestNull(TEXT("selected servant removed from the registry: selection cleared"), Menu->GetSelectedServant());
		TestFalse(TEXT("nothing to assign afterwards"), Menu->AssignSelectedServant(EServantTask::AttackDefense));
	});

	It("Edge case: a selected servant that is destroyed is no longer selected and cannot be assigned", [this]()
	{
		if (!RequireWorld()) { return; }
		AServant* Servant = SpawnServant(World, TEXT("ServantDestroyed"));
		if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }
		Menu->SelectServant(Servant);
		Servant->Destroy();

		TestNull(TEXT("selection cleared after Destroy"), Menu->GetSelectedServant());
		TestFalse(TEXT("AssignSelectedServant returns false"), Menu->AssignSelectedServant(EServantTask::FieldHarvest));
		TestEqual(TEXT("no ghost entry in the FieldHarvest list"), Subsystem->GetServantsWithTask(EServantTask::FieldHarvest).Num(), 0);
	});

	It("InitializeMenu called a second time rebinds to the new registry", [this]()
	{
		if (!RequireWorld()) { return; }
		// A second world gives a second, independent UServantSubsystem.
		FTestWorldWrapper SecondWrapper;
		if (!TestTrue(TEXT("harness: second test world"), SecondWrapper.CreateTestWorld(EWorldType::Game) && SecondWrapper.BeginPlayInTestWorld())) { return; }
		UWorld* SecondWorld = SecondWrapper.GetTestWorld();
		UServantSubsystem* SecondSubsystem = SecondWorld->GetSubsystem<UServantSubsystem>();
		if (!TestNotNull(TEXT("harness: second registry"), SecondSubsystem) || !TestTrue(TEXT("harness: two distinct registries"), SecondSubsystem != Subsystem)) { return; }
		AServant* Servant = SpawnServant(SecondWorld, TEXT("ServantSecondWorld"));
		if (!TestNotNull(TEXT("servant spawned in the second world"), Servant)) { return; }

		Menu->InitializeMenu(SecondSubsystem);
		TestTrue(TEXT("GetServantSubsystem returns the new registry"), Menu->GetServantSubsystem() == SecondSubsystem);

		// Assignment goes to the new registry
		Menu->SelectServant(Servant);
		TestTrue(TEXT("assignment through the new registry returns true"), Menu->AssignSelectedServant(EServantTask::FieldHarvest));
		TestTrue(TEXT("new registry updated"), SecondSubsystem->GetServantTask(Servant) == EServantTask::FieldHarvest);

		// Subscription follows the new registry
		Menu->SelectServant(Servant);
		SecondSubsystem->UnregisterServant(Servant);
		TestNull(TEXT("removal from the new registry clears the selection"), Menu->GetSelectedServant());

		Menu->InitializeMenu(Subsystem); // back to the first world before SecondWrapper destroys its world
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
