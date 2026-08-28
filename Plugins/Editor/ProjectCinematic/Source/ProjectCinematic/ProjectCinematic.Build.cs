// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

public class ProjectCinematic : ModuleRules
{
	public ProjectCinematic(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Editor module used during PIE recording and MRQ rendering only.
		// The descriptor and project target allowlist keep this module and its
		// capture dependencies out of cooked game/client/server targets.

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",         // ILookInputModifier interface
			"ProjectSinglePlay"    // ASinglePlayerGameMode parent class
		});

		// Note: UWorldPartitionStreamingSourceComponent + FStreamingSourceShape
		// live in the Engine module (Runtime/Engine/Classes/Components/...) in
		// UE 5.7, not a separate "WorldPartition" module. The "Engine" public
		// dep above is sufficient.

		// MRQ detection is editor-only. The plugin itself is Editor-type so
		// this guard is belt+suspenders; keep it explicit so the dependency
		// rationale stays readable.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("MovieRenderPipelineEditor");
			PrivateDependencyModuleNames.Add("UnrealEd"); // GEditor
		}
	}
}
