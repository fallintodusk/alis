// Copyright ALIS. All Rights Reserved.

#include "ProjectInventory.h"
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"
#include "Components/ProjectInventoryComponent.h"
#include "Services/ObjectDefinitionCache.h"
#include "Data/ObjectCatalog.h"
#include "Engine/AssetManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "Interaction/InventoryInteractionHandler.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY(LogProjectInventory);

#define LOCTEXT_NAMESPACE "FProjectInventoryModule"

void FProjectInventoryModule::StartupModule()
{
	// Create interaction handler (subscribes to IInteractionService)
	InteractionHandler = MakeShared<FInventoryInteractionHandler>();

	// Try subscribing immediately; retry via ticker if IInteractionService is not yet registered.
	if (TrySubscribeInteraction(0.0f))
	{
		RetryHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FProjectInventoryModule::TrySubscribeInteraction),
			0.5f);
	}

	// REGISTRATION ONLY - no init happens here
	// GameMode will call this lambda later, in order, after pawn spawn
	// See: Plugins/Gameplay/ProjectFeature/README.md for design rationale
	FFeatureRegistry::RegisterFeature(TEXT("Inventory"), [](const FFeatureInitContext& Context)
	{
		UE_LOG(LogProjectInventory, Log, TEXT("Inventory feature initializing (Mode: %s)"),
			*Context.ModeName.ToString());

		// ObjectDefinitions loaded via UAssetManager::LoadPrimaryAssets() during warmup.
		// GetPrimaryAssetObject() only returns in-memory objects (nullptr if not loaded).

		APawn* Pawn = Context.Pawn;
		if (!Pawn)
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Inventory feature: No pawn in context"));
			return;
		}

		// Create cache with GameInstance outer for stable lifetime
		UGameInstance* GameInstance = Context.World ? Context.World->GetGameInstance() : nullptr;
		UObject* CacheOuter = GameInstance ? static_cast<UObject*>(GameInstance) : GetTransientPackage();
		UObjectDefinitionCache* Cache = NewObject<UObjectDefinitionCache>(CacheOuter);

		UProjectInventoryComponent* Inventory = NewObject<UProjectInventoryComponent>(Pawn);
		Inventory->SetObjectDefinitionCache(Cache);
		Pawn->AddInstanceComponent(Inventory);
		Inventory->RegisterComponent();

		UE_LOG(LogProjectInventory, Log, TEXT("Inventory feature: Attached UProjectInventoryComponent to %s"),
			*Pawn->GetName());

		// Parse ConfigJson for ObjectCatalog
		if (!Context.ConfigJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Context.ConfigJson);
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				FString CatalogIdStr;
				if (JsonObject->TryGetStringField(TEXT("ObjectCatalog"), CatalogIdStr))
				{
					FPrimaryAssetId CatalogId = FPrimaryAssetId::FromString(CatalogIdStr);
					if (CatalogId.IsValid())
					{
						UE_LOG(LogProjectInventory, Log, TEXT("Loading ObjectCatalog: %s"), *CatalogIdStr);

						// Load catalog async, then warmup items
						UAssetManager& AM = UAssetManager::Get();
						AM.LoadPrimaryAsset(CatalogId, TArray<FName>(),
							FStreamableDelegate::CreateLambda([Cache, CatalogId]()
							{
								UAssetManager& AMInner = UAssetManager::Get();
								UObjectCatalog* Catalog = AMInner.GetPrimaryAssetObject<UObjectCatalog>(CatalogId);
								if (Catalog && Catalog->Objects.Num() > 0)
								{
									UE_LOG(LogProjectInventory, Log, TEXT("ObjectCatalog loaded: %d objects"), Catalog->Objects.Num());
									Cache->Warmup(Catalog->Objects);
								}
								else
								{
									UE_LOG(LogProjectInventory, Warning, TEXT("ObjectCatalog empty or failed to load: %s"), *CatalogId.ToString());
								}
							}));
					}
					else
					{
						UE_LOG(LogProjectInventory, Warning, TEXT("Invalid ObjectCatalog asset id: %s"), *CatalogIdStr);
					}
				}
			}
		}
	});

	UE_LOG(LogProjectInventory, Log, TEXT("StartupModule() - Inventory registered with FFeatureRegistry"));
}

bool FProjectInventoryModule::TrySubscribeInteraction(float /*DeltaTime*/)
{
	if (!InteractionHandler.IsValid())
	{
		return false;
	}

	InteractionHandler->Subscribe();
	if (!InteractionHandler->IsSubscribed())
	{
		return true;
	}

	RetryHandle.Reset();
	return false;
}

void FProjectInventoryModule::ShutdownModule()
{
	if (RetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RetryHandle);
		RetryHandle.Reset();
	}

	// Unsubscribe and release interaction handler
	if (InteractionHandler.IsValid())
	{
		InteractionHandler->Unsubscribe();
		InteractionHandler.Reset();
	}

	UE_LOG(LogProjectInventory, Log, TEXT("ShutdownModule()"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectInventoryModule, ProjectInventory)
