// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "Templates/SharedPointer.h"

class FJsonObject;

namespace McpSequenceFrameRate {

bool TryParse(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName,
              FFrameRate &OutRate, FString &OutError);

}
