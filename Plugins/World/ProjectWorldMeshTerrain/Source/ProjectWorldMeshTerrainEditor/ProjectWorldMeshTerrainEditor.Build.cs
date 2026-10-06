using UnrealBuildTool;

public class ProjectWorldMeshTerrainEditor : ModuleRules
{
	public ProjectWorldMeshTerrainEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"Json",
			"MeshPartition",
			"MeshPartitionEditor",
			"ProjectCore",
			"ProjectWorld",
			"ProjectWorldEditor",
			"ProjectWorldMeshTerrain",
			"RenderCore",
			"UnrealEd"
		});
	}
}
