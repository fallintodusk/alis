using UnrealBuildTool;

// ProjectLoading: Core loading pipeline that broadcasts events.
// No dependency on ProjectLoadingMoviePlayer - observer pattern allows any module to subscribe.
public class ProjectLoading : ModuleRules
{
	public ProjectLoading(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"ProjectCore"
		});

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore"  // For FShaderPipelineCache shader warmup
            // NOTE: Slate pumping moved to UI layer via Context.OnPumpFrame delegate
        });
	}
}
