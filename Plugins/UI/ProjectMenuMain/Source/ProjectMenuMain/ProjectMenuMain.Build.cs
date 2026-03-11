using UnrealBuildTool;
using System.IO;

public class ProjectMenuMain : ModuleRules
{
    public ProjectMenuMain(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "ProjectCore",
            "ProjectUI"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Slate",
            "SlateCore",
            "Projects",
            "Json",
            "JsonUtilities",
            "ProjectSettingsUI"
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
