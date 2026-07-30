// License terms: see repository root LICENSE.

using UnrealBuildTool;

public class Alis : ModuleRules
{
	public Alis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ApplicationCore"});
        PublicDependencyModuleNames.AddRange(new string[] { "ProjectCore", "ProjectLoading" });
	}
}
