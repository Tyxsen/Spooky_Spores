// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/BaseZone.h"

#include "Base/BaseZoneSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Spooky_Spores.h"

namespace
{
	// Local-space (unscaled) edge tolerance in cm. Must stay below 0.5 cm.
	constexpr double BoundsTolerance = 0.1;
}

ABaseZone::ABaseZone()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBox = CreateDefaultSubobject<UBoxComponent>(FName("ZoneBox"));
	SetRootComponent(ZoneBox);
	ZoneBox->SetBoxExtent(FVector(500.0, 500.0, 200.0));
	// Pure geometry: invisible to traces, no overlap, never carves the NavMesh.
	ZoneBox->SetCollisionProfileName(TEXT("NoCollision"));
	ZoneBox->SetGenerateOverlapEvents(false);
	ZoneBox->SetCanEverAffectNavigation(false);
}

void ABaseZone::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UBaseZoneSubsystem* Subsystem = World->GetSubsystem<UBaseZoneSubsystem>())
		{
			Subsystem->RegisterZone(this);
		}
	}
}

void ABaseZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UBaseZoneSubsystem* Subsystem = World->GetSubsystem<UBaseZoneSubsystem>())
		{
			Subsystem->UnregisterZone(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool ABaseZone::IsOrientedBoxValid(const FTransform& BoxTransform, const FVector& UnscaledExtent)
{
	const FVector AbsScale = BoxTransform.GetScale3D().GetAbs();

	// Non-finite values (NaN, infinity) make the zone invalid: an infinite extent would yield NaN random points.
	if (!FMath::IsFinite(UnscaledExtent.X) || !FMath::IsFinite(UnscaledExtent.Y) || !FMath::IsFinite(UnscaledExtent.Z)) return false;
	if (!FMath::IsFinite(AbsScale.X) || !FMath::IsFinite(AbsScale.Y) || !FMath::IsFinite(AbsScale.Z)) return false;

	return UnscaledExtent.X > KINDA_SMALL_NUMBER && UnscaledExtent.Y > KINDA_SMALL_NUMBER && UnscaledExtent.Z > KINDA_SMALL_NUMBER
		&& AbsScale.X > KINDA_SMALL_NUMBER && AbsScale.Y > KINDA_SMALL_NUMBER && AbsScale.Z > KINDA_SMALL_NUMBER;
}

bool ABaseZone::IsPointInOrientedBox(const FTransform& BoxTransform, const FVector& UnscaledExtent,
									const FVector& WorldPoint, bool bInIgnoreHeight)
{
	// Must be checked first: with a zero scale, InverseTransformPosition maps every point to the local origin.
	if (!IsOrientedBoxValid(BoxTransform, UnscaledExtent)) return false;
	if (WorldPoint.ContainsNaN() || !FMath::IsFinite(WorldPoint.X) || !FMath::IsFinite(WorldPoint.Y) || !FMath::IsFinite(WorldPoint.Z)) return false;

	// Scale (even non-uniform or negative) is already part of the inverse transform: compare against the unscaled extent.
	const FVector Local = BoxTransform.InverseTransformPosition(WorldPoint);
	// A NaN rotation or translation in the transform would otherwise slip through the comparisons below.
	if (Local.ContainsNaN()) return false;

	if (FMath::Abs(Local.X) > UnscaledExtent.X + BoundsTolerance) return false;
	if (FMath::Abs(Local.Y) > UnscaledExtent.Y + BoundsTolerance) return false;
	if (!bInIgnoreHeight && FMath::Abs(Local.Z) > UnscaledExtent.Z + BoundsTolerance) return false;

	return true;
}

bool ABaseZone::TryGetRandomPointInOrientedBox(const FTransform& BoxTransform, const FVector& UnscaledExtent,
											bool bInIgnoreHeight, FVector& OutPoint)
{
	OutPoint = FVector::ZeroVector;
	if (!IsOrientedBoxValid(BoxTransform, UnscaledExtent)) return false;

	// Draw in local space (affine transform keeps the density uniform), then go to world space.
	const FVector Local(
		FMath::FRandRange(-UnscaledExtent.X, UnscaledExtent.X),
		FMath::FRandRange(-UnscaledExtent.Y, UnscaledExtent.Y),
		bInIgnoreHeight ? 0.0 : FMath::FRandRange(-UnscaledExtent.Z, UnscaledExtent.Z));

	const FVector WorldPoint = BoxTransform.TransformPosition(Local);

	// A non-finite translation or rotation in the transform gives a non-finite point: IsPointInOrientedBox would reject it.
	if (!FMath::IsFinite(WorldPoint.X) || !FMath::IsFinite(WorldPoint.Y) || !FMath::IsFinite(WorldPoint.Z))
	{
		OutPoint = FVector::ZeroVector;
		return false;
	}

	OutPoint = WorldPoint;
	return true;
}

bool ABaseZone::IsZoneValid() const
{
	if (!ZoneBox) return false;

	const bool bIsValid = IsOrientedBoxValid(ZoneBox->GetComponentTransform(), ZoneBox->GetUnscaledBoxExtent());
	if (!bIsValid && !bHasWarnedInvalid)
	{
		bHasWarnedInvalid = true;
		UE_LOG(LogSpooky_Spores, Warning, TEXT("BaseZone %s is invalid (an extent or scale component is <= KINDA_SMALL_NUMBER)."), *GetName());
	}
	return bIsValid;
}

bool ABaseZone::IsPointInZone(const FVector& WorldPoint) const
{
	if (!IsZoneValid()) return false;

	return IsPointInOrientedBox(ZoneBox->GetComponentTransform(), ZoneBox->GetUnscaledBoxExtent(), WorldPoint, bIgnoreHeight);
}

bool ABaseZone::GetRandomPointInZone(FVector& OutPoint) const
{
	OutPoint = FVector::ZeroVector;
	if (!IsZoneValid()) return false;

	return TryGetRandomPointInOrientedBox(ZoneBox->GetComponentTransform(), ZoneBox->GetUnscaledBoxExtent(), bIgnoreHeight, OutPoint);
}

FVector ABaseZone::GetZoneExtent() const
{
	if (!ZoneBox) return FVector::ZeroVector;

	// GetScaledBoxExtent() keeps the sign of a negative scale: take the absolute value ourselves.
	return ZoneBox->GetUnscaledBoxExtent() * ZoneBox->GetComponentTransform().GetScale3D().GetAbs();
}
