// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectCore : ModuleRules
{
	public ProjectCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Projects",       // IPluginManager for FProjectPaths
			"Json",           // JSON serialization for orchestrator registry
			"JsonUtilities",  // JSON utilities for parsing/serialization
			"GameplayTags"    // Hierarchical tag system
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Intentionally empty for now
		});

		// No asset dependencies allowed in ProjectCore module
		// This module provides only interfaces, types, and utilities
	}
}
