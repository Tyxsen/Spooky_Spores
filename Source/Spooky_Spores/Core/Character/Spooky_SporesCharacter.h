// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Spooky_SporesCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class AServant;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class ASpooky_SporesCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SprintAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MouseLookAction;
	
	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* InteractAction;
	
	/** Launch Objects Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LaunchObjectsAction;

	/** Toggle controls panel Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ToggleControlsAction;

	/** Cast a spell Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* CastSpellAction;

	/** Place a servant (consumes a Necrospore) Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* PlaceServantAction;

	/** Toggle servant task menu Input Action (must have bTriggerWhenPaused, the menu pauses the game) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleServantMenuAction = nullptr;

	/** SERVANTS */
	/** Servant class spawned when a Necrospore is placed (set to the Blueprint child in BP_FirstPersonCharacter) */
	UPROPERTY(EditAnywhere, Category = "Servants")
	TSubclassOf<AServant> ServantClass;

	/** MOVEMENT */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float WalkSpeed = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintSpeed = 650.0f;
	
public:
	ASpooky_SporesCharacter();

protected:

	virtual void BeginPlay() override;

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();
	
	/** Handles interaction with things such as environment or grabbable objects */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteractAction();

	/** Handles launch while holding a grabbable object */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLaunchObjects();

	/** Handles UI interface for controls panel */
	UFUNCTION(BlueprintImplementableEvent, Category="UI")
	void OnToggleControlsPanel();

	/** Handles sprint if stamina is above zero */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoSprintStart();

	/** Handles sprint end */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoSprintStop();

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCastSpell();

	/** Places a servant at the aimed ground point if the player has a Necrospore */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoPlaceServant();

	/** Show or hide the servant task menu widget (pause, cursor and input mode are already handled in C++) */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnServantMenuToggled(bool bIsOpen);

	UFUNCTION()
	void HandleSprintStateChanged(bool bNewIsSprinting);

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

private:
	void DoToggleControlsPanel();

	void DoToggleServantMenu();

	/** Pauses the game and gives the cursor to the menu, or the reverse. Not a generic pause system (single consumer). */
	void SetServantMenuOpen(bool bOpen);

	bool bIsServantMenuOpen = false;

public:

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsServantMenuOpen() const { return bIsServantMenuOpen; }

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

};

