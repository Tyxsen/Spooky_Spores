// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/InteractionComponent.h"
#include "CollisionChannels.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"

// Sets default values for this component's properties
UInteractionComponent::UInteractionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
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

