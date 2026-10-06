using UnrealBuildTool;

public class ProjectWorldVegetationEditor : ModuleRules
{
	public ProjectWorldVegetationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "Json", "GeometryCore",
			"GeometryAlgorithms", "ProjectCore", "ProjectWorldEditor", "UnrealEd"
		});
	}
}
