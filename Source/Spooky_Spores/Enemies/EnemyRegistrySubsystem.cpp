// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemies/EnemyRegistrySubsystem.h"

#include "Enemies/EnemySoldier.h"

void UEnemyRegistrySubsystem::RegisterEnemy(AEnemySoldier* Enemy)
{
	if (!Enemy) return;

	Enemies.AddUnique(Enemy);
}

void UEnemyRegistrySubsystem::UnregisterEnemy(AEnemySoldier* Enemy)
{
	if (!Enemy) return;

	Enemies.Remove(Enemy);
}

TArray<AEnemySoldier*> UEnemyRegistrySubsystem::GetLivingEnemies() const
{
	TArray<AEnemySoldier*> Result;
	for (const TWeakObjectPtr<AEnemySoldier>& Entry : Enemies)
	{
		AEnemySoldier* Enemy = Entry.Get();
		if (Enemy && !Enemy->IsDead())
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}
