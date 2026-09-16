// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Spooky_Spores : ModuleRules
{
	public Spooky_Spores(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Spooky_Spores",
			"Spooky_Spores/Variant_Horror",
			"Spooky_Spores/Variant_Horror/UI",
			"Spooky_Spores/Variant_Shooter",
			"Spooky_Spores/Variant_Shooter/AI",
			"Spooky_Spores/Variant_Shooter/UI",
			"Spooky_Spores/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
