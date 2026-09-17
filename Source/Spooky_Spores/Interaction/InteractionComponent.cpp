// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/InteractionComponent.h"
#include "CollisionChannels.h"
#include "GameFramework/Pawn.h"
#include "Interaction/Grabbable.h"
#include "Interaction/Interactable.h"

// Sets default values for this component's properties
UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractionComponent::UpdateInteractionTrace()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	
	FVector EyeLocation;
	FRotator EyeRotation;
	
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	float MaxRange = FMath::Max(GrabRange, InteractRange);
	FVector TraceEnd = EyeLocation + (EyeRotation.Vector() * MaxRange);
	
	FCollisionObjectQueryParams CollisionObjectParams;
	CollisionObjectParams.AddObjectTypesToQuery(COLLISION_INTERACTABLE);
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Pawn);
	
	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByObjectType(HitResult, EyeLocation, TraceEnd, CollisionObjectParams, QueryParams))
	{
		ActualTarget = HitResult.GetActor();
		CurrentTargetDistance = FVector::Dist(EyeLocation, HitResult.Location);
		DrawDebugLine(
			GetWorld(),
			EyeLocation,
			HitResult.Location,
			FColor(0, 255, 0),
			false,
			0.0f,
			0,
			1.0f
		);
	} else {
		ActualTarget = nullptr;
		CurrentTargetDistance = TNumericLimits<float>::Max();
		DrawDebugLine(
			GetWorld(),
			EyeLocation,
			TraceEnd,
			FColor(255, 0, 0),
			false,
			0.0f,
			0,
			1.0f
		);
	}
		
	
	
}

// Called every frame
void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateInteractionTrace();
}

bool UInteractionComponent::IsGrabbableTarget(UPrimitiveComponent*& OutComponent) const
{
	OutComponent = nullptr;

	if (!ActualTarget.IsValid()) return false;
	if (CurrentTargetDistance > GrabRange) return false;
	if (!ActualTarget->Implements<UGrabbable>()) return false;

	OutComponent = IGrabbable::Execute_GetGrabbableComponent(ActualTarget.Get());
	return true;
}

bool UInteractionComponent::IsInteractableTarget() const
{
	if (!ActualTarget.IsValid()) return false;
	if (CurrentTargetDistance > InteractRange) return false;

	return ActualTarget->Implements<UInteractable>();
}

void UInteractionComponent::TryInteract()
{
	if (!IsInteractableTarget()) return;

	IInteractable::Execute_OnInteract(ActualTarget.Get(), GetOwner());
}

