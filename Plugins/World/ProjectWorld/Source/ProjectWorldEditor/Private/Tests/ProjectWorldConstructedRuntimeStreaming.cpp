// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeStreaming.h"
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedRuntimeMesh.h"
#include "Tests/ProjectWorldConstructedRuntimePawn.h"
#include "ProjectWorldRuntimePartitionPolicy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "ExternalPackageHelper.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	static const FName ControlTag(TEXT("ProjectWorld.PawnScene.AlwaysLoadedControl"));
	static FName StreamingUnitTag(int32 Index) { return FName(*FString::Printf(TEXT("ProjectWorld.PawnScene.Unit.%d"),Index)); }
	FStreamingScene::~FStreamingScene()
	{
		ReleaseLoader();
		for (const auto& Pair : ExternalMounts) FPackageName::UnRegisterMountPoint(Pair.Key,Pair.Value);
	}
	bool FStreamingScene::SaveAndReload(UWorld*& World,const FInput& Input,const FSavedCandidate& Saved,FAutomationTestBase& Test)
	{
		auto* Partition=World->GetWorldPartition();
		if (!Test.TestNotNull(TEXT("The streaming discriminator uses native World Partition."),Partition)) return false;
		Partition->SetEnableStreaming(true); Partition->SetDefaultHLODLayer(nullptr);
		FBox Bounds(ForceInit);
		for (const auto& Unit : Input.Units) for (const auto& Face : Unit.Triangles) for (const auto& Point : Face.Points) Bounds+=Point+Unit.Origin;
		FProjectWorldRuntimePartitionSettings Policy; Policy.PartitionCount=1; Policy.CellSizeCentimeters=12800;
		Policy.LoadingRangeCentimeters=FMath::Max(Policy.CellSizeCentimeters,FMath::CeilToInt(Bounds.GetSize().Length()+Policy.CellSizeCentimeters));
		Policy.bIs2D=true; Policy.bBlockOnSlowStreaming=false;
		bool Changed=false; FString Error;
		if (!ProjectWorldRuntimePartitionPolicy::Apply(World,Policy,Changed,Error)) { Test.AddError(Error); return false; }
		LoadingRange=Policy.LoadingRangeCentimeters;
		ControlOrigin=Input.Units[0].Origin+FVector(0,0,-10000);
		const auto& Face=Input.Units[0].Triangles[0]; ControlPoint=(Face.Points[0]+Face.Points[1]+Face.Points[2])/3+ControlOrigin;
		ControlMesh=Saved.PackageNames[0]+TEXT(".Surface");
		auto* Control=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),ControlOrigin,FRotator::ZeroRotator);
		if (!Test.TestNotNull(TEXT("The streaming discriminator owns an always-loaded collision control."),Control)) return false;
		Control->Tags.Add(ControlTag); Control->SetIsSpatiallyLoaded(false); Control->SetHLODLayer(nullptr);
		auto* Component=Control->GetStaticMeshComponent(); Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*ControlMesh));
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->SetCanEverAffectNavigation(false); Component->RecreatePhysicsState();
		TMap<FName,FGuid> Guids;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const bool Calibration=It->ActorHasTag(ControlTag);
			It->SetIsSpatiallyLoaded(!Calibration); It->SetHLODLayer(nullptr); It->SetPackageExternal(true);
			for (int32 I=0; I<Input.Units.Num(); ++I) if (It->ActorHasTag(StreamingUnitTag(I))) Guids.Add(StreamingUnitTag(I),It->GetActorGuid());
			if (Calibration) Guids.Add(ControlTag,It->GetActorGuid());
		}
		if (!Test.TestEqual(TEXT("Every streaming source actor has a distinct saved GUID."),Guids.Num(),Input.Units.Num()+1)) return false;
		const FString MapName=Saved.Mount+TEXT("StreamingScene");
		const FString Filename=FPackageName::LongPackageNameToFilename(MapName,FPackageName::GetMapPackageExtension());
		ExternalMounts.Add(ULevel::GetExternalActorsPath(MapName)+TEXT("/"),Saved.Directory+TEXT("__ExternalActors__/StreamingScene/"));
		ExternalMounts.Add(FExternalPackageHelper::GetExternalObjectsPath(MapName)+TEXT("/"),Saved.Directory+TEXT("__ExternalObjects__/StreamingScene/"));
		for (const auto& Pair : ExternalMounts)
		{
			IFileManager::Get().MakeDirectory(*Pair.Value,true); FPackageName::RegisterMountPoint(Pair.Key,Pair.Value);
			const auto Resolved=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Pair.Key+TEXT("ConfinementProbe")));
			if (!Test.TestTrue(TEXT("Native external actor/object roots resolve under scratch before any map save."),FPaths::IsUnderDirectory(Resolved,Saved.Directory))) return false;
		}
		if (!Test.TestTrue(TEXT("The map itself resolves under scratch before saving."),FPaths::IsUnderDirectory(Filename,Saved.Directory))) return false;
		auto CapturePackages=[&]()
		{
			Packages.AddUnique(World->GetPackage()->GetName());
			for (auto* Package : World->GetPackage()->GetExternalPackages()) Packages.AddUnique(Package->GetName());
		};
		CapturePackages();
		const bool SavedMap=FEditorFileUtils::SaveMap(World,Filename); CapturePackages();
		if (!Test.TestTrue(TEXT("The native map save includes only this disposable world's external actors."),SavedMap)) return false;
		Packages.Reset(); CapturePackages();
		for (const auto& Name : Packages)
		{
			const FString File=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Name,
				Name==MapName ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension()));
			bool OwnedName=Name.StartsWith(Saved.Mount);
			for (const auto& Pair : ExternalMounts) OwnedName|=Name.StartsWith(Pair.Key);
			if (!Test.TestTrue(TEXT("Every saved map/OFPA package exists inside the registered scratch mount."),OwnedName &&
				FPaths::IsUnderDirectory(File,Saved.Directory) && IFileManager::Get().FileExists(*File))) return false;
		}
		TWeakObjectPtr<UWorld> Before(World);
		World=GEditor->NewMap(); CollectGarbage(RF_NoFlags);
		if (!Test.TestFalse(TEXT("The map reload cannot reuse its original Editor world."),Before.IsValid())) return false;
		const bool LoadedMap=FEditorFileUtils::LoadMap(Filename,false,false);
		World=GEditor->GetEditorWorldContext().World(); EditorWorld=World;
		if (!Test.TestTrue(TEXT("The isolated partitioned map reopens through native map loading."),LoadedMap) ||
			!Test.TestNotNull(TEXT("Native map loading retains an owned Editor world."),World)) return false;
		Partition=World->GetWorldPartition();
		if (!Test.TestTrue(TEXT("The reopened map retains enabled World Partition."),Partition && Partition->IsStreamingEnabled())) return false;
		FProjectWorldRuntimePartitionSettings Actual;
		if (!ProjectWorldRuntimePartitionPolicy::Read(World,Actual,Error)) { Test.AddError(Error); return false; }
		if (!Test.TestTrue(TEXT("The saved map retains the exact synthetic streaming policy."),Actual.PartitionCount==1 && Actual.bIs2D &&
			Actual.CellSizeCentimeters==Policy.CellSizeCentimeters && Actual.LoadingRangeCentimeters==LoadingRange && !Actual.bBlockOnSlowStreaming)) return false;
		auto* Adapter=Partition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(World,Bounds.ExpandBy(20000),TEXT("Constructed streaming fixture reload"));
		Loader=Adapter; Adapter->GetLoaderAdapter()->Load(); World->UpdateWorldComponents(false,false); World->SendAllEndOfFrameUpdates();
		int32 Count=0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			for (const auto& Pair : Guids) if (It->ActorHasTag(Pair.Key))
			{
				++Count; auto* External=It->GetExternalPackage();
				if (!Test.TestTrue(TEXT("Reloaded actor GUID, OFPA owner and spatial policy are persisted."),It->GetActorGuid()==Pair.Value &&
					It->IsPackageExternal() && External && Packages.Contains(External->GetName()) && It->GetIsSpatiallyLoaded()!=(Pair.Key==ControlTag))) return false;
				int32 Index=0;
				while (Index<Input.Units.Num() && Pair.Key!=StreamingUnitTag(Index)) ++Index;
				const FVector Origin=Pair.Key==ControlTag ? ControlOrigin : Input.Units[Index].Origin;
				const FString MeshPath=Pair.Key==ControlTag ? ControlMesh : Saved.PackageNames[Index]+TEXT(".Surface");
				const auto* Reloaded=It->GetStaticMeshComponent();
				if (!Test.TestTrue(TEXT("The native map reload preserves exact actor/component transform and saved mesh identity."),
					Reloaded && Reloaded->GetStaticMesh() && Reloaded->GetStaticMesh()->GetPathName()==MeshPath && It->GetActorLocation()==Origin &&
					It->GetActorQuat()==FQuat::Identity && It->GetActorScale3D()==FVector::OneVector && Reloaded->GetComponentLocation()==Origin &&
					Reloaded->GetComponentQuat()==FQuat::Identity && Reloaded->GetComponentScale()==FVector::OneVector)) return false;
			}
		}
		if (!Test.TestEqual(TEXT("All saved fixture actors reload exactly once."),Count,Guids.Num())) return false;
		Far=Bounds.GetCenter()+FVector(4*(Bounds.GetSize().Length()+LoadingRange+Policy.CellSizeCentimeters),0,500);
		Test.AddInfo(FString::Printf(TEXT("ProjectStreamingSaved units=%d actors=%d packages=%d cell_cm=%d loading_range_cm=%d far=%s; native save/reopen; synthetic policy"),
			Input.Units.Num(),Count,Packages.Num(),Policy.CellSizeCentimeters,LoadingRange,*Far.ToString()));
		return true;
	}
	void FStreamingScene::ReleaseLoader()
	{
		if (EditorWorld.IsValid() && Loader.IsValid()) EditorWorld->GetWorldPartition()->ReleaseEditorLoaderAdapter(Loader.Get());
		Loader.Reset(); EditorWorld.Reset();
	}
	void FStreamingScene::UnloadPackages(FAutomationTestBase& Test)
	{
		TArray<UPackage*> Loaded;
		for (const auto& Name : Packages) if (auto* Package=FindPackage(nullptr,*Name)) Loaded.Add(Package);
		if (!Loaded.IsEmpty()) Test.TestTrue(TEXT("Disposable map/OFPA packages unload before their mesh mount is removed."),FSavedCandidate::Unload(Loaded));
	}
	class FStreamingReturn final : public IAutomationLatentCommand
	{
	public:
		FStreamingReturn(FAutomationTestBase& InTest,const FInput& InInput,const FPawnSelection& InSelection,FStreamingScene& InScene,
			TFunction<bool(UWorld*,ACharacter*)> InMeasure):Test(InTest),Input(InInput),Selection(InSelection),Scene(InScene),Measure(MoveTemp(InMeasure)) {}
		bool Update() override
		{
			if (Test.HasAnyErrors()) return true;
			UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;
			if (!Test.TestNotNull(TEXT("Streaming runs in the actual native PIE world."),World)) return true;
			auto* Partition=World->GetWorldPartition();
			if (!Test.TestTrue(TEXT("PIE owns enabled native streaming."),Partition && Partition->IsStreamingEnabled())) return true;
			if (!Pawn.IsValid())
			{
				ACharacter* Character=nullptr; APlayerController* Controller=nullptr;
				if (!SpawnPawn(World,Input.Start,Selection,Test,Character,Controller)) return true;
				Pawn=Character; Character->GetCharacterMovement()->StopMovementImmediately(); Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				PhaseStarted=FPlatformTime::Seconds(); return false;
			}
			if (LastFrame==GFrameCounter) return false; LastFrame=GFrameCounter;
			if (FPlatformTime::Seconds()-PhaseStarted>90)
			{ Test.AddError(FString::Printf(TEXT("Native streaming phase timed out phase=%d actors=%d completed=%d sources=%d pawn=%s"),
				Phase,Count(World),Partition->IsStreamingCompleted(nullptr),Partition->GetStreamingSources().Num(),*Pawn->GetActorLocation().ToString())); return true; }
			const auto& Sources=Partition->GetStreamingSources(); bool SourceNear=false;
			for (const auto& Source : Sources) SourceNear|=FVector2D(Source.Location-Pawn->GetActorLocation()).Length()<1000;
			if (!SourceNear || !Partition->IsStreamingCompleted(nullptr)) return false;
			const int32 Expected=Phase==1 ? 0 : Input.Units.Num();
			if (Count(World)!=Expected) return false;
			if (!Control(World)) return true;
			if (Phase==1)
			{
				int32 Probes=0,Hits=0;
				for (const auto& Unit : Input.Units) for (const auto& Face : Unit.Triangles)
				{
					++Probes; Hits+=Contact(World,(Face.Points[0]+Face.Points[1]+Face.Points[2])/3+Unit.Origin,nullptr);
				}
				if (!Test.TestEqual(TEXT("All formerly supported face probes lose pawn collision after native unload."),Hits,0)) return true;
				Test.AddInfo(FString::Printf(TEXT("ProjectStreamingUnloaded probes=%d hits=%d"),Probes,Hits));
			}
			else if (!Measure(World,Pawn.Get())) return true;
			Test.AddInfo(FString::Printf(TEXT("ProjectStreamingPhase phase=%d actors=%d native_complete=1 source_follows_pawn=1 control_collision=1 input=%s"),Phase,Expected,*Input.Hash));
			if (Phase==2) { Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling); return true; }
			++Phase; Pawn->SetActorLocation(Phase==1 ? Scene.Far : Input.Start+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
			Pawn->GetCharacterMovement()->StopMovementImmediately(); PhaseStarted=FPlatformTime::Seconds(); return false;
		}
	private:
		int32 Count(UWorld* World) const
		{
			int32 Result=0;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) for (int32 I=0; I<Input.Units.Num(); ++I) Result+=It->ActorHasTag(StreamingUnitTag(I));
			return Result;
		}
		bool Contact(UWorld* World,const FVector& Point,UStaticMeshComponent* Expected) const
		{
			const auto* Capsule=Pawn->GetCapsuleComponent(); FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldStreamingReturn),true); Query.AddIgnoredActor(Pawn.Get());
			return World->SweepSingleByChannel(Hit,Point+FVector(0,0,250),Point-FVector(0,0,100),FQuat::Identity,ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query) && Hit.bBlockingHit &&
				(!Expected || Hit.GetComponent()==Expected);
		}
		bool Control(UWorld* World)
		{
			TArray<AStaticMeshActor*> Found;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) if (It->ActorHasTag(ControlTag)) Found.Add(*It);
			if (!Test.TestEqual(TEXT("The nonspatial control remains present in every streaming phase."),Found.Num(),1)) return false;
			auto* Actor=Found[0]; auto* Component=Actor->GetStaticMeshComponent(); UStaticMesh* Mesh=Component->GetStaticMesh();
			return Test.TestTrue(TEXT("The always-loaded control retains its exact mesh and transform while spatial geometry streams."),
				!Actor->GetIsSpatiallyLoaded() && Mesh && Mesh->GetPathName()==Scene.ControlMesh && Actor->GetActorLocation()==Scene.ControlOrigin &&
				Actor->GetActorQuat()==FQuat::Identity && Actor->GetActorScale3D()==FVector::OneVector && Component->GetComponentLocation()==Scene.ControlOrigin &&
				Component->GetComponentQuat()==FQuat::Identity && Component->GetComponentScale()==FVector::OneVector && Contact(World,Scene.ControlPoint,Component));
		}
		FAutomationTestBase& Test;
		const FInput& Input;
		FPawnSelection Selection;
		FStreamingScene& Scene;
		TFunction<bool(UWorld*,ACharacter*)> Measure;
		TWeakObjectPtr<ACharacter> Pawn;
		int32 Phase=0;
		uint64 LastFrame=0;
		double PhaseStarted=0;
	};
	TSharedRef<IAutomationLatentCommand> StreamingReturn(FAutomationTestBase& Test,const FInput& Input,const FPawnSelection& Selection,FStreamingScene& Scene,
		TFunction<bool(UWorld*,ACharacter*)> Measure)
	{
		return MakeShared<FStreamingReturn>(Test,Input,Selection,Scene,MoveTemp(Measure));
	}
}
#endif
