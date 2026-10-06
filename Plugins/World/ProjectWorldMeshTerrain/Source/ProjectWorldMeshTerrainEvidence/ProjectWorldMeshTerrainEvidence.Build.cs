using System.IO;
using UnrealBuildTool;

public class ProjectWorldMeshTerrainEvidence : ModuleRules
{
	public ProjectWorldMeshTerrainEvidence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// MeshPartition's editor headers include Engine/Internal headers (MaterialCache), which UBT
		// exposes only to engine-scope modules.
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Internal"));

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MeshPartition",
			"MeshPartitionEditor",
			"ProjectWorldEditor",
			"ProjectWorldMeshTerrain"
		});
	}
}
