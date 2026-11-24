// Automation Spec for UEnemyRegistrySubsystem (criterion 4 of docs/specs/servant-combat.md, removed at merge).
// "Alive" = registered, valid and !IsDead(). Soldiers register in BeginPlay and unregister in EndPlay.
//
// Soldiers are ATestPassiveEnemySoldier (no controller, no spline: nothing moves or acts). The state "dead but still in the world and
// still registered" (the spec: "a dead soldier may still be registered until its EndPlay") is produced by detaching the soldier's own
// death reaction (ServantCombatTest::MakeDeathLinger), otherwise HandleDeath would destroy it first and the !IsDead() filter could not
// be observed on its own.
// Every test names, in a "Mutation:" comment, the behaviour change that must turn it red.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ServantCombatTestHelpers.h"
#include "Enemies/EnemyRegistrySubsystem.h"
#include "Enemies/EnemySoldier.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"

BEGIN_DEFINE_SPEC(FEnemyRegistrySpec, "SpookySpores.Enemies.Registry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = 0.05f;

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;

	bool RequireWorld();
	UEnemyRegistrySubsystem* Registry() const;
	ATestPassiveEnemySoldier* SpawnEnemy(const TCHAR* Name, const FVector& Location);

END_DEFINE_SPEC(FEnemyRegistrySpec)

bool FEnemyRegistrySpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

UEnemyRegistrySubsystem* FEnemyRegistrySpec::Registry() const
{
	return World->GetSubsystem<UEnemyRegistrySubsystem>();
}

ATestPassiveEnemySoldier* FEnemyRegistrySpec::SpawnEnemy(const TCHAR* Name, const FVector& Location)
{
	ATestPassiveEnemySoldier* Enemy = ServantCombatTest::SpawnEnemy(World, Name, Location);
	TestNotNull(TEXT("harness: enemy spawned"), Enemy);
	return Enemy;
}

void FEnemyRegistrySpec::Define()
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
			for (const FString& Message : Messages) { AddError(FString::Printf(TEXT("FTestWorldWrapper: %s"), *Message)); }
		}
	});

	AfterEach([this]()
	{
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	Describe("GetLivingEnemies (criterion 4)", [this]()
	{
		It("is empty without any soldier, and contains exactly the soldiers that BeginPlay registered", [this]()
		{
			// Mutation: AEnemySoldier::BeginPlay does not register (the list stays empty), or registers twice (Num is 4), or the
			// subsystem returns a stale/global list (the empty-world check fails).
			if (!RequireWorld()) { return; }
			const UEnemyRegistrySubsystem* Subsystem = Registry();
			if (!TestNotNull(TEXT("harness: registry subsystem exists in a game world"), Subsystem)) { return; }
			TestEqual(TEXT("no soldier: no living enemy"), Subsystem->GetLivingEnemies().Num(), 0);

			AEnemySoldier* A = SpawnEnemy(TEXT("EnemyA"), FVector(0.0, 0.0, 100.0));
			AEnemySoldier* B = SpawnEnemy(TEXT("EnemyB"), FVector(300.0, 0.0, 100.0));
			if (A == nullptr || B == nullptr) { return; }
			const TArray<AEnemySoldier*> Living = Subsystem->GetLivingEnemies();
			TestEqual(TEXT("two soldiers: two living enemies"), Living.Num(), 2);
			TestTrue(TEXT("A is listed"), Living.Contains(A));
			TestTrue(TEXT("B is listed"), Living.Contains(B));
		});

		It("excludes a soldier killed through the regular damage path, keeps the other one", [this]()
		{
			// Mutation: the death is not reflected in the list (a dead soldier stays listed), or the whole list is emptied on a death.
			if (!RequireWorld()) { return; }
			const UEnemyRegistrySubsystem* Subsystem = Registry();
			AEnemySoldier* Dead = SpawnEnemy(TEXT("EnemyDead"), FVector(0.0, 0.0, 100.0));
			AEnemySoldier* Alive = SpawnEnemy(TEXT("EnemyAlive"), FVector(300.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: registry"), Subsystem) || Dead == nullptr || Alive == nullptr) { return; }

			EnemyTest::KillActor(Dead);
			Wrapper->TickTestWorld(TickStep); // a death may destroy the soldier at the end of the frame
			const TArray<AEnemySoldier*> Living = Subsystem->GetLivingEnemies();
			TestFalse(TEXT("the killed soldier is not listed"), Living.Contains(Dead));
			TestTrue(TEXT("the other soldier is still listed"), Living.Contains(Alive));
			TestEqual(TEXT("exactly one living enemy"), Living.Num(), 1);
		});

		It("excludes a soldier that is dead but still alive in the world and still registered (the !IsDead() filter on its own)", [this]()
		{
			// Mutation: the filter only checks "registered and valid" (IsDead() not tested): the lingering corpse stays in the list.
			if (!RequireWorld()) { return; }
			const UEnemyRegistrySubsystem* Subsystem = Registry();
			AEnemySoldier* Corpse = SpawnEnemy(TEXT("EnemyCorpse"), FVector(0.0, 0.0, 100.0));
			AEnemySoldier* Alive = SpawnEnemy(TEXT("EnemyAlive"), FVector(300.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: registry"), Subsystem) || Corpse == nullptr || Alive == nullptr) { return; }
			if (!TestTrue(TEXT("harness: death reaction detached"), ServantCombatTest::MakeDeathLinger(Corpse))) { return; }
			TestTrue(TEXT("precondition: listed while alive"), Subsystem->GetLivingEnemies().Contains(Corpse));

			EnemyTest::KillActor(Corpse);
			Wrapper->TickTestWorld(TickStep);
			if (!TestTrue(TEXT("precondition: the soldier is dead"), Corpse->IsDead())) { return; }
			if (!TestTrue(TEXT("precondition: and still valid in the world (not destroyed)"), IsValid(Corpse))) { return; }
			const TArray<AEnemySoldier*> Living = Subsystem->GetLivingEnemies();
			TestFalse(TEXT("the dead, still valid soldier is not listed"), Living.Contains(Corpse));
			TestTrue(TEXT("the living soldier is listed"), Living.Contains(Alive));
		});

		It("excludes a destroyed soldier (EndPlay unregisters), keeps the other one", [this]()
		{
			// Mutation: EndPlay does not unregister AND the list does not test validity (a stale weak pointer is returned), or a null
			// entry is returned (the Contains(nullptr) line).
			if (!RequireWorld()) { return; }
			const UEnemyRegistrySubsystem* Subsystem = Registry();
			AEnemySoldier* Doomed = SpawnEnemy(TEXT("EnemyDoomed"), FVector(0.0, 0.0, 100.0));
			AEnemySoldier* Alive = SpawnEnemy(TEXT("EnemyAlive"), FVector(300.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: registry"), Subsystem) || Doomed == nullptr || Alive == nullptr) { return; }

			Doomed->Destroy();
			Wrapper->TickTestWorld(TickStep);
			const TArray<AEnemySoldier*> Living = Subsystem->GetLivingEnemies();
			TestEqual(TEXT("exactly one living enemy"), Living.Num(), 1);
			TestTrue(TEXT("the survivor is the living soldier"), Living.Num() == 1 && Living[0] == Alive);
			TestFalse(TEXT("no null entry"), Living.Contains(nullptr));
		});
	});

	Describe("RegisterEnemy / UnregisterEnemy (API of the spec)", [this]()
	{
		It("UnregisterEnemy removes a living soldier from the list; registering it twice lists it once", [this]()
		{
			// Mutation: UnregisterEnemy does nothing (still listed), or removes every soldier, or RegisterEnemy appends a duplicate
			// (the soldier is already registered by BeginPlay: a second call must not make it appear twice, nor survive one Unregister).
			if (!RequireWorld()) { return; }
			UEnemyRegistrySubsystem* Subsystem = Registry();
			AEnemySoldier* A = SpawnEnemy(TEXT("EnemyA"), FVector(0.0, 0.0, 100.0));
			AEnemySoldier* B = SpawnEnemy(TEXT("EnemyB"), FVector(300.0, 0.0, 100.0));
			if (!TestNotNull(TEXT("harness: registry"), Subsystem) || A == nullptr || B == nullptr) { return; }

			Subsystem->RegisterEnemy(A); // already registered by BeginPlay
			int32 CountOfA = 0;
			for (const AEnemySoldier* Enemy : Subsystem->GetLivingEnemies()) { CountOfA += (Enemy == A) ? 1 : 0; }
			TestEqual(TEXT("a duplicate registration is ignored: A is listed once"), CountOfA, 1);

			Subsystem->UnregisterEnemy(A);
			const TArray<AEnemySoldier*> Living = Subsystem->GetLivingEnemies();
			TestFalse(TEXT("A is no longer listed after UnregisterEnemy (a single call is enough)"), Living.Contains(A));
			TestTrue(TEXT("B is untouched"), Living.Contains(B));
			TestTrue(TEXT("the soldier itself is untouched by the unregistration"), IsValid(A) && !A->IsDead());

			Subsystem->UnregisterEnemy(A); // unknown soldier: ignored, no side effect
			TestTrue(TEXT("unregistering an unknown soldier changes nothing"), Subsystem->GetLivingEnemies().Contains(B) && Subsystem->GetLivingEnemies().Num() == 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
