// Copyright Epic Games, Inc. All Rights Reserved.

#include "Spooky_SporesCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Spooky_Spores.h"
#include "Interaction/GrabComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Engine/World.h"
#include "Movement/StaminaComponent.h"
#include "Resources/ResourceCounterComponent.h"
#include "Servants/Servant.h"
#include "Spells/SpellCastingComponent.h"

ASpooky_SporesCharacter::ASpooky_SporesCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void ASpooky_SporesCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	
	if (UStaminaComponent* StaminaComponent = FindComponentByClass<UStaminaComponent>())
		StaminaComponent->OnSprintStateChanged.AddDynamic(this, &ASpooky_SporesCharacter::HandleSprintStateChanged);
}

void ASpooky_SporesCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FirstPersonCameraComponent->GetComponentLocation();
	OutRotation = FirstPersonCameraComponent->GetComponentRotation();
}

void ASpooky_SporesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASpooky_SporesCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASpooky_SporesCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASpooky_SporesCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASpooky_SporesCharacter::LookInput);
	
		// Interacting/launching
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoInteractAction);
		EnhancedInputComponent->BindAction(LaunchObjectsAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoLaunchObjects);

		// Sprinting
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoSprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASpooky_SporesCharacter::DoSprintStop);

		// Controls Panel
		EnhancedInputComponent->BindAction(ToggleControlsAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoToggleControlsPanel);

		// Cast Spell
		EnhancedInputComponent->BindAction(CastSpellAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoCastSpell);

		// Place Servant
		EnhancedInputComponent->BindAction(PlaceServantAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoPlaceServant);

		// Servant Task Menu
		EnhancedInputComponent->BindAction(ToggleServantMenuAction, ETriggerEvent::Started, this, &ASpooky_SporesCharacter::DoToggleServantMenu);
	}
	else
	{
		UE_LOG(LogSpooky_Spores, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ASpooky_SporesCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ASpooky_SporesCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ASpooky_SporesCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ASpooky_SporesCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ASpooky_SporesCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void ASpooky_SporesCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void ASpooky_SporesCharacter::DoInteractAction()
{
	UGrabComponent* Grab = FindComponentByClass<UGrabComponent>();
	UInteractionComponent* Interaction = FindComponentByClass<UInteractionComponent>();
	if (!Grab || !Interaction) return;
	
	if (Grab->IsHolding())
	{
		Grab->Release();
		return;
	}
	
	UPrimitiveComponent* TargetComponent = nullptr;
	if (Interaction->IsGrabbableTarget(TargetComponent))
	{
		Grab->Grab(TargetComponent);
		return;
	}
	
	Interaction->TryInteract();
}

void ASpooky_SporesCharacter::DoLaunchObjects()
{
	if (UGrabComponent* Grab = FindComponentByClass<UGrabComponent>())
	{
		Grab->Launch();
	}
}

void ASpooky_SporesCharacter::DoSprintStart()
{
	UStaminaComponent* StaminaComponent = FindComponentByClass<UStaminaComponent>();
	if (!StaminaComponent) return;

	StaminaComponent->TryStartSprint();
}

void ASpooky_SporesCharacter::DoSprintStop()
{
	UStaminaComponent* StaminaComponent = FindComponentByClass<UStaminaComponent>();
	if (!StaminaComponent) return;

	StaminaComponent->StopSprint();
}

void ASpooky_SporesCharacter::DoCastSpell()
{
	if (USpellCastingComponent* SpellCasting = FindComponentByClass<USpellCastingComponent>())
		SpellCasting->TryCastSpell();
}

void ASpooky_SporesCharacter::DoPlaceServant()
{
	if (!ServantClass)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' has no ServantClass set, cannot place a servant"), *GetNameSafe(this));
		return;
	}

	UInteractionComponent* Interaction = FindComponentByClass<UInteractionComponent>();
	UResourceCounterComponent* Counter = FindComponentByClass<UResourceCounterComponent>();
	if (!Interaction || !Counter) return;

	// 1. Find a valid ground point first, so aiming at the sky or a wall never costs a Necrospore
	FVector PlacementPoint;
	if (!Interaction->GetPlacementPoint(PlacementPoint)) return;

	// 2. Single access point to the resurrection item storage: to replace here once the real inventory exists
	if (!Counter->TrySpendResource(EResourceType::Necrospore, 1)) return;

	// 3. A Character's origin is the centre of its capsule: raise it by the half height so its feet touch the ground
	const ACharacter* ServantDefaults = ServantClass->GetDefaultObject<ACharacter>();
	const float HalfHeight = ServantDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector SpawnLocation = PlacementPoint + FVector(0.0f, 0.0f, HalfHeight);

	// Yaw only, facing the player
	const FRotator SpawnRotation(0.0f, (GetActorLocation() - PlacementPoint).Rotation().Yaw, 0.0f);

	// Always spawn (moved out of overlaps if possible), so a spent Necrospore never needs to be refunded
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (!GetWorld()->SpawnActor<AServant>(ServantClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		// Only reachable on a programming error (e.g. world being torn down): the Necrospore is lost, make it visible
		UE_LOG(LogSpooky_Spores, Error, TEXT("'%s' failed to spawn a servant after spending a Necrospore"), *GetNameSafe(this));
	}
}

void ASpooky_SporesCharacter::HandleSprintStateChanged(bool bNewIsSprinting)
{
	GetCharacterMovement()->MaxWalkSpeed = bNewIsSprinting ? SprintSpeed : WalkSpeed;
}

void ASpooky_SporesCharacter::DoToggleControlsPanel()
{
	OnToggleControlsPanel();
}

void ASpooky_SporesCharacter::DoToggleServantMenu()
{
	SetServantMenuOpen(!bIsServantMenuOpen);
}

void ASpooky_SporesCharacter::SetServantMenuOpen(bool bOpen)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		UE_LOG(LogSpooky_Spores, Warning, TEXT("'%s' is not controlled by a PlayerController, cannot toggle the servant menu"), *GetNameSafe(this));
		return;
	}

	bIsServantMenuOpen = bOpen;

	// While paused, only IA_ToggleServantMenu (bTriggerWhenPaused) and UMG clicks stay active.
	// Held actions (sprint, jump) receive Completed on the first paused frame, so nothing stays stuck.
	PlayerController->SetPause(bOpen);
	PlayerController->SetShowMouseCursor(bOpen);

	if (bOpen)
	{
		// Not UIOnly: it ignores game input, so the toggle key could not close the menu
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
	}

	OnServantMenuToggled(bOpen);
}
