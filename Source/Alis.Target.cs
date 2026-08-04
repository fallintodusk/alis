// License terms: see repository root LICENSE.

using UnrealBuildTool;
using System.Collections.Generic;

public class AlisTarget : TargetRules
{
    public AlisTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;

        // UE 5.8 settings
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] { "Alis" });

        // Installed engines lack UnrealGame import libraries for modular game
        // targets. Source non-Shipping builds stay modular for CDN iteration.
        bool bSourceEngine = !bIsEngineInstalled;
        LinkType = (bIsEngineInstalled ||
                    Configuration == UnrealTargetConfiguration.Shipping)
            ? TargetLinkType.Monolithic
            : TargetLinkType.Modular;

        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            bBuildDeveloperTools = false;

            // DO NOT enable bUseChecksInShipping - crashes at RHI init.
            // Engine bug UE-86148: GraphicsContext is null because
            // CAN_TOGGLE_COMMAND_LIST_BYPASS=false in Shipping.
            // Use Test config for diagnostic builds with assertions.
            // bUseChecksInShipping = true;

            if (bSourceEngine)
            {
                // Public release diagnostics require a source engine because
                // launcher Shipping binaries are precompiled without logging.
                bUseLoggingInShipping = true;
                BuildEnvironment = TargetBuildEnvironment.Unique;
            }
        }
    }
}
