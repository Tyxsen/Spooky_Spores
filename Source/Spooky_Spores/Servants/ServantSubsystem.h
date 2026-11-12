// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Servants/ServantTask.h"
#include "ServantSubsystem.generated.h"

class AServant;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnServantAssignmentsChanged);

// One registered servant. Weak pointer: the registry never keeps a servant alive and never dangles.
struct FServantTaskEntry
{
	TWeakObjectPtr<AServant> Servant;
	EServantTask Task = EServantTask::Unassigned;
	int32 Number = 0;
};

/**
 * Registry of the servants of a world and single source of truth for their task.
 * Servants register on BeginPlay and unregister on EndPlay. Nothing is stored on AServant,
 * so a task can never change without OnAssignmentsChanged being broadcast.
 */
UCLASS()
class SPOOKY_SPORES_API UServantSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Returns true only if the task actually changed (broadcasts OnAssignmentsChanged in that case).
	UFUNCTION(BlueprintCallable, Category = "Servant|Task")
	bool AssignTask(AServant* Servant, EServantTask NewTask);

	// Unassigned if the servant is not registered.
	UFUNCTION(BlueprintPure, Category = "Servant|Task")
	EServantTask GetServantTask(const AServant* Servant) const;

	// Servants with this task, in registration order.
	UFUNCTION(BlueprintPure, Category = "Servant|Task")
	TArray<AServant*> GetServantsWithTask(EServantTask Task) const;

	// Display number ("Servant N"), 0 if the servant is not registered. Numbers are never reused.
	UFUNCTION(BlueprintPure, Category = "Servant|Task")
	int32 GetServantNumber(const AServant* Servant) const;

	UFUNCTION(BlueprintPure, Category = "Servant|Task")
	int32 GetServantCount() const;

	UFUNCTION(BlueprintPure, Category = "Servant|Task")
	bool IsServantRegistered(const AServant* Servant) const;

	// Called by AServant::BeginPlay / EndPlay only. Duplicates and unknown servants are ignored.
	void RegisterServant(AServant* Servant);
	void UnregisterServant(AServant* Servant);

private:
	// INDEX_NONE if not registered
	int32 FindEntryIndex(const AServant* Servant) const;

// -----------PROPERTIES-----------
public:
	UPROPERTY(BlueprintAssignable, Category = "Servant|Task")
	FOnServantAssignmentsChanged OnAssignmentsChanged;

private:
	// Registration order
	TArray<FServantTaskEntry> Entries;

	// Last number handed out in this world
	int32 LastServantNumber = 0;
};
