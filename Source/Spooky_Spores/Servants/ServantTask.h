// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ServantTask.generated.h"

// Task category a servant is assigned to. Adding a category = a new value here + a section in WBP_ServantTaskMenu.
UENUM(BlueprintType)
enum class EServantTask : uint8
{
	Unassigned		UMETA(DisplayName = "Non assigné"),
	AttackDefense	UMETA(DisplayName = "Attaque / défense"),
	FieldHarvest	UMETA(DisplayName = "Récolte de champ")
};
