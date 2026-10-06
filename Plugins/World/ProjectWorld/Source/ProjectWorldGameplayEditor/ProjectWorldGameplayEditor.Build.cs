using UnrealBuildTool;

public class ProjectWorldGameplayEditor : ModuleRules
{
	public ProjectWorldGameplayEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "Json", "ProjectCore", "ProjectWorld",
			"ProjectWorldEditor", "UnrealEd"
		});
	}
}
