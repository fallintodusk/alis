// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRuntimePartitionPolicy.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "WorldPartition/WorldPartition.h"

namespace ProjectWorldMapVerifyProjection
{
	FString TagValue(const AActor& Actor, const FString& Prefix)
	{
		for (const FName& Tag : Actor.Tags)
		{
			const FString Value = Tag.ToString();
			if (Value.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				return Value.RightChop(Prefix.Len());
			}
		}
		return FString();
	}

	FString Number(double Value)
	{
		return FString::Printf(TEXT("%.9g"), Value);
	}

	void AddExposure(FProjectWorldProjectionRecord& Record, const FPostProcessSettings& Settings)
	{
		Record.Fields.Add(TEXT("camera.method"), FString::FromInt(static_cast<int32>(Settings.AutoExposureMethod)));
		Record.Fields.Add(TEXT("camera.physical"), Settings.AutoExposureApplyPhysicalCameraExposure ? TEXT("true") : TEXT("false"));
		Record.Fields.Add(TEXT("camera.iso"), Number(Settings.CameraISO));
		Record.Fields.Add(TEXT("camera.shutter"), Number(Settings.CameraShutterSpeed));
		Record.Fields.Add(TEXT("camera.fstop"), Number(Settings.DepthOfFieldFstop));
		Record.Fields.Add(TEXT("camera.bias"), Number(Settings.AutoExposureBias));
		Record.Fields.Add(TEXT("camera.overrides"), FString::Printf(TEXT("%d%d%d%d%d%d"),
			Settings.bOverride_AutoExposureMethod,
			Settings.bOverride_AutoExposureApplyPhysicalCameraExposure,
			Settings.bOverride_CameraISO,
			Settings.bOverride_CameraShutterSpeed,
			Settings.bOverride_DepthOfFieldFstop,
			Settings.bOverride_AutoExposureBias));
	}

	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		if (Scope.World == nullptr || Scope.World->GetWorldPartition() == nullptr)
		{
			OutError = TEXT("Map verify requires a loaded World Partition map.");
			return false;
		}
		FProjectWorldRuntimePartitionSettings Partition;
		if (!ProjectWorldRuntimePartitionPolicy::Read(Scope.World, Partition, OutError))
		{
			return false;
		}
		FProjectWorldProjectionRecord& World = Projection.Add(TEXT("world"), TEXT("partition"));
		World.Fields.Add(TEXT("class"), Scope.World->GetWorldPartition()->GetClass()->GetPathName());
		World.Fields.Add(TEXT("partition_count"), FString::FromInt(Partition.PartitionCount));
		World.Fields.Add(TEXT("partition_cell_size_cm"), FString::FromInt(Partition.CellSizeCentimeters));
		World.Fields.Add(TEXT("partition_loading_range_cm"), FString::FromInt(Partition.LoadingRangeCentimeters));
		World.Fields.Add(TEXT("partition_is_2d"), Partition.bIs2D ? TEXT("true") : TEXT("false"));
		World.Fields.Add(TEXT("partition_block_on_slow_streaming"), Partition.bBlockOnSlowStreaming ? TEXT("true") : TEXT("false"));

		for (TActorIterator<AActor> It(Scope.World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor == nullptr || !Actor->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag))
			{
				continue;
			}
			const FString PresentationRole = TagValue(*Actor, TEXT("ProjectWorld.PresentationRole="));
			const FString RuntimeRole = TagValue(*Actor, TEXT("ProjectWorld.RuntimeRole="));
			const FString OverlayId = TagValue(*Actor, TEXT("ProjectWorld.AuthoredOverlay="));
			const FString Role = !PresentationRole.IsEmpty() ? TEXT("presentation:") + PresentationRole
				: !RuntimeRole.IsEmpty() ? TEXT("runtime:") + RuntimeRole
				: !OverlayId.IsEmpty() ? TEXT("overlay:") + OverlayId : FString();
			if (Role.IsEmpty())
			{
				continue;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("actor"), Role);
			ProjectWorldProjection::AddActorCommon(Record, *Actor);
			if (const ADirectionalLight* Sun = Cast<ADirectionalLight>(Actor))
			{
				Record.Fields.Add(TEXT("sun.intensity"), Number(Sun->GetComponent()->Intensity));
				Record.Fields.Add(TEXT("sun.mobility"), FString::FromInt(Sun->GetComponent()->Mobility));
				Record.Fields.Add(TEXT("sun.atmosphere"), Sun->GetComponent()->bAtmosphereSunLight ? TEXT("true") : TEXT("false"));
			}
			else if (const ASkyLight* Sky = Cast<ASkyLight>(Actor))
			{
				Record.Fields.Add(TEXT("sky.intensity"), Number(Sky->GetLightComponent()->Intensity));
				Record.Fields.Add(TEXT("sky.realtime"), Sky->GetLightComponent()->bRealTimeCapture ? TEXT("true") : TEXT("false"));
			}
			else if (const AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(Actor))
			{
				Record.Fields.Add(TEXT("fog.density"), Number(Fog->GetComponent()->FogDensity));
				Record.Fields.Add(TEXT("fog.falloff"), Number(Fog->GetComponent()->FogHeightFalloff));
				Record.Fields.Add(TEXT("fog.volumetric"), Fog->GetComponent()->bEnableVolumetricFog ? TEXT("true") : TEXT("false"));
			}
			else if (const AVolumetricCloud* Cloud = Cast<AVolumetricCloud>(Actor))
			{
				const UVolumetricCloudComponent* Component = Cloud->FindComponentByClass<UVolumetricCloudComponent>();
				Record.Fields.Add(TEXT("cloud.material"), ProjectWorldProjection::ObjectPath(
					Component != nullptr ? Component->GetMaterial() : nullptr, Scope.ArtifactRoot));
			}
			else if (const APostProcessVolume* Post = Cast<APostProcessVolume>(Actor))
			{
				Record.Fields.Add(TEXT("post.unbound"), Post->bUnbound ? TEXT("true") : TEXT("false"));
				Record.Fields.Add(TEXT("post.blend_weight"), Number(Post->BlendWeight));
				AddExposure(Record, Post->Settings);
			}
			else if (const ACameraActor* Camera = Cast<ACameraActor>(Actor))
			{
				const UCameraComponent* Component = Camera->GetCameraComponent();
				if (Component != nullptr)
				{
					Record.Fields.Add(TEXT("camera.fov"), Number(Component->FieldOfView));
					Record.Fields.Add(TEXT("camera.blend_weight"), Number(Component->PostProcessBlendWeight));
					AddExposure(Record, Component->PostProcessSettings);
				}
			}
			else if (const ALevelInstance* Overlay = Cast<ALevelInstance>(Actor))
			{
				Record.Fields.Add(TEXT("overlay.world_asset"), Overlay->GetWorldAsset().ToSoftObjectPath().ToString());
			}
			else if (const ANavMeshBoundsVolume* NavBounds = Cast<ANavMeshBoundsVolume>(Actor))
			{
				Record.Fields.Add(TEXT("navigation.bounds_extent"),
					ProjectWorldProjection::Transform(FTransform(NavBounds->GetComponentsBoundingBox(true).GetExtent())));
			}
		}
		for (TActorIterator<ARecastNavMesh> It(Scope.World); It; ++It)
		{
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("navigation"), TEXT("recast"));
			Record.Fields.Add(TEXT("class"), It->GetClass()->GetPathName());
			Record.Fields.Add(TEXT("tile_size_uu"), Number(It->GetTileSizeUU()));
		}
		return true;
	}

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("map"), 1, 1, &Project);
}
