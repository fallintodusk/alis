using UnrealBuildTool;

public class ProjectWorldRoadEditor : ModuleRules
{
	public ProjectWorldRoadEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "ProjectWorldEditor" });
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject", "Engine", "Json", "MeshDescription",
			"StaticMeshDescription", "GeometryCore", "GeometryAlgorithms",
			"AssetRegistry", "ProjectCore", "UnrealEd"
		});
	}
}
