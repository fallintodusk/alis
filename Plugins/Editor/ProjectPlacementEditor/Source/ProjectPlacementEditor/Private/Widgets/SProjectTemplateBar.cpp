// Copyright ALIS. All Rights Reserved.

#include "Widgets/SProjectTemplateBar.h"
#include "ProjectPlacementEditor.h"

#include "Template/Openable/HingedOpenable.h"
#include "Template/Openable/SlidingOpenable.h"
#include "Template/Interactable/InteractableActor.h"

#include "Editor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/AppStyle.h"
#include "DragAndDrop/ClassDragDropOp.h"
#include "SourceCodeNavigation.h"

#define LOCTEXT_NAMESPACE "SProjectTemplateBar"

// ============================================================================
// SDraggableTemplateTile - supports click and drag-to-viewport
// ============================================================================

void SDraggableTemplateTile::Construct(const FArguments& InArgs)
{
	ActorClass = InArgs._ActorClass;
	DisplayName = InArgs._DisplayName;

	const FSlateBrush* Icon = FSlateIconFinder::FindIconBrushForClass(ActorClass);
	constexpr float IconSize = 64.0f;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		.ToolTipText(InArgs._Tooltip)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0, 4, 0, 4)
			[
				SNew(SImage)
				.Image(Icon)
				.DesiredSizeOverride(FVector2D(IconSize, IconSize))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(DisplayName)
				.Justification(ETextJustify::Center)
			]
		]
	];
}

FReply SDraggableTemplateTile::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}
	return FReply::Unhandled();
}

FReply SDraggableTemplateTile::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ActorClass)
	{
		FSourceCodeNavigation::NavigateToClass(ActorClass);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDraggableTemplateTile::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ActorClass)
	{
		TSharedRef<FClassDragDropOp> DragOp = FClassDragDropOp::New(MakeWeakObjectPtr(ActorClass.Get()));
		return FReply::Handled().BeginDragDrop(DragOp);
	}
	return FReply::Unhandled();
}

// ============================================================================
// SProjectTemplateBar
// ============================================================================

void SProjectTemplateBar::Construct(const FArguments& InArgs)
{
	struct FTemplateInfo
	{
		TSubclassOf<AActor> ActorClass;
		FText DisplayName;
		FText Tooltip;
	};

	// NOTE: Use "Replace with Definition" submenu for data-driven actors
	// (capability-based system via ObjectDefinition JSONs)
	const TArray<FTemplateInfo> Templates = {
		{ AInteractableActor::StaticClass(), LOCTEXT("Interactable", "Interactable"), LOCTEXT("InteractableTip", "Generic interactable object") },
		{ AHingedOpenable::StaticClass(), LOCTEXT("Hinged", "Hinged Openable"), LOCTEXT("HingedTip", "Door, window, hatch") },
		{ ASlidingOpenable::StaticClass(), LOCTEXT("Sliding", "Sliding Openable"), LOCTEXT("SlidingTip", "Sliding door, drawer") },
	};

	TSharedRef<SHorizontalBox> ButtonRow = SNew(SHorizontalBox);

	for (const FTemplateInfo& Info : Templates)
	{
		ButtonRow->AddSlot()
		.AutoWidth()
		.Padding(4.0f)
		[
			SNew(SDraggableTemplateTile)
			.ActorClass(Info.ActorClass)
			.DisplayName(Info.DisplayName)
			.Tooltip(Info.Tooltip)
		];
	}

	ChildSlot
	[
		ButtonRow
	];

	UE_LOG(LogProjectPlacementEditor, Log, TEXT("SProjectTemplateBar: %d templates"), Templates.Num());
}

#undef LOCTEXT_NAMESPACE

FReply SProjectTemplateBar::OnTemplateClicked(TSubclassOf<AActor> ActorClass)
{
	UE_LOG(LogProjectPlacementEditor, Log, TEXT("Template clicked: %s"),
		ActorClass ? *ActorClass->GetName() : TEXT("null"));
	SpawnActor(ActorClass);
	return FReply::Handled();
}

void SProjectTemplateBar::SpawnActor(TSubclassOf<AActor> ActorClass)
{
	if (!ActorClass)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("SpawnActor: No editor world"));
		return;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transactional;

	AActor* Actor = World->SpawnActor<AActor>(ActorClass, GetSpawnTransform(), Params);
	if (Actor)
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Actor, true, true);
		UE_LOG(LogProjectPlacementEditor, Log, TEXT("  Spawned: %s"), *Actor->GetName());
	}
}

FTransform SProjectTemplateBar::GetSpawnTransform() const
{
	FVector Loc = GEditor->ClickLocation;
	if (Loc.IsNearlyZero())
	{
		Loc = FVector(0.0f, 0.0f, 100.0f);
	}
	return FTransform(Loc);
}
