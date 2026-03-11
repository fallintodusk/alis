using UnrealBuildTool;

public class ProjectDialogue : ModuleRules
{
	public ProjectDialogue(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ProjectCore",
			"ProjectFeature"  // Feature registry
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"GameplayAbilities"  // For IAbilitySystemInterface (condition checks)
		});
	}
}
