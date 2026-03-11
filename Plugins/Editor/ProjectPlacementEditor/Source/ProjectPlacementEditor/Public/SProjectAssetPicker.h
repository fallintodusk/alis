// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SProjectObjectBrowser;

/**
 * Main placement panel widget - composition of specialized sub-widgets.
 *
 * Layout:
 * - Templates section: SProjectTemplateBar (actor spawn buttons)
 * - Objects section: SProjectObjectBrowser (folder tree + asset tiles, with tag filters for items)
 *
 * Single Responsibility: Compose sub-widgets into panel layout.
 * Open for extension: Add new sections by adding new sub-widgets.
 */
class SProjectAssetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectAssetPicker) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refresh all asset browsers (call after generating new assets). */
	void RefreshAssetView();

private:
	TSharedPtr<SProjectObjectBrowser> ObjectBrowser;
};
