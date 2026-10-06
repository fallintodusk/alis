// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

// The gameplay layer owns one placement actor per object id and the host state the definition
// provider writes on it. Project-owned components and actor properties are observed as their
// editable values that differ from the class default.
namespace ProjectWorldGameplayVerifyProjection
{
	const FName PlacementTag(TEXT("ProjectWorld.GameplayPlacement.v1"));
	const FString ObjectTagPrefix(TEXT("ProjectWorld.GameplayObject="));
	const FString ProjectScriptPrefix(TEXT("/Script/Project"));

	bool ReadObjectId(const AActor& Actor, FString& OutObjectId)
	{
		if (!Actor.Tags.Contains(PlacementTag))
		{
			return false;
		}
		for (const FName& Tag : Actor.Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(ObjectTagPrefix, ESearchCase::CaseSensitive))
			{
				OutObjectId = Text.RightChop(ObjectTagPrefix.Len());
				return !OutObjectId.IsEmpty();
			}
		}
		return false;
	}

	void AddChangedProperties(
		FProjectWorldProjectionRecord& Record,
		const FString& Prefix,
		const UObject& Object,
		const AActor& Owner,
		TFunctionRef<bool(const FProperty&)> Include)
	{
		const UObject* Defaults = Object.GetClass()->GetDefaultObject();
		const FString OwnerPath = Owner.GetPathName();
		for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
		{
			const FProperty& Property = **It;
			if (!Property.HasAnyPropertyFlags(CPF_Edit) || Property.HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated) ||
				!Include(Property))
			{
				continue;
			}
			for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
			{
				if (Property.Identical_InContainer(&Object, Defaults, Index))
				{
					continue;
				}
				FString Value;
				Property.ExportText_InContainer(Index, Value, &Object, Defaults, const_cast<UObject*>(&Object), PPF_None);
				// Paths inside the placement actor do not depend on which map holds it.
				Value.ReplaceInline(*OwnerPath, TEXT("<actor>"), ESearchCase::CaseSensitive);
				Record.Fields.Add(Prefix + Property.GetName() + (Property.ArrayDim > 1 ? FString::Printf(TEXT("[%d]"), Index) : FString()), Value);
			}
		}
	}

	void AddComponent(FProjectWorldProjectionRecord& Record, const UActorComponent& Component, const AActor& Owner, const FString& ArtifactRoot)
	{
		const FString Prefix = TEXT("component.") + Component.GetName() + TEXT(".");
		const FString ClassPath = Component.GetClass()->GetPathName();
		Record.Fields.Add(Prefix + TEXT("class"), ClassPath);
		TArray<FString> ComponentTags;
		for (const FName& Tag : Component.ComponentTags)
		{
			ComponentTags.Add(Tag.ToString());
		}
		ComponentTags.Sort([](const FString& Left, const FString& Right) { return Left.Compare(Right, ESearchCase::CaseSensitive) < 0; });
		Record.Fields.Add(Prefix + TEXT("component_tags"), FString::Join(ComponentTags, TEXT(",")));
		if (const USceneComponent* Scene = Cast<USceneComponent>(&Component))
		{
			Record.Fields.Add(Prefix + TEXT("relative_transform"), ProjectWorldProjection::Transform(Scene->GetRelativeTransform()));
			Record.Fields.Add(Prefix + TEXT("mobility"),
				ProjectWorldProjection::EnumName(StaticEnum<EComponentMobility::Type>(), Scene->Mobility));
			Record.Fields.Add(Prefix + TEXT("attach_parent"), Scene->GetAttachParent() != nullptr
				? Scene->GetAttachParent()->GetName() : FString(TEXT("none")));
		}
		if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(&Component))
		{
			Record.Fields.Add(Prefix + TEXT("collision_profile"), Primitive->GetCollisionProfileName().ToString());
			Record.Fields.Add(Prefix + TEXT("collision_enabled"),
				ProjectWorldProjection::EnumName(StaticEnum<ECollisionEnabled::Type>(), Primitive->GetCollisionEnabled()));
			TArray<FString> Materials;
			for (int32 Index = 0; Index < Primitive->GetNumMaterials(); ++Index)
			{
				Materials.Add(ProjectWorldProjection::ObjectPath(Primitive->GetMaterial(Index), ArtifactRoot));
			}
			Record.Fields.Add(Prefix + TEXT("materials"), FString::Join(Materials, TEXT(",")));
		}
		if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(&Component))
		{
			Record.Fields.Add(Prefix + TEXT("mesh"), ProjectWorldProjection::ObjectPath(Mesh->GetStaticMesh(), ArtifactRoot));
		}
		if (ClassPath.StartsWith(ProjectScriptPrefix, ESearchCase::CaseSensitive))
		{
			AddChangedProperties(Record, Prefix + TEXT("property."), Component, Owner, [](const FProperty&) { return true; });
		}
	}

	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		for (TActorIterator<AActor> It(Scope.World); It; ++It)
		{
			FString ObjectId;
			if (!ReadObjectId(**It, ObjectId))
			{
				continue;
			}
			const AProjectWorldActor* Host = Cast<AProjectWorldActor>(*It);
			if (Host == nullptr)
			{
				OutError = FString::Printf(TEXT("Gameplay placement actor is not a ProjectWorld actor: %s"), *ObjectId);
				return false;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("actor"), TEXT("gameplay:") + ObjectId);
			ProjectWorldProjection::AddActorCommon(Record, *Host);
			Record.Fields.Add(TEXT("data_id"), Host->DataId.ToString(EGuidFormats::DigitsWithHyphensLower));
			Record.Fields.Add(TEXT("definition_id"), Host->ObjectDefinitionId.ToString());
			Record.Fields.Add(TEXT("definition_asset"), Host->DefinitionAssetPath.ToString());
			Record.Fields.Add(TEXT("applied_structure_hash"), Host->AppliedStructureHash);
			Record.Fields.Add(TEXT("applied_content_hash"), Host->AppliedContentHash);
			AddChangedProperties(Record, TEXT("property."), *Host, *Host, [](const FProperty& Property)
			{
				static const TSet<FName> Explicit = {
					GET_MEMBER_NAME_CHECKED(AProjectWorldActor, DataId),
					GET_MEMBER_NAME_CHECKED(AProjectWorldActor, ObjectDefinitionId),
					GET_MEMBER_NAME_CHECKED(AProjectWorldActor, DefinitionAssetPath)};
				return Property.GetOwnerClass() != nullptr &&
					Property.GetOwnerClass()->GetPathName().StartsWith(ProjectScriptPrefix, ESearchCase::CaseSensitive) &&
					!Explicit.Contains(Property.GetFName());
			});
			TArray<UActorComponent*> Components;
			Host->GetComponents(Components);
			Components.Sort([](const UActorComponent& Left, const UActorComponent& Right)
			{
				return Left.GetName().Compare(Right.GetName(), ESearchCase::CaseSensitive) < 0;
			});
			TArray<FString> Names;
			for (const UActorComponent* Component : Components)
			{
				AddComponent(Record, *Component, *Host, Scope.ArtifactRoot);
				Names.Add(Component->GetName());
			}
			Record.Fields.Add(TEXT("components"), FString::Join(Names, TEXT(",")));
			Record.Fields.Add(TEXT("root"), Host->GetRootComponent() != nullptr
				? Host->GetRootComponent()->GetName() : FString(TEXT("none")));
		}
		return true;
	}

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_gameplay_placement"), 1, 1, &Project);
}
