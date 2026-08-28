using UnrealBuildTool;

public class ProjectSinglePlayClient : ModuleRules
{
	public ProjectSinglePlayClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput",
			"GameplayAbilities",
			"InputCore",
			"Json",
			"ProjectCore",
			"ProjectObject",          // AInteractableActor cast for GetLastRespondingComponent
			"ProjectSinglePlay",
			"ProjectUI",
			"ProjectVitalsUI",
			"ProjectInventoryUI",
			"ProjectMindUI",
			"RHI",
			"Slate",
			"SlateCore",
			"UMG"
		});

		// Cinematic capture pipeline (recorder + stamper + Take Recorder +
		// LevelSequence integration) lives in Plugins/Editor/ProjectCinematic.
		// This module has no cinematic dependencies anymore -- the controller
		// only publishes generic observability delegates that the cinematic
		// plugin subscribes to.
	}
}
