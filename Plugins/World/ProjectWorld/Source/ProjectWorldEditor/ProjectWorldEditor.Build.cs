// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

public class ProjectWorldEditor : ModuleRules
{
	public ProjectWorldEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectWorld"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Foliage",
			"GeoReferencing",
			"Json",
			"Landscape",
			"ProceduralMeshComponent",
			"ProjectCore",
			"UnrealEd"
		});
	}
}
