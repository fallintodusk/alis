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
			"AssetRegistry",
			"Foliage",
			"GeoReferencing",
			"GeometryAlgorithms",
			"GeometryCore",
			"Json",
			"Landscape",
			"MeshDescription",
			"NavigationSystem",
			"ProceduralMeshComponent",
			"ProjectCore",
			"Projects",
			"RHI",
			"StaticMeshDescription",
			"UnrealEd"
		});
	}
}
