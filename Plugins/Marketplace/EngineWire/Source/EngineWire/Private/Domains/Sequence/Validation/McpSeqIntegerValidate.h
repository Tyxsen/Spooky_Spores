// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpSequenceIntegerValidation {

bool ValidateSequenceIntegerFields(
    const TSharedPtr<FJsonObject> &Payload, FString &OutError);

}
