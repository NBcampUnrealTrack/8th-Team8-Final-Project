// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StrikeForge : ModuleRules
{
	public StrikeForge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			// basic, input
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			
			// ai
			"AIModule", "NavigationSystem", "GameplayTasks",
			
			// ui
			"UMG", "Slate", "SlateCore", "CommonUI", "GameplayTags", "MediaAssets",

			// effect
			"Niagara",
			
			// gc
			"Chaos",
			"GeometryCollectionEngine",
			"FieldSystemEngine",
        });

		PublicIncludePaths.AddRange(new string[] { "StrikeForge" });
		// Deferred cleanup for the development-only menu screenshot renderer.
		PrivateDependencyModuleNames.Add("RenderCore");
		PrivateDependencyModuleNames.Add("ApplicationCore");
		PrivateDependencyModuleNames.Add("MoviePlayer");
		if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test)
			PrivateDependencyModuleNames.AddRange(new[] { "MovieSceneCapture", "AudioMixer", "ImageCore" });
		PublicDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd");

        PublicDefinitions.Add("ECC_LANDSCAPE=ECC_GameTraceChannel1");

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
