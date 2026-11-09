// Copyright (c) 2026 ChiR24. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

bool McpRejectReservedParams(
	const TSharedPtr<FJsonObject>& Params, const FString& GatewayAction,
	FMcpSemanticError& OutError);
