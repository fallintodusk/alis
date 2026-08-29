// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldBuildingInventory.h"

#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldBuildingInventory
{
	namespace
	{
		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		bool AddPackageArtifact(
			const FString& PackageName,
			const FString& Kind,
			const FString& SemanticHash,
			FProjectWorldLayerInventory& Inventory,
			FString& OutError)
		{
			FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			Filename = FPaths::ConvertRelativePathToFull(Filename);
			if (!FPaths::FileExists(Filename))
			{
				OutError = FString::Printf(TEXT("Building layer package was not saved: %s"), *PackageName);
				return false;
			}
			FString Relative = Filename;
			const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			if (!FPaths::MakePathRelativeTo(Relative, *ProjectRoot) || Relative.StartsWith(TEXT("..")))
			{
				OutError = FString::Printf(TEXT("Building layer package escapes the project: %s"), *Filename);
				return false;
			}
			Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
			FString Digest;
			if (!FProjectSha256::HashFile(Filename, Digest))
			{
				OutError = FString::Printf(TEXT("Cannot hash building layer package: %s"), *Filename);
				return false;
			}
			Inventory.Artifacts.Add({Relative, Kind, Digest, SemanticHash});
			return true;
		}
	}

	bool Capture(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		Inventory.Artifacts.Reset();
		TSet<FString> ExpectedCells;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			FProjectWorldBuildingMeshBuildResult Build;
			if (!ProjectWorldBuildingRealization::BuildCellOutput(
				Bundle, Cell, Layer, AuthoredOverlaySet, Build, OutError))
			{
				return false;
			}
			OutResult.BuildingTriangleCount += Build.TriangleCount;
			OutResult.BuildingCandidateFragmentCount += Build.Stats.CandidateFragmentCount;
			OutResult.BuildingAcceptedFragmentCount += Build.Stats.AcceptedFragmentCount;
			OutResult.BuildingDuplicateFragmentCount += Build.Stats.DuplicateFragmentCount;
			OutResult.BuildingContainedFragmentCount += Build.Stats.ContainedFragmentCount;
			OutResult.BuildingConflictFragmentCount += Build.Stats.ConflictFragmentCount;
			OutResult.BuildingMalformedFragmentCount += Build.Stats.MalformedFragmentCount;
			OutResult.BuildingAuthoredMaskExcludedFragmentCount += Build.Stats.AuthoredMaskExcludedFragmentCount;
			if (Build.TriangleCount > 0)
			{
				ExpectedCells.Add(Cell.CellId);
			}
		}

		TSet<FString> ActualCells;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			FString CellId;
			FString MeshSemantic;
			if (!ProjectWorldBuildingRealization::ReadActorIdentity(*It, CellId, MeshSemantic))
			{
				continue;
			}
			UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
			FString OwnershipFailure;
			if (!ExpectedCells.Contains(CellId)) OwnershipFailure = TEXT("cell has no accepted building fragments");
			else if (ActualCells.Contains(CellId)) OwnershipFailure = TEXT("cell actor is duplicated");
			else if (Mesh == nullptr) OwnershipFailure = TEXT("StaticMesh is missing");
			else if (!Mesh->GetNaniteSettings().bEnabled) OwnershipFailure = TEXT("building mesh has Nanite disabled");
			else if (Mesh->GetBodySetup() == nullptr || Mesh->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple)
				OwnershipFailure = TEXT("building collision is not complex-as-simple");
			else if (!Mesh->GetBodySetup()->bDoubleSidedGeometry)
				OwnershipFailure = TEXT("building collision is not double-sided");
			else if (Component->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
				OwnershipFailure = TEXT("building collision is not query-and-physics");
			else if (Component->CanEverAffectNavigation()) OwnershipFailure = TEXT("building affects navigation");
			else if (!It->GetIsSpatiallyLoaded()) OwnershipFailure = TEXT("actor is not spatially loaded");
			else if (!It->IsPackageExternal()) OwnershipFailure = TEXT("actor is not OFPA/external");
			else if (It->bEnableAutoLODGeneration) OwnershipFailure = TEXT("actor permits HLOD generation");
			else if (It->GetHLODLayer() != nullptr) OwnershipFailure = TEXT("actor has an HLOD layer");
			if (!OwnershipFailure.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Persistent building ownership is invalid for cell %s: %s"),
					*CellId,
					*OwnershipFailure);
				return false;
			}
			ActualCells.Add(CellId);
			if (!AddPackageArtifact(Mesh->GetOutermost()->GetName(), TEXT("asset"), MeshSemantic, Inventory, OutError))
			{
				return false;
			}
			FString ActorSemantic;
			if (!HashText(TEXT("project_building_actor_v1|") + CellId + TEXT("|") + MeshSemantic, ActorSemantic) ||
				!AddPackageArtifact(It->GetPackage()->GetName(), TEXT("external_actor"), ActorSemantic, Inventory, OutError))
			{
				return false;
			}
		}
		if (ActualCells.Num() != ExpectedCells.Num())
		{
			OutError = TEXT("Building cell inventory does not exactly cover accepted building fragments.");
			return false;
		}
		OutResult.BuildingCellActorCount = ActualCells.Num();
		OutResult.BuildingMeshAssetCount = ActualCells.Num();
		return true;
	}
}
