// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ProjectUI : ModuleRules
{
	public ProjectUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		PublicIncludePaths.AddRange(new string[]
		{
			Path.Combine(ModuleDirectory, "Public", "Theme"),
			Path.Combine(ModuleDirectory, "Public", "Widgets"),
			Path.Combine(ModuleDirectory, "Public", "Animation"),
			Path.Combine(ModuleDirectory, "Public", "Effects"),
			Path.Combine(ModuleDirectory, "Public", "Dialogs"),
			Path.Combine(ModuleDirectory, "Public", "Overlay"),
			Path.Combine(ModuleDirectory, "Public", "Interaction"),
			Path.Combine(ModuleDirectory, "Public", "Presentation"),
			Path.Combine(ModuleDirectory, "Public", "Subsystems")
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",               // JSON parsing for widget layout configs
			"CommonUI",           // Modern UI framework for navigation and input
			"DeveloperSettings",  // For UProjectUISettings
			"Projects",           // For IPluginManager (GetPluginUIConfigPath)
			"ProjectCore",        // For ILoadingService + ServiceLocator
			"ProjectLoading",     // For UProjectLoadingSubsystem + types
			"GameplayTags"        // For FGameplayTag in UIExtensionSubsystem
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"ApplicationCore"  // For FSlateApplication (frame pumping during loading)
		});

		// DirectoryWatcher only available in Editor builds (used for editor hot-reload)
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("DirectoryWatcher");
		}

		StageDataDir(Target);
		StageFontsDir(Target);
	}

	private void StageDataDir(ReadOnlyTargetRules Target)
	{
		// Only stage for cooked builds (Game, Client, Server), not Editor
		if (Target.Type == TargetType.Editor)
			return;

		string DataDir = Path.Combine(PluginDirectory, "Data");
		if (!Directory.Exists(DataDir))
			return;

		// Recursively stage everything under Data as UFS
		RuntimeDependencies.Add(Path.Combine(DataDir, "..."), StagedFileType.UFS);
	}

	private void StageFontsDir(ReadOnlyTargetRules Target)
	{
		if (Target.Type == TargetType.Editor)
			return;

		// Font files (.ttf) are raw files loaded via FStandaloneCompositeFont at runtime.
		// They must be staged as NonUFS so they exist on disk for FPaths::FileExists().
		string FontsDir = Path.Combine(PluginDirectory, "Content", "Slate", "Fonts");
		if (!Directory.Exists(FontsDir))
			return;

		RuntimeDependencies.Add(Path.Combine(FontsDir, "*.ttf"), StagedFileType.NonUFS);
	}
}
