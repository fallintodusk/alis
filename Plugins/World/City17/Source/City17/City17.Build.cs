using UnrealBuildTool;

public class City17 : ModuleRules
{
	public City17(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
