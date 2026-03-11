using UnrealBuildTool;

// ProjectMotionSystem: Skeleton-agnostic motion primitives.
// Provides FProjectMotionPose, procedural components, IK solvers.
// No skeletal mesh dependencies - usable by characters, trees, props.
public class ProjectMotionSystem : ModuleRules
{
	public ProjectMotionSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// No additional private dependencies
		});
	}
}
