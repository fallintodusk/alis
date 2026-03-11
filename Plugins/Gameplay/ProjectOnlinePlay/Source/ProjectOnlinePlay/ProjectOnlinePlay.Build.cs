using UnrealBuildTool;

public class ProjectOnlinePlay : ModuleRules
{
	public ProjectOnlinePlay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectGameplay"
		});
	}
}
