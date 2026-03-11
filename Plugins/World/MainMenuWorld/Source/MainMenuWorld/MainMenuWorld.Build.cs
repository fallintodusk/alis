using UnrealBuildTool;

public class MainMenuWorld : ModuleRules
{
	public MainMenuWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
