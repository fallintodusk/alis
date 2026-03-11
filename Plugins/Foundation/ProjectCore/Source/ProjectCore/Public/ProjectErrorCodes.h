// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Centralized error code definitions for Project loading and runtime systems.
 *
 * Error codes are organized by loading phase and system component.
 * Range: 100-999 (codes outside this range are invalid).
 *
 * Usage:
 *   PhaseState.ErrorCode = ProjectErrorCodes::ResolveAssets::ManifestNotFound;
 *   PhaseState.ErrorMessage = NSLOCTEXT("Loading", "ManifestNotFound", "World manifest not found");
 *
 * Phase-specific ranges:
 *   100-199: Phase 1 (ResolveAssets)
 *   200-299: Phase 2 (MountContent)
 *   300-399: Phase 3 (PreloadCriticalAssets)
 *   400-499: Phase 4 (ActivateFeatures)
 *   500-599: Phase 5 (Travel)
 *   600-699: Phase 6 (Warmup)
 *   700-799: Reserved for future phases
 *   800-899: General system errors
 *   900-999: Reserved
 *
 * See: docs/systems/loading_pipeline.md
 */
namespace ProjectErrorCodes
{
	/** Error code range validation */
	constexpr int32 MinErrorCode = 100;
	constexpr int32 MaxErrorCode = 999;

	inline bool IsValidErrorCode(int32 ErrorCode)
	{
		return ErrorCode >= MinErrorCode && ErrorCode <= MaxErrorCode;
	}

	/** Phase 1: ResolveAssets (100-199) */
	namespace ResolveAssets
	{
		constexpr int32 RangeStart = 100;
		constexpr int32 RangeEnd = 199;

		constexpr int32 ManifestNotFound = 101;           // World or experience manifest missing
		constexpr int32 InvalidManifest = 102;            // Manifest validation failed
		constexpr int32 AssetManagerNotReady = 103;       // Asset Manager not initialized
		constexpr int32 DependencyResolutionFailed = 104; // Cannot resolve content pack dependencies
		constexpr int32 CircularDependency = 105;         // Circular dependency detected in packs
	}

	/** Phase 2: MountContent (200-299) */
	namespace MountContent
	{
		constexpr int32 RangeStart = 200;
		constexpr int32 RangeEnd = 299;

		constexpr int32 DownloadFailed = 201;             // Content pack download failed
		constexpr int32 MountFailed = 202;                // PAK mount operation failed
		constexpr int32 ChunkNotCached = 203;             // Chunk not in cache when expected
		constexpr int32 PakCorrupted = 204;               // PAK file integrity check failed
		constexpr int32 InsufficientDiskSpace = 205;      // Not enough space for download
		constexpr int32 NetworkTimeout = 206;             // Download timed out
	}

	/** Phase 3: PreloadCriticalAssets (300-399) */
	namespace PreloadAssets
	{
		constexpr int32 RangeStart = 300;
		constexpr int32 RangeEnd = 399;

		constexpr int32 AssetLoadFailed = 301;            // Failed to load critical asset
		constexpr int32 StreamingTimeout = 302;           // Streaming took too long
		constexpr int32 InvalidAssetPath = 303;           // Asset path malformed or missing
		constexpr int32 AssetTypeMismatch = 304;          // Loaded asset is wrong type
	}

	/** Phase 4: ActivateFeatures (400-499) */
	namespace ActivateFeatures
	{
		constexpr int32 RangeStart = 400;
		constexpr int32 RangeEnd = 499;

		constexpr int32 FeatureNotFound = 401;            // Feature plugin not registered
		constexpr int32 ActivationFailed = 402;           // Feature failed to load/activate
		constexpr int32 DependencyMissing = 403;          // Required feature dependency missing
		constexpr int32 FeatureIncompatible = 404;        // Feature incompatible with current build
	}

	/** Phase 5: Travel (500-599) */
	namespace Travel
	{
		constexpr int32 RangeStart = 500;
		constexpr int32 RangeEnd = 599;

		constexpr int32 MapNotFound = 501;                // Target map does not exist
		constexpr int32 TravelFailed = 502;               // Map travel operation failed
		constexpr int32 ConnectionLost = 503;             // Network connection lost during travel
		constexpr int32 ServerFull = 504;                 // Target server is full
	}

	/** Phase 6: Warmup (600-699) */
	namespace Warmup
	{
		constexpr int32 RangeStart = 600;
		constexpr int32 RangeEnd = 699;

		constexpr int32 ShaderCompileFailed = 601;        // Shader warmup compilation failed
		constexpr int32 StreamingWarmupFailed = 602;      // Level streaming pre-cache failed
		constexpr int32 WarmupTimeout = 603;              // Warmup phase took too long
	}

	/** General system errors (800-899) */
	namespace System
	{
		constexpr int32 RangeStart = 800;
		constexpr int32 RangeEnd = 899;

		constexpr int32 UnknownError = 800;               // Unclassified error
		constexpr int32 Cancelled = 801;                  // Operation cancelled by user
		constexpr int32 Timeout = 802;                    // Generic timeout
		constexpr int32 OutOfMemory = 803;                // Insufficient memory
		constexpr int32 InvalidState = 804;               // System in invalid state
	}
}
