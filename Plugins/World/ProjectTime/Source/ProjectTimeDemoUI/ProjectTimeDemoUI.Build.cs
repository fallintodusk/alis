using UnrealBuildTool;
using System.IO;

public class ProjectTimeDemoUI : ModuleRules
{
	public ProjectTimeDemoUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProjectTime", "ProjectUI", "UMG" });
		PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "InputCore", "ProjectCore" });
		if (Target.Type != TargetType.Editor)
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Data", "..."), StagedFileType.UFS);
	}
}
