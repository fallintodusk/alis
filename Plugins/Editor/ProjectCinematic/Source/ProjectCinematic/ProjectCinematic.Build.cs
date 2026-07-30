// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

public class ProjectCinematic : ModuleRules
{
	public ProjectCinematic(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Runtime module of an Editor-type plugin. The plugin's "Type": "Editor"
		// guarantees this module never ships in cooked client/server builds;
		// ACinematicGameMode is purely a developer/editor tool used during PIE
		// recording sessions and MRQ render in the editor.

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
