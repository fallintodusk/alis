using UnrealBuildTool;

// ProjectLoadingMoviePlayer: Subscribes to ProjectLoading events and displays MoviePlayer loading screen.
// Depends on ProjectLoading (one-way) - no cyclic dependency.
public class ProjectLoadingMoviePlayer : ModuleRules
{
    public ProjectLoadingMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "MoviePlayer",
            "UMG",
            "Slate",
            "SlateCore",
            "ProjectCore",
            "ProjectLoading"
        });
    }
}
