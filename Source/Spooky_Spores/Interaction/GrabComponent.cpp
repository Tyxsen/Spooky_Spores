// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/GrabComponent.h"
#include "GameFramework/Pawn.h"

// Sets default values for this component's properties
UGrabComponent::UGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>("PhysicsHandle");
}

bool UGrabComponent::IsHolding() const
{
	if (PhysicsHandle->GetGrabbedComponent() != nullptr) return true;
	
	return false;
}

void UGrabComponent::Grab(UPrimitiveComponent* ComponentToGrab)
{
	if (ComponentToGrab == nullptr) return;
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	
	FVector EyeLocation;
	FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	
	FVector GrabTargetLocation = EyeLocation + (EyeRotation.Vector() * HoldDistance);
	
	PhysicsHandle->GrabComponentAtLocationWithRotation(
		ComponentToGrab,
		NAME_None,
		GrabTargetLocation,
		EyeRotation
	);
}

void UGrabComponent::Release()
{
	PhysicsHandle->ReleaseComponent();
}

void UGrabComponent::Launch()
{
	if (!IsHolding()) return;
	
	UPrimitiveComponent* GrabbedComponent = PhysicsHandle->GetGrabbedComponent();
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !GrabbedComponent)
	{
		Release();
		return;
	}
	
	FVector EyeLocation;
	FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	
	GrabbedComponent->AddImpulse(EyeRotation.Vector() * LaunchImpulseStrength, NAME_None, true);
	
	Release();
}

// Called every frame
void UGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsHolding()) return;
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;

	FVector EyeLocation;
	FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	
	FVector GrabTargetLocation = EyeLocation + (EyeRotation.Vector() * HoldDistance);

	PhysicsHandle->SetTargetLocationAndRotation(GrabTargetLocation, EyeRotation);
}

