using UnrealBuildTool;
using System.IO;

public class ProjectMind : ModuleRules
{
	public ProjectMind(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"Json",
			"ProjectCore"
		});

		StageDataDir(Target);
	}

	// Mind reads dialogue mappings, scan rules, and vitals mappings from
	// Plugins/Gameplay/ProjectMind/Data/ at runtime via FProjectPaths::GetPluginDataDir.
	// Without this stage hook the JSONs ship only in Editor; cooked builds fall through
	// to the "no mapping" branch and journal entries never emit.
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
