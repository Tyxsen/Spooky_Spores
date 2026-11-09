// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULevel;
class UWorld;

namespace McpLevelHandlers {
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World);
} // namespace McpLevelHandlers
