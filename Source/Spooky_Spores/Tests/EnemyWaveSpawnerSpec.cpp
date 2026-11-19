// Automation Spec for AEnemyWaveSpawner inside a minimal test world.
// Source of truth: docs/specs/enemy-wave-spawner.md (removed at merge), acceptance criteria 1 to 7 and the edge cases.
//
// World: FTestWorldWrapper creates a transient Game world; time advances only through TickTestWorld(DeltaTime), which ticks the
// FTimerManager. No real waiting: WaveInterval and SpawnStagger are written on the instance by reflection (10 s and 1 s instead of
// 180 s and 1.5 s) and the world is ticked in 0.05 s steps. Sampling instants sit mid-period (e.g. t = 0.9 / 1.1 for a 1 s stagger)
// so that float accumulation in the timer manager cannot decide the verdict.
// No NavMesh and no floor: ATestEnemySoldier flies and follows the spline with AddMovementInput. The test spline is 100 km long, so
// no soldier reaches the end of its path (Engage, which logs an error without a target) during a test.
// Kills go through the regular damage path (ApplyDamage), which ends in HandleDeath -> Destroy() -> OnDestroyed.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/EnemyTestHelpers.h"
#include "Tests/EnemyTestActors.h"
#include "Enemies/EnemyWaveSpawner.h"
#include "Enemies/EnemySoldier.h"
#include "Enemies/EnemyAIController.h"
#include "Enemies/EnemyPathSpline.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Templates/Function.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

using EnemyTest::EWaveEvent;

BEGIN_DEFINE_SPEC(FEnemyWaveSpawnerSpec, "SpookySpores.Enemies.WaveSpawner",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float TickStep = 0.05f;
	// Slack after a timer is due: one frame of pending-timer activation plus float accumulation.
	static constexpr double TimerSlack = 0.25;

	const FVector SplineStart = FVector(1000.0, 2000.0, 300.0);

	TUniquePtr<FTestWorldWrapper> Wrapper;
	UWorld* World = nullptr;
	bool bWorldReady = false;
	TStrongObjectPtr<UEnemyWaveTestListener> Listener;
	AEnemyWaveSpawner* Spawner = nullptr;

	bool RequireWorld();
	FEnemyWaveEntry Entry(int32 Count, TSubclassOf<AEnemySoldier> EnemyClass = ATestEnemySoldier::StaticClass()) const;
	EnemyTest::FSpawnerConfig Config(const TArray<FEnemyWaveEntry>& Waves, double Interval, double Stagger);
	bool MakeSpawner(const EnemyTest::FSpawnerConfig& Config);
	TArray<AEnemySoldier*> Soldiers(const AActor* Exclude = nullptr) const;
	void KillSoldiers(const AActor* Exclude = nullptr);
	int32 NumStarted() const { return Listener->Count(EWaveEvent::Started); }
	int32 NumCleared() const { return Listener->Count(EWaveEvent::Cleared); }
	int32 NumAllCleared() const { return Listener->Count(EWaveEvent::AllCleared); }
	void TickFor(double Seconds);
	bool TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds);

END_DEFINE_SPEC(FEnemyWaveSpawnerSpec)

bool FEnemyWaveSpawnerSpec::RequireWorld()
{
	if (!bWorldReady)
	{
		AddError(TEXT("Test world could not be created or started: harness problem, the assertions of this test did not run."));
	}
	return bWorldReady;
}

FEnemyWaveEntry FEnemyWaveSpawnerSpec::Entry(int32 Count, TSubclassOf<AEnemySoldier> EnemyClass) const
{
	FEnemyWaveEntry Result;
	Result.EnemyClass = EnemyClass;
	Result.Count = Count;
	return Result;
}

EnemyTest::FSpawnerConfig FEnemyWaveSpawnerSpec::Config(const TArray<FEnemyWaveEntry>& Waves, double Interval, double Stagger)
{
	EnemyTest::FSpawnerConfig Result;
	Result.Waves = Waves;
	Result.WaveInterval = Interval;
	Result.SpawnStagger = Stagger;
	Result.Spline = EnemyTest::SpawnSpline(World, SplineStart, FVector(0.0, 1.0, 0.0), 100000.0);
	return Result;
}

bool FEnemyWaveSpawnerSpec::MakeSpawner(const EnemyTest::FSpawnerConfig& Cfg)
{
	// Deferred spawn: bAutoStart must be written before BeginPlay, which is where the auto-start timer is armed.
	Spawner = World->SpawnActorDeferred<AEnemyWaveSpawner>(AEnemyWaveSpawner::StaticClass(), FTransform::Identity, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("harness: spawner spawned"), Spawner)) { return false; }
	const bool bWritten = EnemyTest::WriteWaves(Spawner, Cfg.Waves)
		&& EnemyTest::WriteNumber(Spawner, TEXT("WaveInterval"), Cfg.WaveInterval)
		&& EnemyTest::WriteNumber(Spawner, TEXT("SpawnStagger"), Cfg.SpawnStagger)
		&& EnemyTest::WriteNumber(Spawner, TEXT("FirstWaveDelay"), Cfg.FirstWaveDelay)
		&& EnemyTest::WriteBool(Spawner, TEXT("bAutoStart"), Cfg.bAutoStart)
		&& EnemyTest::WriteObject(Spawner, TEXT("PathSplineActor"), Cfg.Spline);
	if (!TestTrue(TEXT("harness: spawner properties written by reflection (names from the spec)"), bWritten)) { return false; }
	Spawner->FinishSpawning(FTransform::Identity);

	Listener.Reset(NewObject<UEnemyWaveTestListener>(GetTransientPackage()));
	Listener->World = World;
	Spawner->OnWaveStarted.AddDynamic(Listener.Get(), &UEnemyWaveTestListener::HandleWaveStarted);
	Spawner->OnWaveCleared.AddDynamic(Listener.Get(), &UEnemyWaveTestListener::HandleWaveCleared);
	Spawner->OnAllWavesCleared.AddDynamic(Listener.Get(), &UEnemyWaveTestListener::HandleAllWavesCleared);
	return true;
}

TArray<AEnemySoldier*> FEnemyWaveSpawnerSpec::Soldiers(const AActor* Exclude) const
{
	TArray<AEnemySoldier*> Result;
	for (TActorIterator<AEnemySoldier> It(World); It; ++It)
	{
		if (*It != Exclude) { Result.Add(*It); }
	}
	return Result;
}

void FEnemyWaveSpawnerSpec::KillSoldiers(const AActor* Exclude)
{
	// Collected first: a death destroys the actor, which must not happen while iterating.
	for (AEnemySoldier* Soldier : Soldiers(Exclude))
	{
		EnemyTest::KillActor(Soldier);
	}
}

void FEnemyWaveSpawnerSpec::TickFor(double Seconds)
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Seconds / TickStep - 0.001));
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Wrapper->TickTestWorld(TickStep);
	}
}

bool FEnemyWaveSpawnerSpec::TickUntil(TFunctionRef<bool()> Condition, double MaxSeconds)
{
	const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt32(MaxSeconds / TickStep));
	for (int32 Index = 0; Index < MaxSteps; ++Index)
	{
		if (Condition()) { return true; }
		Wrapper->TickTestWorld(TickStep);
	}
	return Condition();
}

void FEnemyWaveSpawnerSpec::Define()
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
		Listener.Reset();
		Spawner = nullptr;
		Wrapper.Reset();
		World = nullptr;
		bWorldReady = false;
	});

	Describe("Defaults", [this]()
	{
		It("The CDO carries the spec defaults: 20 waves, 180 s interval, 1.5 s stagger, no auto-start, 180 s first delay, no spline", [this]()
		{
			const AEnemyWaveSpawner* Default = GetDefault<AEnemyWaveSpawner>();
			double Interval = 0.0, Stagger = 0.0, FirstDelay = 0.0;
			bool bAutoStart = true;
			UObject* SplineRef = nullptr;
			if (!TestTrue(TEXT("harness: defaults readable by reflection"),
				EnemyTest::ReadNumber(Default, TEXT("WaveInterval"), Interval) && EnemyTest::ReadNumber(Default, TEXT("SpawnStagger"), Stagger)
				&& EnemyTest::ReadNumber(Default, TEXT("FirstWaveDelay"), FirstDelay) && EnemyTest::ReadBool(Default, TEXT("bAutoStart"), bAutoStart)
				&& EnemyTest::ReadObject(Default, TEXT("PathSplineActor"), SplineRef))) { return; }
			const int32 NumWaves = EnemyTest::ReadWavesNum(Default);
			TestEqual(TEXT("Waves has 20 entries"), NumWaves, 20);
			for (int32 Index = 0; Index < NumWaves; ++Index)
			{
				FEnemyWaveEntry Wave;
				if (!TestTrue(TEXT("harness: wave entry readable"), EnemyTest::ReadWaveAt(Default, Index, Wave))) { break; }
				// Placeholders per the spec (class and counts are set in the placed instance); the count is the implementer's documented default.
				TestEqual(*FString::Printf(TEXT("Waves[%d].Count == 3 (placeholder default)"), Index), Wave.Count, 3);
				TestNull(*FString::Printf(TEXT("Waves[%d].EnemyClass is null by default"), Index), Wave.EnemyClass.Get());
			}
			TestEqual(TEXT("WaveInterval == 180"), Interval, 180.0);
			TestEqual(TEXT("SpawnStagger == 1.5"), Stagger, 1.5);
			TestEqual(TEXT("FirstWaveDelay == 180"), FirstDelay, 180.0);
			TestFalse(TEXT("bAutoStart == false"), bAutoStart);
			TestNull(TEXT("PathSplineActor is null by default"), SplineRef);
		});
	});

	Describe("Spawn (criterion 1)", [this]()
	{
		It("C1: a spawned soldier has its spline at once, appears at point 0 facing the tangent, and its controller advances along it", [this]()
		{
			if (!RequireWorld()) { return; }
			const EnemyTest::FSpawnerConfig Cfg = Config({ Entry(1) }, 10.0, 1.0);
			if (!MakeSpawner(Cfg)) { return; }

			Spawner->StartWaves();

			const TArray<AEnemySoldier*> Spawned = Soldiers();
			if (!TestEqual(TEXT("C1: one soldier right after StartWaves (first one at t = 0)"), Spawned.Num(), 1)) { return; }
			AEnemySoldier* Soldier = Spawned[0];
			TestTrue(TEXT("C1: the soldier is of the configured EnemyClass"), Soldier->IsA(ATestEnemySoldier::StaticClass()));
			TestTrue(TEXT("C1: GetPathSplineActor() is the spline, before any tick"), Soldier->GetPathSplineActor() == Cfg.Spline);
			TestTrue(FString::Printf(TEXT("C1: spawned at spline point 0 (%s), got %s"), *SplineStart.ToString(), *Soldier->GetActorLocation().ToString()),
				Soldier->GetActorLocation().Equals(SplineStart, 1.0));
			// The spline runs along +Y: the tangent at point 0 is yaw 90.
			TestTrue(FString::Printf(TEXT("C1: oriented along the tangent (yaw 90), got %f"), Soldier->GetActorRotation().Yaw),
				FMath::Abs(FRotator::NormalizeAxis(Soldier->GetActorRotation().Yaw - 90.0)) < 5.0);
			if (!TestNotNull(TEXT("C1: possessed by an AEnemyAIController at spawn"), Cast<AEnemyAIController>(Soldier->GetController()))) { return; }

			// The controller reads the spline in OnPossess: set after FinishSpawning, the setter would come too late and nothing would move.
			Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			TickFor(2.0);
			const FVector Moved = Soldier->GetActorLocation() - SplineStart;
			TestTrue(FString::Printf(TEXT("C1: the controller advanced along the spline (+Y by more than 100 u in 2 s), got %s"), *Moved.ToString()), Moved.Y > 100.0);
			TestTrue(FString::Printf(TEXT("C1: and stayed on the spline line (|dX| < 50), got %s"), *Moved.ToString()), FMath::Abs(Moved.X) < 50.0);
		});
	});

	Describe("Stagger (criterion 2)", [this]()
	{
		It("C2: a wave of 3 spawns exactly 3 soldiers, the k-th at k x SpawnStagger, the first at t = 0", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(3) }, 100.0, 1.0))) { return; }

			Spawner->StartWaves();
			TestEqual(TEXT("C2: t = 0, 1 soldier"), Soldiers().Num(), 1);
			TickFor(0.9);
			TestEqual(TEXT("C2: t = 0.9, still 1 (second one not before 1 stagger)"), Soldiers().Num(), 1);
			TickFor(0.2);
			TestEqual(TEXT("C2: t = 1.1, 2 soldiers"), Soldiers().Num(), 2);
			TickFor(0.8);
			TestEqual(TEXT("C2: t = 1.9, still 2"), Soldiers().Num(), 2);
			TickFor(0.2);
			TestEqual(TEXT("C2: t = 2.1, 3 soldiers"), Soldiers().Num(), 3);
			TickFor(5.0);
			TestEqual(TEXT("C2: exactly N, never more"), Soldiers().Num(), 3);
			TestEqual(TEXT("C2: one OnWaveStarted only"), NumStarted(), 1);
			TestEqual(TEXT("C2: no OnWaveCleared while soldiers are alive"), NumCleared(), 0);
		});
	});

	Describe("Wave end and interval (criterion 3)", [this]()
	{
		It("C3: killing the whole wave emits OnWaveCleared once, then WaveInterval later (not before) the next wave starts", [this]()
		{
			if (!RequireWorld()) { return; }
			constexpr double Interval = 10.0;
			if (!MakeSpawner(Config({ Entry(2), Entry(1) }, Interval, 1.0))) { return; }
			TestEqual(TEXT("C3: GetWaveIndex() == -1 before the first wave"), Spawner->GetWaveIndex(), -1);

			Spawner->StartWaves();
			TestEqual(TEXT("C3: GetWaveIndex() == 0 once started"), Spawner->GetWaveIndex(), 0);
			TickFor(1.1);
			if (!TestEqual(TEXT("C3: precondition, the 2 soldiers are spawned"), Soldiers().Num(), 2)) { return; }

			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("C3: OnWaveCleared emitted once"), NumCleared(), 1);
			TestTrue(TEXT("C3: with the index of the wave (0)"), Listener->TimeOf(EWaveEvent::Cleared, 0) >= 0.0);
			TestEqual(TEXT("C3: not yet the next wave"), NumStarted(), 1);

			TickFor(0.9 * Interval);
			TestEqual(TEXT("C3: no next wave before WaveInterval"), NumStarted(), 1);
			TestEqual(TEXT("C3: index unchanged before WaveInterval"), Spawner->GetWaveIndex(), 0);

			const bool bStarted = TickUntil([this]() { return NumStarted() == 2; }, 0.1 * Interval + TimerSlack);
			if (!TestTrue(TEXT("C3: the next wave starts about WaveInterval after the end of the previous one"), bStarted)) { return; }
			const double Gap = Listener->TimeOf(EWaveEvent::Started, 1) - Listener->TimeOf(EWaveEvent::Cleared, 0);
			TestTrue(FString::Printf(TEXT("C3: interval measured from the end of the wave, got %f"), Gap), Gap >= Interval - 0.001 && Gap <= Interval + TimerSlack);
			TestEqual(TEXT("C3: GetWaveIndex() == 1"), Spawner->GetWaveIndex(), 1);
			TestEqual(TEXT("C3: the next wave spawned its soldier at once"), Soldiers().Num(), 1);
			TestEqual(TEXT("C3: OnWaveCleared not emitted again"), NumCleared(), 1);
		});

		It("C3: soldiers killed during the stagger do not end the wave before the last one is spawned and killed", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(3), Entry(1) }, 10.0, 1.0))) { return; }

			Spawner->StartWaves();
			TickFor(1.1);
			if (!TestEqual(TEXT("precondition: 2 of 3 spawned"), Soldiers().Num(), 2)) { return; }
			KillSoldiers();
			TickFor(0.5); // t = 1.6, the third one is due at t = 2
			TestEqual(TEXT("C3: nobody alive but the wave is not finished: no OnWaveCleared"), NumCleared(), 0);
			TestEqual(TEXT("C3: no soldier alive yet"), Soldiers().Num(), 0);
			TickFor(0.5); // t = 2.1
			if (!TestEqual(TEXT("C3: the third soldier is still spawned after the first two died"), Soldiers().Num(), 1)) { return; }
			TestEqual(TEXT("C3: still no OnWaveCleared while the last one lives"), NumCleared(), 0);

			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("C3: OnWaveCleared once the last soldier is spawned and killed"), NumCleared(), 1);
			TestTrue(TEXT("C3: for wave 0"), Listener->TimeOf(EWaveEvent::Cleared, 0) >= 0.0);
		});

		It("C3: a soldier destroyed by anything (not a death) counts as dead", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(1), Entry(1) }, 10.0, 1.0))) { return; }
			Spawner->StartWaves();
			const TArray<AEnemySoldier*> Spawned = Soldiers();
			if (!TestEqual(TEXT("precondition: 1 soldier"), Spawned.Num(), 1)) { return; }

			Spawned[0]->Destroy();
			TickFor(TickStep);
			TestEqual(TEXT("C3: OnWaveCleared after a plain Destroy()"), NumCleared(), 1);
		});

		It("Hand-placed soldiers never count in a wave (neither to block its end nor to end it)", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(1), Entry(1) }, 3.0, 1.0))) { return; }
			// The placed soldier has no spline: its controller logs nothing, so no error is masked here.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AEnemySoldier* Placed = World->SpawnActor<ATestEnemySoldier>(ATestEnemySoldier::StaticClass(), FTransform(FVector(-5000.0, 0.0, 300.0)), Params);
			if (!TestNotNull(TEXT("harness: placed soldier"), Placed)) { return; }

			Spawner->StartWaves();
			if (!TestEqual(TEXT("precondition: one spawned soldier besides the placed one"), Soldiers(Placed).Num(), 1)) { return; }
			KillSoldiers(Placed);
			TickFor(TickStep);
			TestEqual(TEXT("the wave ends although a placed soldier is still alive"), NumCleared(), 1);

			TickUntil([this]() { return NumStarted() == 2; }, 3.0 + TimerSlack);
			if (!TestEqual(TEXT("precondition: wave 1 started"), NumStarted(), 2)) { return; }
			EnemyTest::KillActor(Placed);
			TickFor(TickStep);
			TestEqual(TEXT("killing the placed soldier ends nothing"), NumCleared(), 1);
			TestEqual(TEXT("and starts nothing"), NumAllCleared(), 0);
		});
	});

	Describe("Last wave (criterion 4)", [this]()
	{
		It("C4: killing the last soldier of the last wave emits OnWaveCleared then OnAllWavesCleared, then nothing more happens", [this]()
		{
			if (!RequireWorld()) { return; }
			constexpr double Interval = 3.0;
			if (!MakeSpawner(Config({ Entry(1), Entry(1) }, Interval, 1.0))) { return; }

			Spawner->StartWaves();
			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("C4: not the last wave: no OnAllWavesCleared yet"), NumAllCleared(), 0);
			if (!TestTrue(TEXT("precondition: wave 1 starts"), TickUntil([this]() { return NumStarted() == 2; }, Interval + TimerSlack))) { return; }

			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("C4: OnAllWavesCleared emitted once"), NumAllCleared(), 1);
			TestEqual(TEXT("C4: the last wave also emitted its OnWaveCleared"), NumCleared(), 2);
			const int32 EventCount = Listener->Events.Num();
			if (!TestTrue(TEXT("harness: events recorded"), EventCount >= 2)) { return; }
			TestTrue(TEXT("C4: OnWaveCleared(1) comes before OnAllWavesCleared"),
				Listener->Events[EventCount - 2].Type == EWaveEvent::Cleared && Listener->Events[EventCount - 2].Index == 1
				&& Listener->Events[EventCount - 1].Type == EWaveEvent::AllCleared);

			TickFor(4.0 * Interval);
			TestEqual(TEXT("C4: no further event after the end"), Listener->Events.Num(), EventCount);
			TestEqual(TEXT("C4: no further spawn after the end"), Soldiers().Num(), 0);
			TestEqual(TEXT("C4: the index stays on the last wave"), Spawner->GetWaveIndex(), 1);

			// "Apres OnAllWavesCleared, il repart de la vague 0" (spec, Lancement).
			Spawner->StartWaves();
			TestEqual(TEXT("C4: StartWaves after the end restarts at wave 0"), Spawner->GetWaveIndex(), 0);
			TestEqual(TEXT("C4: with a new OnWaveStarted(0)"), NumStarted(), 3);
			TestEqual(TEXT("C4: and a new soldier"), Soldiers().Num(), 1);
		});
	});

	Describe("Start rules (criterion 5)", [this]()
	{
		It("C5: StartWaves twice in a row makes one single sequence", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(2) }, 100.0, 1.0))) { return; }

			Spawner->StartWaves();
			Spawner->StartWaves();
			TestEqual(TEXT("C5: one OnWaveStarted"), NumStarted(), 1);
			TestEqual(TEXT("C5: one soldier at t = 0, not two"), Soldiers().Num(), 1);
			TickFor(3.0);
			TestEqual(TEXT("C5: the wave still has exactly 2 soldiers (a second sequence would add its own)"), Soldiers().Num(), 2);
			TestEqual(TEXT("C5: still one OnWaveStarted"), NumStarted(), 1);
		});

		It("C5: bAutoStart arms a FirstWaveDelay timer in BeginPlay that starts wave 0", [this]()
		{
			if (!RequireWorld()) { return; }
			EnemyTest::FSpawnerConfig Cfg = Config({ Entry(1) }, 100.0, 1.0);
			Cfg.bAutoStart = true;
			Cfg.FirstWaveDelay = 2.0;
			if (!MakeSpawner(Cfg)) { return; }

			TickFor(1.8);
			TestEqual(TEXT("C5: nothing before FirstWaveDelay"), NumStarted(), 0);
			TestEqual(TEXT("C5: index -1 before the first wave"), Spawner->GetWaveIndex(), -1);
			TickFor(0.5);
			TestEqual(TEXT("C5: wave 0 started after FirstWaveDelay"), NumStarted(), 1);
			TestEqual(TEXT("C5: with its first soldier"), Soldiers().Num(), 1);
		});

		It("C5: without bAutoStart (default), nothing starts by itself even after FirstWaveDelay", [this]()
		{
			if (!RequireWorld()) { return; }
			EnemyTest::FSpawnerConfig Cfg = Config({ Entry(1) }, 100.0, 1.0);
			Cfg.FirstWaveDelay = 2.0;
			if (!MakeSpawner(Cfg)) { return; }
			TickFor(5.0);
			TestEqual(TEXT("C5: no OnWaveStarted"), NumStarted(), 0);
			TestEqual(TEXT("C5: no soldier"), Soldiers().Num(), 0);
		});

		It("C5: StartWaves during FirstWaveDelay starts at once and the auto-start timer does not start anything again", [this]()
		{
			if (!RequireWorld()) { return; }
			EnemyTest::FSpawnerConfig Cfg = Config({ Entry(1) }, 100.0, 1.0);
			Cfg.bAutoStart = true;
			Cfg.FirstWaveDelay = 5.0;
			if (!MakeSpawner(Cfg)) { return; }

			TickFor(1.0);
			TestEqual(TEXT("precondition: auto-start not fired yet"), NumStarted(), 0);
			Spawner->StartWaves();
			TestEqual(TEXT("C5: started immediately, without waiting for the delay"), NumStarted(), 1);
			TestEqual(TEXT("C5: first soldier at once"), Soldiers().Num(), 1);

			// One single wave: killing its soldier ends the sequence, so a stale auto-start timer would restart it at t = 5.
			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("precondition: sequence ended"), NumAllCleared(), 1);
			TickFor(6.0);
			TestEqual(TEXT("C5: the auto-start timer was cancelled: no second OnWaveStarted"), NumStarted(), 1);
			TestEqual(TEXT("C5: and no new soldier"), Soldiers().Num(), 0);
		});
	});

	Describe("StopWaves (criterion 6)", [this]()
	{
		It("C6: StopWaves during the stagger stops spawns and events, leaves the soldiers alive, and StartWaves restarts at wave 0", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(4), Entry(1) }, 3.0, 1.0))) { return; }

			Spawner->StartWaves();
			TickFor(1.1);
			if (!TestEqual(TEXT("precondition: 2 of 4 spawned"), Soldiers().Num(), 2)) { return; }
			Spawner->StopWaves();
			const int32 EventCount = Listener->Events.Num();

			TickFor(5.0);
			TestEqual(TEXT("C6: no spawn after StopWaves"), Soldiers().Num(), 2);
			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("sanity: the kills removed the soldiers"), Soldiers().Num(), 0);
			TickFor(10.0);
			TestEqual(TEXT("C6: no event, even when the soldiers already there die"), Listener->Events.Num(), EventCount);
			TestEqual(TEXT("C6: nothing spawned meanwhile either"), Soldiers().Num(), 0);

			Spawner->StartWaves();
			TestEqual(TEXT("C6: StartWaves after StopWaves restarts at wave 0"), Spawner->GetWaveIndex(), 0);
			TestEqual(TEXT("C6: with a new OnWaveStarted"), NumStarted(), 2);
			TestEqual(TEXT("C6: and a new soldier"), Soldiers().Num(), 1);
		});

		It("C6: StopWaves called from OnWaveStarted: nothing is spawned after it and no event follows", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(3), Entry(1) }, 3.0, 1.0))) { return; }
			AEnemyWaveSpawner* Target = Spawner;
			int32 SoldiersAtStop = -1;
			Listener->OnStartedHook = [this, Target, &SoldiersAtStop](int32)
			{
				SoldiersAtStop = Soldiers().Num();
				Target->StopWaves();
			};

			Spawner->StartWaves();
			Listener->OnStartedHook = nullptr; // the captured local must not outlive this scope
			if (!TestTrue(TEXT("precondition: the hook ran"), SoldiersAtStop >= 0)) { return; }
			const int32 EventCount = Listener->Events.Num();
			TickFor(10.0);
			TestEqual(TEXT("C6: no soldier spawned after StopWaves called from OnWaveStarted"), Soldiers().Num(), SoldiersAtStop);
			KillSoldiers();
			TickFor(10.0);
			TestEqual(TEXT("C6: no event after it (neither OnWaveCleared nor a next OnWaveStarted)"), Listener->Events.Num(), EventCount);
			TestEqual(TEXT("C6: no soldier of a later wave either"), Soldiers().Num(), 0);
		});

		It("C6: StopWaves during the WaveInterval: the next wave never starts", [this]()
		{
			if (!RequireWorld()) { return; }
			constexpr double Interval = 4.0;
			if (!MakeSpawner(Config({ Entry(1), Entry(1) }, Interval, 1.0))) { return; }

			Spawner->StartWaves();
			KillSoldiers();
			TickFor(TickStep);
			if (!TestEqual(TEXT("precondition: wave 0 cleared, interval running"), NumCleared(), 1)) { return; }
			TickFor(0.5 * Interval);
			Spawner->StopWaves();
			const int32 EventCount = Listener->Events.Num();

			TickFor(3.0 * Interval);
			TestEqual(TEXT("C6: no OnWaveStarted(1) after StopWaves"), NumStarted(), 1);
			TestEqual(TEXT("C6: no event at all"), Listener->Events.Num(), EventCount);
			TestEqual(TEXT("C6: no soldier of wave 1"), Soldiers().Num(), 0);
		});

		It("C6: the soldiers alive at StopWaves stay in play (not destroyed)", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(2) }, 3.0, 1.0))) { return; }
			Spawner->StartWaves();
			TickFor(1.1);
			const TArray<AEnemySoldier*> Before = Soldiers();
			if (!TestEqual(TEXT("precondition: 2 soldiers"), Before.Num(), 2)) { return; }
			Spawner->StopWaves();
			TickFor(1.0);
			for (const AEnemySoldier* Soldier : Before)
			{
				TestTrue(TEXT("C6: the soldier survived StopWaves"), IsValid(Soldier));
			}
		});

		It("C6: StopWaves called from an OnWaveCleared callback leaves no timer armed", [this]()
		{
			if (!RequireWorld()) { return; }
			if (!MakeSpawner(Config({ Entry(1), Entry(1) }, 3.0, 1.0))) { return; }
			AEnemyWaveSpawner* Target = Spawner;
			Listener->OnClearedHook = [Target](int32) { Target->StopWaves(); };

			Spawner->StartWaves();
			KillSoldiers();
			TickFor(TickStep);
			TestEqual(TEXT("precondition: the callback ran"), NumCleared(), 1);
			TickFor(10.0);
			TestEqual(TEXT("C6: the WaveInterval timer was not armed after the callback stopped the sequence"), NumStarted(), 1);
			TestEqual(TEXT("C6: no soldier spawned afterwards"), Soldiers().Num(), 0);
		});
	});

	Describe("Configuration errors (criterion 7)", [this]()
	{
		It("C7: a null spline logs an error and StartWaves starts nothing", [this]()
		{
			if (!RequireWorld()) { return; }
			// Exactly one error, checked by the framework at the end of the test (a missing or repeated error fails it).
			AddExpectedErrorPlain(EnemyTest::ErrorNoSpline, EAutomationExpectedErrorFlags::Contains, 1);
			EnemyTest::FSpawnerConfig Cfg = Config({ Entry(1) }, 3.0, 1.0);
			Cfg.Spline = nullptr;
			if (!MakeSpawner(Cfg)) { return; }

			Spawner->StartWaves();
			TickFor(5.0);
			TestEqual(TEXT("C7: OnWaveStarted never emitted"), NumStarted(), 0);
			TestEqual(TEXT("C7: no soldier"), Soldiers().Num(), 0);
			TestEqual(TEXT("C7: index stays -1"), Spawner->GetWaveIndex(), -1);
		});

		It("C7: an empty Waves array logs an error and StartWaves starts nothing", [this]()
		{
			if (!RequireWorld()) { return; }
			AddExpectedErrorPlain(EnemyTest::ErrorNoWaves, EAutomationExpectedErrorFlags::Contains, 1);
			if (!MakeSpawner(Config({}, 3.0, 1.0))) { return; }

			Spawner->StartWaves();
			TickFor(5.0);
			TestEqual(TEXT("C7: OnWaveStarted never emitted"), NumStarted(), 0);
			TestEqual(TEXT("C7: no soldier"), Soldiers().Num(), 0);
			TestEqual(TEXT("C7: index stays -1"), Spawner->GetWaveIndex(), -1);
		});

		It("C7: a null class (error) and Count 0 or negative give empty waves that start and end at once, the sequence goes on", [this]()
		{
			if (!RequireWorld()) { return; }
			// Only wave 0 has a null class (the others have a valid class and a Count of 0, -2, 1): exactly one error.
			AddExpectedErrorPlain(EnemyTest::ErrorNoEnemyClass, EAutomationExpectedErrorFlags::Contains, 1);
			constexpr double Interval = 2.0;
			if (!MakeSpawner(Config({ Entry(3, nullptr), Entry(0), Entry(-2), Entry(1) }, Interval, 1.0))) { return; }

			Spawner->StartWaves();
			TestEqual(TEXT("C7: empty wave 0: OnWaveStarted at once"), NumStarted(), 1);
			TestEqual(TEXT("C7: empty wave 0: OnWaveCleared at once"), NumCleared(), 1);
			TestEqual(TEXT("C7: no soldier for a null class"), Soldiers().Num(), 0);

			TickFor(0.9 * Interval);
			TestEqual(TEXT("C7: the interval separates empty waves too"), NumStarted(), 1);
			if (!TestTrue(TEXT("C7: Count = 0: wave 1 starts after the interval"), TickUntil([this]() { return NumStarted() == 2; }, 0.1 * Interval + TimerSlack))) { return; }
			TestEqual(TEXT("C7: Count = 0: OnWaveCleared at once"), NumCleared(), 2);
			TestEqual(TEXT("C7: Count = 0: no soldier"), Soldiers().Num(), 0);

			if (!TestTrue(TEXT("C7: Count < 0: wave 2 starts after the interval"), TickUntil([this]() { return NumStarted() == 3; }, Interval + TimerSlack))) { return; }
			TestEqual(TEXT("C7: Count < 0: OnWaveCleared at once"), NumCleared(), 3);
			TestEqual(TEXT("C7: Count < 0: no soldier"), Soldiers().Num(), 0);

			if (!TestTrue(TEXT("C7: the sequence reaches the real wave 3"), TickUntil([this]() { return NumStarted() == 4; }, Interval + TimerSlack))) { return; }
			TestEqual(TEXT("C7: wave 3 spawned its soldier"), Soldiers().Num(), 1);
			TestEqual(TEXT("C7: wave 3 is not cleared while its soldier lives"), NumCleared(), 3);
			TestEqual(TEXT("C7: GetWaveIndex() == 3"), Spawner->GetWaveIndex(), 3);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
