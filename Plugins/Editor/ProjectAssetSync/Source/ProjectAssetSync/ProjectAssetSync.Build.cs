// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectAssetSync : ModuleRules
{
	public ProjectAssetSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorSubsystem",
			"AssetRegistry",
			"AssetTools",
			"DeveloperSettings",
			"SourceControl",
			"Projects",
			"ProjectCore",
			"ProjectEditorCore"
		});
	}
}
