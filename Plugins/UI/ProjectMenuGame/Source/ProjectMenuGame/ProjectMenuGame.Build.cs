using UnrealBuildTool;
using System.IO;

public class ProjectMenuGame : ModuleRules
{
	public ProjectMenuGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"ProjectCore",
			"ProjectUI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"ProjectSettingsUI",
			"Slate",
			"SlateCore"
		});

		if (Target.Type != TargetType.Editor)
		{
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Data", "..."), StagedFileType.UFS);
		}
	}
}
