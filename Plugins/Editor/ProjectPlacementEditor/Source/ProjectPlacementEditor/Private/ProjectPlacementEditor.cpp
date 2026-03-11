// Copyright ALIS. All Rights Reserved.

#include "ProjectPlacementEditor.h"
#include "SProjectAssetPicker.h"
#include "ObjectDefinitionThumbnailRenderer.h"
#include "ReplaceWithDefinitionMenuExtension.h"
#include "Customization/LootEntryViewCustomization.h"

#include "PropertyEditorModule.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Misc/ObjectThumbnail.h"
#include "Data/ObjectDefinition.h"

DEFINE_LOG_CATEGORY(LogProjectPlacementEditor);

const FName FProjectPlacementEditorModule::AssetPickerTabId(TEXT("ProjectAssetPicker"));

#define LOCTEXT_NAMESPACE "FProjectPlacementEditorModule"

void FProjectPlacementEditorModule::StartupModule()
{
	UE_LOG(LogProjectPlacementEditor, Log, TEXT("StartupModule()"));

	// Register thumbnail renderer (show mesh thumbnails for definitions)
	UThumbnailManager::Get().RegisterCustomRenderer(
		UObjectDefinition::StaticClass(),
		UObjectDefinitionThumbnailRenderer::StaticClass());

	// Clear stale cached thumbnails from ObjectDefinition packages.
	// Thumbnails are baked into .uasset during save. If the renderer was broken
	// at save time, black thumbnails persist across editor restarts because UE
	// uses cached package thumbnails without calling Draw().
	// This one-time clear forces UE to re-render via our corrected renderer.
	{
		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AllDefs;
		ARM.Get().GetAssetsByClass(UObjectDefinition::StaticClass()->GetClassPathName(), AllDefs);

		int32 ClearedCount = 0;
		int32 LoadedPackagesForClear = 0;
		for (const FAssetData& AD : AllDefs)
		{
			UPackage* Pkg = FindPackage(nullptr, *AD.PackageName.ToString());
			if (!Pkg)
			{
				Pkg = LoadPackage(nullptr, *AD.PackageName.ToString(), LOAD_None);
				if (Pkg)
				{
					++LoadedPackagesForClear;
				}
			}

			if (!Pkg || !Pkg->HasThumbnailMap())
			{
				continue;
			}

			FThumbnailMap& ThumbMap = Pkg->AccessThumbnailMap();
			if (ThumbMap.Num() > 0)
			{
				ThumbMap.Empty();
				ClearedCount++;
			}
		}

		UE_LOG(LogProjectPlacementEditor, Log,
			TEXT("Cleared cached thumbnails from %d/%d ObjectDefinition packages (loaded %d packages for clear)"),
			ClearedCount, AllDefs.Num(), LoadedPackagesForClear);
	}

	// Register the tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AssetPickerTabId,
		FOnSpawnTab::CreateRaw(this, &FProjectPlacementEditorModule::SpawnAssetPickerTab))
		.SetDisplayName(LOCTEXT("AssetPickerTabTitle", "Project Placement"))
		.SetTooltipText(LOCTEXT("AssetPickerTabTooltip", "Browse and spawn project DataAssets"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset"));

	// Register menu extensions
	RegisterMenus();
	FReplaceWithDefinitionMenuExtension::Register();

	// Listen for asset changes to auto-refresh
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FProjectPlacementEditorModule::OnAssetAdded);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FProjectPlacementEditorModule::OnAssetRemoved);

	// Register property customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		TEXT("LootEntryView"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLootEntryViewCustomization::MakeInstance));
}

void FProjectPlacementEditorModule::ShutdownModule()
{
	UE_LOG(LogProjectPlacementEditor, Log, TEXT("ShutdownModule()"));

	// Unregister thumbnail renderer
	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UObjectDefinition::StaticClass());
	}

	// Unregister asset listeners
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
	}

	// Unregister property customizations
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("LootEntryView"));
	}

	// Unregister menus
	UnregisterMenus();

	// Unregister tab
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssetPickerTabId);

	AssetPickerWidget.Reset();
}

void FProjectPlacementEditorModule::RegisterMenus()
{
	// Extend Level Editor Window menu
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		if (Menu)
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("LevelEditor");
			Section.AddMenuEntry(
				"ProjectPlacement",
				LOCTEXT("OpenAssetPicker", "Project Placement"),
				LOCTEXT("OpenAssetPickerTooltip", "Open the Project Asset Picker panel"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset"),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FGlobalTabmanager::Get()->TryInvokeTab(AssetPickerTabId);
				}))
			);
		}
	}));
}

void FProjectPlacementEditorModule::UnregisterMenus()
{
	UToolMenus::UnregisterOwner(this);
}

TSharedRef<SDockTab> FProjectPlacementEditorModule::SpawnAssetPickerTab(const FSpawnTabArgs& Args)
{
	// Create picker widget if not exists
	if (!AssetPickerWidget.IsValid())
	{
		AssetPickerWidget = SNew(SProjectAssetPicker);
	}

	// Panel became visible -> refresh list and warm thumbnails without manual hover.
	AssetPickerWidget->RefreshAssetView();

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("AssetPickerTabLabel", "Project Placement"))
		[
			AssetPickerWidget.ToSharedRef()
		];
}

void FProjectPlacementEditorModule::OnAssetAdded(const FAssetData& AssetData)
{
	// Only care about ObjectDefinition assets
	if (AssetData.AssetClassPath != UObjectDefinition::StaticClass()->GetClassPathName())
	{
		return;
	}

	UE_LOG(LogProjectPlacementEditor, Log, TEXT("Asset added: %s"), *AssetData.AssetName.ToString());

	// Refresh picker if it exists
	if (AssetPickerWidget.IsValid())
	{
		AssetPickerWidget->RefreshAssetView();
	}
}

void FProjectPlacementEditorModule::OnAssetRemoved(const FAssetData& AssetData)
{
	// Only care about ObjectDefinition assets
	if (AssetData.AssetClassPath != UObjectDefinition::StaticClass()->GetClassPathName())
	{
		return;
	}

	UE_LOG(LogProjectPlacementEditor, Log, TEXT("Asset removed: %s"), *AssetData.AssetName.ToString());

	// Refresh picker if it exists
	if (AssetPickerWidget.IsValid())
	{
		AssetPickerWidget->RefreshAssetView();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectPlacementEditorModule, ProjectPlacementEditor)
