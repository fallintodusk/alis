/**
 * SinglePlayModeDefaults.cpp
 *
 * AGENT-EDITABLE FILE: Define single-play mode configurations here.
 *
 * This file contains all mode definitions for ProjectSinglePlay.
 * Agents can add, modify, or remove modes by editing this file.
 *
 * LINKER DEPENDENCY NOTE:
 * This file uses static initializers (DEFINE_SINGLEPLAY_MODE macro) to register
 * modes with FSinglePlayModeRegistry at startup. The linker must include this
 * translation unit for modes to be available at runtime.
 *
 * If modes are missing at runtime:
 * 1. Verify this .cpp is compiled (check ProjectSinglePlay.Build.cs)
 * 2. Ensure no linker optimizations strip "unused" code
 * 3. Check FSinglePlayModeRegistry::GetAllModes() returns expected count
 *
 * Usage:
 *   DEFINE_SINGLEPLAY_MODE(ModeName)
 *   {
 *       Config.ModeName = FName(TEXT("ModeName"));
 *       Config.DefaultPawnClass = ...;  // TSoftClassPtr
 *       Config.PlayerControllerClass = ...;  // TSoftClassPtr
 *       Config.FeatureNames.Add(TEXT("Combat"));  // Features to init via registry
 *       Config.FeatureConfigs.Add(FName("Combat"), TEXT("{...}"));  // Per-feature config
 *   }
 *
 * Available fields in FSinglePlayModeConfig:
 *   - ModeName: FName identifier (must match DEFINE_SINGLEPLAY_MODE argument)
 *   - DefaultPawnClass: TSoftClassPtr<APawn> - pawn class to spawn
 *   - PlayerControllerClass: TSoftClassPtr<APlayerController> - controller class
 *   - FeatureNames: TArray<FName> - ordered features to init via FFeatureRegistry
 *   - FeatureConfigs: TMap<FName, FString> - per-feature JSON config strings
 */

#include "SinglePlayModeRegistry.h"
#include "GameFramework/DefaultPawn.h"

// =============================================================================
// MODE: Beginner (Easier difficulty)
// =============================================================================
DEFINE_SINGLEPLAY_MODE(Beginner)
{
	Config.ModeName = FName(TEXT("Beginner"));

	// Character class (ProjectCharacter plugin)
	Config.DefaultPawnClass = TSoftClassPtr<APawn>(FSoftClassPath(TEXT("/Script/ProjectCharacter.ProjectCharacter")));
	Config.PlayerControllerClass = TSoftClassPtr<APlayerController>(FSoftClassPath(TEXT("/Script/ProjectSinglePlayClient.SinglePlayerPlayerController")));

	// Features to initialize (order matters)
	Config.FeatureNames.Add(TEXT("Interaction"));
	Config.FeatureNames.Add(TEXT("Combat"));
	Config.FeatureNames.Add(TEXT("Inventory"));
	Config.FeatureNames.Add(TEXT("Dialogue"));

	// Per-feature configuration
	Config.FeatureConfigs.Add(FName(TEXT("Combat")), TEXT("{\"damageMultiplier\":0.5}"));
}

// =============================================================================
// MODE: Medium (Default single-player mode - ACTIVE)
// =============================================================================
DEFINE_SINGLEPLAY_MODE(Medium)
{
	Config.ModeName = FName(TEXT("Medium"));

	// Character class (ProjectCharacter plugin)
	Config.DefaultPawnClass = TSoftClassPtr<APawn>(FSoftClassPath(TEXT("/Script/ProjectCharacter.ProjectCharacter")));
	Config.PlayerControllerClass = TSoftClassPtr<APlayerController>(FSoftClassPath(TEXT("/Script/ProjectSinglePlayClient.SinglePlayerPlayerController")));

	// Features to initialize (order matters)
	Config.FeatureNames.Add(TEXT("Interaction"));
	Config.FeatureNames.Add(TEXT("Combat"));
	Config.FeatureNames.Add(TEXT("Inventory"));
	Config.FeatureNames.Add(TEXT("Dialogue"));

	// Per-feature configuration
	Config.FeatureConfigs.Add(FName(TEXT("Combat")), TEXT("{\"damageMultiplier\":1.0}"));
}

// =============================================================================
// MODE: Hardcore (Harder difficulty)
// =============================================================================
DEFINE_SINGLEPLAY_MODE(Hardcore)
{
	Config.ModeName = FName(TEXT("Hardcore"));

	// Character class (ProjectCharacter plugin)
	Config.DefaultPawnClass = TSoftClassPtr<APawn>(FSoftClassPath(TEXT("/Script/ProjectCharacter.ProjectCharacter")));
	Config.PlayerControllerClass = TSoftClassPtr<APlayerController>(FSoftClassPath(TEXT("/Script/ProjectSinglePlayClient.SinglePlayerPlayerController")));

	// Features to initialize (order matters)
	Config.FeatureNames.Add(TEXT("Interaction"));
	Config.FeatureNames.Add(TEXT("Combat"));
	Config.FeatureNames.Add(TEXT("Inventory"));
	Config.FeatureNames.Add(TEXT("Dialogue"));

	// Per-feature configuration
	Config.FeatureConfigs.Add(FName(TEXT("Combat")), TEXT("{\"damageMultiplier\":2.0}"));
}

// =============================================================================
// ADD NEW MODES BELOW
// =============================================================================
// Example:
//
// DEFINE_SINGLEPLAY_MODE(Survival)
// {
//     Config.ModeName = FName(TEXT("Survival"));
//     Config.FeatureNames.Add(TEXT("Combat"));
//     Config.FeatureNames.Add(TEXT("Inventory"));
//     Config.FeatureNames.Add(TEXT("Hunger"));  // Custom feature
//     Config.FeatureConfigs.Add(FName(TEXT("Hunger")), TEXT("{\"enabled\":true}"));
// }
