// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;
using System.Collections.Generic;

public class AlisClientTarget : TargetRules
{
	public AlisClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;

		// UE 5.8 settings
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange( new string[] { "Alis" } );
	}
}
