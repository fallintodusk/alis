using UnrealBuildTool;

public class ProjectTime : ModuleRules
{
	public ProjectTime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProjectCore" });
	}
}
