// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class AlisServerTarget : TargetRules
{
	public AlisServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;

		// UE 5.7 settings
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange( new string[] { "Alis" } );
	}
}
