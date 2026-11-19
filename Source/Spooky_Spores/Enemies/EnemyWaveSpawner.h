// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyWaveSpawner.generated.h"

class AEnemyPathSpline;
class AEnemySoldier;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCleared, int32, WaveIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllWavesCleared);

// One wave: Count soldiers of EnemyClass.
USTRUCT(BlueprintType)
struct SPOOKY_SPORES_API FEnemyWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TSubclassOf<AEnemySoldier> EnemyClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	int32 Count = 0;
};

/**
 * Spawns waves of AEnemySoldier at point 0 of an AEnemyPathSpline. The timer between waves starts
 * when the previous wave is entirely dead. After the last wave is killed the sequence stops.
 * Only soldiers spawned by this actor are tracked; soldiers placed by hand are ignored.
 */
UCLASS()
class SPOOKY_SPORES_API AEnemyWaveSpawner : public AActor
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	AEnemyWaveSpawner();

	// Starts wave 0 immediately. Cancels a pending auto-start timer. Ignored while a sequence is running.
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StartWaves();

	// Cancels every timer and unbinds living soldiers (they stay in the game). No event is emitted afterwards.
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StopWaves();

	// 0-based index of the current (or last started) wave, -1 before the first one.
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetWaveIndex() const;

protected:
	virtual void BeginPlay() override;

private:
	void BeginWave(int32 WaveIndex);
	void SpawnNextSoldier();
	void StartNextWave();
	void HandleWaveCleared();
	bool IsCurrentWaveFinished() const;

	UFUNCTION()
	void HandleSoldierDestroyed(AActor* DestroyedActor);

// -----------PROPERTIES-----------
public:
	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveStarted OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveCleared OnWaveCleared;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnAllWavesCleared OnAllWavesCleared;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	// Path the soldiers follow; they spawn at its point 0. Placeholder counts, classes to set in the placed instance.
	UPROPERTY(EditInstanceOnly, Category = "Waves")
	TObjectPtr<AEnemyPathSpline> PathSplineActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Waves")
	TArray<FEnemyWaveEntry> Waves;

	// Seconds from the end of a wave (all dead) to the start of the next one.
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (ClampMin = "0.01"))
	float WaveInterval = 180.0f;

	// Seconds between two soldiers of the same wave; the first one spawns when the wave starts.
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (ClampMin = "0.01"))
	float SpawnStagger = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Waves")
	bool bAutoStart = false;

	// Placeholder, not validated. Only used when bAutoStart is true.
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (EditCondition = "bAutoStart"))
	float FirstWaveDelay = 180.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Waves")
	int32 CurrentWaveIndex = -1;

	// Soldiers of the current wave still alive (bound to OnDestroyed).
	TArray<TWeakObjectPtr<AEnemySoldier>> AliveSoldiers;

	int32 CurrentWaveSize = 0;
	int32 SpawnedInCurrentWave = 0;
	bool bIsRunning = false;
	bool bIsWaveInProgress = false;

	// Auto-start delay, then WaveInterval between waves (never both at once).
	FTimerHandle WaveTimerHandle;
	FTimerHandle SpawnTimerHandle;
};
