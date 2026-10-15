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
			"AssetRegistry", "FileUtilities", "ProjectCore", "ProjectWorldEditor", "RHI", "UnrealEd"
		});
	}
}
