// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

using UnrealBuildTool;

public class NodeToCode : ModuleRules
{
	public NodeToCode(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
        
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine", 
				"InputCore",
				"Json",
				"UnrealEd",
				"BlueprintGraph",
				"Slate",
				"SlateCore",
				"Kismet",
				"GraphEditor",
				"HTTP",
				"UMG",
				"ToolMenus",
				"ApplicationCore",
				"Projects",
				"HttpServer"
			}
		);
        
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings", 
				"Blutility", 
				"UMGEditor",
				"KismetWidgets",          // For action menu widgets
				"EditorSubsystem",        // For editor subsystem access
				"EditorWidgets",          // For editor widgets
				"AssetRegistry"           // For asset path resolution
			}
		);
	}
}
