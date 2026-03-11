// Copyright ALIS. All Rights Reserved.

/**
 * SProjectAssetPicker.cpp
 *
 * Main placement panel - composes sub-widgets into cohesive UI.
 * Follows SOLID principles:
 * - Single Responsibility: Only handles layout composition
 * - Open/Closed: New sections added by creating new sub-widgets
 * - Dependency Inversion: Depends on abstractions (sub-widget interfaces)
 *
 * Sub-widgets:
 * - SProjectTemplateBar: Template actor buttons
 * - SProjectObjectBrowser: Folder tree + object asset browser (includes items via tag filters)
 */

#include "SProjectAssetPicker.h"
#include "ProjectPlacementEditor.h"

#include "Widgets/SProjectTemplateBar.h"
#include "Widgets/SProjectObjectBrowser.h"

#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SProjectAssetPicker"

void SProjectAssetPicker::Construct(const FArguments& InArgs)
{
	UE_LOG(LogProjectPlacementEditor, Log, TEXT("=== SProjectAssetPicker::Construct ==="));

	// Create sub-widgets
	TSharedRef<SProjectTemplateBar> TemplateBar = SNew(SProjectTemplateBar);
	ObjectBrowser = SNew(SProjectObjectBrowser);

	// Compose into main layout with global scroll
	ChildSlot
	[
		SNew(SScrollBox)

		// Templates section (collapsible)
		+ SScrollBox::Slot()
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("Templates", "Templates"))
			.InitiallyCollapsed(false)
			.BodyContent()
			[
				TemplateBar
			]
		]

		// Objects section (includes items via ALIS.Section.Item tag filter)
		+ SScrollBox::Slot()
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("Objects", "Objects"))
			.InitiallyCollapsed(false)
			.BodyContent()
			[
				SNew(SBox)
				.MinDesiredHeight(200.0f)
				.MaxDesiredHeight(400.0f)
				[
					ObjectBrowser.ToSharedRef()
				]
			]
		]
	];

	UE_LOG(LogProjectPlacementEditor, Log, TEXT("=== SProjectAssetPicker::Construct Complete ==="));
}

#undef LOCTEXT_NAMESPACE

void SProjectAssetPicker::RefreshAssetView()
{
	if (ObjectBrowser.IsValid())
	{
		ObjectBrowser->RefreshAssets();
	}
}
