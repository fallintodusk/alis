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

            // Source engine diagnostics: opt-in via environment variable.
            // Installed engine -> env var unset -> vanilla Shipping (safe).
            // Source engine   -> env var set   -> logging + checks enabled.
            //
            // PowerShell: $env:ENGINE_FROM_SOURCE = "1"
            // Bash:       export ENGINE_FROM_SOURCE=1
            bool bSourceDiagnostics =
                Environment.GetEnvironmentVariable("ENGINE_FROM_SOURCE") == "1";

            if (bSourceDiagnostics)
            {
                BuildEnvironment = TargetBuildEnvironment.Unique;
                bOverrideBuildEnvironment = true;
                bUseLoggingInShipping = true;
                bUseChecksInShipping = true;
            }
        }
    }
}
