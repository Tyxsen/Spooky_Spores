// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;

namespace VolumeHelpers
{
TSharedPtr<FJsonObject> CreateVectorObject(const FVector& Value);
TSharedPtr<FJsonObject> CreateVolumeResponse(AActor* Volume, const FString& VolumeClass);
}
