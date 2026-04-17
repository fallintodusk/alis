// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System;
using System.Collections.Generic;

public class AlisTarget : TargetRules
{
    public AlisTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;

        // UE 5.7 settings
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] { "Alis" });

        // Shipping uses monolithic: installed/launcher engines lack Shipping .lib
        // stubs for engine modules, so modular linking fails.
        // Development/DebugGame stay modular for CDN hot-loading iteration.
        LinkType = (Configuration == UnrealTargetConfiguration.Shipping)
            ? TargetLinkType.Monolithic
            : TargetLinkType.Modular;

        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            bBuildDeveloperTools = false;
            bUseLoggingInShipping = true;

            // DO NOT enable bUseChecksInShipping - crashes at RHI init.
            // Engine bug UE-86148: GraphicsContext is null because
            // CAN_TOGGLE_COMMAND_LIST_BYPASS=false in Shipping.
            // Use Test config for diagnostic builds with assertions.
            // bUseChecksInShipping = true;

            // Source engine needs Unique build env to avoid UBT binary
            // sharing mismatch. Set by build/packaging scripts automatically.
            bool bSourceEngine =
                Environment.GetEnvironmentVariable("ENGINE_FROM_SOURCE") == "1";

            if (bSourceEngine)
            {
                BuildEnvironment = TargetBuildEnvironment.Unique;
                bOverrideBuildEnvironment = true;
            }
        }
    }
}
