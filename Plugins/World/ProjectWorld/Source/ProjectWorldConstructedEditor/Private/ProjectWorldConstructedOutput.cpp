// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedOutput.h"
#include "ProjectWorldConstructedPlan.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldVerify.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace ProjectWorldConstructedRealization
{
	namespace
	{
		const FName Owned(TEXT("ProjectWorld.ConstructedSurface.v1"));
		bool Fail(FString& Error,const FString& Reason) {Error=Reason; return false;}
		FString Tag(const AActor& Actor,const FString& Prefix)
		{
			FString Out;
			for (FName Name : Actor.Tags)
			{
				const FString Text=Name.ToString();
				if (!Text.StartsWith(Prefix)) continue;
				if (!Out.IsEmpty()) return FString();
				Out=Text.Mid(Prefix.Len());
			}
			return Out;
		}
		FString AssetName(const FString& Id) {return TEXT("SM_ProjectConstructed_")+Id;}
		FString Package(const FProjectWorldLayerApplyContext& Context,const FString& Id)
		{return FPaths::Combine(Context.Inputs.Layer.ArtifactRoot,TEXT("Cells"),AssetName(Id));}
		bool Save(UPackage& Package,UObject* Asset)
		{
			const FString Filename=FPackageName::LongPackageNameToFilename(Package.GetName(),FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
			FSavePackageArgs Args; Args.SaveFlags=SAVE_NoError;
			if (Asset) Args.TopLevelFlags=RF_Public|RF_Standalone;
			return UPackage::SavePackage(&Package,Asset,*Filename,Args);
		}
		bool Existing(const FProjectWorldLayerApplyContext& Context,TMap<FString,AStaticMeshActor*>& Out,FString& Error)
		{
			for (TActorIterator<AActor> It(&Context.World); It; ++It)
			{
				if (!It->Tags.Contains(Owned)) continue;
				auto* Actor=Cast<AStaticMeshActor>(*It);
				if (!Actor || !Actor->GetStaticMeshComponent()) return Fail(Error,TEXT("Structural owner tags require an intact StaticMesh actor."));
				const IProjectWorldLayerProducer* Owner=nullptr;
				if (!ProjectWorldLayerProducerRegistry::FindActorOwner(*Actor,Owner,Error)) return false;
				const FString Id=Tag(**It,TEXT("ProjectWorld.ConstructedUnit="));
				if (Id.Len()!=32 || Out.Contains(Id) || Tag(**It,TEXT("ProjectWorld.Grid="))!=Context.Inputs.Bundle.GridId)
					return Fail(Error,TEXT("Structural actor identity is missing, duplicated or belongs to another grid."));
				for (TCHAR C : Id) if (!FChar::IsHexDigit(C)) return Fail(Error,TEXT("Structural unit identity is malformed."));
				const FString Cell=Tag(**It,TEXT("ProjectWorld.ConstructedCell="));
				if (!Context.Inputs.Bundle.Cells.ContainsByPredicate([&](const auto& Value){return Value.CellId==Cell;}))
					return Fail(Error,TEXT("Structural actor has no current canonical cell."));
				Out.Add(Id,Actor);
			}
			return true;
		}
		bool Current(const FProjectWorldLayerApplyContext& Context,const FUnit& Unit,AStaticMeshActor* Actor,
			UMaterialInterface* Material,FString& Error)
		{
			if (!Actor || !Actor->GetStaticMeshComponent()) return Fail(Error,TEXT("Required structural unit actor is missing."));
			const auto* Component=Actor->GetStaticMeshComponent(); UStaticMesh* Mesh=Component->GetStaticMesh();
			const FString MeshFile=FPackageName::LongPackageNameToFilename(Package(Context,Unit.Id),FPackageName::GetAssetPackageExtension());
			if (!Mesh || Mesh->GetOutermost()->GetName()!=Package(Context,Unit.Id) ||
				!IFileManager::Get().FileExists(*MeshFile) ||
				Tag(*Actor,TEXT("ProjectWorld.ConstructedCell="))!=Unit.CellId ||
				Tag(*Actor,TEXT("ProjectWorld.ConstructedModel="))!=Unit.ModelHash ||
				Tag(*Actor,TEXT("ProjectWorld.ConstructedInput="))!=Unit.InputHash ||
				Tag(*Actor,TEXT("ProjectWorld.ConstructedGeometry="))!=Unit.GeometryHash)
				return Fail(Error,TEXT("Structural unit has stale model, input, cell or mesh ownership."));
			if (Mesh->GetOutermost()->IsDirty() || !Actor->GetExternalPackage() || Actor->GetExternalPackage()->IsDirty())
				return Fail(Error,TEXT("Structural live state is not authenticated by clean saved mesh and actor packages."));
			const FTransform Expected(FQuat::Identity,Unit.Origin,FVector::OneVector);
			if (!Actor->GetActorTransform().Equals(Expected,0) || !Component->GetComponentTransform().Equals(Expected,0) ||
				!Actor->GetIsSpatiallyLoaded() || Actor->bEnableAutoLODGeneration || Actor->GetHLODLayer()!=nullptr ||
				Component->Mobility!=EComponentMobility::Static || Component->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics ||
				Component->GetCollisionProfileName()!=UCollisionProfile::BlockAll_ProfileName || !Component->CanEverAffectNavigation())
				return Fail(Error,FString::Printf(TEXT("Structural actor state differs: actor_frame=%d component_frame=%d spatial=%d auto_lod=%d hlod_present=%d mobility=%d collision=%d profile=%s nav=%d expected_origin=%.17g,%.17g,%.17g actual_origin=%.17g,%.17g,%.17g."),
					Actor->GetActorTransform().Equals(Expected,0),Component->GetComponentTransform().Equals(Expected,0),Actor->GetIsSpatiallyLoaded(),Actor->bEnableAutoLODGeneration,
					Actor->GetHLODLayer()!=nullptr,static_cast<int32>(Component->Mobility),static_cast<int32>(Component->GetCollisionEnabled()),
					*Component->GetCollisionProfileName().ToString(),Component->CanEverAffectNavigation(),Unit.Origin.X,Unit.Origin.Y,Unit.Origin.Z,
					Actor->GetActorLocation().X,Actor->GetActorLocation().Y,Actor->GetActorLocation().Z));
			const auto& Nanite=Mesh->GetNaniteSettings(); const auto* Body=Mesh->GetBodySetup();
			if (!Nanite.bEnabled || !Mesh->HasValidNaniteData() || Nanite.FallbackTarget!=ENaniteFallbackTarget::PercentTriangles ||
				Nanite.FallbackPercentTriangles!=1 || Nanite.KeepPercentTriangles!=1 || Nanite.FallbackRelativeError!=0 || Nanite.TrimRelativeError!=0 ||
				!Body || Body->CollisionTraceFlag!=CTF_UseComplexAsSimple || !Body->bDoubleSidedGeometry || !Body->bCreatedPhysicsMeshes ||
				!Mesh->bHasNavigationData || !Mesh->GetNavCollision())
				return Fail(Error,TEXT("Structural mesh lost its complete fallback, cooked physical collision or navigation support."));
			if (!Material || Mesh->GetStaticMaterials().Num()!=1 || Mesh->GetStaticMaterials()[0].MaterialInterface!=Material || Component->GetMaterial(0)!=Material)
				return Fail(Error,TEXT("Structural mesh differs from its resolved presentation binding."));
			auto* Description=Mesh->GetMeshDescription(0); FString Actual;
			if (!Description || !Describe(*Description,Actual) || Actual!=Unit.GeometryHash || !Qualify(*Unit.Surface,*Description,Error))
				return Fail(Error,TEXT("Actual saved structural geometry or source-role provenance differs from the canonical unit."));
			return true;
		}
		UMaterialInterface* Material(const FProjectWorldLayerApplyContext& Context)
		{auto* Found=Context.PresentationMaterials.Find(TEXT("road")); return Found ? *Found : nullptr;}
		bool Dirty(const FProjectWorldLayerApplyContext& Context,const FString& Cell)
		{return Context.FinalDirtyUnits.Contains(TEXT("*")) || Context.FinalDirtyUnits.Contains(Cell);}
		bool Remove(const FProjectWorldLayerApplyContext& Context,const FString& Id,AStaticMeshActor& Actor,FString& Error)
		{
			auto* External=Actor.GetExternalPackage(); FString ActorFile;
			if (External) ActorFile=FPackageName::LongPackageNameToFilename(External->GetName(),FPackageName::GetAssetPackageExtension());
			if (!Context.World.EditorDestroyActor(&Actor,true)) return Fail(Error,TEXT("Cannot retire structural actor."));
			if (External) ResetLoaders(External);
			// Reloaded mesh bulk data can retain a read handle until its owning linker is detached.
			if (auto* OwnedPackage=FindPackage(nullptr,*Package(Context,Id))) ResetLoaders(OwnedPackage);
			const FString MeshFile=FPackageName::LongPackageNameToFilename(Package(Context,Id),FPackageName::GetAssetPackageExtension());
			for (const FString& File : {ActorFile,MeshFile}) if (!File.IsEmpty() && IFileManager::Get().FileExists(*File) && !IFileManager::Get().Delete(*File,false,true))
				return Fail(Error,TEXT("Cannot retire structural output package: ")+File);
			return true;
		}
	}
	bool ApplyOutput(const FProjectWorldLayerApplyContext& Context,FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& Result,FString& Error)
	{
		FPlan Expected; TMap<FString,AStaticMeshActor*> Actors; auto* Resolved=Material(Context);
		if (!Resolved) return Fail(Error,TEXT("Structural presentation material is unavailable."));
		if (!Plan(Context.Inputs.Bundle,Context.Inputs.Layer,Expected,Error) || !Existing(Context,Actors,Error)) return false;
		FString Root=Context.Inputs.Layer.ArtifactRoot; Root.RemoveFromEnd(TEXT("/"));
		if (!FPackageName::IsValidLongPackageName(Root)) return Fail(Error,TEXT("Structural artifact root is not a valid package domain."));
		for (const auto& Unit : Expected.Units)
		{
			FString Filename;
			if (!FPackageName::TryConvertLongPackageNameToFilename(Package(Context,Unit.Id),Filename,FPackageName::GetAssetPackageExtension()))
				return Fail(Error,TEXT("Structural artifact root has no registered package mapping."));
			if (!Dirty(Context,Unit.CellId) && !Current(Context,Unit,Actors.FindRef(Unit.Id),Resolved,Error)) return false;
			const FString Name=TEXT("ProjectWorld_Constructed_")+Unit.Id;
			if (auto* Collision=FindObject<AActor>(Context.World.PersistentLevel,*Name); Collision && Collision!=Actors.FindRef(Unit.Id))
				return Fail(Error,TEXT("An unowned actor occupies a structural unit identity."));
		}
		for (const auto& Pair : Actors) if (!Expected.Units.ContainsByPredicate([&](const auto& U){return U.Id==Pair.Key;}) &&
			!Dirty(Context,Tag(*Pair.Value,TEXT("ProjectWorld.ConstructedCell="))))
			return Fail(Error,TEXT("A removed structural unit was not declared dirty."));
		for (const auto& Pair : Actors) if (!Expected.Units.ContainsByPredicate([&](const auto& U){return U.Id==Pair.Key;}))
		{
			if (!Remove(Context,Pair.Key,*Pair.Value,Error)) return false;
			++Result.RemovedActorCount; ++Result.SelfSavedActorMutationCount;
		}
		int32 Written=0,Reused=0;
		for (auto& Unit : Expected.Units)
		{
			if (!Dirty(Context,Unit.CellId)) continue;
			auto* Actor=Actors.FindRef(Unit.Id); UStaticMesh* Mesh=Actor ? Actor->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			if (Actor && Current(Context,Unit,Actor,Resolved,Error)) {++Reused; continue;} Error.Reset();
			FString Geometry;
			const FString MeshFile=FPackageName::LongPackageNameToFilename(Package(Context,Unit.Id),FPackageName::GetAssetPackageExtension());
			const bool Reuse=Mesh && Mesh->GetOutermost()->GetName()==Package(Context,Unit.Id) &&
				!Mesh->GetOutermost()->IsDirty() && IFileManager::Get().FileExists(*MeshFile) && Mesh->GetMeshDescription(0) &&
				Describe(*Mesh->GetMeshDescription(0),Geometry) && Geometry==Unit.GeometryHash && Mesh->HasValidNaniteData() &&
				Mesh->GetNaniteSettings().bEnabled && Mesh->GetNaniteSettings().FallbackTarget==ENaniteFallbackTarget::PercentTriangles &&
				Mesh->GetNaniteSettings().FallbackPercentTriangles==1 && Mesh->GetNaniteSettings().KeepPercentTriangles==1 &&
				Mesh->GetNaniteSettings().FallbackRelativeError==0 && Mesh->GetNaniteSettings().TrimRelativeError==0 &&
				Mesh->GetBodySetup() && Mesh->GetBodySetup()->bCreatedPhysicsMeshes && Mesh->GetBodySetup()->bDoubleSidedGeometry &&
				Mesh->GetBodySetup()->CollisionTraceFlag==CTF_UseComplexAsSimple && Mesh->bHasNavigationData && Mesh->GetNavCollision();
			if (!Reuse)
			{
				auto* AssetPackage=CreatePackage(*Package(Context,Unit.Id));
				Mesh=FindObject<UStaticMesh>(AssetPackage,*AssetName(Unit.Id));
				const bool New=Mesh==nullptr;
				if (!Mesh) Mesh=NewObject<UStaticMesh>(AssetPackage,*AssetName(Unit.Id),RF_Public|RF_Standalone);
				Mesh->Modify(); Mesh->GetStaticMaterials()={FStaticMaterial(Resolved,TEXT("ConstructedSurface"))}; Mesh->bAllowCPUAccess=false;
				auto Nanite=Mesh->GetNaniteSettings(); Nanite.bEnabled=true; Nanite.FallbackTarget=ENaniteFallbackTarget::PercentTriangles;
				Nanite.FallbackPercentTriangles=Nanite.KeepPercentTriangles=1; Nanite.FallbackRelativeError=Nanite.TrimRelativeError=0;
				Mesh->SetNaniteSettings(Nanite); Mesh->SetNumSourceModels(1); Mesh->bHasNavigationData=true; Mesh->bGenerateMeshDistanceField=false;
				Mesh->GetSourceModel(0).BuildSettings.bRemoveDegenerates=false;
				UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bUseHashAsGuid=true;
				if (!Mesh->BuildFromMeshDescriptions({&Unit.Description},Params)) return Fail(Error,TEXT("Cannot compile structural mesh."));
				FAssetCompilingManager::Get().FinishAllCompilation(); Mesh->CreateBodySetup(); auto* Body=Mesh->GetBodySetup();
				Body->CollisionTraceFlag=CTF_UseComplexAsSimple; Body->bDoubleSidedGeometry=true; Body->InvalidatePhysicsData(); Body->CreatePhysicsMeshes(); Mesh->CreateNavCollision(true);
				if (New) FAssetRegistryModule::AssetCreated(Mesh);
				if (!Save(*AssetPackage,Mesh)) return Fail(Error,TEXT("Cannot save structural mesh."));
				++Written;
			}
			else
			{
				++Reused;
				if (Mesh->GetStaticMaterials().Num()!=1 || Mesh->GetStaticMaterials()[0].MaterialInterface!=Resolved)
				{ Mesh->GetStaticMaterials()={FStaticMaterial(Resolved,TEXT("ConstructedSurface"))}; if (!Save(*Mesh->GetOutermost(),Mesh)) return false; }
			}
			const bool Updating=Actor!=nullptr;
			if (!Actor)
			{
				FActorSpawnParameters Spawn; Spawn.Name=FName(*(TEXT("ProjectWorld_Constructed_")+Unit.Id));
				Spawn.NameMode=FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				Spawn.OverrideActorGuid=ProjectWorldGeneratedGeometry::StableGuid(Context.Inputs.Bundle.GridId+TEXT("|constructed|")+Unit.Id);
				Actor=Context.World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),Unit.Origin,FRotator::ZeroRotator,Spawn);
				if (!Actor) return Fail(Error,TEXT("Cannot create structural actor.")); Actor->SetPackageExternal(true);
			}
			Actor->Modify(); Actor->Tags={ProjectWorldGeneratedGeometry::GeneratedTag,Owned,
				FName(*(TEXT("ProjectWorld.Grid=")+Context.Inputs.Bundle.GridId)),FName(*(TEXT("ProjectWorld.ConstructedCell=")+Unit.CellId)),
				FName(*(TEXT("ProjectWorld.ConstructedUnit=")+Unit.Id)),FName(*(TEXT("ProjectWorld.ConstructedModel=")+Unit.ModelHash)),
				FName(*(TEXT("ProjectWorld.ConstructedInput=")+Unit.InputHash)),FName(*(TEXT("ProjectWorld.ConstructedGeometry=")+Unit.GeometryHash))};
			Actor->SetActorTransform(FTransform(FQuat::Identity,Unit.Origin,FVector::OneVector)); Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration=false; Actor->SetHLODLayer(nullptr);
			auto* Component=Actor->GetStaticMeshComponent();
			Component->SetStaticMesh(Mesh); Component->SetMobility(EComponentMobility::Static);
			Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Component->SetCanEverAffectNavigation(true); Actor->MarkPackageDirty();
			if (!Actor->GetExternalPackage() || !Save(*Actor->GetExternalPackage(),nullptr)) return Fail(Error,TEXT("Cannot save structural actor."));
			++Result.SelfSavedActorMutationCount; if (Updating) ++Result.UpdatedActorCount; else ++Result.CreatedActorCount;
		}
		Inventory.Metrics.Add(TEXT("meshes_written"),Written); Inventory.Metrics.Add(TEXT("meshes_reused"),Reused);
		return true;
	}
	bool CaptureOutput(const FProjectWorldLayerApplyContext& Context,FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& Result,FString& Error)
	{
		FPlan Expected; TMap<FString,AStaticMeshActor*> Actors;
		if (!Plan(Context.Inputs.Bundle,Context.Inputs.Layer,Expected,Error) || !Existing(Context,Actors,Error)) return false;
		if (Actors.Num()!=Expected.Units.Num()) return Fail(Error,TEXT("Structural actor census does not exactly cover canonical feature/cell units."));
		FProjectWorldLayerInventory Candidate=Inventory;
		Candidate.Artifacts.Reset();
		for (const auto& Unit : Expected.Units)
		{
			auto* Actor=Actors.FindRef(Unit.Id);
			if (!Current(Context,Unit,Actor,Material(Context),Error)) return false;
			if (!ProjectWorldLayerProducerRegistry::AddPackageArtifact(Package(Context,Unit.Id),TEXT("asset"),Unit.GeometryHash,Candidate,Error) ||
				!Actor->GetExternalPackage() || !ProjectWorldLayerProducerRegistry::AddPackageArtifact(Actor->GetExternalPackage()->GetName(),
					TEXT("external_actor"),ProjectWorldProjection::HashText(Unit.InputHash+TEXT("|")+Unit.ModelHash),Candidate,Error)) return false;
		}
		Candidate.Metrics.Add(TEXT("physical_units"),Expected.Units.Num()); Inventory=MoveTemp(Candidate);
		Inventory.Metrics.Add(TEXT("boundary_contacts"),Expected.BoundaryContacts);
		Result.MutableProducerMetric(TEXT("project_constructed_surface"),TEXT("physical_units"))=Expected.Units.Num(); return true;
	}
}
