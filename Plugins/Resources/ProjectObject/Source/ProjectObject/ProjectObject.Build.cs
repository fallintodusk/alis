using UnrealBuildTool;

public class ProjectObject : ModuleRules
{
	public ProjectObject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"GameplayTags",
			"HairStrandsCore",
			"Json",
			"JsonUtilities",
			"ProjectCore",
			"ProjectWorld",
			"ProjectObjectCapabilities"
		});
	}
}
