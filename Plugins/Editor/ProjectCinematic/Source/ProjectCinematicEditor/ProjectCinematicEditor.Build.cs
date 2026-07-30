// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

public class ProjectCinematicEditor : ModuleRules
{
	public ProjectCinematicEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"LevelSequence",
			"ProjectCore",      // IInteractionComponentInterface (visual mesh lookup) + future cross-plugin interfaces
			"ProjectCinematic"  // UCinematicHideMetadata (soft-ref carrier read by Director thunk)
		});

		// All the editor-only Sequencer + Take Recorder surfaces the
		// recorder/stamper drive. This module is Editor-type and only loads
		// inside an Editor-type plugin, so editor modules are unconditionally
		// available here.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorSubsystem",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"MovieScene",
			"MovieSceneTracks",
			"TakeRecorder",
			"TakesCore",
			"TakeRecorderSources",
			"TakeTrackRecorders",
			"AssetTools",
			"AssetRegistry",
			"ProjectSinglePlay",
			"ProjectSinglePlayClient",     // ASinglePlayController (delegates + SetPanel_*Visible)
			"ProjectObject"
		});
	}
}
