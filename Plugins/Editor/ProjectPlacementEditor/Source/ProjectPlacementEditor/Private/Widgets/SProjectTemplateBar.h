// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * Horizontal bar of template actor buttons for quick spawning.
 *
 * Displays C++ actor classes (Openables, Pickups, Loot Containers) with
 * icons. Click to spawn at editor click location.
 *
 * Single Responsibility: Template actor button UI and spawn triggering.
 */
/** Draggable template tile - click to spawn, drag to viewport. */
class SDraggableTemplateTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDraggableTemplateTile) {}
		SLATE_ARGUMENT(TSubclassOf<AActor>, ActorClass)
		SLATE_ARGUMENT(FText, DisplayName)
		SLATE_ARGUMENT(FText, Tooltip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSubclassOf<AActor> ActorClass;
	FText DisplayName;
};

class SProjectTemplateBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectTemplateBar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnTemplateClicked(TSubclassOf<AActor> ActorClass);
	void SpawnActor(TSubclassOf<AActor> ActorClass);
	FTransform GetSpawnTransform() const;
};
