// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductBuildingEvidence.h"

#include "Utilities/ProjectSha256.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr double BuildingReferenceToleranceCm = 0.05;

	bool VectorField(const FJsonObject& Row, const TCHAR* Name, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Row.TryGetArrayField(Name, Values) || Values->Num() != 3) return false;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			double Value = 0.0;
			if (!(*Values)[Index]->TryGetNumber(Value) || !FMath::IsFinite(Value)) return false;
			Out[Index] = Value;
		}
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
	{
		return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z)};
	}
}

bool FProjectWorldProductBuildingEvidence::Load(const FString& Path, const FString& Hash,
	const FString& Map, const FString& RuntimeHash, FString& OutError)
{
	FString Payload, ActualHash;
	if (FPaths::IsRelative(Path) || Hash.Len() != 64 || !FProjectSha256::HashFile(Path, ActualHash) ||
		ActualHash != Hash || !FFileHelper::LoadFileToString(Payload, *Path) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Reference) || !Reference.IsValid())
	{
		OutError = TEXT("Building surface reference path/hash/JSON is unauthenticated.");
		return false;
	}
	FString Schema, Status, ReferenceMap, ReferenceRuntime;
	const TArray<TSharedPtr<FJsonValue>>* Samples = nullptr;
	if (!Reference->TryGetStringField(TEXT("schema"), Schema) ||
		Schema != TEXT("project-world-building-surface-reference:v1") ||
		!Reference->TryGetStringField(TEXT("status"), Status) || Status != TEXT("accepted") ||
		!Reference->TryGetStringField(TEXT("map_package"), ReferenceMap) || ReferenceMap != Map ||
		!Reference->TryGetStringField(TEXT("runtime_profile_sha256"), ReferenceRuntime) || ReferenceRuntime != RuntimeHash ||
		!Reference->TryGetArrayField(TEXT("samples"), Samples) || Samples->Num() != 6)
	{
		OutError = TEXT("Building surface reference identity or six-ray coverage differs.");
		return false;
	}
	TSet<FString> Identities;
	TSet<FString> Meshes;
	int32 Walls = 0, Roofs = 0;
	const FString MeshRoot = FPackageName::GetLongPackagePath(Map) + TEXT("/Buildings/");
	for (const TSharedPtr<FJsonValue>& Value : *Samples)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		FString Kind, Mesh, Guid, TransformText;
		FGuid ParsedGuid;
		FTransform Transform;
		FVector LocalTarget, LocalNormal, Target, Normal, Hit;
		double Triangle = 0.0, SourceError = 0.0;
		if (!Value->TryGetObject(Row) || !(*Row)->TryGetStringField(TEXT("kind"), Kind) ||
			(Kind != TEXT("wall") && Kind != TEXT("roof")) ||
			!(*Row)->TryGetStringField(TEXT("mesh"), Mesh) || !Mesh.StartsWith(MeshRoot) ||
			!(*Row)->TryGetStringField(TEXT("body_setup_guid"), Guid) || !FGuid::Parse(Guid, ParsedGuid) || !ParsedGuid.IsValid() ||
			!(*Row)->TryGetStringField(TEXT("component_transform"), TransformText) || !Transform.InitFromString(TransformText) ||
			Transform.ContainsNaN() || !(*Row)->TryGetNumberField(TEXT("source_triangle"), Triangle) ||
			!FMath::IsFinite(Triangle) || Triangle < 0.0 || FMath::FloorToDouble(Triangle) != Triangle ||
			!VectorField(**Row, TEXT("source_local_target_cm"), LocalTarget) ||
			!VectorField(**Row, TEXT("source_local_normal"), LocalNormal) ||
			!VectorField(**Row, TEXT("source_target_cm"), Target) || !VectorField(**Row, TEXT("normal"), Normal) ||
			!VectorField(**Row, TEXT("reference_hit_cm"), Hit) ||
			!(*Row)->TryGetNumberField(TEXT("source_error_cm"), SourceError) || !FMath::IsFinite(SourceError) || SourceError < 0.0 ||
			!FMath::IsNearlyEqual(LocalNormal.SizeSquared(), 1.0, 0.0001) ||
			!FMath::IsNearlyEqual(Normal.SizeSquared(), 1.0, 0.0001) ||
			FVector::Distance(Transform.TransformPosition(LocalTarget), Target) > 0.01 ||
			FVector::Distance(Transform.TransformVectorNoScale(LocalNormal).GetSafeNormal(), Normal) > 0.0001 ||
			!FMath::IsNearlyEqual(FVector::Distance(Hit, Target), SourceError, 0.001) ||
			Identities.Contains(Mesh + TEXT("|") + Kind))
		{
			OutError = TEXT("A building surface sample is malformed, duplicated, or inconsistent with its source transform.");
			return false;
		}
		Identities.Add(Mesh + TEXT("|") + Kind);
		Meshes.Add(Mesh);
		Walls += Kind == TEXT("wall"); Roofs += Kind == TEXT("roof");
	}
	if (Walls != 3 || Roofs != 3 || Meshes.Num() != 3)
	{
		OutError = TEXT("Building evidence must cover one wall and roof on each of three source meshes.");
		return false;
	}
	ReferenceHash = Hash;
	return true;
}

bool FProjectWorldProductBuildingEvidence::Probe(UWorld& World, ACharacter& Character, FString& OutError)
{
	Evidence.Reset();
	UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	if (Character.GetWorld() != &World || !Character.GetController() || Character.GetController()->GetPawn() != &Character ||
		!Capsule || !Capsule->IsRegistered() || !Capsule->IsQueryCollisionEnabled())
	{
		OutError = TEXT("Building capsule evidence requires the possessed player's registered collision component.");
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> Hits;
	double MaximumReferenceError = 0.0, MaximumSourceError = 0.0;
	const double Radius = Capsule->GetScaledCapsuleRadius(), HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius <= 0 || HalfHeight < Radius)
	{
		OutError = TEXT("Building capsule geometry dimensions are invalid.");
		return false;
	}
	auto SaveEvidence = [&](const TCHAR* Status)
	{
		Evidence = MakeShared<FJsonObject>();
		Evidence->SetStringField(TEXT("schema"), TEXT("project-world-building-surface-evidence:v2"));
		Evidence->SetStringField(TEXT("status"), Status);
		Evidence->SetStringField(TEXT("reference_sha256"), ReferenceHash);
		Evidence->SetNumberField(TEXT("reference_tolerance_cm"), BuildingReferenceToleranceCm);
		Evidence->SetNumberField(TEXT("maximum_reference_error_cm"), MaximumReferenceError);
		Evidence->SetNumberField(TEXT("maximum_source_error_cm"), MaximumSourceError);
		Evidence->SetStringField(TEXT("capsule_query"), TEXT("UWorld::ComponentSweepMulti"));
		Evidence->SetStringField(TEXT("capsule_pawn_class"), Character.GetClass()->GetPathName());
		Evidence->SetNumberField(TEXT("capsule_radius_cm"), Radius);
		Evidence->SetNumberField(TEXT("capsule_half_height_cm"), HalfHeight);
		Evidence->SetNumberField(TEXT("capsule_query_channel"), Capsule->GetCollisionObjectType());
		Evidence->SetArrayField(TEXT("samples"), Hits);
	};
	for (const TSharedPtr<FJsonValue>& Value : Reference->GetArrayField(TEXT("samples")))
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		UStaticMeshComponent* Found = nullptr;
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			if (It->Tags.Contains(TEXT("ProjectWorld.BuildingMassing.v2")) && Component && Component->GetStaticMesh() &&
				Component->GetStaticMesh()->GetPathName() == Row->GetStringField(TEXT("mesh")))
			{
				if (Found) { OutError = TEXT("A named building mesh has duplicate runtime consumers."); return false; }
				Found = Component;
			}
		}
		FTransform ExpectedTransform;
		ExpectedTransform.InitFromString(Row->GetStringField(TEXT("component_transform")));
		if (!Found || !Found->IsRegistered() || !Found->IsCollisionEnabled() || !Found->IsPhysicsStateCreated() ||
			!Found->GetBodyInstance() || !Found->GetBodyInstance()->IsValidBodyInstance() ||
			Found->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block || !Found->GetStaticMesh()->GetBodySetup() ||
			Found->GetStaticMesh()->GetBodySetup()->BodySetupGuid.ToString() != Row->GetStringField(TEXT("body_setup_guid")) ||
			!Found->GetComponentTransform().Equals(ExpectedTransform, 0.001))
		{
			OutError = TEXT("Waiting for the named source building mesh/body/transform to be loaded with blocking collision.");
			return false;
		}
		FVector Target, Normal, ReferenceHit;
		VectorField(*Row, TEXT("source_target_cm"), Target); VectorField(*Row, TEXT("normal"), Normal);
		VectorField(*Row, TEXT("reference_hit_cm"), ReferenceHit);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldBuildingCookedReference), true);
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->Values = Row->Values;
		const bool bRayHit = Found->LineTraceComponent(Hit, Target + Normal * 100.0, Target - Normal * 100.0, Params);
		Result->SetBoolField(TEXT("ray_query_returned_hit"), bRayHit);
		Result->SetStringField(TEXT("ray_query_status"), bRayHit ? TEXT("invalid_hit") : TEXT("miss"));
		if (bRayHit && !Hit.ImpactPoint.ContainsNaN())
		{
			Result->SetArrayField(TEXT("cooked_hit_cm"), VectorJson(Hit.ImpactPoint));
			Result->SetNumberField(TEXT("reference_error_cm"), FVector::Distance(Hit.ImpactPoint, ReferenceHit));
			Result->SetNumberField(TEXT("cooked_source_error_cm"), FVector::Distance(Hit.ImpactPoint, Target));
		}
		if (bRayHit && !Hit.ImpactNormal.ContainsNaN()) Result->SetArrayField(TEXT("cooked_hit_normal"), VectorJson(Hit.ImpactNormal));
		if (!bRayHit ||
			Hit.GetComponent() != Found || Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() ||
			!FMath::IsNearlyEqual(Hit.ImpactNormal.SizeSquared(), 1.0, 0.0001) ||
			FVector::Distance(Hit.ImpactPoint, ReferenceHit) > BuildingReferenceToleranceCm)
		{
			if (bRayHit && Hit.GetComponent() == Found && !Hit.ImpactPoint.ContainsNaN() &&
				FVector::Distance(Hit.ImpactPoint, ReferenceHit) > BuildingReferenceToleranceCm)
				Result->SetStringField(TEXT("ray_query_status"), TEXT("reference_mismatch"));
			Hits.Add(MakeShared<FJsonValueObject>(Result));
			SaveEvidence(TEXT("rejected"));
			OutError = FString::Printf(TEXT("Named building %s ray differs from current source physics reference: %s."),
				*Row->GetStringField(TEXT("kind")), *Row->GetStringField(TEXT("mesh")));
			return false;
		}
		const double ReferenceError = FVector::Distance(Hit.ImpactPoint, ReferenceHit);
		MaximumReferenceError = FMath::Max(MaximumReferenceError, ReferenceError);
		MaximumSourceError = FMath::Max(MaximumSourceError, FVector::Distance(Hit.ImpactPoint, Target));
		Result->SetStringField(TEXT("ray_query_status"), TEXT("hit_named_component"));
		const double Support = Radius + (HalfHeight - Radius) * FMath::Abs(FVector::DotProduct(Normal, Capsule->GetUpVector()));
		const FVector Start = ReferenceHit + Normal * (Support + 100.0), End = ReferenceHit - Normal * 100.0;
		TArray<FHitResult> CapsuleHits;
		FComponentQueryParams CapsuleParams(SCENE_QUERY_STAT(ProjectWorldBuildingCapsule), &Character);
		World.ComponentSweepMulti(CapsuleHits, Capsule, Start, End, Capsule->GetComponentQuat(), CapsuleParams);
		const FHitResult* Contact = nullptr;
		int32 OtherBlockers = 0, Penetrations = 0;
		for (const FHitResult& Candidate : CapsuleHits)
		{
			Penetrations += Candidate.bStartPenetrating;
			if (!Candidate.bBlockingHit) continue;
			if (Candidate.GetComponent() == Found) Contact = &Candidate;
			else ++OtherBlockers;
		}
		const bool bAcceptedContact = Contact && !Contact->bStartPenetrating && OtherBlockers == 0 && Penetrations == 0 &&
			!Contact->ImpactPoint.ContainsNaN() && !Contact->ImpactNormal.ContainsNaN() && !Contact->ImpactNormal.IsNearlyZero();
		Result->SetStringField(TEXT("capsule_query_status"), bAcceptedContact ? TEXT("blocked_named_component") : TEXT("unresolved_contact"));
		Result->SetNumberField(TEXT("capsule_hit_count"), CapsuleHits.Num());
		Result->SetNumberField(TEXT("capsule_other_blocking_hit_count"), OtherBlockers);
		Result->SetNumberField(TEXT("capsule_initial_penetration_count"), Penetrations);
		Result->SetArrayField(TEXT("capsule_trace_start_cm"), VectorJson(Start));
		Result->SetArrayField(TEXT("capsule_trace_end_cm"), VectorJson(End));
		if (Contact)
		{
			Result->SetStringField(TEXT("capsule_hit_mesh"), Found->GetStaticMesh()->GetPathName());
			Result->SetBoolField(TEXT("capsule_blocking_hit"), Contact->bBlockingHit);
			Result->SetBoolField(TEXT("capsule_start_penetrating"), Contact->bStartPenetrating);
			Result->SetNumberField(TEXT("capsule_hit_time"), Contact->Time);
			Result->SetArrayField(TEXT("capsule_hit_cm"), VectorJson(Contact->ImpactPoint));
			Result->SetArrayField(TEXT("capsule_hit_normal"), VectorJson(Contact->ImpactNormal));
			Result->SetNumberField(TEXT("capsule_source_plane_error_cm"), FMath::Abs(FVector::DotProduct(Contact->ImpactPoint - Target, Normal)));
			Result->SetNumberField(TEXT("capsule_reference_plane_error_cm"), FMath::Abs(FVector::DotProduct(Contact->ImpactPoint - ReferenceHit, Normal)));
		}
		Hits.Add(MakeShared<FJsonValueObject>(Result));
		if (!bAcceptedContact)
		{
			SaveEvidence(TEXT("rejected"));
			OutError = FString::Printf(TEXT("Named building capsule contact unresolved: mesh=%s kind=%s hits=%d other_blockers=%d penetrations=%d."),
				*Row->GetStringField(TEXT("mesh")), *Row->GetStringField(TEXT("kind")), CapsuleHits.Num(), OtherBlockers, Penetrations);
			return false;
		}
	}
	SaveEvidence(TEXT("accepted"));
	return true;
}

void FProjectWorldProductBuildingEvidence::AppendReceipt(FJsonObject& Receipt) const
{
	if (Evidence) Receipt.SetObjectField(TEXT("building_surface_evidence"), Evidence.ToSharedRef());
}
