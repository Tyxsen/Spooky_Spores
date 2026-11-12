// Automation Spec for the servant task registry (UServantSubsystem).
// Source of truth: the servant task menu spec (docs/specs/servant-task-menu.md, removed at merge), acceptance criteria 1 to 7
// and the edge case "servant destroyed while assigned".
//
// World: FTestWorldWrapper (Engine, Tests/AutomationCommon.h) creates a transient Game world with a GameInstance and BeginPlay done,
// so a spawned AServant runs BeginPlay (registration) and Destroy() runs EndPlay (removal) synchronously.
// The world is never ticked: nothing but the calls of the test can change the registry or the wandering state.
// Broadcasts are counted with UServantTaskTestListener (Tests/ServantTaskTestListener.h), bound before any servant is spawned.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantTaskTestListener.h"
#include "Servants/Servant.h"
#include "Servants/ServantAIController.h"
#include "Servants/ServantSubsystem.h"
#include "Servants/ServantTask.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Class.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FServantSubsystemSpec, "SpookySpores.Servants.Task.Registry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	UServantSubsystem* Subsystem = nullptr;
	TStrongObjectPtr<UServantTaskTestListener> Listener;
	bool bWorldReady = false;

	bool RequireWorld();
	AServant* SpawnServant(const TCHAR* Name);
	TArray<EServantTask> AllTasks() const;
	void ExpectList(const FString& What, EServantTask Task, const TArray<AServant*>& Expected);
	void ExpectInNoListExcept(const FString& What, const AServant* Servant, EServantTask OnlyTask, bool bExpectedInOnlyTask);
	void ExpectBroadcasts(const FString& What, int32 Expected);

END_DEFINE_SPEC(FServantSubsystemSpec)

bool FServantSubsystemSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world or UServantSubsystem unavailable: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

AServant* FServantSubsystemSpec::SpawnServant(const TCHAR* Name)
{
	FActorSpawnParameters Params;
	Params.Name = FName(Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AServant>(AServant::StaticClass(), FTransform::Identity, Params);
}

TArray<EServantTask> FServantSubsystemSpec::AllTasks() const
{
	// Read from the reflected enum, so a category added later is covered by the "in no other list" checks.
	TArray<EServantTask> Tasks;
	const UEnum* Enum = StaticEnum<EServantTask>();
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // last entry is the generated _MAX
	{
		Tasks.Add(static_cast<EServantTask>(Enum->GetValueByIndex(Index)));
	}
	return Tasks;
}

void FServantSubsystemSpec::ExpectList(const FString& What, EServantTask Task, const TArray<AServant*>& Expected)
{
	const TArray<AServant*> Actual = Subsystem->GetServantsWithTask(Task);
	FString ActualNames;
	for (const AServant* Servant : Actual) { ActualNames += GetNameSafe(Servant) + TEXT(" "); }
	FString ExpectedNames;
	for (const AServant* Servant : Expected) { ExpectedNames += GetNameSafe(Servant) + TEXT(" "); }
	TestTrue(FString::Printf(TEXT("%s: list %s expected [%s], got [%s]"), *What, *UEnum::GetValueAsString(Task), *ExpectedNames, *ActualNames),
	         Actual == Expected);
}

void FServantSubsystemSpec::ExpectInNoListExcept(const FString& What, const AServant* Servant, EServantTask OnlyTask, bool bExpectedInOnlyTask)
{
	for (const EServantTask Task : AllTasks())
	{
		const int32 Occurrences = Subsystem->GetServantsWithTask(Task).FilterByPredicate([Servant](const AServant* Item) { return Item == Servant; }).Num();
		const int32 Expected = (Task == OnlyTask && bExpectedInOnlyTask) ? 1 : 0;
		TestEqual(FString::Printf(TEXT("%s: occurrences in list %s"), *What, *UEnum::GetValueAsString(Task)), Occurrences, Expected);
	}
}

void FServantSubsystemSpec::ExpectBroadcasts(const FString& What, int32 Expected)
{
	TestEqual(FString::Printf(TEXT("%s: OnAssignmentsChanged broadcasts"), *What), Listener->BroadcastCount, Expected);
}

void FServantSubsystemSpec::Define()
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
		bWorldReady = Subsystem != nullptr;
		if (bWorldReady)
		{
			Listener.Reset(NewObject<UServantTaskTestListener>());
			Subsystem->OnAssignmentsChanged.AddDynamic(Listener.Get(), &UServantTaskTestListener::HandleAssignmentsChanged);
		}
	});

	AfterEach([this]()
	{
		// The wrapper destructor ends play and destroys the world (servants, controllers, subsystem).
		Wrapper.Reset();
		Listener.Reset();
		World = nullptr;
		Subsystem = nullptr;
		bWorldReady = false;
	});

	Describe("Registration and removal (criteria 1, 3)", [this]()
	{
		It("Criterion 1: a spawned servant is registered Unassigned with number previous + 1; a destroyed one leaves and its number is not reused", [this]()
		{
			if (!RequireWorld()) { return; }
			TestEqual(TEXT("precondition: empty registry"), Subsystem->GetServantCount(), 0);

			AServant* First = SpawnServant(TEXT("ServantFirst"));
			if (!TestNotNull(TEXT("first servant spawned"), First)) { return; }
			TestEqual(TEXT("count after first spawn"), Subsystem->GetServantCount(), 1);
			TestTrue(TEXT("first servant registered"), Subsystem->IsServantRegistered(First));
			TestTrue(TEXT("first servant task is Unassigned"), Subsystem->GetServantTask(First) == EServantTask::Unassigned);
			TestEqual(TEXT("first number of the world is 1"), Subsystem->GetServantNumber(First), 1);

			AServant* Second = SpawnServant(TEXT("ServantSecond"));
			if (!TestNotNull(TEXT("second servant spawned"), Second)) { return; }
			TestEqual(TEXT("count after second spawn"), Subsystem->GetServantCount(), 2);
			TestEqual(TEXT("second number = previous + 1"), Subsystem->GetServantNumber(Second), 2);

			// Assigned before destruction: the edge case "destroyed while assigned" must leave no ghost entry.
			Subsystem->AssignTask(Second, EServantTask::FieldHarvest);
			Second->Destroy();
			TestEqual(TEXT("count after destroying the second servant"), Subsystem->GetServantCount(), 1);
			TestFalse(TEXT("destroyed servant no longer registered"), Subsystem->IsServantRegistered(Second));
			TestEqual(TEXT("destroyed servant number is 0 (not registered)"), Subsystem->GetServantNumber(Second), 0);
			ExpectInNoListExcept(TEXT("destroyed servant"), Second, EServantTask::Unassigned, false);
			ExpectList(TEXT("only the first servant remains"), EServantTask::Unassigned, { First });

			AServant* Third = SpawnServant(TEXT("ServantThird"));
			if (!TestNotNull(TEXT("third servant spawned"), Third)) { return; }
			TestEqual(TEXT("number 2 is not reused: third servant gets 3"), Subsystem->GetServantNumber(Third), 3);
			TestEqual(TEXT("first servant keeps its number"), Subsystem->GetServantNumber(First), 1);
		});

		It("Criterion 3: exactly one broadcast per registration and per removal; duplicate and unknown calls are ignored", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantBroadcast"));
			if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }
			ExpectBroadcasts(TEXT("after one registration"), 1);

			Subsystem->RegisterServant(Servant);
			ExpectBroadcasts(TEXT("duplicate registration is ignored"), 1);
			TestEqual(TEXT("duplicate registration: count unchanged"), Subsystem->GetServantCount(), 1);
			TestEqual(TEXT("duplicate registration: number unchanged"), Subsystem->GetServantNumber(Servant), 1);

			Subsystem->UnregisterServant(Servant);
			ExpectBroadcasts(TEXT("after one removal"), 2);
			Subsystem->UnregisterServant(Servant);
			ExpectBroadcasts(TEXT("removing an unknown servant is ignored"), 2);
			Subsystem->UnregisterServant(nullptr);
			ExpectBroadcasts(TEXT("removing nullptr is ignored"), 2);

			// Already unregistered: the EndPlay removal must not broadcast a second time.
			Servant->Destroy();
			ExpectBroadcasts(TEXT("destroying an already removed servant"), 2);

			AServant* Other = SpawnServant(TEXT("ServantBroadcastOther"));
			if (!TestNotNull(TEXT("other servant spawned"), Other)) { return; }
			ExpectBroadcasts(TEXT("second registration"), 3);
			Other->Destroy();
			ExpectBroadcasts(TEXT("removal through Destroy (EndPlay)"), 4);
		});
	});

	Describe("AssignTask (criteria 2, 3, 4, 5)", [this]()
	{
		It("Criterion 2: AssignTask(S, FieldHarvest) returns true and S is in the FieldHarvest list only", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantHarvest"));
			if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }

			TestTrue(TEXT("AssignTask returns true"), Subsystem->AssignTask(Servant, EServantTask::FieldHarvest));
			TestTrue(TEXT("GetServantTask == FieldHarvest"), Subsystem->GetServantTask(Servant) == EServantTask::FieldHarvest);
			ExpectInNoListExcept(TEXT("assigned servant"), Servant, EServantTask::FieldHarvest, true);
			TestEqual(TEXT("assignment does not change the count"), Subsystem->GetServantCount(), 1);
			TestEqual(TEXT("assignment does not change the number"), Subsystem->GetServantNumber(Servant), 1);
		});

		It("Criterion 3: exactly one broadcast per AssignTask that changes the task", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantAssignBroadcast"));
			if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }
			const int32 Before = Listener->BroadcastCount;

			Subsystem->AssignTask(Servant, EServantTask::AttackDefense);
			ExpectBroadcasts(TEXT("Unassigned -> AttackDefense"), Before + 1);
			Subsystem->AssignTask(Servant, EServantTask::FieldHarvest);
			ExpectBroadcasts(TEXT("AttackDefense -> FieldHarvest"), Before + 2);
		});

		It("Criterion 4: same task returns false without broadcast; Unassigned from a category returns true and moves S back", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantReassign"));
			if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }
			const int32 Before = Listener->BroadcastCount;

			TestFalse(TEXT("Unassigned -> Unassigned returns false"), Subsystem->AssignTask(Servant, EServantTask::Unassigned));
			ExpectBroadcasts(TEXT("Unassigned -> Unassigned"), Before);

			TestTrue(TEXT("Unassigned -> FieldHarvest returns true"), Subsystem->AssignTask(Servant, EServantTask::FieldHarvest));
			TestFalse(TEXT("FieldHarvest -> FieldHarvest returns false"), Subsystem->AssignTask(Servant, EServantTask::FieldHarvest));
			ExpectBroadcasts(TEXT("one change, one same-task call"), Before + 1);
			TestTrue(TEXT("task still FieldHarvest after the same-task call"), Subsystem->GetServantTask(Servant) == EServantTask::FieldHarvest);

			TestTrue(TEXT("FieldHarvest -> Unassigned returns true"), Subsystem->AssignTask(Servant, EServantTask::Unassigned));
			ExpectBroadcasts(TEXT("back to Unassigned"), Before + 2);
			TestTrue(TEXT("GetServantTask == Unassigned"), Subsystem->GetServantTask(Servant) == EServantTask::Unassigned);
			ExpectInNoListExcept(TEXT("servant back to Unassigned"), Servant, EServantTask::Unassigned, true);
		});

		It("Criterion 5: AssignTask on nullptr or on an unregistered servant returns false, without broadcast or state change", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Registered = SpawnServant(TEXT("ServantRegistered"));
			AServant* Unregistered = SpawnServant(TEXT("ServantUnregistered"));
			if (!TestNotNull(TEXT("servants spawned"), Registered) || !TestNotNull(TEXT("servants spawned"), Unregistered)) { return; }
			// Alive but out of the registry: the only way to get a live, unregistered servant through the public API.
			Subsystem->UnregisterServant(Unregistered);
			if (!TestFalse(TEXT("precondition: servant not registered"), Subsystem->IsServantRegistered(Unregistered))) { return; }
			const int32 Before = Listener->BroadcastCount;

			TestFalse(TEXT("AssignTask(nullptr) returns false"), Subsystem->AssignTask(nullptr, EServantTask::FieldHarvest));
			TestFalse(TEXT("AssignTask(unregistered) returns false"), Subsystem->AssignTask(Unregistered, EServantTask::FieldHarvest));
			ExpectBroadcasts(TEXT("no broadcast"), Before);

			TestFalse(TEXT("the unregistered servant was not registered by AssignTask"), Subsystem->IsServantRegistered(Unregistered));
			TestEqual(TEXT("count unchanged"), Subsystem->GetServantCount(), 1);
			TestTrue(TEXT("GetServantTask(unregistered) == Unassigned"), Subsystem->GetServantTask(Unregistered) == EServantTask::Unassigned);
			ExpectInNoListExcept(TEXT("unregistered servant"), Unregistered, EServantTask::Unassigned, false);
			ExpectList(TEXT("FieldHarvest list stays empty"), EServantTask::FieldHarvest, {});
			ExpectList(TEXT("registered servant untouched"), EServantTask::Unassigned, { Registered });
		});
	});

	Describe("Lists (criterion 6)", [this]()
	{
		It("Criterion 6: without any servant, every task list is empty", [this]()
		{
			if (!RequireWorld()) { return; }
			TestEqual(TEXT("count"), Subsystem->GetServantCount(), 0);
			for (const EServantTask Task : AllTasks())
			{
				ExpectList(TEXT("empty registry"), Task, {});
			}
		});

		It("Criterion 6: lists follow the registration order, not the assignment order", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* A = SpawnServant(TEXT("ServantOrderA"));
			AServant* B = SpawnServant(TEXT("ServantOrderB"));
			AServant* C = SpawnServant(TEXT("ServantOrderC"));
			if (!TestTrue(TEXT("servants spawned"), A != nullptr && B != nullptr && C != nullptr)) { return; }
			ExpectList(TEXT("after registration"), EServantTask::Unassigned, { A, B, C });

			// Assignment order C, A, B: an implementation appending on assignment would give [C, A, B].
			Subsystem->AssignTask(C, EServantTask::FieldHarvest);
			Subsystem->AssignTask(A, EServantTask::FieldHarvest);
			Subsystem->AssignTask(B, EServantTask::FieldHarvest);
			ExpectList(TEXT("all assigned"), EServantTask::FieldHarvest, { A, B, C });
			ExpectList(TEXT("all assigned"), EServantTask::Unassigned, {});

			// Back and forth: B must come back at its registration rank, not at the end.
			Subsystem->AssignTask(B, EServantTask::Unassigned);
			Subsystem->AssignTask(B, EServantTask::FieldHarvest);
			ExpectList(TEXT("after B left and came back"), EServantTask::FieldHarvest, { A, B, C });
		});

		It("Criterion 6: removing the first servant keeps the registration order of the others", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* A = SpawnServant(TEXT("ServantRemoveA"));
			AServant* B = SpawnServant(TEXT("ServantRemoveB"));
			AServant* C = SpawnServant(TEXT("ServantRemoveC"));
			if (!TestTrue(TEXT("servants spawned"), A != nullptr && B != nullptr && C != nullptr)) { return; }
			ExpectList(TEXT("after registration"), EServantTask::Unassigned, { A, B, C });

			// Real case: destruction runs EndPlay, which removes the servant. A swap removal would give [C, B].
			A->Destroy();
			ExpectList(TEXT("after destroying the first servant"), EServantTask::Unassigned, { B, C });
		});
	});

	Describe("Wandering untouched (criterion 7)", [this]()
	{
		It("Criterion 7: AssignTask leaves IsWandering() unchanged, whatever the task and the starting state", [this]()
		{
			if (!RequireWorld()) { return; }
			AServant* Servant = SpawnServant(TEXT("ServantWander"));
			if (!TestNotNull(TEXT("servant spawned"), Servant)) { return; }
			AServantAIController* Controller = Cast<AServantAIController>(Servant->GetController());
			if (!TestNotNull(TEXT("servant possessed by an AServantAIController"), Controller)) { return; }

			// Both starting states are forced, so that an AssignTask calling either StopWandering or StartWandering is caught.
			// The world is not ticked, so no wander decision can change the state between two reads.
			for (const bool bStartWandering : { true, false })
			{
				if (bStartWandering) { Controller->StartWandering(); } else { Controller->StopWandering(); }
				const bool bBefore = Controller->IsWandering();
				if (!TestTrue(TEXT("precondition: starting state forced"), bBefore == bStartWandering)) { continue; }

				// Unassigned -> each category -> back, so every task value is both a target and a source.
				TArray<EServantTask> Sequence = AllTasks();
				Sequence.Add(EServantTask::Unassigned);
				for (const EServantTask Task : Sequence)
				{
					Subsystem->AssignTask(Servant, Task);
					TestTrue(FString::Printf(TEXT("IsWandering unchanged after AssignTask(%s), starting state %s"),
					                         *UEnum::GetValueAsString(Task), bStartWandering ? TEXT("wandering") : TEXT("stopped")),
					         Controller->IsWandering() == bBefore);
				}
				TestTrue(TEXT("controller still possesses the servant"), Controller->GetPawn() == Servant);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
