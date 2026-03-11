using UnrealBuildTool;

public class ProjectMenuPlay : ModuleRules
{
	public ProjectMenuPlay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectLoading",  // For UProjectLoadingSubsystem (ILoadingService implementation)
			"ProjectMenuMain"
		});
	}
}
