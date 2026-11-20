using UnrealBuildTool;

public class ProjectTimeTests : ModuleRules
{
	public ProjectTimeTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProjectTime", "ProjectCore", "ProjectTimeDemoUI", "ProjectUI", "UMG", "Slate", "SlateCore", "InputCore", "AutomationDriver" });
	}
}
