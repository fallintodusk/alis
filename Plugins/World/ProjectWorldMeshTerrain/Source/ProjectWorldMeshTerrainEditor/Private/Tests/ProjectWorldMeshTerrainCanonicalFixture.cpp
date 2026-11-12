#include "ProjectWorldMeshTerrainCanonicalFixture.h"
#include "ProjectWorldMeshTerrainRegionDomain.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "MaterialShared.h"
#include "Dom/JsonObject.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainCanonicalFixture
{
	bool Read(FAutomationTestBase& Test,const FJsonObject& Document,FProjectWorldCanonicalBundle& Bundle,bool Region)
	{
		const TSharedPtr<FJsonObject>* Result=nullptr;FString Path,Expected,Actual,Cell;
		TSet<FString> SelectedCells;FString SelectionError;
		if (!ProjectWorldRegionDomain::Selection(Document,Region,SelectedCells,Cell,SelectionError))
		{Test.AddError(SelectionError);return false;}
		const TArray<TSharedPtr<FJsonValue>>* Witnesses=nullptr;
		if (!Test.TestTrue(TEXT("The canonical consumer declares one authenticated result and selected original cell."),
			Document.TryGetObjectField(TEXT("canonical_result"),Result) && Result &&
			(*Result)->TryGetStringField(TEXT("path"),Path) && (*Result)->TryGetStringField(TEXT("sha256"),Expected) &&
			Document.TryGetStringField(TEXT("cell_id"),Cell) && !Cell.IsEmpty() &&
			Document.TryGetArrayField(TEXT("required_physics_points"),Witnesses) && !Witnesses->IsEmpty() &&
			Document.HasTypedField<EJson::Object>(TEXT("player_eye")))) return false;
		if (FPaths::IsRelative(Path)) Path=FPaths::ProjectDir()/Path;
		Path=FPaths::ConvertRelativePathToFull(Path);
		if (!Test.TestTrue(TEXT("The complete canonical result is authenticated and scratch-local."),
			FPaths::IsUnderDirectory(Path,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
			Expected.Len()==64 && FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) && Expected==Actual)) return false;
		FProjectWorldCanonicalValidation Validation;
		if (!Test.TestTrue(TEXT("The actual canonical reader admits every retained input and final original cell before selection."),
			FProjectWorldCanonicalLoader::Load(Path,Bundle,Validation)))
		{Test.AddError(Validation.ErrorCode+TEXT(": ")+Validation.Message+TEXT(" ")+Validation.Detail);return false;}
		const auto* Selected=Bundle.Cells.FindByPredicate([&](const auto& Entry){return Entry.CellId==Cell;});
		for (const auto& Id : SelectedCells)
			if (!Test.TestTrue(TEXT("Every selected original cell exists in the fully authenticated bundle."),
				Bundle.Cells.ContainsByPredicate([&](const auto& Entry){return Entry.CellId==Id;}))) return false;
		const TArray<TSharedPtr<FJsonValue>>* Queries=nullptr;
		if (!Test.TestTrue(TEXT("Canonical query witnesses are declared against the complete loaded original cell."),
			Selected && Document.TryGetArrayField(TEXT("terrain_queries"),Queries) && !Queries->IsEmpty() && Queries->Num()<=64)) return false;
		TSet<FString> WitnessedCells;
		bool DistinguishesRepresentations=false;
		for (const auto& Value : *Queries)
		{
			const TSharedPtr<FJsonObject>* Query=nullptr;const TArray<TSharedPtr<FJsonValue>>* XY=nullptr;
			double X=0,Y=0,ExpectedBase=0,ExpectedFinal=0,Base=0,Final=0;FString Error;
			if (Value && Value->TryGetObject(Query) && Query)
			{
				FString QueryCell=Cell;
				if (!ProjectWorldRegionDomain::QueryCell(**Query,Cell,QueryCell))
				{Test.AddError(TEXT("A present query cell_id must be an explicit nonempty string."));return false;}
				Selected=SelectedCells.Contains(QueryCell) ? Bundle.Cells.FindByPredicate([&](const auto& Entry){return Entry.CellId==QueryCell;}) : nullptr;
				if (!Test.TestNotNull(TEXT("Query witnesses stay in their explicitly selected original cell."),Selected)) return false;
				WitnessedCells.Add(QueryCell);
			}
			if (!Test.TestTrue(TEXT("Every terrain query witness declares finite owned XY and expected base/final heights."),
				Value && Value->TryGetObject(Query) && Query &&
				(*Query)->TryGetArrayField(TEXT("xy"),XY) && XY->Num()==2 &&
				(*XY)[0]->Type==EJson::Number && (*XY)[0]->TryGetNumber(X) && FMath::IsFinite(X) &&
				(*XY)[1]->Type==EJson::Number && (*XY)[1]->TryGetNumber(Y) && FMath::IsFinite(Y) &&
				X>=Selected->Bounds.X && X<=Selected->Bounds.Z && Y>=Selected->Bounds.Y && Y<=Selected->Bounds.W &&
				(*Query)->TryGetNumberField(TEXT("base_m"),ExpectedBase) && FMath::IsFinite(ExpectedBase) &&
				(*Query)->TryGetNumberField(TEXT("final_m"),ExpectedFinal) && FMath::IsFinite(ExpectedFinal))) return false;
			if (!Test.TestTrue(TEXT("Both existing public terrain-query modes sample the fully admitted cell."),
				ProjectWorldGeneratedGeometry::SampleTerrain(*Selected,X,Y,Base,Error,EProjectWorldTerrainQuery::ImmutableGrid) &&
				ProjectWorldGeneratedGeometry::SampleTerrain(*Selected,X,Y,Final,Error,EProjectWorldTerrainQuery::FinalSurface)))
			{Test.AddError(Error);return false;}
			Test.AddInfo(FString::Printf(TEXT("CanonicalTerrainQuery x=%.17g y=%.17g immutable_m=%.17g final_m=%.17g delta_m=%.17g"),X,Y,Base,Final,Final-Base));
			if (!Test.TestTrue(TEXT("Native immutable and final query modes match their independent retained witnesses."),
				FMath::IsFinite(Base) && FMath::IsFinite(Final) && FMath::Abs(Base-ExpectedBase)<=1e-8 && FMath::Abs(Final-ExpectedFinal)<=1e-8)) return false;
			DistinguishesRepresentations|=FMath::Abs(Final-Base)>1e-8;
		}
		if (!Region && !Test.TestTrue(TEXT("At least one retained witness distinguishes immutable bilinear and final piecewise-affine sampling."),DistinguishesRepresentations)) return false;
		if (!Test.TestEqual(TEXT("Every selected complete cell has its own authenticated query witnesses."),WitnessedCells.Num(),SelectedCells.Num())) return false;
		if (Region && !Test.TestTrue(TEXT("Regional observation explicitly declares its reduced Water preparation scope."),
			Document.HasTypedField<EJson::Boolean>(TEXT("regional_observation")) && Document.GetBoolField(TEXT("regional_observation")) &&
			Document.HasTypedField<EJson::String>(TEXT("water_preparation_scope")) &&
			Document.GetStringField(TEXT("water_preparation_scope"))==(SelectedCells.Num()==1 ? TEXT("selected_complete_cell") : TEXT("selected_complete_cells")))) return false;
		Bundle.Cells.RemoveAll([&](const auto& Entry){return !SelectedCells.Contains(Entry.CellId);});
		return Test.TestTrue(TEXT("Consumer selection retains the complete original cell and actual authenticated final geometry."),
			Bundle.Cells.Num()==SelectedCells.Num() && Bundle.Cells[0].Terrain.ArtifactHash.Len()==64 &&
			(Region || (!Bundle.Cells[0].Terrain.RefinedQuads.IsEmpty() && Bundle.Cells[0].Terrain.BaseArtifactHash.Len()==64)));
	}
	bool Capture(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
		const FJsonObject& Document,const FString& Root,FJsonObject& Receipt,bool Region)
	{
		const TSharedPtr<FJsonObject>* Eye=nullptr;FVector Points[2];
		if (!Document.TryGetObjectField(TEXT("player_eye"),Eye) || !Eye) return false;
		for (int32 I=0;I<2;++I)
		{
			const TArray<TSharedPtr<FJsonValue>>* Row=nullptr;
			if (!(*Eye)->TryGetArrayField(I==0 ? TEXT("position") : TEXT("target"),Row) || Row->Num()!=3) return false;
			for (int32 J=0;J<3;++J) if (!(*Row)[J] || (*Row)[J]->Type!=EJson::Number ||
				!(*Row)[J]->TryGetNumber(Points[I][J]) || !FMath::IsFinite(Points[I][J])) return false;
			Points[I]=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,Points[I]);
		}
		if (!Test.TestTrue(TEXT("The representative eye and target define a finite forward perspective."),
			FVector::Distance(Points[0],Points[1])>50)) return false;
		FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;
		auto* Actor=World.SpawnActor<ASceneCapture2D>(Points[0],(Points[1]-Points[0]).Rotation(),Spawn);
		auto* Light=World.SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-45,-30,0),Spawn);
		if (!Test.TestTrue(TEXT("The saved scene owns its temporary representative camera and ordinary lighting."),Actor && Light)) return false;
		ON_SCOPE_EXIT {World.DestroyActor(Actor);World.DestroyActor(Light);};
		Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);Light->GetLightComponent()->SetIntensity(Region ? 200 : 10);
		Receipt.SetNumberField(TEXT("diagnostic_directional_light_lux"),Light->GetLightComponent()->Intensity);
		auto* C=Actor->GetCaptureComponent2D();auto* Target=NewObject<UTextureRenderTarget2D>(Actor);
		Target->RenderTargetFormat=RTF_RGBA32f;Target->InitAutoFormat(640,360);Target->UpdateResourceImmediate(true);
		C->TextureTarget=Target;C->bCaptureEveryFrame=false;C->bCaptureOnMovement=false;C->FOVAngle=70;
		C->bAlwaysPersistRenderingState=true;
		C->PostProcessSettings.bOverride_AutoExposureMethod=true;C->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		C->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;C->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
		C->PostProcessSettings.bOverride_AutoExposureBias=true;C->PostProcessSettings.AutoExposureBias=0;
		C->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		int32 Components=0;
		const FName CaptureVariant(TEXT("nanite"));TSet<FName> CapturedVariants;
		struct FProjectHiddenTerrainVisual {TWeakObjectPtr<UPrimitiveComponent> Component;bool Visible,Shadow;};
		TArray<FProjectHiddenTerrainVisual> HiddenVisuals;
		ON_SCOPE_EXIT
		{
			for (const auto& Saved:HiddenVisuals) if (auto* Component=Saved.Component.Get())
			{
				Component->SetVisibility(Saved.Visible);Component->SetCastShadow(Saved.Shadow);
				Test.TestTrue(TEXT("Regional capture restores every suppressed visual's visibility and shadow state."),
					Component->IsVisible()==Saved.Visible && Component->CastShadow==Saved.Shadow);
			}
		};
		TArray<TSharedPtr<FJsonValue>> MaterialBindings;
		for (TActorIterator<UE::MeshPartition::ACompiledSection> It(&World);It;++It)
		{
			if (Region && !Test.TestTrue(TEXT("Regional appearance requires actual native section channel textures and table."),
				It->GetChannelTexture()!=nullptr && !It->GetChannelTable().IsEmpty())) return false;
			for (const auto& Component : It->GetMeshComponents())
			{
				if (Region && It->GetBuildInfo().BuildVariantName!=CaptureVariant)
				{
					HiddenVisuals.Add({Component.Get(),Component->IsVisible(),bool(Component->CastShadow)});
					Component->SetVisibility(false);Component->SetCastShadow(false);continue;
				}
				CapturedVariants.Add(It->GetBuildInfo().BuildVariantName);
				auto* Material=Component->GetMaterial(0);
				auto* Expected=It->GetBuildInfo().GetMegaMeshDefinition()->GetMaterial();
				const auto* Instance=Cast<UMaterialInstance>(Material);
				if (Material) Material->EnsureIsComplete();
				const auto* Resource=Material ? Material->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(World.GetFeatureLevel())) : nullptr;
				Test.AddInfo(FString::Printf(TEXT("CanonicalCaptureMaterial actual=%s expected=%s complete=%d"),
					*GetPathNameSafe(Material),*GetPathNameSafe(Expected),Resource && Resource->IsGameThreadShaderMapComplete()));
				if (!Test.TestTrue(TEXT("The actual rendered Terrain component retains a complete accepted material."),
					Material && (Material==Expected || (Instance && Instance->IsChildOf(Expected))) &&
					Resource && Resource->IsGameThreadShaderMapComplete())) return false;
				MaterialBindings.Add(MakeShared<FJsonValueString>(Material->GetPathName()));
				C->ShowOnlyComponent(Component.Get());++Components;
			}
		}
		Receipt.SetArrayField(TEXT("terrain_capture_materials"),MaterialBindings);
		if (Region)
		{
			if (!Test.TestTrue(TEXT("A regional view must contain exactly the selected visual variant rather than coincident geometry."),
				CapturedVariants.Num()==1 && CapturedVariants.Contains(CaptureVariant))) return false;
			Receipt.SetStringField(TEXT("terrain_capture_variant"),CaptureVariant.ToString());
			Receipt.SetNumberField(TEXT("terrain_capture_components"),Components);
			Receipt.SetNumberField(TEXT("terrain_suppressed_visual_components"),HiddenVisuals.Num());
		}
		if (!Test.TestTrue(TEXT("The player-eye view contains actual saved compiled Terrain components."),Components>0)) return false;
		World.UpdateWorldComponents(false,false);World.SendAllEndOfFrameUpdates();FlushRenderingCommands();
		C->CaptureSource=SCS_SceneDepth;
		TArray<FLinearColor> Positive,Hidden;
		for (int32 I=0;I<3;++I) {C->CaptureScene();FlushRenderingCommands();}
		if (!Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Positive) || Positive.Num()!=640*360) return false;
		C->ShowOnlyComponents.Empty();C->CaptureScene();FlushRenderingCommands();
		if (!Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Hidden) || Hidden.Num()!=Positive.Num()) return false;
		const int32 Center=180*640+320;const double Depth=Positive[Center].R,Absent=Hidden[Center].R;
		if (!Test.TestTrue(TEXT("The representative eye center renders Terrain and refuses the empty-geometry control."),
			FMath::IsFinite(Depth) && Depth>0 && Depth<100000 && Absent>Depth+1)) return false;
		for (TActorIterator<UE::MeshPartition::ACompiledSection> It(&World);It;++It)
			if (!Region || It->GetBuildInfo().BuildVariantName==CaptureVariant)
				for (const auto& Component : It->GetMeshComponents()) C->ShowOnlyComponent(Component.Get());
		if (Region) for (TActorIterator<AStaticMeshActor> It(&World);It;++It)
			if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1")) ||
				(Document.HasField(TEXT("constructed_feature_id")) && It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1"))))
			{
				auto* Material=It->GetStaticMeshComponent()->GetMaterial(0);
				if (Material) Material->EnsureIsComplete();
				const auto* Resource=Material ? Material->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(World.GetFeatureLevel())) : nullptr;
				if (!Test.TestTrue(TEXT("Regional separate-object appearance requires its actual material shader resource."),
					Resource && Resource->IsGameThreadShaderMapComplete())) return false;
				C->ShowOnlyComponent(It->GetStaticMeshComponent());
			}
		TArray<TSharedPtr<FJsonValue>> Exposures;
		struct FDiagnosticShot {int32 Bias;ESceneCaptureSource Source;const TCHAR* Name;};
		for (const auto& Shot: {FDiagnosticShot{0,SCS_FinalColorLDR,TEXT("player-eye.png")},
			FDiagnosticShot{-4,SCS_FinalColorLDR,TEXT("player-eye-ev-4.png")},
			FDiagnosticShot{-8,SCS_FinalColorLDR,TEXT("player-eye-ev-8.png")},
			FDiagnosticShot{0,SCS_BaseColor,TEXT("surface-base-color.png")},FDiagnosticShot{0,SCS_Normal,TEXT("surface-normal.png")}})
		{
			if (!Region && (Shot.Bias!=0 || Shot.Source!=SCS_FinalColorLDR)) continue;
			C->CaptureSource=Shot.Source;C->PostProcessSettings.AutoExposureBias=Shot.Bias;
			for (int32 I=0;I<3;++I) {C->CaptureScene();FlushRenderingCommands();}
			TArray<FColor> Pixels;TArray64<uint8> Png;
			if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) || Pixels.Num()!=640*360) return false;
			FImageUtils::PNGCompressImageArray(640,360,Pixels,Png);
			const FString Name=Shot.Name;
			const FString Path=Root/Name;FString Hash;
			if (!Test.TestTrue(TEXT("Actual saved-field diagnostic exposures are retained and authenticated."),
				!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png,*Path) && FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Hash))) return false;
			auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("compensation_ev"),Shot.Bias);Row->SetNumberField(TEXT("capture_source"),Shot.Source);
			Row->SetStringField(TEXT("file"),Name);Row->SetStringField(TEXT("sha256"),Hash);Exposures.Add(MakeShared<FJsonValueObject>(Row));
			if (Shot.Source==SCS_FinalColorLDR && Shot.Bias==0) Receipt.SetStringField(TEXT("player_eye_png_sha256"),Hash);
		}
		Receipt.SetArrayField(TEXT("diagnostic_exposures"),Exposures);
		Receipt.SetNumberField(TEXT("player_eye_center_depth_cm"),Depth);
		Receipt.SetStringField(TEXT("diagnostic_exposure"),TEXT("Manual, no physical-camera exposure; controlled geometry/material exposure brackets, not World lighting acceptance."));
		Receipt.SetNumberField(TEXT("empty_geometry_center_depth_cm"),Absent);
		Receipt.SetStringField(TEXT("player_eye_scope"),Region ?
			TEXT("Saved regional Terrain and ordinary Water; selected-cell preparation domain, no full-world equivalence, Hero, streaming or geographic acceptance.") :
			TEXT("Perspective image of actual saved compiled Terrain and accepted material; no Hero, navigation, Water presentation or visual-quality verdict."));
		return true;
	}
}
#endif
