// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/BaseZoneSubsystem.h"

#include "Base/BaseZone.h"
#include "Spooky_Spores.h"

ABaseZone* UBaseZoneSubsystem::GetBaseZone() const
{
	return RegisteredZone.Get();
}

void UBaseZoneSubsystem::RegisterZone(ABaseZone* Zone)
{
	if (!Zone) return;

	ABaseZone* Current = RegisteredZone.Get();
	if (Current == Zone) return;

	if (Current)
	{
		// Deterministic: the first zone stays, the second is ignored.
		UE_LOG(LogSpooky_Spores, Error, TEXT("Several BaseZone actors in one world: keeping %s, ignoring %s."),
			*Current->GetName(), *Zone->GetName());
		return;
	}

	RegisteredZone = Zone;
}

void UBaseZoneSubsystem::UnregisterZone(ABaseZone* Zone)
{
	// Only the registered instance can unregister itself (an ignored second zone must not clear the first).
	if (Zone && RegisteredZone.Get() == Zone)
	{
		RegisteredZone.Reset();
	}
}
