// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEngineWireSubsystem;
class FMcpBridgeWebSocket;

class FBlueprintCreationHandlers
{
public:
    static bool HandleBlueprintCreate(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
};
