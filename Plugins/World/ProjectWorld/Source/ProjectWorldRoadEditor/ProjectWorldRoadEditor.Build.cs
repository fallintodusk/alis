using UnrealBuildTool;

public class ProjectWorldRoadEditor : ModuleRules
{
	public ProjectWorldRoadEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "Json", "MeshDescription",
			"StaticMeshDescription", "AssetRegistry", "ProjectCore", "ProjectWorldEditor", "UnrealEd"
		});
	}
}
