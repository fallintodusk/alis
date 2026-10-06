using UnrealBuildTool;

public class ProjectWorldWaterEditor : ModuleRules
{
	public ProjectWorldWaterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "Json", "MeshDescription",
			"StaticMeshDescription", "GeometryCore", "GeometryAlgorithms",
			"AssetRegistry", "ProjectCore", "ProjectWorldEditor", "RHI", "UnrealEd"
		});
	}
}
