using UnrealBuildTool;

public class ProjectWorldConstructedEditor : ModuleRules
{
	public ProjectWorldConstructedEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "ProjectWorldEditor" });
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject", "Engine", "Json", "MeshDescription", "StaticMeshDescription",
			"GeometryCore", "AssetRegistry", "ProjectCore", "UnrealEd"
		});
	}
}
