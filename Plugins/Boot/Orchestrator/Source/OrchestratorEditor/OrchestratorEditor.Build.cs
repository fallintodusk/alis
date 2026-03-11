// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class OrchestratorEditor : ModuleRules
{
	public OrchestratorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"EditorSubsystem",
			"OrchestratorCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"
		});
	}
}
