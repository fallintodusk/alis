// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
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

        // CDN hot-loading requires runtime modules to stay as DLLs even in Shipping.
        // Force modular linking so the base game does not bake every module into the exe.
        LinkType = TargetLinkType.Modular;

        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            bBuildDeveloperTools = false;

            // Alpha: enable game-module logging in Shipping so testers can send us logs.
            // Engine DLLs (prebuilt) remain silent; only Project* module logs are emitted.
            // Remove once the game reaches stable release.
            bUseLoggingInShipping = true;
        }
    }
}
