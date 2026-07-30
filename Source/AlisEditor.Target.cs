// License terms: see repository root LICENSE.

using UnrealBuildTool;
using System.Collections.Generic;

public class AlisEditorTarget : TargetRules
{
	public AlisEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		// UE 5.7 settings
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange( new string[] { "Alis" } );
	}
}
