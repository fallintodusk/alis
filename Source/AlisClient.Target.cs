// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class AlisClientTarget : TargetRules
{
	public AlisClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;

		// UE 5.7 settings
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange( new string[] { "Alis" } );
	}
}
