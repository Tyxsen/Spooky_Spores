#include "Enemies/EnemyWaveSpawner.h"
#include "Enemies/EnemyPathSpline.h"
#include "Enemies/EnemySoldier.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Spooky_Spores.h"

namespace
{
	// Placeholder wave count and size, to tune in the placed instance.
	constexpr int32 DefaultWaveCount = 20;
	constexpr int32 PlaceholderEnemyCount = 3;
}

AEnemyWaveSpawner::AEnemyWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Waves.SetNum(DefaultWaveCount);
	for (FEnemyWaveEntry& Entry : Waves)
	{
		Entry.Count = PlaceholderEnemyCount;
	}
}

void AEnemyWaveSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart)
	{
		GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AEnemyWaveSpawner::StartWaves, FirstWaveDelay, false);
	}
}

void AEnemyWaveSpawner::StartWaves()
{
	if (bIsRunning) return;

	if (!PathSplineActor)
	{
		UE_LOG(LogSpooky_Spores, Error, TEXT("WaveSpawner %s has no PathSplineActor, cannot start waves."), *GetNameSafe(this));
		return;
	}
	if (Waves.IsEmpty())
	{
		UE_LOG(LogSpooky_Spores, Error, TEXT("WaveSpawner %s has an empty Waves array, cannot start waves."), *GetNameSafe(this));
		return;
	}

	// Cancels a pending auto-start timer (no-op when StartWaves is the timer callback itself).
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);

	bIsRunning = true;
	CurrentWaveIndex = -1;
	BeginWave(0);
}

void AEnemyWaveSpawner::StopWaves()
{
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

	for (const TWeakObjectPtr<AEnemySoldier>& Soldier : AliveSoldiers)
	{
		if (Soldier.IsValid())
		{
			Soldier->OnDestroyed.RemoveDynamic(this, &AEnemyWaveSpawner::HandleSoldierDestroyed);
		}
	}
	AliveSoldiers.Reset();

	bIsRunning = false;
	bIsWaveInProgress = false;
}

int32 AEnemyWaveSpawner::GetWaveIndex() const
{
	return CurrentWaveIndex;
}

void AEnemyWaveSpawner::BeginWave(int32 WaveIndex)
{
	CurrentWaveIndex = WaveIndex;
	SpawnedInCurrentWave = 0;
	CurrentWaveSize = 0;
	bIsWaveInProgress = true;

	const FEnemyWaveEntry& Entry = Waves[WaveIndex];
	if (!Entry.EnemyClass)
	{
		UE_LOG(LogSpooky_Spores, Error, TEXT("WaveSpawner %s: wave %d has no EnemyClass, the wave is empty."), *GetNameSafe(this), WaveIndex);
	}
	else if (Entry.Count > 0)
	{
		CurrentWaveSize = Entry.Count;
	}

	OnWaveStarted.Broadcast(WaveIndex);
	// A listener may have called StopWaves.
	if (!bIsRunning) return;

	if (CurrentWaveSize == 0)
	{
		HandleWaveCleared();
		return;
	}

	// Armed before the first spawn: SpawnNextSoldier clears it once the wave is fully spawned.
	if (CurrentWaveSize > 1)
	{
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemyWaveSpawner::SpawnNextSoldier, SpawnStagger, true);
	}
	SpawnNextSoldier();
}

void AEnemyWaveSpawner::SpawnNextSoldier()
{
	USplineComponent* Spline = PathSplineActor->GetSplineComponent();
	const FVector SpawnLocation = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FRotator SpawnRotation = Spline->GetDirectionAtSplinePoint(0, ESplineCoordinateSpace::World).Rotation();
	// A character stays upright: only the heading of the tangent is used.
	SpawnRotation.Pitch = 0.0f;
	SpawnRotation.Roll = 0.0f;
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	// Deferred spawn: the path must be set before FinishSpawning, because OnPossess reads it.
	AEnemySoldier* Soldier = GetWorld()->SpawnActorDeferred<AEnemySoldier>(
		Waves[CurrentWaveIndex].EnemyClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Soldier)
	{
		Soldier->SetPathSplineActor(PathSplineActor);
		Soldier->FinishSpawning(SpawnTransform);
	}

	++SpawnedInCurrentWave;
	if (SpawnedInCurrentWave >= CurrentWaveSize)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	}

	// A soldier already destroyed at spawn counts as dead: never bound, never alive.
	if (IsValid(Soldier))
	{
		Soldier->OnDestroyed.AddDynamic(this, &AEnemyWaveSpawner::HandleSoldierDestroyed);
		AliveSoldiers.Add(Soldier);
	}

	if (IsCurrentWaveFinished())
	{
		HandleWaveCleared();
	}
}

void AEnemyWaveSpawner::StartNextWave()
{
	BeginWave(CurrentWaveIndex + 1);
}

void AEnemyWaveSpawner::HandleSoldierDestroyed(AActor* DestroyedActor)
{
	const TWeakObjectPtr<AEnemySoldier> DestroyedSoldier(Cast<AEnemySoldier>(DestroyedActor));
	AliveSoldiers.RemoveSingleSwap(DestroyedSoldier);

	if (IsCurrentWaveFinished())
	{
		HandleWaveCleared();
	}
}

bool AEnemyWaveSpawner::IsCurrentWaveFinished() const
{
	return bIsRunning && bIsWaveInProgress && SpawnedInCurrentWave >= CurrentWaveSize && AliveSoldiers.IsEmpty();
}

void AEnemyWaveSpawner::HandleWaveCleared()
{
	// Reset before broadcasting: a listener may call StopWaves or re-enter through a synchronous callback.
	bIsWaveInProgress = false;
	const int32 ClearedIndex = CurrentWaveIndex;

	OnWaveCleared.Broadcast(ClearedIndex);
	if (!bIsRunning) return;

	if (ClearedIndex >= Waves.Num() - 1)
	{
		bIsRunning = false;
		OnAllWavesCleared.Broadcast();
		return;
	}

	GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AEnemyWaveSpawner::StartNextWave, WaveInterval, false);
}
