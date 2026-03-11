using UnrealBuildTool;
using System.IO;

public class ProjectSettingsUI : ModuleRules
{
	public ProjectSettingsUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"ProjectUI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities"
		});

		StageDataDir(Target);
	}

	private void StageDataDir(ReadOnlyTargetRules Target)
	{
		if (Target.Type == TargetType.Editor)
			return;

		string DataDir = Path.Combine(PluginDirectory, "Data");
		if (!Directory.Exists(DataDir))
			return;

		RuntimeDependencies.Add(Path.Combine(DataDir, "..."), StagedFileType.UFS);
	}
}
