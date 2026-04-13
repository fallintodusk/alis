// Inline helper functions for ClipMatrix tests.
// Geometry, mesh lookup, JSON loading, debug drawing.
// Separated from ClipMatrixTypes.h to keep the types header lean.

#pragma once

#include "ClipMatrixTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DrawDebugHelpers.h"

namespace ClipMatrixHelpers
{

// -------------------------------------------------------------------------
// Geometry helpers
// -------------------------------------------------------------------------

inline float ComputeUpperChainLeanDeg(const FVector& UpperChainCameraDelta)
{
	const FVector2D DeltaXZ(UpperChainCameraDelta.X, UpperChainCameraDelta.Z);
	const float DeltaXZLen = DeltaXZ.Size();
	if (DeltaXZLen <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float DownDot = FMath::Clamp(-UpperChainCameraDelta.Z / DeltaXZLen, -1.0f, 1.0f);
	return FMath::RadiansToDegrees(FMath::Acos(DownDot));
}

inline float ComputePointToSegmentDistance(
	const FVector& Point,
	const FVector& Start,
	const FVector& End,
	FVector* OutClosestPoint = nullptr)
{
	const FVector ClosestPoint = FMath::ClosestPointOnSegment(Point, Start, End);
	if (OutClosestPoint)
	{
		*OutClosestPoint = ClosestPoint;
	}
	return FVector::Dist(Point, ClosestPoint);
}

inline void EvaluateRaySegmentCapsule(
	const FVector& RayOrigin,
	const FVector& RayDir,
	const float RayLength,
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const float CapsuleRadius,
	bool& bOutHit,
	float& OutRayDist,
	float& OutPerpDist)
{
	bOutHit = false;
	OutRayDist = -1.f;
	OutPerpDist = -1.f;

	const FVector RayEnd = RayOrigin + RayDir * RayLength;
	FVector ClosestOnRay = FVector::ZeroVector;
	FVector ClosestOnSegment = FVector::ZeroVector;
	FMath::SegmentDistToSegmentSafe(
		RayOrigin, RayEnd,
		SegmentStart, SegmentEnd,
		ClosestOnRay, ClosestOnSegment);

	OutPerpDist = FVector::Dist(ClosestOnRay, ClosestOnSegment);
	if (OutPerpDist > CapsuleRadius)
	{
		return;
	}

	const float HitDist = FVector::DotProduct(ClosestOnRay - RayOrigin, RayDir);
	if (HitDist < 0.f)
	{
		return;
	}

	bOutHit = true;
	OutRayDist = HitDist;
}

inline void DrawDebugSegmentCapsule(
	UWorld* World,
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const float Radius,
	const FColor& Color,
	const float LifeTime)
{
	if (!World)
	{
		return;
	}

	const FVector Axis = SegmentEnd - SegmentStart;
	const float SegmentLength = Axis.Size();
	const FVector Center = (SegmentStart + SegmentEnd) * 0.5f;
	const FQuat Rotation = SegmentLength > KINDA_SMALL_NUMBER
		? FQuat::FindBetweenNormals(FVector::UpVector, Axis / SegmentLength)
		: FQuat::Identity;
	DrawDebugCapsule(
		World, Center,
		(SegmentLength * 0.5f) + Radius, Radius,
		Rotation, Color, false, LifeTime, 0, 1.2f);
}

// -------------------------------------------------------------------------
// Mesh helpers
// -------------------------------------------------------------------------

inline USkeletalMeshComponent* FindMeshByRole(AActor* Actor, const FString& Role)
{
	if (!Actor) return nullptr;
	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);
	const FString Tag = TEXT("AssemblyRole=") + Role;
	for (USkeletalMeshComponent* M : Meshes)
	{
		if (M->ComponentTags.Contains(FName(*Tag)))
		{
			return M;
		}
	}
	return nullptr;
}

inline USkeletalMeshComponent* FindOwnerVisibleMesh(AActor* Actor)
{
	if (!Actor) return nullptr;

	USkeletalMeshComponent* LocalBody = FindMeshByRole(Actor, TEXT("LocalBody"));
	if (LocalBody)
	{
		TArray<USceneComponent*> Children;
		LocalBody->GetChildrenComponents(false, Children);
		for (USceneComponent* Child : Children)
		{
			USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(Child);
			if (SKC && SKC->ComponentTags.Contains(
				FName(TEXT("AssemblyRole=LocalBodyCustomization"))) &&
				SKC->GetSkeletalMeshAsset())
			{
				return SKC;
			}
		}
		if (LocalBody->GetSkeletalMeshAsset())
		{
			return LocalBody;
		}
	}
	return nullptr;
}

inline USkeletalMeshComponent* FindMeshByName(AActor* Actor, const FString& MeshName)
{
	if (!Actor || MeshName.IsEmpty() || MeshName == TEXT("none") || MeshName == TEXT("unknown"))
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetName() == MeshName)
		{
			return Mesh;
		}
	}

	return nullptr;
}

inline FString GetCopyPoseSourceName(USkeletalMeshComponent* Mesh)
{
	if (!Mesh) return TEXT("none");
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	if (!Anim) return TEXT("no_anim");

	UFunction* Func = Anim->FindFunction(FName("GetCurrentSourceName"));
	if (Func)
	{
		struct { FName ReturnValue; } Params;
		Anim->ProcessEvent(Func, &Params);
		return Params.ReturnValue.ToString();
	}

	return TEXT("unknown");
}

inline TSharedPtr<FJsonObject> CollectMeshIdentity(AActor* Actor)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	if (!Actor) return Root;

	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);

	TArray<TSharedPtr<FJsonValue>> MeshArray;
	for (USkeletalMeshComponent* M : Meshes)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), M->GetName());
		Obj->SetStringField(TEXT("asset"),
			M->GetSkeletalMeshAsset() ? M->GetSkeletalMeshAsset()->GetName() : TEXT("null"));

		FString RoleStr;
		for (const FName& Tag : M->ComponentTags)
		{
			const FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("AssemblyRole=")))
			{
				RoleStr = TagStr.Mid(13);
				break;
			}
		}
		Obj->SetStringField(TEXT("role"), RoleStr);

		Obj->SetStringField(TEXT("animClass"),
			M->GetAnimInstance()
				? M->GetAnimInstance()->GetClass()->GetName()
				: TEXT("none"));
		Obj->SetStringField(TEXT("leaderPose"),
			M->LeaderPoseComponent.IsValid()
				? M->LeaderPoseComponent->GetName()
				: TEXT("none"));
		Obj->SetBoolField(TEXT("hidden"), M->bHiddenInGame);
		Obj->SetBoolField(TEXT("ownerNoSee"), M->bOwnerNoSee);
		Obj->SetBoolField(TEXT("onlyOwnerSee"), M->bOnlyOwnerSee);
		Obj->SetBoolField(TEXT("castHiddenShadow"), M->bCastHiddenShadow);
		Obj->SetBoolField(TEXT("visible"), M->IsVisible());
		Obj->SetStringField(TEXT("parent"),
			M->GetAttachParent() ? M->GetAttachParent()->GetName() : TEXT("root"));

		MeshArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("meshes"), MeshArray);

	FString ownerVisibleName = TEXT("unknown");
	FString ownerHiddenName = TEXT("unknown");
	for (USkeletalMeshComponent* M : Meshes)
	{
		if (M->bOnlyOwnerSee && M->IsVisible() && !M->bHiddenInGame)
		{
			ownerVisibleName = M->GetName();
		}
		if (M->bOwnerNoSee && !M->bHiddenInGame)
		{
			ownerHiddenName = M->GetName();
		}
	}
	Root->SetStringField(TEXT("ownerVisibleMesh"), ownerVisibleName);
	Root->SetStringField(TEXT("ownerHiddenMesh"), ownerHiddenName);

	return Root;
}

// -------------------------------------------------------------------------
// JSON loading helpers
// -------------------------------------------------------------------------

inline FVector LoadNeckOffsetFromHeroJson()
{
	FVector Result(-8.f, 0.f, -10.f);
	const FString Path = FPaths::ProjectPluginsDir() /
		TEXT("Resources/ProjectObject/Content/Human/Hero/Hero.json");
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *Path))
	{
		return Result;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* Sections = nullptr;
	if (!Root->TryGetObjectField(TEXT("sections"), Sections))
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* View = nullptr;
	if (!(*Sections)->TryGetObjectField(TEXT("view"), View))
	{
		return Result;
	}

	FString OffsetStr;
	if ((*View)->TryGetStringField(TEXT("neckOffset"), OffsetStr))
	{
		FVector Parsed;
		if (Parsed.InitFromString(OffsetStr))
		{
			Result = Parsed;
		}
	}

	return Result;
}

inline FVector LoadViewRelativeOffsetFromHeroJson()
{
	FVector Result(23.f, 0.f, 55.f);
	const FString Path = FPaths::ProjectPluginsDir() /
		TEXT("Resources/ProjectObject/Content/Human/Hero/Hero.json");
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *Path))
	{
		return Result;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* Sections = nullptr;
	if (!Root->TryGetObjectField(TEXT("sections"), Sections))
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* View = nullptr;
	if (!(*Sections)->TryGetObjectField(TEXT("view"), View))
	{
		return Result;
	}

	FString OffsetStr;
	if ((*View)->TryGetStringField(TEXT("relativeOffset"), OffsetStr))
	{
		FVector Parsed;
		if (Parsed.InitFromString(OffsetStr))
		{
			Result = Parsed;
		}
	}

	return Result;
}

// -------------------------------------------------------------------------
// Anim instance targeting
// -------------------------------------------------------------------------

// Find the anim instance that owns LocalBody correction (has UpperChainMode).
// Primary: LocalBody role mesh. Fallback: scan all skeletal meshes.
inline UAnimInstance* FindLocalBodyCorrectionAnimInstance(
	ACharacter* Character,
	FString* OutMeshName = nullptr)
{
	if (!Character) return nullptr;

	// Primary: LocalBody role mesh
	if (USkeletalMeshComponent* LB = FindMeshByRole(Character, TEXT("LocalBody")))
	{
		if (UAnimInstance* Anim = LB->GetAnimInstance())
		{
			if (Anim->GetClass()->FindPropertyByName(FName("UpperChainMode")))
			{
				if (OutMeshName) *OutMeshName = LB->GetName();
				return Anim;
			}
		}
	}

	// Fallback: scan all skeletal meshes
	TArray<USkeletalMeshComponent*> AllSKCs;
	Character->GetComponents<USkeletalMeshComponent>(AllSKCs);
	for (USkeletalMeshComponent* SKC : AllSKCs)
	{
		if (UAnimInstance* Anim = SKC->GetAnimInstance())
		{
			if (Anim->GetClass()->FindPropertyByName(FName("UpperChainMode")))
			{
				if (OutMeshName) *OutMeshName = SKC->GetName();
				return Anim;
			}
		}
	}

	return nullptr;
}

} // namespace ClipMatrixHelpers
