using UnrealBuildTool;
using System.IO;

public class ProjectSinglePlay : ModuleRules
{
	public ProjectSinglePlay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectFeature"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",           // For JSON override loading
			"JsonUtilities",  // For JSON parsing utilities
			"ProjectGameplay"
		});

		StageDataDir(Target);
	}

	private void StageDataDir(ReadOnlyTargetRules Target)
	{
		if (Target.Type == TargetType.Editor)
			return;

		string DataDir = System.IO.Path.Combine(PluginDirectory, "Data");
		if (!System.IO.Directory.Exists(DataDir))
			return;

		RuntimeDependencies.Add(System.IO.Path.Combine(DataDir, "..."), StagedFileType.UFS);
	}
}
