// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemies/EnemyTargetSubsystem.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"

void UEnemyTargetSubsystem::SetClaim(const AController* Claimant, AActor* Target)
{
	if (!Claimant) return;

	Claims.Add(Claimant, Target);
}

void UEnemyTargetSubsystem::ReleaseClaim(const AController* Claimant)
{
	Claims.Remove(Claimant);
}

int32 UEnemyTargetSubsystem::GetClaimCount(const AActor* Target) const
{
	if (!IsValid(Target)) return 0;

	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<const AController>, TWeakObjectPtr<AActor>>& Pair : Claims)
	{
		if (Pair.Key.IsValid() && Pair.Value.Get() == Target)
		{
			++Count;
		}
	}
	return Count;
}

AActor* UEnemyTargetSubsystem::PickTarget(const AController* Claimant, const FVector& Origin, const TArray<AActor*>& Candidates) const
{
	AActor* BestTarget = nullptr;
	int32 BestCount = TNumericLimits<int32>::Max();
	double BestDistanceSquared = TNumericLimits<double>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate)) continue;

		int32 Count = 0;
		for (const TPair<TWeakObjectPtr<const AController>, TWeakObjectPtr<AActor>>& Pair : Claims)
		{
			if (Pair.Key.IsValid() && Pair.Key.Get() != Claimant && Pair.Value.Get() == Candidate)
			{
				++Count;
			}
		}

		const double DistanceSquared = FVector::DistSquared2D(Origin, Candidate->GetActorLocation());
		if (Count < BestCount || (Count == BestCount && DistanceSquared < BestDistanceSquared))
		{
			BestTarget = Candidate;
			BestCount = Count;
			BestDistanceSquared = DistanceSquared;
		}
	}

	return BestTarget;
}
