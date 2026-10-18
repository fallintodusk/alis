// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductMaterialEvidence.h"

#include "ProjectWorldTerrainRuntimeRole.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "TextureResource.h"

namespace
{
	FString Role(const AActor& Actor)
	{
		if (ProjectWorldTerrainRuntimeRole::HasRole(Actor)) return TEXT("terrain");
		if (Actor.Tags.Contains(TEXT("ProjectWorld.Road.v1"))) return TEXT("roads");
		if (Actor.Tags.Contains(TEXT("ProjectWorld.BuildingMassing.v2"))) return TEXT("buildings");
		return FString();
	}
}

bool ProjectWorldProductMaterialEvidence::ReadBindings(UWorld& World, FJsonObject& Receipt, FString& OutError)
{
	TArray<TSharedPtr<FJsonValue>> Bindings;
	TSet<FString> Keys, Roles;
	TMap<FString, const UTexture*> SharedResources;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		const FString ConsumerRole = Role(**It);
		if (ConsumerRole.IsEmpty()) continue;
		TInlineComponentArray<UStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (!Component || !Component->IsRegistered() || !Component->IsVisible() ||
				!Component->bRenderInMainPass) continue;
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Component->GetMaterial(Slot);
				if (!Material) continue;
				TArray<FMaterialParameterInfo> Parameters;
				TArray<FGuid> ParameterIds;
				Material->GetAllTextureParameterInfo(Parameters, ParameterIds);
				for (const FMaterialParameterInfo& Parameter : Parameters)
				{
					UTexture* Texture = nullptr;
					if (!Material->GetTextureParameterValue(Parameter, Texture) || !Texture) continue;
					const UTextureRenderTarget2D* Target = Cast<UTextureRenderTarget2D>(Texture);
					if (!Target) continue;
					const FString SharedKey = Texture->GetPathName();
					const UTexture* const* Previous = SharedResources.Find(SharedKey);
					if (!Target->GetResource() || !Target->GetResource()->TextureRHI.IsValid() || Target->SizeX <= 0 || Target->SizeY <= 0 ||
						(Previous && *Previous != Texture))
					{
						OutError = TEXT("A generated material output has no live resource or duplicate unshared runtime resources.");
						return false;
					}
					SharedResources.Add(SharedKey, Texture);
					Roles.Add(ConsumerRole);
					const FString Key = ConsumerRole + TEXT("|") + SharedKey + TEXT("|") + Parameter.Name.ToString();
					if (Keys.Contains(Key)) continue;
					Keys.Add(Key);
					TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
					Row->SetStringField(TEXT("role"), ConsumerRole);
					Row->SetStringField(TEXT("material"), Material->GetPathName());
					Row->SetStringField(TEXT("parameter"), Parameter.Name.ToString());
					Row->SetStringField(TEXT("texture"), Texture->GetPathName());
					TArray<TSharedPtr<FJsonValue>> Chain;
					for (UMaterialInterface* Parent = Material; Parent;)
					{
						Chain.Add(MakeShared<FJsonValueString>(Parent->GetPathName()));
						const UMaterialInstance* Instance = Cast<UMaterialInstance>(Parent);
						Parent = Instance ? Instance->Parent.Get() : nullptr;
					}
					Row->SetArrayField(TEXT("material_chain"), Chain);
					Row->SetBoolField(TEXT("resource_ready"), true);
					Row->SetNumberField(TEXT("width"), Target->SizeX);
					Row->SetNumberField(TEXT("height"), Target->SizeY);
					Bindings.Add(MakeShared<FJsonValueObject>(Row));
				}
			}
		}
	}
	if (!Roles.Contains(TEXT("terrain")) || !Roles.Contains(TEXT("roads")) || !Roles.Contains(TEXT("buildings")))
	{
		OutError = TEXT("Material binding evidence needs generated live texture consumers for terrain, roads and buildings.");
		return false;
	}
	Receipt.SetArrayField(TEXT("material_texture_bindings"), Bindings);
	Receipt.SetStringField(TEXT("material_texture_binding_scope"), TEXT("engine_parameter_readback_not_rendered_consumption"));
	return true;
}
