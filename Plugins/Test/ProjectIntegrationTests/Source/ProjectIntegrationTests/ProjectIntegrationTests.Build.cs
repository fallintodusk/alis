using UnrealBuildTool;
using System.IO;
using System.Linq;

public class ProjectIntegrationTests : ModuleRules
{
	public ProjectIntegrationTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"Json",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"GameplayAbilities",   // UAbilitySystemComponent for ASC-backed vitals tests
			"ProjectGAS",          // AttributeSet classes used by vitals ASC fixture tests
			"ProjectUI",            // Widgets, ProjectUIThemeManager.h
			"ProjectInventory",     // Container session subsystem and inventory runtime tests
			"ProjectInventoryUI",   // Inventory panel definitions and ViewModel
			"ProjectVitalsUI",      // Vitals ViewModel and widget contracts
			"ProjectMenuMain",      // Screens/ProjectMenuScreen.h, ViewModels
			"ProjectDialogue",      // UProjectDialogueComponent integration coverage
			"ProjectDialogueUI",    // DialogueViewModel for auto_visibility tests
			"ProjectMind",          // FMindServiceImpl integration coverage
			"ProjectMindUI",        // MindThoughtViewModel integration tests
			"ProjectVitals",        // Vitals component logic contracts
			"ProjectCore",          // Types/ProjectLoadRequest.h
			"ProjectInteraction",   // UInteractionComponent focus/selection integration tests
			"ProjectObject",        // ObjectDefinition and spawn utility integration tests
			"ProjectObjectCapabilities", // Lockable capability test coverage
			"ProjectMotionSystem",  // Spring motion capability mesh-target selection coverage
			"ProjectWorld",         // ObjectDefinition host helper integration tests
			"ProjectDefinitionGenerator", // JSON parser spawnClass normalization tests
			"ProjectPlacementEditor", // DefinitionActorSyncSubsystem passive-load integration tests
			"OrchestratorCore",     // Boot orchestration, updatable feature loading
			"ProjectLoading",       // UProjectLoadingSubsystem
			"UnrealEd",
			"InputCore",
			"EditorSubsystem",
			"AutomationController",
			"Projects"
		});

		bool bHasProjectBoot = false;
		if (Target.ProjectFile != null)
		{
			string projectDir = Path.GetDirectoryName(Target.ProjectFile.FullName);
			string pluginsDir = Path.Combine(projectDir, "Plugins");
			if (Directory.Exists(pluginsDir))
			{
				bHasProjectBoot = Directory.GetFiles(pluginsDir, "ProjectBoot.uplugin", SearchOption.AllDirectories).Any();
			}
		}

		if (bHasProjectBoot)
		{
			PrivateDependencyModuleNames.Add("ProjectBoot");
			PrivateDefinitions.Add("WITH_PROJECTBOOT=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_PROJECTBOOT=0");
		}
	}
}
