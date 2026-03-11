// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ProjectVitalsUI : ModuleRules
{
	public ProjectVitalsUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
			"InputCore",
			"GameplayAbilities",
			"GameplayTags",
			"ProjectCore",
			"ProjectUI",
			"ProjectGAS",
			"ProjectVitals"
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
