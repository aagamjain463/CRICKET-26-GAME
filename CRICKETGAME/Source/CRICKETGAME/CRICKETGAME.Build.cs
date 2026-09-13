// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CRICKETGAME : ModuleRules
{
	public CRICKETGAME(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
			"ProceduralMeshComponent",
			"DeveloperSettings",
			"HTTP",
			"Json",
			"JsonUtilities",
            "AnimGraphRuntime",
            "RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"CRICKETGAME",
			"CRICKETGAME/Variant_Platforming",
			"CRICKETGAME/Variant_Platforming/Animation",
			"CRICKETGAME/Variant_Combat",
			"CRICKETGAME/Variant_Combat/AI",
			"CRICKETGAME/Variant_Combat/Animation",
			"CRICKETGAME/Variant_Combat/Gameplay",
			"CRICKETGAME/Variant_Combat/Interfaces",
			"CRICKETGAME/Variant_Combat/UI",
			"CRICKETGAME/Variant_SideScrolling",
			"CRICKETGAME/Variant_SideScrolling/AI",
			"CRICKETGAME/Variant_SideScrolling/Gameplay",
			"CRICKETGAME/Variant_SideScrolling/Interfaces",
			"CRICKETGAME/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
