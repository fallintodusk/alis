// Copyright ALIS. All Rights Reserved.

#include "ReplaceWithDefinitionMenuExtension.h"
#include "ProjectPlacementEditor.h"
#include "ProjectObjectActorFactory.h"

#include "ToolMenus.h"
#include "Editor.h"
#include "Selection.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/ObjectDefinition.h"
#include "CapabilityRegistry.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ReplaceWithDefinition"

void FReplaceWithDefinitionMenuExtension::Register()
{
	UE_LOG(LogProjectPlacementEditor, Log, TEXT("ReplaceWithDefinition: Register() called"));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UE_LOG(LogProjectPlacementEditor, Log, TEXT("ReplaceWithDefinition: Startup callback executing"));

		// Extend the actor context menu (LevelEditor.ActorContextMenu)
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
		UE_LOG(LogProjectPlacementEditor, Log, TEXT("ReplaceWithDefinition: ExtendMenu returned %s"), Menu ? TEXT("valid menu") : TEXT("nullptr"));

		if (Menu)
		{
			// Add our own section with submenu - insert after ActorGeneral section (where "Replace Selected Actors with" lives)
			// Note: ActorGeneral is the internal name, "Asset Options" is just the display label
			FToolMenuSection& Section = Menu->AddSection(
				"ProjectDefinitions",
				LOCTEXT("ProjectDefinitionsSection", "Project"),
				FToolMenuInsert("ActorGeneral", EToolMenuInsertType::After)
			);

			Section.AddSubMenu(
				"ReplaceWithDefinition",
				LOCTEXT("ReplaceWithDefinition", "Replace with Definition"),
				LOCTEXT("ReplaceWithDefinitionTooltip", "Replace selected actors with ObjectDefinition-based actors"),
				FNewToolMenuDelegate::CreateStatic(&FReplaceWithDefinitionMenuExtension::ExtendActorContextMenu),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction::CreateStatic(&FReplaceWithDefinitionMenuExtension::HasSelectedActors)
				),
				EUserInterfaceActionType::Button,
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset")
			);

			UE_LOG(LogProjectPlacementEditor, Log, TEXT("ReplaceWithDefinition: Section and submenu added"));
		}
	}));
}

void FReplaceWithDefinitionMenuExtension::Unregister()
{
	// UToolMenus handles cleanup
}

void FReplaceWithDefinitionMenuExtension::ExtendActorContextMenu(UToolMenu* Menu)
{
	// Get available capabilities
	TArray<FName> Capabilities = GetAvailableCapabilities();

	if (Capabilities.Num() == 0)
	{
		return;
	}

	// Add a single section with submenus for each capability
	FToolMenuSection& Section = Menu->AddSection("Capabilities", FText::GetEmpty());

	for (const FName& CapName : Capabilities)
	{
		// Add submenu for this capability (e.g., "Hinged", "Pickup", "Sliding")
		Section.AddSubMenu(
			FName(*FString::Printf(TEXT("Cap_%s"), *CapName.ToString())),
			FText::FromName(CapName),
			FText::Format(LOCTEXT("CapabilitySubmenuTooltip", "Replace with {0} objects"), FText::FromName(CapName)),
			FNewToolMenuDelegate::CreateLambda([CapName](UToolMenu* SubMenu)
			{
				BuildCapabilitySubmenu(SubMenu, CapName);
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset")
		);
	}
}

void FReplaceWithDefinitionMenuExtension::BuildCapabilitySubmenu(UToolMenu* Menu, FName CapabilityName)
{
	TArray<FAssetData> Definitions = GetDefinitionsByCapability(CapabilityName);

	if (Definitions.Num() == 0)
	{
		FToolMenuSection& Section = Menu->AddSection("Empty", FText::GetEmpty());
		Section.AddMenuEntry(
			"NoItems",
			LOCTEXT("NoItemsInCategory", "No items in this category"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction()
		);
		return;
	}

	// Sort by name
	Definitions.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.AssetName.LexicalLess(B.AssetName);
	});

	FToolMenuSection& Section = Menu->AddSection("Definitions", FText::GetEmpty());

	for (const FAssetData& Definition : Definitions)
	{
		Section.AddMenuEntry(
			Definition.AssetName,
			FText::FromName(Definition.AssetName),
			FText::Format(LOCTEXT("ReplaceWithTooltip", "Replace selection with {0}"), FText::FromName(Definition.AssetName)),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset"),
			FUIAction(FExecuteAction::CreateLambda([Definition]()
			{
				ExecuteReplace(Definition);
			}))
		);
	}
}

void FReplaceWithDefinitionMenuExtension::ExecuteReplace(const FAssetData& DefinitionAsset)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("ReplaceWithDefinition: No world"));
		return;
	}

	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("ReplaceWithDefinition: No level"));
		return;
	}

	// Create factory
	UProjectObjectActorFactory* Factory = NewObject<UProjectObjectActorFactory>();

	// Load the definition asset
	UObject* Asset = DefinitionAsset.GetAsset();
	if (!Asset)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("ReplaceWithDefinition: Failed to load definition"));
		return;
	}

	// Get selected actors
	TArray<AActor*> SelectedActors;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			SelectedActors.Add(Actor);
		}
	}

	if (SelectedActors.Num() == 0)
	{
		UE_LOG(LogProjectPlacementEditor, Warning, TEXT("ReplaceWithDefinition: No actors selected"));
		return;
	}

	// Begin transaction for undo
	GEditor->BeginTransaction(LOCTEXT("ReplaceWithDefinitionTransaction", "Replace with Definition"));

	TArray<AActor*> NewActors;

	for (AActor* OldActor : SelectedActors)
	{
		// Store transform
		FTransform Transform = OldActor->GetActorTransform();

		// Spawn new actor
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transactional;

		AActor* NewActor = Factory->SpawnActor(Asset, Level, Transform, Params);
		if (NewActor)
		{
			NewActors.Add(NewActor);
			UE_LOG(LogProjectPlacementEditor, Log, TEXT("Replaced %s with %s at %s"),
				*OldActor->GetName(),
				*NewActor->GetName(),
				*Transform.GetLocation().ToString());

			// Destroy old actor
			OldActor->Modify();
			World->EditorDestroyActor(OldActor, true);
		}
		else
		{
			UE_LOG(LogProjectPlacementEditor, Warning, TEXT("Failed to spawn replacement for %s"), *OldActor->GetName());
		}
	}

	GEditor->EndTransaction();

	// Select new actors
	GEditor->SelectNone(false, true);
	for (AActor* NewActor : NewActors)
	{
		GEditor->SelectActor(NewActor, true, true);
	}

	UE_LOG(LogProjectPlacementEditor, Log, TEXT("Replaced %d actors with %s"), NewActors.Num(), *DefinitionAsset.AssetName.ToString());
}

TArray<FName> FReplaceWithDefinitionMenuExtension::GetAvailableCapabilities()
{
	TSet<FName> UniqueCapabilities;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UObjectDefinition::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/ProjectObject")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> ObjectAssets;
	AssetRegistry.GetAssets(Filter, ObjectAssets);

	for (const FAssetData& Asset : ObjectAssets)
	{
		Asset.TagsAndValues.ForEach([&UniqueCapabilities](const TPair<FName, FAssetTagValueRef>& Pair)
		{
			FString TagNameStr = Pair.Key.ToString();
			FString TagValueStr = Pair.Value.AsString();

			if (TagNameStr.StartsWith(TEXT("ALIS.Cap.")) && TagValueStr == TEXT("true"))
			{
				FString CapName = TagNameStr.RightChop(9);
				FName CapabilityId = FName(*CapName);

				// Only add implemented capabilities
				if (FCapabilityRegistry::HasCapability(CapabilityId))
				{
					UniqueCapabilities.Add(CapabilityId);
				}
			}
		});
	}

	TArray<FName> Result = UniqueCapabilities.Array();
	Result.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Result;
}

TArray<FAssetData> FReplaceWithDefinitionMenuExtension::GetDefinitionsByCapability(FName CapabilityName)
{
	TArray<FAssetData> Result;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UObjectDefinition::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/ProjectObject")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> ObjectAssets;
	AssetRegistry.GetAssets(Filter, ObjectAssets);

	for (const FAssetData& Asset : ObjectAssets)
	{
		// If no capability filter, include all
		if (CapabilityName == NAME_None)
		{
			Result.Add(Asset);
			continue;
		}

		// Check if asset has the requested capability
		FString TagName = FString::Printf(TEXT("ALIS.Cap.%s"), *CapabilityName.ToString());
		FString TagValue;
		Asset.GetTagValue(*TagName, TagValue);
		if (TagValue == TEXT("true"))
		{
			Result.Add(Asset);
		}
	}

	return Result;
}

bool FReplaceWithDefinitionMenuExtension::HasSelectedActors()
{
	return GEditor && GEditor->GetSelectedActorCount() > 0;
}

#undef LOCTEXT_NAMESPACE
