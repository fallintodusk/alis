// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class OrchestratorTests : ModuleRules
{
	public OrchestratorTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OrchestratorCore"
			// BootROM dependency removed - FBootManifest deleted, tests use FOrchestratorManifest
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities"
		});

		// Required for automation tests
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
