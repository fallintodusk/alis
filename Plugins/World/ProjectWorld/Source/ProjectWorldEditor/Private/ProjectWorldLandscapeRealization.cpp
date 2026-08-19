// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldLandscapeRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationService.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeConfigHelper.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"

namespace ProjectWorldLandscapeRealization
{
	const FName GeneratedTag(TEXT("ProjectWorld.Generated.v1"));
	const FName LandscapeTag(TEXT("ProjectWorld.Landscape.v1"));
	const FName GeneratedBaseLayerName(TEXT("Generated Base"));
	const FName GeneratedRoadsLayerName(TEXT("Generated Roads"));
	const FName AuthoredCorrectionsLayerName(TEXT("Authored Corrections"));
	const FName TerrainRowOrderTag(TEXT("ProjectWorld.TerrainRows=north_to_south_v1"));
	const FString LogicalLandscapeTagPrefix(TEXT("ProjectWorld.LogicalLandscape="));
	const FString TerrainCellTagPrefix(TEXT("ProjectWorld.TerrainCell="));
	const FString TerrainInputTagPrefix(TEXT("ProjectWorld.TerrainInput="));

	FGuid StableGuid(const FString& Value)
	{
		FGuid Guid;
		FGuid::ParseExact(FMD5::HashAnsiString(*Value), EGuidFormats::Digits, Guid);
		return Guid;
	}

	bool SetIdentityTag(AActor* Actor, const FString& Prefix, const FString& Value)
	{
		const FName Expected(*(Prefix + Value));
		if (Actor->Tags.Contains(Expected) &&
			!Actor->Tags.ContainsByPredicate([&Prefix, &Expected](const FName& Tag)
			{
				return Tag != Expected && Tag.ToString().StartsWith(Prefix);
			}))
		{
			return false;
		}
		Actor->Tags.RemoveAll([&Prefix](const FName& Tag)
		{
			return Tag.ToString().StartsWith(Prefix);
		});
		Actor->Tags.Add(Expected);
		return true;
	}

	bool RemoveIdentityTags(AActor* Actor, const FString& Prefix)
	{
		return Actor->Tags.RemoveAll([&Prefix](const FName& Tag)
		{
			return Tag.ToString().StartsWith(Prefix);
		}) > 0;
	}

	bool HasIdentityTag(const AActor* Actor, const FString& Prefix, const FString& Value)
	{
		return Actor->Tags.Contains(FName(*(Prefix + Value)));
	}

	bool SetComponentIdentityTag(ULandscapeComponent* Component, const FString& Prefix, const FString& Value)
	{
		const FName Expected(*(Prefix + Value));
		if (Component->ComponentTags.Contains(Expected) &&
			!Component->ComponentTags.ContainsByPredicate([&Prefix, &Expected](const FName& Tag)
			{
				return Tag != Expected && Tag.ToString().StartsWith(Prefix);
			}))
		{
			return false;
		}
		Component->ComponentTags.RemoveAll([&Prefix](const FName& Tag)
		{
			return Tag.ToString().StartsWith(Prefix);
		});
		Component->ComponentTags.Add(Expected);
		return true;
	}

	bool HasComponentIdentityTag(
		const ULandscapeComponent* Component,
		const FString& Prefix,
		const FString& Value)
	{
		return Component->ComponentTags.Contains(FName(*(Prefix + Value)));
	}

	int32 CountLandscapeComponents(ULandscapeInfo* LandscapeInfo)
	{
		int32 Count = 0;
		LandscapeInfo->ForAllLandscapeComponents([&Count](ULandscapeComponent*)
		{
			++Count;
		});
		return Count;
	}

	bool EnsureCellProxyTopology(
		UWorld* World,
		ALandscape* Landscape,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		int32 ComponentsPerProxy,
		bool& bOutChanged,
		FString& OutError)
	{
		bOutChanged = false;
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		if (LandscapeInfo == nullptr || ComponentsPerProxy != 1)
		{
			OutError = TEXT("Landscape profile does not resolve to one component per streaming proxy.");
			return false;
		}
		const int32 ExpectedComponents = Layout.ComponentCount.X * Layout.ComponentCount.Y;
		if (CountLandscapeComponents(LandscapeInfo) != ExpectedComponents)
		{
			OutError = TEXT("Landscape component population differs from the canonical cell domain.");
			return false;
		}
		int32 MinimumCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		const int32 ComponentQuads = Layout.SectionsPerComponent * Layout.QuadsPerSection;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		}
		if (!World->IsPartitionedWorld())
		{
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				const FIntPoint ComponentIndex(
					((Cell.CellX - MinimumCellX) * Bundle.CellQuads.X) / ComponentQuads,
					((MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y) / ComponentQuads);
				ULandscapeComponent* const* Component = LandscapeInfo->XYtoComponentMap.Find(ComponentIndex);
				if (Component == nullptr)
				{
					OutError = FString::Printf(
						TEXT("Canonical cell has no Landscape component: %s"),
						*Cell.CellId);
					return false;
				}
				if (SetComponentIdentityTag(*Component, TerrainInputTagPrefix, Cell.Terrain.ArtifactHash))
				{
					(*Component)->MarkPackageDirty();
				}
			}
			return true;
		}

		bool bNeedsPartition = Landscape->LandscapeComponents.Num() > 0;
		int32 ProxyCount = 0;
		for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
		{
			if (It->GetLandscapeActor() != Landscape)
			{
				continue;
			}
			++ProxyCount;
			bNeedsPartition |= It->LandscapeComponents.Num() != 1;
		}
		bNeedsPartition |= ProxyCount != ExpectedComponents;
		if (bNeedsPartition && !FLandscapeConfigHelper::PartitionLandscape(
			World,
			LandscapeInfo,
			static_cast<uint32>(ComponentsPerProxy)))
		{
			OutError = TEXT("Epic LandscapeConfigHelper could not partition the logical Landscape.");
			return false;
		}
		bOutChanged |= bNeedsPartition;

		TSet<ALandscapeStreamingProxy*> ClaimedProxies;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FIntPoint ComponentIndex(
				((Cell.CellX - MinimumCellX) * Bundle.CellQuads.X) / ComponentQuads,
				((MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y) / ComponentQuads);
			ULandscapeComponent* const* Component = LandscapeInfo->XYtoComponentMap.Find(ComponentIndex);
			ALandscapeStreamingProxy* Proxy = Component != nullptr
				? Cast<ALandscapeStreamingProxy>((*Component)->GetTypedOuter<ALandscapeProxy>())
				: nullptr;
			if (Proxy == nullptr || ClaimedProxies.Contains(Proxy))
			{
				OutError = FString::Printf(
					TEXT("Canonical cell has no unique Landscape streaming proxy: %s"),
					*Cell.CellId);
				return false;
			}
			if (!Proxy->IsPackageExternal() || !Proxy->IsMainPackageActor() || !Proxy->IsAsset())
			{
				OutError = FString::Printf(
					TEXT("Landscape proxy is not a discoverable external actor: cell=%s external=%d main=%d asset=%d."),
					*Cell.CellId,
					Proxy->IsPackageExternal() ? 1 : 0,
					Proxy->IsMainPackageActor() ? 1 : 0,
					Proxy->IsAsset() ? 1 : 0);
				return false;
			}
			const FIntPoint ExpectedSectionBase = ComponentIndex * ComponentQuads;
			if ((*Component)->GetSectionBase() != ExpectedSectionBase)
			{
				OutError = FString::Printf(
					TEXT("Landscape component SectionBase differs from canonical cell %s."),
					*Cell.CellId);
				return false;
			}
			const FVector NorthWest = FProjectWorldCanonicalLoader::UnrealToCanonical(
				Bundle,
				Landscape->GetActorTransform().TransformPosition(
					FVector(ExpectedSectionBase.X, ExpectedSectionBase.Y, 0.0)));
			const FVector SouthEast = FProjectWorldCanonicalLoader::UnrealToCanonical(
				Bundle,
				Landscape->GetActorTransform().TransformPosition(FVector(
					ExpectedSectionBase.X + ComponentQuads,
					ExpectedSectionBase.Y + ComponentQuads,
					0.0)));
			const double Tolerance = FMath::Max(Bundle.CoordinateQuantizationMeters, 0.000001);
			if (!FVector2D(NorthWest.X, NorthWest.Y).Equals(FVector2D(Cell.Bounds.X, Cell.Bounds.W), Tolerance) ||
				!FVector2D(SouthEast.X, SouthEast.Y).Equals(FVector2D(Cell.Bounds.Z, Cell.Bounds.Y), Tolerance))
			{
				OutError = FString::Printf(
					TEXT("Landscape component bounds differ from canonical cell %s."),
					*Cell.CellId);
				return false;
			}
			ClaimedProxies.Add(Proxy);
			bool bProxyChanged = SetIdentityTag(Proxy, TerrainCellTagPrefix, Cell.CellId);
			if (!Proxy->Tags.Contains(GeneratedTag))
			{
				Proxy->Tags.Add(GeneratedTag);
				bProxyChanged = true;
			}
			if (!Proxy->Tags.Contains(LandscapeTag))
			{
				Proxy->Tags.Add(LandscapeTag);
				bProxyChanged = true;
			}
			if (SetComponentIdentityTag(*Component, TerrainInputTagPrefix, Cell.Terrain.ArtifactHash))
			{
				(*Component)->MarkPackageDirty();
			}
			if (!Proxy->GetIsSpatiallyLoaded())
			{
				Proxy->SetIsSpatiallyLoaded(true);
				bProxyChanged = true;
			}
			if (Proxy->bEnableAutoLODGeneration || Proxy->GetHLODLayer() != nullptr)
			{
				Proxy->bEnableAutoLODGeneration = false;
				Proxy->SetHLODLayer(nullptr);
				bProxyChanged = true;
			}
			if (bProxyChanged)
			{
				Proxy->MarkPackageDirty();
				bOutChanged = true;
			}
		}
		if (Landscape->GetIsSpatiallyLoaded())
		{
			Landscape->SetIsSpatiallyLoaded(false);
			bOutChanged = true;
		}
		return ClaimedProxies.Num() == ExpectedComponents;
	}

	bool AddRequiredLayers(
		ALandscape* Landscape,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(0);
		if (BaseLayer == nullptr)
		{
			OutError = TEXT("Landscape import created no default edit layer.");
			return false;
		}
		BaseLayer->SetName(GeneratedBaseLayerName, true);

		if (Landscape->CreateLayer(GeneratedRoadsLayerName) == INDEX_NONE ||
			Landscape->CreateLayer(AuthoredCorrectionsLayerName) == INDEX_NONE)
		{
			OutError = TEXT("Cannot create the required generated and authored Landscape layers.");
			return false;
		}
		ULandscapeEditLayerBase* RoadsLayer = Landscape->GetEditLayer(GeneratedRoadsLayerName);
		ULandscapeEditLayerBase* AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (RoadsLayer == nullptr || AuthoredLayer == nullptr)
		{
			OutError = TEXT("Landscape layer creation returned no editable layer.");
			return false;
		}
		OutResult.AuthoredCorrectionLayerGuid =
			AuthoredLayer->GetGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		return true;
	}

	bool UpdateGeneratedBase(
		ALandscape* Landscape,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		const FProjectWorldLandscapeHeightfield& Heightfield,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(GeneratedBaseLayerName);
		ULandscapeEditLayerBase* AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (BaseLayer == nullptr || AuthoredLayer == nullptr)
		{
			OutError = TEXT("Existing generated Landscape is missing a required edit layer.");
			return false;
		}

		const FGuid AuthoredGuidBefore = AuthoredLayer->GetGuid();
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		if (LandscapeInfo == nullptr)
		{
			OutError = TEXT("Existing generated Landscape has no LandscapeInfo.");
			return false;
		}

		int32 MinimumCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		}

		const int32 ComponentQuads = Layout.SectionsPerComponent * Layout.QuadsPerSection;
		const bool bTerrainRowOrderChanged = !Landscape->Tags.Contains(TerrainRowOrderTag);
		FScopedSetLandscapeEditingLayer LayerScope(
			Landscape,
			BaseLayer->GetGuid(),
			[Landscape]()
			{
				Landscape->RequestLayersContentUpdateForceAll();
			});
		FLandscapeEditDataInterface EditData(LandscapeInfo, BaseLayer->GetGuid());
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const int32 CanonicalOffsetX = (Cell.CellX - MinimumCellX) * Bundle.CellQuads.X;
			const int32 CanonicalOffsetY = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
			const int32 RegionX1 = CanonicalOffsetX;
			const int32 RegionX2 = RegionX1 + Bundle.CellQuads.X;
			const int32 RegionY1 = CanonicalOffsetY;
			const int32 RegionY2 = RegionY1 + Bundle.CellQuads.Y;
			if (RegionX1 % ComponentQuads != 0 || RegionY1 % ComponentQuads != 0 ||
				Bundle.CellQuads.X % ComponentQuads != 0 || Bundle.CellQuads.Y % ComponentQuads != 0)
			{
				OutError = FString::Printf(
					TEXT("Changed terrain cell %s is not component-aligned for partial Landscape update."),
					*Cell.CellId);
				return false;
			}
			const FIntPoint ComponentIndex(
				CanonicalOffsetX / ComponentQuads,
				CanonicalOffsetY / ComponentQuads);
			ULandscapeComponent* const* Component = LandscapeInfo->XYtoComponentMap.Find(ComponentIndex);
			ALandscapeProxy* Proxy = Component != nullptr
				? (*Component)->GetTypedOuter<ALandscapeProxy>()
				: nullptr;
			if (Proxy == nullptr)
			{
				OutError = FString::Printf(
					TEXT("Changed terrain cell has no Landscape proxy: %s"),
					*Cell.CellId);
				return false;
			}
			if (!bTerrainRowOrderChanged &&
				HasComponentIdentityTag(*Component, TerrainInputTagPrefix, Cell.Terrain.ArtifactHash))
			{
				continue;
			}

			EditData.SetHeightData(
				RegionX1,
				RegionY1,
				RegionX2,
				RegionY2,
				Heightfield.EncodedHeights.GetData() +
					RegionY1 * Heightfield.VertexCount.X + RegionX1,
				Heightfield.VertexCount.X,
				true);
			SetComponentIdentityTag(*Component, TerrainInputTagPrefix, Cell.Terrain.ArtifactHash);
			(*Component)->MarkPackageDirty();
			OutResult.UpdatedLandscapeComponentCount +=
				(Bundle.CellQuads.X / ComponentQuads) * (Bundle.CellQuads.Y / ComponentQuads);
		}

		AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (AuthoredLayer == nullptr || AuthoredLayer->GetGuid() != AuthoredGuidBefore)
		{
			OutError = TEXT("Authored Corrections Landscape layer identity changed during regeneration.");
			return false;
		}
		OutResult.bAuthoredCorrectionLayerPreserved = true;
		OutResult.AuthoredCorrectionLayerGuid =
			AuthoredGuidBefore.ToString(EGuidFormats::DigitsWithHyphensLower);
		Landscape->Tags.AddUnique(TerrainRowOrderTag);

		// Composition barrier. RequestLayersContentUpdateForceAll only MARKS the edit-layer
		// stack dirty; the merge itself is driven by ALandscape::TickLayers, which requires an
		// editor tick loop that a commandlet never runs. Without this synchronous force the
		// Generated Base source layer is correct while the final/base heightmap keeps its
		// initialized flat contents (raw height 0 = -256 m) - the shipped Kazan defect.
		// ForceUpdateLayersContent waits for streaming and flushes render, so the final
		// surface is composed before anything is verified or saved. It needs a render-capable
		// envelope: UE skips PrepareTextureResources when FApp::CanEverRender() is false, so
		// the realize wrapper declares Rendering=Required (scripts/ue/world/execution_envelope.ps1).
		Landscape->ForceUpdateLayersContent();
		return true;
	}

	bool IsGeneratedLandscape(const AActor* Actor)
	{
		return Actor != nullptr && Actor->IsA<ALandscapeProxy>() &&
			Actor->Tags.Contains(GeneratedTag) && Actor->Tags.Contains(LandscapeTag);
	}

	bool BuildHeightfield(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		FProjectWorldLandscapeHeightfield& OutHeightfield,
		FString& OutError)
	{
		if (!Layout.bCompatible || Bundle.Cells.IsEmpty())
		{
			OutError = TEXT("Canonical grid is not compatible with a stock Landscape layout.");
			return false;
		}

		int32 MinimumCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		}

		OutHeightfield.VertexCount = Layout.TotalQuads + FIntPoint(1, 1);
		const int32 SampleCount = OutHeightfield.VertexCount.X * OutHeightfield.VertexCount.Y;
		TArray<double> CanonicalHeights;
		CanonicalHeights.SetNumUninitialized(SampleCount);
		TBitArray<> Written(false, SampleCount);
		OutHeightfield.MinimumHeightMeters = TNumericLimits<double>::Max();
		OutHeightfield.MaximumHeightMeters = TNumericLimits<double>::Lowest();

		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			if (Cell.Terrain.SamplesX != Bundle.CellQuads.X + 1 ||
				Cell.Terrain.SamplesY != Bundle.CellQuads.Y + 1 ||
				!Cell.Terrain.SampleSpacing.Equals(Bundle.SampleSpacingMeters, KINDA_SMALL_NUMBER))
			{
				OutError = FString::Printf(TEXT("Terrain sampling contract differs in %s."), *Cell.CellId);
				return false;
			}

			const int32 OffsetX = (Cell.CellX - MinimumCellX) * Bundle.CellQuads.X;
			const int32 OffsetY = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
			for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
			{
				for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
				{
					const int32 GlobalX = OffsetX + Column;
					const int32 GlobalY = OffsetY + Row;
					const int32 GlobalIndex = GlobalY * OutHeightfield.VertexCount.X + GlobalX;
					const double Height = Cell.Terrain.HeightsMeters[
						Row * Cell.Terrain.SamplesX + Column];
					if (Written[GlobalIndex] &&
						!FMath::IsNearlyEqual(CanonicalHeights[GlobalIndex], Height, Bundle.HeightQuantizationMeters))
					{
						OutError = FString::Printf(TEXT("Terrain seam differs at canonical sample %d,%d."), GlobalX, GlobalY);
						return false;
					}
					CanonicalHeights[GlobalIndex] = Height;
					Written[GlobalIndex] = true;
					OutHeightfield.MinimumHeightMeters = FMath::Min(OutHeightfield.MinimumHeightMeters, Height);
					OutHeightfield.MaximumHeightMeters = FMath::Max(OutHeightfield.MaximumHeightMeters, Height);
				}
			}
		}

		OutHeightfield.EncodedHeights.SetNumUninitialized(SampleCount);
		for (int32 LandscapeY = 0; LandscapeY < OutHeightfield.VertexCount.Y; ++LandscapeY)
		{
			for (int32 X = 0; X < OutHeightfield.VertexCount.X; ++X)
			{
				const int32 CanonicalIndex = LandscapeY * OutHeightfield.VertexCount.X + X;
				if (!Written[CanonicalIndex])
				{
					OutError = FString::Printf(TEXT("Terrain envelope has no sample at %d,%d."), X, LandscapeY);
					return false;
				}
				const float RelativeHeight = static_cast<float>(
					CanonicalHeights[CanonicalIndex] - Bundle.HeightOriginMeters);
				const uint16 Encoded = LandscapeDataAccess::GetTexHeight(RelativeHeight);
				const double Decoded = LandscapeDataAccess::GetLocalHeight(Encoded) + Bundle.HeightOriginMeters;
				if (FMath::Abs(Decoded - CanonicalHeights[CanonicalIndex]) > Bundle.HeightQuantizationMeters)
				{
					OutError = TEXT("Landscape 16-bit height encoding exceeds canonical height tolerance.");
					return false;
				}
				OutHeightfield.EncodedHeights[
					LandscapeY * OutHeightfield.VertexCount.X + X] = Encoded;
			}
		}
		return true;
	}

	bool CreateOrUpdate(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		const FString& LogicalLandscapeId,
		int32 ComponentsPerProxy,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError,
		UMaterialInterface* LandscapeMaterial)
	{
		FProjectWorldLandscapeHeightfield Heightfield;
		if (!BuildHeightfield(Bundle, Layout, Heightfield, OutError))
		{
			return false;
		}

		ALandscape* Landscape = nullptr;
		bool bCreatedLandscape = false;
		bool bLogicalContractChanged = false;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (!IsGeneratedLandscape(*It))
			{
				continue;
			}
			if (Landscape != nullptr)
			{
				OutError = TEXT("Generated map contains more than one owned Landscape.");
				return false;
			}
			Landscape = *It;
		}

		const int32 ComponentQuads = Layout.SectionsPerComponent * Layout.QuadsPerSection;
		if (Landscape != nullptr)
		{
			ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
			if (LandscapeInfo == nullptr)
			{
				OutError = TEXT("Existing generated Landscape has no LandscapeInfo.");
				return false;
			}
			// External proxies load after the persistent Landscape. Rebuild Epic's
			// registry before validating the complete logical family.
			ULandscapeInfo::RecreateLandscapeInfo(World, false);
			bLogicalContractChanged = !Landscape->Tags.Contains(TerrainRowOrderTag);
			LandscapeInfo = Landscape->GetLandscapeInfo();
			const bool bGridMatches = HasIdentityTag(
				Landscape, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			const bool bLogicalIdentityMatches = LogicalLandscapeId.IsEmpty() ||
				HasIdentityTag(Landscape, LogicalLandscapeTagPrefix, LogicalLandscapeId);
			const int32 ActualComponentCount = LandscapeInfo != nullptr
				? CountLandscapeComponents(LandscapeInfo)
				: -1;
			const int32 ExpectedComponentCount = Layout.ComponentCount.X * Layout.ComponentCount.Y;
			if (!bGridMatches || !bLogicalIdentityMatches ||
				Landscape->ComponentSizeQuads != ComponentQuads ||
				Landscape->NumSubsections != Layout.SectionsPerComponent ||
				Landscape->SubsectionSizeQuads != Layout.QuadsPerSection ||
				LandscapeInfo == nullptr || ActualComponentCount != ExpectedComponentCount)
			{
				int32 LoadedProxyCount = 0;
				int32 LoadedProxyComponentCount = 0;
				for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
				{
					if (It->GetLandscapeGuid() == Landscape->GetLandscapeGuid())
					{
						++LoadedProxyCount;
						LoadedProxyComponentCount += It->LandscapeComponents.Num();
					}
				}
				OutError = FString::Printf(
					TEXT("Existing generated Landscape differs: grid=%d logical=%d component_quads=%d/%d subsections=%d/%d subsection_quads=%d/%d components=%d/%d loaded_proxies=%d loaded_proxy_components=%d."),
					bGridMatches ? 1 : 0,
					bLogicalIdentityMatches ? 1 : 0,
					Landscape->ComponentSizeQuads,
					ComponentQuads,
					Landscape->NumSubsections,
					Layout.SectionsPerComponent,
					Landscape->SubsectionSizeQuads,
					Layout.QuadsPerSection,
					ActualComponentCount,
					ExpectedComponentCount,
					LoadedProxyCount,
					LoadedProxyComponentCount);
				return false;
			}
			if (!UpdateGeneratedBase(Landscape, Bundle, Layout, Heightfield, OutResult, OutError))
			{
				return false;
			}
		}
		else
		{
			FVector4d Envelope = Bundle.Cells[0].Bounds;
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				Envelope.X = FMath::Min(Envelope.X, Cell.Bounds.X);
				Envelope.Y = FMath::Min(Envelope.Y, Cell.Bounds.Y);
				Envelope.Z = FMath::Max(Envelope.Z, Cell.Bounds.Z);
				Envelope.W = FMath::Max(Envelope.W, Cell.Bounds.W);
			}
			const FVector ActorLocation = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(Envelope.X, Envelope.W, Bundle.HeightOriginMeters));
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = FName(TEXT("ProjectWorld_Landscape"));
			SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
			SpawnParameters.OverrideActorGuid = StableGuid(LogicalLandscapeId.IsEmpty()
				? Bundle.GridId + TEXT("|landscape")
				: Bundle.GridId + TEXT("|landscape|") + LogicalLandscapeId);
			Landscape = World->SpawnActor<ALandscape>(
				ALandscape::StaticClass(),
				ActorLocation,
				FRotator::ZeroRotator,
				SpawnParameters);
			if (Landscape == nullptr)
			{
				OutError = TEXT("Cannot create the generated Landscape actor.");
				return false;
			}
			bCreatedLandscape = true;

			Landscape->SetActorScale3D(FVector(
				Bundle.SampleSpacingMeters.X * 100.0,
				Bundle.SampleSpacingMeters.Y * 100.0,
				100.0));
			Landscape->Tags.Add(GeneratedTag);
			Landscape->Tags.Add(LandscapeTag);
			// TerrainRowOrderTag is deliberately NOT set here. UpdateGeneratedBase treats a
			// missing tag as "write every cell" and adds the tag itself once the canonical
			// heights have landed in Generated Base, so creation cannot silently skip cells.
			Landscape->SetActorLabel(TEXT("ProjectWorld Landscape"));
			Landscape->SetIsSpatiallyLoaded(false);
			TMap<FGuid, TArray<uint16>> HeightData;
			HeightData.Add(FGuid(), Heightfield.EncodedHeights);
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerData;
			MaterialLayerData.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
			Landscape->Import(
				StableGuid(Bundle.GridId + TEXT("|landscape-data")),
				0,
				0,
				Heightfield.VertexCount.X - 1,
				Heightfield.VertexCount.Y - 1,
				Layout.SectionsPerComponent,
				Layout.QuadsPerSection,
				HeightData,
				TEXT(""),
				MaterialLayerData,
				ELandscapeImportAlphamapType::Additive,
				TArrayView<const FLandscapeLayer>());
			if (!AddRequiredLayers(Landscape, OutResult, OutError))
			{
				return false;
			}
			// Import() only bootstraps Landscape topology and the raw heightmap, which UE
			// discards as soon as the edit-layer stack composites. Imported raw heights are
			// never successful generated terrain on their own, so creation goes through the
			// same Generated Base write path as incremental update.
			if (!UpdateGeneratedBase(Landscape, Bundle, Layout, Heightfield, OutResult, OutError))
			{
				return false;
			}
			OutResult.UpdatedLandscapeComponentCount =
				Layout.ComponentCount.X * Layout.ComponentCount.Y;
			++OutResult.CreatedActorCount;
		}

		UMaterialInterface* DesiredLandscapeMaterial = LandscapeMaterial != nullptr
			? LandscapeMaterial
			: LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
		const bool bMaterialChanged = Landscape->LandscapeMaterial != DesiredLandscapeMaterial;
		bool bLandscapeChanged = bCreatedLandscape || bMaterialChanged || bLogicalContractChanged;
		bLandscapeChanged |= SetIdentityTag(Landscape, TEXT("ProjectWorld.Grid="), Bundle.GridId);
		if (!LogicalLandscapeId.IsEmpty())
		{
			bLandscapeChanged |= SetIdentityTag(Landscape, LogicalLandscapeTagPrefix, LogicalLandscapeId);
		}
		bLandscapeChanged |= RemoveIdentityTags(Landscape, TEXT("ProjectWorld.Input="));
		bLandscapeChanged |= RemoveIdentityTags(Landscape, TEXT("ProjectWorld.TerrainCell="));
		Landscape->LandscapeMaterial = DesiredLandscapeMaterial;
		if (bCreatedLandscape || bMaterialChanged)
		{
			Landscape->UpdateAllComponentMaterialInstances(true);
		}
		bool bTopologyChanged = false;
		if (!EnsureCellProxyTopology(
			World,
			Landscape,
			Bundle,
			Layout,
			ComponentsPerProxy,
			bTopologyChanged,
			OutError))
		{
			return false;
		}
		if (bLandscapeChanged || bTopologyChanged)
		{
			Landscape->MarkPackageDirty();
			if (!bCreatedLandscape)
			{
				++OutResult.UpdatedActorCount;
			}
		}
		OutResult.LandscapeComponentCount = Layout.ComponentCount.X * Layout.ComponentCount.Y;
		return true;
	}

	bool ClearGeneratedLayers(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		ALandscape* Landscape = nullptr;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (IsGeneratedLandscape(*It))
			{
				Landscape = *It;
				break;
			}
		}
		if (Landscape == nullptr)
		{
			return true;
		}
		if (!HasIdentityTag(Landscape, TEXT("ProjectWorld.Grid="), Bundle.GridId))
		{
			OutError = TEXT("Generated Landscape grid identity differs from the deletion receipt.");
			return false;
		}

		ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(GeneratedBaseLayerName);
		ULandscapeEditLayerBase* RoadsLayer = Landscape->GetEditLayer(GeneratedRoadsLayerName);
		ULandscapeEditLayerBase* AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (BaseLayer == nullptr || RoadsLayer == nullptr || AuthoredLayer == nullptr)
		{
			OutError = TEXT("Generated Landscape is missing a required protected layer boundary.");
			return false;
		}

		const FGuid AuthoredGuid = AuthoredLayer->GetGuid();
		Landscape->ClearEditLayer(BaseLayer->GetGuid());
		Landscape->ClearEditLayer(RoadsLayer->GetGuid());
		AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (AuthoredLayer == nullptr || AuthoredLayer->GetGuid() != AuthoredGuid)
		{
			OutError = TEXT("Authored Corrections identity changed while clearing generated layers.");
			return false;
		}

		RemoveIdentityTags(Landscape, TEXT("ProjectWorld.Input="));
		RemoveIdentityTags(Landscape, TEXT("ProjectWorld.TerrainCell="));
		Landscape->RequestLayersContentUpdateForceAll();
		Landscape->MarkPackageDirty();
		OutResult.bAuthoredCorrectionLayerPreserved = true;
		OutResult.AuthoredCorrectionLayerGuid =
			AuthoredGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		OutResult.LandscapeComponentCount = LandscapeInfo != nullptr
			? CountLandscapeComponents(LandscapeInfo)
			: 0;
		OutResult.UpdatedLandscapeComponentCount = OutResult.LandscapeComponentCount;
		return true;
	}
}
