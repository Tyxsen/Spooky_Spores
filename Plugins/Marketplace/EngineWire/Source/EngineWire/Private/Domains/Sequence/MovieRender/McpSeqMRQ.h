// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpSequenceMovieRender {
bool TryHandle(UEngineWireSubsystem *Subsystem, const FString &RequestId,
               const FString &Action, const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
