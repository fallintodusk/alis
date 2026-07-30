// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

// ProjectOpenableTemplates: Legacy actor templates for openable world objects.
// Depends on ProjectMotionSystem for SpringRotator/SpringSlider components.
// The data-driven path uses AInteractableActor + capabilities from JSON.
public class ProjectOpenableTemplates : ModuleRules
{
	public ProjectOpenableTemplates(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectWorld",
			"ProjectObject",
			"ProjectMotionSystem",
			"ProjectObjectCapabilities"
		});
	}
}
