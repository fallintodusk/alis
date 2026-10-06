using UnrealBuildTool;

public class ProjectWorldBuildingEditor : ModuleRules
{
	public ProjectWorldBuildingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "Json", "MeshDescription",
			"StaticMeshDescription", "GeometryCore", "GeometryAlgorithms",
			"AssetRegistry", "ProjectCore", "ProjectWorldEditor", "UnrealEd"
		});
	}
}
