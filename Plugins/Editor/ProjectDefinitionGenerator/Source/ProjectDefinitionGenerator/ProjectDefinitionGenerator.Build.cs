// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectDefinitionGenerator : ModuleRules
{
	public ProjectDefinitionGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"AssetRegistry",
			"UnrealEd",
			"ProjectCore",
			"ProjectEditorCore", // FDefinitionEvents delegate
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetTools",
			"AssetDefinition",
			"EditorSubsystem",
			"DirectoryWatcher",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"InputCore",
			"ProjectObject",
			"ProjectObjectCapabilities",
			"ProjectGAS",          // UProjectAbilitySet generation
			"GameplayAbilities"    // TSubclassOf<UGameplayAbility>
			// NOTE: ProjectPlacementEditor subscribes to OnDefinitionRegenerated (not the other way)
		});
	}
}
