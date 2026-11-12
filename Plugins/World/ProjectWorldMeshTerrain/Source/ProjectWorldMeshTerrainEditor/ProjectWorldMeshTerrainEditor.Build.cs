using UnrealBuildTool;
using System.IO;

public class ProjectWorldMeshTerrainEditor : ModuleRules
{
	public ProjectWorldMeshTerrainEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Native section headers expose MaterialCache types from Engine/Internal.
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Internal"));
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"Chaos",
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"Json",
			"MeshDescription",
			"StaticMeshDescription",
			"PhysicsCore",
			"MeshPartition",
			"MeshPartitionEditor",
			"ProjectCore",
			"ProjectWorld",
			"ProjectWorldEditor",
			"ProjectWorldMeshTerrain",
			"Projects",
			"RenderCore",
			"RHI",
			"SourceControl",
			"UnrealEd"
		});
	}
}
