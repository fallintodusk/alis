using UnrealBuildTool;

public class ProjectLight : ModuleRules
{
	public ProjectLight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProjectCore" });
		PrivateDependencyModuleNames.AddRange(new[] { "SunPosition" });
	}
}
