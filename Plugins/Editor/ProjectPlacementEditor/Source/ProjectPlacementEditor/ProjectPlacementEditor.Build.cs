// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectPlacementEditor : ModuleRules
{
	public ProjectPlacementEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"AssetRegistry",
			"ToolMenus",
			"ContentBrowser",
			"LevelEditor",
			"WorkspaceMenuStructure",
			"EditorFramework",
			"InputCore",
			"ApplicationCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Runtime modules
			"ProjectCore",               // FLootEntryView (for property customization)
			"ProjectWorld",              // AProjectWorldActor base class for data-driven actors
			"ProjectObject",             // UObjectDefinition, ObjectSpawnUtility
			"ProjectObjectCapabilities", // FCapabilityRegistry
			"GameplayTags",              // FGameplayTag special-case in SetPropertyByName

			// Editor modules
			"PropertyEditor",            // FPropertyEditorModule, IPropertyTypeCustomization
			"ProjectEditorCore",         // FDefinitionEvents delegate (decoupled pub/sub)
			"ProjectDefinitionGenerator",// UDefinitionGeneratorSubsystem (single-asset regenerate from browser)
			"EditorSubsystem"            // UEditorSubsystem base class (separate module in UE 5.x)
		});
	}
}
