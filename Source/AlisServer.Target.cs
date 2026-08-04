// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class AlisServerTarget : TargetRules
{
	public AlisServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;

		// UE 5.8 settings
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange( new string[] { "Alis" } );
	}
}
