// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Alis : ModuleRules
{
	public Alis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore"});
        PublicDependencyModuleNames.AddRange(new string[] { "ProjectCore", "ProjectLoading" });
	}
}
