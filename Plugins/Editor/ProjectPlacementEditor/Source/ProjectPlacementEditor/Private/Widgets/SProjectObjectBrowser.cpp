// Copyright ALIS. All Rights Reserved.

/**
 * SProjectObjectBrowser - Object browser with folder tree and asset tiles.
 *
 * Mirrors SProjectItemBrowser pattern for ObjectDefinition assets.
 * Uses OnShouldFilterAsset for path filtering (see SProjectItemBrowser for rationale).
 */

#include "Widgets/SProjectObjectBrowser.h"
#include "ProjectObjectActorFactory.h"
#include "ProjectPlacementEditor.h"
#include "DefinitionThumbnailRenderer.h"
#include "DefinitionGeneratorSubsystem.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Data/ObjectDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CapabilityRegistry.h"

#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SToolTip.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Misc/NamePermissionList.h"
#include "Misc/Paths.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "AssetRegistry/AssetIdentifier.h"

void SProjectObjectBrowser::Construct(const FArguments& InArgs)
{
	// Create spawn factory (TStrongObjectPtr prevents GC)
	SpawnFactory.Reset(NewObject<UProjectObjectActorFactory>());

	CurrentPath = ObjectsRootPath;

	FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	// ========================================================================
	// Path Picker (folder tree) - restricted to /ProjectObject subtree
	// ========================================================================
	FPathPickerConfig PathConfig;
	PathConfig.DefaultPath = ObjectsRootPath;
	PathConfig.bFocusSearchBoxWhenOpened = false;
	PathConfig.bForceShowPluginContent = true;
	PathConfig.bShowFavorites = false;
	PathConfig.bAddDefaultPath = true;
	PathConfig.bNotifyDefaultPathSelected = true;
	PathConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SProjectObjectBrowser::OnPathSelected);

	// Restrict folder tree to only show /ProjectObject subtree
	TSharedPtr<FPathPermissionList> FolderFilter = MakeShared<FPathPermissionList>();
	FolderFilter->AddAllowListItem(TEXT("ProjectObjectBrowser"), ObjectsRootPath);
	PathConfig.CustomFolderPermissionList = FolderFilter;

	TSharedRef<SWidget> PathWidget = CBModule.Get().CreatePathPicker(PathConfig);
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: Created PathPicker (default: %s)"), ObjectsRootPath);

	// ========================================================================
	// Asset Picker - filter by class, use OnShouldFilterAsset for path
	// ========================================================================
	FAssetPickerConfig AssetConfig;

	// Filter by class only
	AssetConfig.Filter.ClassPaths.Add(UObjectDefinition::StaticClass()->GetClassPathName());
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: AssetPicker filter: Class=%s"),
		*UObjectDefinition::StaticClass()->GetClassPathName().ToString());

	// View settings
	AssetConfig.InitialAssetViewType = EAssetViewType::Tile;
	AssetConfig.InitialThumbnailSize = EThumbnailSize::Large;
	AssetConfig.ThumbnailScale = 0.25f;
	AssetConfig.ThumbnailLabel = EThumbnailLabel::NoLabel;

	// Behavior
	AssetConfig.bCanShowFolders = false;
	AssetConfig.bCanShowClasses = false;
	AssetConfig.bForceShowPluginContent = true;
	AssetConfig.bForceShowEngineContent = false;
	AssetConfig.bAllowDragging = true;
	AssetConfig.bCanShowRealTimeThumbnails = false;

	// Path filtering via callback
	AssetConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SProjectObjectBrowser::ShouldFilterAsset);

	// Tooltip showing ObjectId
	AssetConfig.OnIsAssetValidForCustomToolTip = FOnIsAssetValidForCustomToolTip::CreateLambda(
		[](const FAssetData&) { return true; });
	AssetConfig.OnGetCustomAssetToolTip = FOnGetCustomAssetToolTip::CreateLambda(
		[](FAssetData& AssetData) -> TSharedRef<SToolTip>
		{
			FString Result = AssetData.AssetName.ToString();

			// Show mesh count and capability count from loaded asset (if available)
			// For performance, could add AssetRegistrySearchable tags to UObjectDefinition

			return SNew(SToolTip).Text(FText::FromString(Result));
		});

	// Callbacks
	AssetConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SProjectObjectBrowser::OnAssetDoubleClicked);
	AssetConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SProjectObjectBrowser::GetAssetContextMenu);
	AssetConfig.RefreshAssetViewDelegates.Add(&RefreshDelegate);

	TSharedRef<SWidget> AssetWidget = CBModule.Get().CreateAssetPicker(AssetConfig);
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: Created AssetPicker"));

	// ========================================================================
	// Capability Filters - Discover from AssetRegistry
	// ========================================================================
	RefreshCapabilityFilters();

	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: Initial capabilities: %d"), AvailableCapabilities.Num());
	for (const FName& Cap : AvailableCapabilities)
	{
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("  - %s"), *Cap.ToString());
	}

	// ========================================================================
	// Layout: Filters + Splitter with PathPicker (25%) | AssetPicker (75%)
	// ========================================================================
	ChildSlot
	[
		SNew(SVerticalBox)

		// Level 1: Capability filter
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Capability:")))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Refresh")))
						.ToolTipText(FText::FromString(TEXT("Refresh capability filters (use after regenerating objects or changing tags)")))
						.OnClicked_Lambda([this]() -> FReply
						{
							UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: Refresh button clicked"));
							RefreshCapabilityFilters();
							BuildCapabilityFilterBar();
							BuildPickupRoleFilterBar();
							RefreshDelegate.ExecuteIfBound(true);
							return FReply::Handled();
						})
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						SAssignNew(CapabilityFilterBarContainer, SHorizontalBox)
					]
				]
			]
		]

		// Level 2: PickupRole filter (conditionally visible)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			.Visibility_Lambda([this]()
			{
				return CurrentCapabilityFilter == TEXT("Pickup") ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Pickup Role:")))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						SAssignNew(PickupRoleFilterBarContainer, SHorizontalBox)
					]
				]
			]
		]

		// Content area
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(4.0f)
				[
					PathWidget
				]
			]

			+ SSplitter::Slot()
			.Value(0.75f)
			[
				AssetWidget
			]
		]
	];

	// Populate filter bars after layout is constructed
	BuildCapabilityFilterBar();
	BuildPickupRoleFilterBar();

	// Auto-refresh when AssetRegistry completes initial scan (for editor startup)
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (AssetRegistry.IsLoadingAssets())
	{
		// OnFilesLoaded fires when scan completes, but tags may not be ready yet
		AssetRegistry.OnFilesLoaded().AddSP(this, &SProjectObjectBrowser::OnAssetRegistryScanComplete);
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: Waiting for AssetRegistry scan"));
	}
	else
	{
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: AssetRegistry already loaded"));
	}

	UE_LOG(LogProjectPlacementEditor, Log, TEXT("SProjectObjectBrowser: Initialized"));

	// Warm visible thumbnails when panel appears so user doesn't need manual hover.
	StartThumbnailWarmupPasses();
}

void SProjectObjectBrowser::RefreshAssets()
{
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser::RefreshAssets"));
	StartThumbnailWarmupPasses();
}

void SProjectObjectBrowser::StartThumbnailWarmupPasses()
{
	// Avoid stale runtime cache interactions across repeated refresh/filter cycles.
	UDefinitionThumbnailRenderer::ResetRuntimeCaches();

	if (GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(ThumbnailWarmupTimerHandle);
	}

	// Run a small burst of one-shot refreshes:
	// 1) immediate, 2) after tile widgets settle, 3) after async loads catch up.
	ThumbnailWarmupPassesRemaining = 3;
	RunThumbnailWarmupPass();
}

void SProjectObjectBrowser::RunThumbnailWarmupPass()
{
	if (ThumbnailWarmupPassesRemaining <= 0)
	{
		return;
	}

	const int32 PassIndex = 4 - ThumbnailWarmupPassesRemaining;
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser: Thumbnail warmup pass %d/3"), PassIndex);
	RefreshDelegate.ExecuteIfBound(true);
	--ThumbnailWarmupPassesRemaining;

	if (ThumbnailWarmupPassesRemaining > 0 && GEditor)
	{
		const float DelaySeconds = (ThumbnailWarmupPassesRemaining == 2) ? 0.10f : 0.35f;
		GEditor->GetTimerManager()->SetTimer(
			ThumbnailWarmupTimerHandle,
			FTimerDelegate::CreateSP(this, &SProjectObjectBrowser::RunThumbnailWarmupPass),
			DelaySeconds,
			false);
	}
}

void SProjectObjectBrowser::OnPathSelected(const FString& Path)
{
	if (CurrentPath == Path)
	{
		return;
	}

	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("SProjectObjectBrowser::OnPathSelected: '%s' -> '%s'"), *CurrentPath, *Path);
	CurrentPath = Path;
	StartThumbnailWarmupPasses();
}

bool SProjectObjectBrowser::ShouldFilterAsset(const FAssetData& AssetData) const
{
	// Path filtering (existing)
	const FString PackagePath = AssetData.PackagePath.ToString();

	FString FilterPath = CurrentPath;
	if (FilterPath.IsEmpty() || FilterPath == TEXT("/"))
	{
		FilterPath = ObjectsRootPath;
	}

	// Ensure exact folder match by adding trailing slash to prevent prefix collisions
	// Example: "/ProjectObject/Human" should NOT match "/ProjectObject/HumanMade"
	if (!FilterPath.EndsWith(TEXT("/")))
	{
		FilterPath += TEXT("/");
	}

	const bool bShowPath = PackagePath.StartsWith(FilterPath);
	if (!bShowPath)
	{
		return true;  // Hide if path doesn't match
	}

	// Level 1: Capability filter
	if (CurrentCapabilityFilter != NAME_None)
	{
		FString TagName = FString::Printf(TEXT("ALIS.Cap.%s"), *CurrentCapabilityFilter.ToString());
		FString TagValue;
		AssetData.GetTagValue(*TagName, TagValue);
		if (TagValue != TEXT("true"))
		{
			return true;  // Hide if doesn't have capability
		}
	}

	// Level 2: Item.Type filter (only when Pickup capability selected)
	// Uses prefix match to support hierarchical tags (e.g., Item.Type.Key.Apartment.Luxury matches "Key" filter)
	if (CurrentCapabilityFilter == TEXT("Pickup") && CachedItemTypeTagName != NAME_None)
	{
		const FString TagPrefix = CachedItemTypeTagName.ToString();
		bool bHasMatchingType = false;
		AssetData.EnumerateTags([&](TPair<FName, FAssetTagValueRef> TagPair)
		{
			if (TagPair.Key.ToString().StartsWith(TagPrefix))
			{
				bHasMatchingType = true;
			}
		});
		if (!bHasMatchingType)
		{
			return true;  // Hide if doesn't have this Item.Type
		}
	}

	return false;  // Show asset
}

void SProjectObjectBrowser::RefreshCapabilityFilters()
{
	AvailableCapabilities.Empty();
	TSet<FName> UniqueCapabilities;
	TSet<FName> SkippedCapabilities;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Use FARFilter to restrict to /ProjectObject tree (avoids scanning all assets)
	FARFilter Filter;
	Filter.ClassPaths.Add(UObjectDefinition::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(ObjectsRootPath));
	Filter.bRecursivePaths = true;

	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("RefreshCapabilityFilters: Scanning %s for %s"),
		ObjectsRootPath, *UObjectDefinition::StaticClass()->GetClassPathName().ToString());

	TArray<FAssetData> ObjectAssets;
	AssetRegistry.GetAssets(Filter, ObjectAssets);

	// Collect all unique capability tags
	int32 AssetsWithoutTags = 0;
	for (const FAssetData& Asset : ObjectAssets)
	{
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("  Asset: %s (Path: %s)"), *Asset.AssetName.ToString(), *Asset.PackagePath.ToString());

		int32 TagCount = 0;
		Asset.TagsAndValues.ForEach([&UniqueCapabilities, &SkippedCapabilities, &TagCount, &Asset](const TPair<FName, FAssetTagValueRef>& Pair)
		{
			FString TagNameStr = Pair.Key.ToString();
			FString TagValueStr = Pair.Value.AsString();

			if (TagNameStr.StartsWith(TEXT("ALIS.")))
			{
				UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("    Tag: %s = %s"), *TagNameStr, *TagValueStr);
				TagCount++;
			}

			if (TagNameStr.StartsWith(TEXT("ALIS.Cap.")) && TagValueStr == TEXT("true"))
			{
				// Extract capability name (e.g., "ALIS.Cap.Hinged" -> "Hinged")
				FString CapName = TagNameStr.RightChop(9); // Skip "ALIS.Cap."
				FName CapabilityId = FName(*CapName);

				// Validate against actual implemented capabilities
				if (FCapabilityRegistry::HasCapability(CapabilityId))
				{
					UniqueCapabilities.Add(CapabilityId);
					UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("      -> Capability: %s"), *CapName);
				}
				else
				{
					SkippedCapabilities.Add(CapabilityId);
				}
			}
		});

		if (TagCount == 0)
		{
			AssetsWithoutTags++;
			UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("    NO ALIS.* tags found"));
		}
	}

	// Convert to sorted array for UI
	AvailableCapabilities = UniqueCapabilities.Array();
	AvailableCapabilities.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	// Warn if no capabilities found (likely means assets need resaving)
	if (AvailableCapabilities.Num() == 0 && ObjectAssets.Num() > 0)
	{
		UE_LOG(LogProjectPlacementEditor, Warning,
			TEXT("RefreshCapabilityFilters: Found %d ObjectDefinition assets but 0 capabilities. Assets may need resaving to populate AssetRegistry tags."),
			ObjectAssets.Num());
	}

	// Discover PickupRoles from Item.Type.* tags (no derived tag needed)
	AvailablePickupRoles.Empty();
	TSet<FName> UniquePickupRoles;
	int32 PickupsWithoutRole = 0;

	const FString ItemTypePrefix = TEXT("ALIS.ItemTag.Item.Type.");

	for (const FAssetData& Asset : ObjectAssets)
	{
		FString HasPickup;
		Asset.GetTagValue(TEXT("ALIS.Cap.Pickup"), HasPickup);
		if (HasPickup == TEXT("true"))
		{
			UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("  Pickup asset: %s"), *Asset.AssetName.ToString());

			// Enumerate tags to find Item.Type.* (e.g., ALIS.ItemTag.Item.Type.Consumable -> "Consumable")
			// For hierarchical tags like Item.Type.Key.Apartment.Luxury, extract only first segment -> "Key"
			bool bFoundRole = false;
			Asset.EnumerateTags([&](TPair<FName, FAssetTagValueRef> TagPair)
			{
				const FString TagStr = TagPair.Key.ToString();
				if (TagStr.StartsWith(ItemTypePrefix))
				{
					// Extract first segment: "ALIS.ItemTag.Item.Type.Key.Apartment.Luxury" -> "Key"
					FString Rest = TagStr.RightChop(ItemTypePrefix.Len());
					FString Role = Rest;
					int32 DotIndex = INDEX_NONE;
					if (Rest.FindChar(TEXT('.'), DotIndex))
					{
						Role = Rest.Left(DotIndex);
					}
					UniquePickupRoles.Add(FName(*Role));
					UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("    -> Item.Type: %s"), *Role);
					bFoundRole = true;
				}
			});

			if (!bFoundRole)
			{
				PickupsWithoutRole++;
				UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("    -> Missing Item.Type.* tag"));
			}
		}
	}

	// Convert to sorted array for UI
	AvailablePickupRoles = UniquePickupRoles.Array();
	AvailablePickupRoles.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	// Summary with skipped capabilities - ERROR with popup
	if (SkippedCapabilities.Num() > 0)
	{
		FString CapabilitiesList = FString::JoinBy(SkippedCapabilities.Array(), TEXT(", "), [](const FName& N) { return N.ToString(); });

		UE_LOG(LogProjectPlacementEditor, Error, TEXT("=== SKIPPED %d unimplemented capabilities: %s ==="),
			SkippedCapabilities.Num(),
			*CapabilitiesList);

		// Show ERROR notification popup
		FNotificationInfo ErrorInfo(FText::Format(
			NSLOCTEXT("ProjectObjectBrowser", "UnimplementedCapabilitiesError",
				"ERROR: {0} capability(ies) in JSON but NOT implemented in C++:\n{1}\n\nEither implement the C++ code or remove from JSON."),
			FText::AsNumber(SkippedCapabilities.Num()),
			FText::FromString(CapabilitiesList)
		));
		ErrorInfo.bFireAndForget = true;
		ErrorInfo.bUseSuccessFailIcons = true;
		ErrorInfo.ExpireDuration = 10.0f;  // 10 seconds for critical error
		ErrorInfo.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.Error"));

		TSharedPtr<SNotificationItem> ErrorNotification = FSlateNotificationManager::Get().AddNotification(ErrorInfo);
		if (ErrorNotification.IsValid())
		{
			ErrorNotification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}

	// Summary log (always visible)
	UE_LOG(LogProjectPlacementEditor, Log, TEXT("RefreshCapabilityFilters: %d assets, %d capabilities, %d pickup roles%s%s%s"),
		ObjectAssets.Num(),
		AvailableCapabilities.Num(),
		AvailablePickupRoles.Num(),
		SkippedCapabilities.Num() > 0 ? *FString::Printf(TEXT(", %d SKIPPED"), SkippedCapabilities.Num()) : TEXT(""),
		AssetsWithoutTags > 0 ? *FString::Printf(TEXT(", %d without tags"), AssetsWithoutTags) : TEXT(""),
		PickupsWithoutRole > 0 ? *FString::Printf(TEXT(", %d pickups missing Item.Type"), PickupsWithoutRole) : TEXT(""));
}

TSharedRef<SWidget> SProjectObjectBrowser::BuildCapabilityFilterBar()
{
	if (!CapabilityFilterBarContainer.IsValid())
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("BuildCapabilityFilterBar: Container not valid"));
		return SNullWidget::NullWidget;
	}

	CapabilityFilterBarContainer->ClearChildren();

	// "All" button
	CapabilityFilterBarContainer->AddSlot()
		.AutoWidth()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("All")))
			.OnClicked_Lambda([this]() -> FReply
			{
				return OnCapabilityFilterChanged(NAME_None);
			})
		];

	// Dynamic capability buttons
	for (const FName& CapName : AvailableCapabilities)
	{
		CapabilityFilterBarContainer->AddSlot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromName(CapName))
				.OnClicked_Lambda([this, CapName]() -> FReply
				{
					return OnCapabilityFilterChanged(CapName);
				})
			];
	}

	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("BuildCapabilityFilterBar: %d buttons"), CapabilityFilterBarContainer->GetChildren()->Num());
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SProjectObjectBrowser::BuildPickupRoleFilterBar()
{
	if (!PickupRoleFilterBarContainer.IsValid())
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("BuildPickupRoleFilterBar: Container not valid"));
		return SNullWidget::NullWidget;
	}

	PickupRoleFilterBarContainer->ClearChildren();

	// "All" button
	PickupRoleFilterBarContainer->AddSlot()
		.AutoWidth()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("All")))
			.OnClicked_Lambda([this]() -> FReply
			{
				return OnPickupRoleFilterChanged(NAME_None);
			})
		];

	// Dynamic PickupRole buttons
	for (const FName& RoleName : AvailablePickupRoles)
	{
		PickupRoleFilterBarContainer->AddSlot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromName(RoleName))
				.OnClicked_Lambda([this, RoleName]() -> FReply
				{
					return OnPickupRoleFilterChanged(RoleName);
				})
			];
	}

	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("BuildPickupRoleFilterBar: %d buttons"), PickupRoleFilterBarContainer->GetChildren()->Num());
	return SNullWidget::NullWidget;
}

FReply SProjectObjectBrowser::OnCapabilityFilterChanged(FName NewFilter)
{
	CurrentCapabilityFilter = NewFilter;
	CurrentPickupRoleFilter = NAME_None; // Reset sub-filter
	StartThumbnailWarmupPasses();
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("Capability filter changed to: %s"), *NewFilter.ToString());
	return FReply::Handled();
}

FReply SProjectObjectBrowser::OnPickupRoleFilterChanged(FName NewRole)
{
	CurrentPickupRoleFilter = NewRole;

	// Cache tag name to avoid string build per asset in ShouldFilterAsset
	if (NewRole != NAME_None)
	{
		CachedItemTypeTagName = FName(*FString::Printf(TEXT("ALIS.ItemTag.Item.Type.%s"), *NewRole.ToString()));
	}
	else
	{
		CachedItemTypeTagName = NAME_None;
	}

	StartThumbnailWarmupPasses();
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("Item.Type filter changed to: %s"), *NewRole.ToString());
	return FReply::Handled();
}

void SProjectObjectBrowser::OnAssetRegistryScanComplete()
{
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("AssetRegistry scan complete - checking for tags"));

	// OnFilesLoaded fires DURING broadcast, tags become ready ~200ms AFTER broadcast completes
	// Wait 1 second (plenty of time), then check every second for up to 30 seconds
	TagCheckAttemptsRemaining = 30; // Max 30 attempts at 1-second intervals

	FTimerHandle UnusedHandle;
	GEditor->GetTimerManager()->SetTimer(
		UnusedHandle,
		FTimerDelegate::CreateSP(this, &SProjectObjectBrowser::CheckForTagsPeriodically),
		1.0f,  // Check after 1 second
		false  // No loop (we'll reschedule if needed)
	);
}

void SProjectObjectBrowser::CheckForTagsPeriodically()
{
	if (TagCheckAttemptsRemaining <= 0)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("=== AssetRegistry tags not ready after 30 seconds - skipping auto-refresh ==="));
		return;
	}

	TagCheckAttemptsRemaining--;

	// Check if tags are ready
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UObjectDefinition::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(ObjectsRootPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> ObjectAssets;
	AssetRegistry.GetAssets(Filter, ObjectAssets);

	// Check if any asset has ALIS tags
	bool bTagsReady = false;
	for (const FAssetData& Asset : ObjectAssets)
	{
		Asset.TagsAndValues.ForEach([&bTagsReady](const TPair<FName, FAssetTagValueRef>& Pair)
		{
			if (Pair.Key.ToString().StartsWith(TEXT("ALIS.")))
			{
				bTagsReady = true;
			}
		});

		if (bTagsReady)
		{
			break;
		}
	}

	if (bTagsReady)
	{
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("AssetRegistry tags ready after %d attempts"), 30 - TagCheckAttemptsRemaining);
		RefreshCapabilityFilters();
		BuildCapabilityFilterBar();
		BuildPickupRoleFilterBar();
		StartThumbnailWarmupPasses();
	}
	else
	{
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("Tags not ready yet, retry %d/30"), 30 - TagCheckAttemptsRemaining);

		// Reschedule for 1 second later
		FTimerHandle UnusedHandle;
		GEditor->GetTimerManager()->SetTimer(
			UnusedHandle,
			FTimerDelegate::CreateSP(this, &SProjectObjectBrowser::CheckForTagsPeriodically),
			1.0f,  // Check every 1 second
			false
		);
	}
}

void SProjectObjectBrowser::OnAssetDoubleClicked(const FAssetData& AssetData)
{
	UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("OnAssetDoubleClicked: %s"), *AssetData.AssetName.ToString());
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetData.GetAsset());
}

void SProjectObjectBrowser::SpawnFromAsset(const FAssetData& AssetData)
{
	if (!SpawnFactory.IsValid())
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("SpawnFromAsset: No factory"));
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("SpawnFromAsset: No world"));
		return;
	}

	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("SpawnFromAsset: No level"));
		return;
	}

	UObject* Asset = AssetData.GetAsset();
	if (!Asset)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("SpawnFromAsset: Failed to load asset"));
		return;
	}

	FText Error;
	if (!SpawnFactory->CanCreateActorFrom(AssetData, Error))
	{
		UE_LOG(LogProjectPlacementEditor, Warning, TEXT("SpawnFromAsset: %s"), *Error.ToString());
		return;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transactional;

	AActor* Actor = SpawnFactory->SpawnActor(Asset, Level, GetSpawnTransform(), Params);
	if (Actor)
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Actor, true, true);
		UE_LOG(LogProjectPlacementEditor, Verbose, TEXT("Spawned: %s"), *Actor->GetName());
	}
	else
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("  Spawn failed"));
	}
}

FTransform SProjectObjectBrowser::GetSpawnTransform() const
{
	FVector Loc = GEditor->ClickLocation;
	if (Loc.IsNearlyZero())
	{
		Loc = FVector(0.0f, 0.0f, 100.0f);
	}
	return FTransform(Loc);
}

TSharedPtr<SWidget> SProjectObjectBrowser::GetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.Num() == 0)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	// Single asset actions
	if (SelectedAssets.Num() == 1)
	{
		const FAssetData& Asset = SelectedAssets[0];

		// Open Asset
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Open")),
			FText::FromString(TEXT("Open the asset editor")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset.GetAsset());
			}))
		);

		// Regenerate this exact definition from source JSON
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Regenerate from Source")),
			FText::FromString(TEXT("Regenerate this definition asset from its source JSON file")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(
				FExecuteAction::CreateLambda([this, Asset]()
				{
					UObjectDefinition* ObjectDef = Cast<UObjectDefinition>(Asset.GetAsset());
					if (!ObjectDef)
					{
						FNotificationInfo Info(FText::FromString(TEXT("Selected asset is not an ObjectDefinition")));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 4.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}

					if (ObjectDef->SourceJsonPath.IsEmpty())
					{
						FNotificationInfo Info(FText::FromString(TEXT("SourceJsonPath is empty. Regenerate all Object definitions once first.")));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 6.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}

					UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
					if (!Generator)
					{
						FNotificationInfo Info(FText::FromString(TEXT("DefinitionGeneratorSubsystem is not available")));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 4.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}

					// Resolve registered generator type from asset class; fallback to Object for ObjectDefinition browser.
					FString TypeName;
					TArray<FString> RegisteredTypes = Generator->GetRegisteredTypeNames();
					for (const FString& CandidateType : RegisteredTypes)
					{
						FDefinitionTypeInfo TypeInfo;
						if (!Generator->GetDefinitionTypeInfo(CandidateType, TypeInfo) || !TypeInfo.DefinitionClass)
						{
							continue;
						}

						if (ObjectDef->GetClass()->IsChildOf(TypeInfo.DefinitionClass))
						{
							TypeName = CandidateType;
							break;
						}
					}

					if (TypeName.IsEmpty() && Asset.AssetClassPath == UObjectDefinition::StaticClass()->GetClassPathName())
					{
						TypeName = TEXT("Object");
					}

					if (TypeName.IsEmpty())
					{
						FNotificationInfo Info(FText::FromString(TEXT("Could not resolve generator type for selected asset")));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 5.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}

					const FString SourceDir = Generator->GetSourceDirectoryForType(TypeName);
					if (SourceDir.IsEmpty())
					{
						FNotificationInfo Info(FText::FromString(TEXT("Generator source directory is not configured for this type")));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 5.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}

					const FString JsonPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(SourceDir, ObjectDef->SourceJsonPath));
					if (!IFileManager::Get().FileExists(*JsonPath))
					{
						FNotificationInfo Info(FText::FromString(FString::Printf(
							TEXT("Source JSON not found: %s"),
							*ObjectDef->SourceJsonPath)));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 6.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}

					const FDefinitionGenerationResult Result = Generator->GenerateFromJson(TypeName, JsonPath, true);
					if (Result.bSuccess)
					{
						RefreshCapabilityFilters();
						BuildCapabilityFilterBar();
						BuildPickupRoleFilterBar();
						StartThumbnailWarmupPasses();

						FNotificationInfo Info(FText::FromString(FString::Printf(
							TEXT("Regenerated %s from source"),
							*Result.AssetId)));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 3.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Success);
						}
					}
					else
					{
						FNotificationInfo Info(FText::FromString(FString::Printf(
							TEXT("Regeneration failed: %s"),
							Result.ErrorMessage.IsEmpty() ? TEXT("unknown error") : *Result.ErrorMessage)));
						Info.bUseSuccessFailIcons = true;
						Info.ExpireDuration = 8.0f;
						if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
					}
				}),
				FCanExecuteAction::CreateLambda([Asset]()
				{
					const UObjectDefinition* ObjectDef = Cast<UObjectDefinition>(Asset.GetAsset());
					return ObjectDef != nullptr;
				})
			)
		);

		MenuBuilder.AddSeparator();

		// Browse to Asset
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Browse to Asset")),
			FText::FromString(TEXT("Show this asset in the Content Browser")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				TArray<FAssetData> Assets = { Asset };
				GEditor->SyncBrowserToObjects(Assets);
			}))
		);

		// Find References
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Reference Viewer")),
			FText::FromString(TEXT("Open the reference viewer for this asset")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Blueprints"),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				TArray<FAssetIdentifier> AssetIdentifiers;
				AssetIdentifiers.Add(FAssetIdentifier(Asset.PackageName));
				FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
			}))
		);

		// Size Map
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Size Map")),
			FText::FromString(TEXT("Open the size map for this asset")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				TArray<FAssetIdentifier> AssetIdentifiers;
				AssetIdentifiers.Add(FAssetIdentifier(Asset.PackageName));
				FEditorDelegates::OnOpenSizeMap.Broadcast(AssetIdentifiers);
			}))
		);

		MenuBuilder.AddSeparator();

		// Copy Reference
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Copy Reference")),
			FText::FromString(TEXT("Copy the asset reference path to clipboard")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				FString Reference = Asset.GetExportTextName();
				FPlatformApplicationMisc::ClipboardCopy(*Reference);
			}))
		);

		// Copy Path
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("Copy Path")),
			FText::FromString(TEXT("Copy the asset path to clipboard")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				FString Path = Asset.GetSoftObjectPath().ToString();
				FPlatformApplicationMisc::ClipboardCopy(*Path);
			}))
		);
	}
	else
	{
		// Multi-select: Browse to all
		MenuBuilder.AddMenuEntry(
			FText::Format(FText::FromString(TEXT("Browse to {0} Assets")), FText::AsNumber(SelectedAssets.Num())),
			FText::FromString(TEXT("Show selected assets in the Content Browser")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
			FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
			{
				GEditor->SyncBrowserToObjects(SelectedAssets);
			}))
		);
	}

	return MenuBuilder.MakeWidget();
}
