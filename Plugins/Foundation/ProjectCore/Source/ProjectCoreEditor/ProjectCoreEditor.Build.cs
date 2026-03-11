using UnrealBuildTool;

public class ProjectCoreEditor : ModuleRules
{
	public ProjectCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"UnrealEd",
			"ContentBrowser",
			"ContentBrowserData",
			"AssetRegistry"
		});
	}
}
