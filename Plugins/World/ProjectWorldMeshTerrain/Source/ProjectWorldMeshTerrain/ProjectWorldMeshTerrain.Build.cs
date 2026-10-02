using UnrealBuildTool;

public class ProjectWorldMeshTerrain : ModuleRules
{
	public ProjectWorldMeshTerrain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MeshPartition"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Json",
			"ProjectWorld"
		});
	}
}
