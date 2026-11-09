// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

namespace McpInputHandlers
{
UInputAction* LoadInputActionAsset(const FString& RawPath, FString& OutNormalizedPath);
UInputMappingContext* LoadInputMappingContextAsset(
    const FString& RawPath,
    FString& OutNormalizedPath);
UObject* LoadInputObjectAsset(const FString& RawPath, FString& OutNormalizedPath);
}
