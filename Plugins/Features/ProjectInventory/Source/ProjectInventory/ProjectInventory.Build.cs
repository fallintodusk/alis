using UnrealBuildTool;

public class ProjectInventory : ModuleRules
{
	public ProjectInventory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",            // FFastArraySerializer
			"GameplayTags",
			"GameplayAbilities",  // For UAbilitySystemComponent
			"Json",               // For JSON parsing in feature init
			"ProjectCore",        // IObjectSpawnService, IItemDataProvider, etc.
			"ProjectSharedTypes",
			"ProjectFeature",
			"ProjectGAS"          // ApplyMagnitudes, FAttributeMagnitude
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ProjectSave"
		});
	}
}
