using UnrealBuildTool;

// ProjectAnimation: Skeleton-specific animation content and locomotion drivers.
// Contains:
// - UProjectAnimInstanceBase (variables + SetMotionPose)
// - USkeletonLocomotionDriver (bone/curve mapping per skeleton)
// - ULocomotionAnimSet data assets (state -> sequences/blendspaces)
//
// Depends on: ProjectCore, ProjectMotionSystem
// May NOT depend on: Gameplay modules (Character, GAS, etc.)
public class ProjectAnimation : ModuleRules
{
	public ProjectAnimation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectMotionSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Add private dependencies here
		});
	}
}
