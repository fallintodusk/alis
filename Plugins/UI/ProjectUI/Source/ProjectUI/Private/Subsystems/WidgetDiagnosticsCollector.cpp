// Copyright ALIS. All Rights Reserved.
// WidgetDiagnosticsCollector.cpp - Collect widget state for diagnostics (SOLID: Single Responsibility)

#include "Subsystems/WidgetDiagnosticsCollector.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Widgets/ProjectUserWidget.h"
#include "MVVM/ProjectViewModel.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/UserWidget.h"

namespace WidgetDiagnosticsCollector
{
	FString GetAlignmentName(EHorizontalAlignment HAlign)
	{
		switch (HAlign)
		{
		case HAlign_Left:   return TEXT("Left");
		case HAlign_Center: return TEXT("Center");
		case HAlign_Right:  return TEXT("Right");
		case HAlign_Fill:   return TEXT("Fill");
		default:            return TEXT("?");
		}
	}

	FString GetAlignmentName(EVerticalAlignment VAlign)
	{
		switch (VAlign)
		{
		case VAlign_Top:    return TEXT("Top");
		case VAlign_Center: return TEXT("Center");
		case VAlign_Bottom: return TEXT("Bottom");
		case VAlign_Fill:   return TEXT("Fill");
		default:            return TEXT("?");
		}
	}

	FString GetSizeRuleName(ESlateSizeRule::Type Rule)
	{
		switch (Rule)
		{
		case ESlateSizeRule::Automatic: return TEXT("Auto");
		case ESlateSizeRule::Fill:      return TEXT("Fill");
		default:                        return TEXT("?");
		}
	}

	FString FormatPadding(const FMargin& Pad)
	{
		if (Pad.Left == Pad.Right && Pad.Top == Pad.Bottom && Pad.Left == Pad.Top)
		{
			return FString::Printf(TEXT("%.0f"), Pad.Left);
		}
		return FString::Printf(TEXT("L%.0f T%.0f R%.0f B%.0f"), Pad.Left, Pad.Top, Pad.Right, Pad.Bottom);
	}

	FString GetSlotInfoString(UWidget* Widget)
	{
		if (!Widget || !Widget->Slot)
		{
			return TEXT("<no slot>");
		}

		UPanelSlot* Slot = Widget->Slot;

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D Pos = CanvasSlot->GetPosition();
			FVector2D Size = CanvasSlot->GetSize();
			bool bAutoSize = CanvasSlot->GetAutoSize();
			FAnchors Anchors = CanvasSlot->GetAnchors();

			return FString::Printf(TEXT("Canvas (Pos=%.0f,%.0f Size=%.0f,%.0f AutoSize=%s Anch=%.1f,%.1f-%.1f,%.1f Z=%d)"),
				Pos.X, Pos.Y, Size.X, Size.Y, bAutoSize ? TEXT("Y") : TEXT("N"),
				Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y,
				CanvasSlot->GetZOrder());
		}

		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
		{
			FMargin Pad = OverlaySlot->GetPadding();
			return FString::Printf(TEXT("Overlay (H=%s V=%s Pad=%s)"),
				*GetAlignmentName(OverlaySlot->GetHorizontalAlignment()),
				*GetAlignmentName(OverlaySlot->GetVerticalAlignment()),
				*FormatPadding(Pad));
		}

		if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			FSlateChildSize Size = HBoxSlot->GetSize();
			FMargin Pad = HBoxSlot->GetPadding();
			return FString::Printf(TEXT("HBox (%s%.1f H=%s V=%s Pad=%s)"),
				*GetSizeRuleName(Size.SizeRule), Size.Value,
				*GetAlignmentName(HBoxSlot->GetHorizontalAlignment()),
				*GetAlignmentName(HBoxSlot->GetVerticalAlignment()),
				*FormatPadding(Pad));
		}

		if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			FSlateChildSize Size = VBoxSlot->GetSize();
			FMargin Pad = VBoxSlot->GetPadding();
			return FString::Printf(TEXT("VBox (%s%.1f H=%s V=%s Pad=%s)"),
				*GetSizeRuleName(Size.SizeRule), Size.Value,
				*GetAlignmentName(VBoxSlot->GetHorizontalAlignment()),
				*GetAlignmentName(VBoxSlot->GetVerticalAlignment()),
				*FormatPadding(Pad));
		}

		return Slot->GetClass()->GetName();
	}

	void CollectBasicInfo(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		Diag.WidgetName = Widget->GetName();
		Diag.ClassName = Widget->GetClass()->GetName();
		Diag.bIsVisible = Widget->IsVisible();
		Diag.bIsInViewport = Widget->IsInViewport();
		Diag.DesiredSize = Widget->GetDesiredSize();

		const FGeometry& CachedGeometry = Widget->GetCachedGeometry();
		Diag.CachedGeometrySize = CachedGeometry.GetLocalSize();
		Diag.CachedGeometryPosition = CachedGeometry.GetAbsolutePosition();
	}

	void CollectParentInfo(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		UWidget* Parent = Widget->GetParent();
		if (Parent)
		{
			Diag.ParentName = Parent->GetName();
			Diag.ParentClass = Parent->GetClass()->GetName();
		}
	}

	void CollectSlotInfo(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		Diag.SlotType = GetSlotInfoString(Widget);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			Diag.bSlotAutoSize = CanvasSlot->GetAutoSize();
			Diag.ZOrder = CanvasSlot->GetZOrder();

			FAnchors Anchors = CanvasSlot->GetAnchors();
			Diag.AnchorsInfo = FString::Printf(TEXT("Min(%.2f,%.2f) Max(%.2f,%.2f)"),
				Anchors.Minimum.X, Anchors.Minimum.Y,
				Anchors.Maximum.X, Anchors.Maximum.Y);

			if (!Anchors.Minimum.Equals(Anchors.Maximum) && !Diag.bSlotAutoSize)
			{
				FVector2D SlotSize = CanvasSlot->GetSize();
				if (SlotSize.IsNearlyZero())
				{
					Diag.Issues.Add(TEXT("Stretched anchors with zero size - widget collapsed"));
				}
			}
		}
	}

	void CollectClippingInfo(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		EWidgetClipping Clipping = Widget->GetClipping();
		switch (Clipping)
		{
		case EWidgetClipping::Inherit:
			Diag.ClippingInfo = TEXT("Inherit");
			break;
		case EWidgetClipping::ClipToBounds:
			Diag.ClippingInfo = TEXT("ClipToBounds");
			break;
		case EWidgetClipping::ClipToBoundsAlways:
			Diag.ClippingInfo = TEXT("ClipToBoundsAlways");
			break;
		case EWidgetClipping::ClipToBoundsWithoutIntersecting:
			Diag.ClippingInfo = TEXT("ClipToBoundsWithoutIntersecting");
			break;
		case EWidgetClipping::OnDemand:
			Diag.ClippingInfo = TEXT("OnDemand");
			break;
		}
	}

	void CollectTransformInfo(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		FWidgetTransform Transform = Widget->GetRenderTransform();
		if (!Transform.IsIdentity())
		{
			Diag.RenderTransformInfo = FString::Printf(TEXT("Translate(%.1f,%.1f) Scale(%.2f,%.2f) Angle=%.1f"),
				Transform.Translation.X, Transform.Translation.Y,
				Transform.Scale.X, Transform.Scale.Y,
				Transform.Angle);
		}
	}

	void CollectChildCount(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			Diag.ChildCount = PanelWidget->GetChildrenCount();
		}
	}

	void CollectParentChain(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		UWidget* CurrentParent = Widget->GetParent();
		while (CurrentParent)
		{
			FVector2D ParentSize = CurrentParent->GetCachedGeometry().GetLocalSize();
			bool bParentVisible = CurrentParent->IsVisible();

			FString ParentInfo = FString::Printf(TEXT("%s (%s) Size=%.0fx%.0f %s"),
				*CurrentParent->GetName(),
				*CurrentParent->GetClass()->GetName(),
				ParentSize.X, ParentSize.Y,
				bParentVisible ? TEXT("") : TEXT("[HIDDEN]"));

			if (UCanvasPanelSlot* ParentCanvasSlot = Cast<UCanvasPanelSlot>(CurrentParent->Slot))
			{
				FVector2D SlotSize = ParentCanvasSlot->GetSize();
				ParentInfo += FString::Printf(TEXT(" Z=%d AutoSize=%s SlotSize=(%.0f,%.0f)"),
					ParentCanvasSlot->GetZOrder(),
					ParentCanvasSlot->GetAutoSize() ? TEXT("Y") : TEXT("N"),
					SlotSize.X, SlotSize.Y);
			}

			Diag.ParentChain.Add(ParentInfo);

			// Check if parent's Slate geometry is valid
			TSharedPtr<SWidget> ParentSlate = CurrentParent->GetCachedWidget();
			bool bParentSlateValid = ParentSlate.IsValid() &&
				ParentSlate->GetTickSpaceGeometry().GetLocalSize().X > 0;

			if (!bParentVisible)
			{
				Diag.Issues.Add(FString::Printf(TEXT("Parent '%s' is HIDDEN - hiding this widget"),
					*CurrentParent->GetName()));
			}

			// Only report parent size issue if Slate layout is complete for parent
			if (ParentSize.IsNearlyZero() && bParentSlateValid)
			{
				Diag.Issues.Add(FString::Printf(TEXT("Parent '%s' has 0x0 size - hiding this widget"),
					*CurrentParent->GetName()));
			}

			CurrentParent = CurrentParent->GetParent();
		}
	}

	void DetectIssues(UWidget* Widget, FProjectWidgetDiagnostics& Diag)
	{
		// Check if Slate geometry is valid (layout pass completed)
		TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
		bool bSlateLayoutComplete = SlateWidget.IsValid() &&
			SlateWidget->GetTickSpaceGeometry().GetLocalSize().X > 0;

		if (!Diag.bIsVisible && Diag.bIsInViewport)
		{
			Diag.Issues.Add(TEXT("Widget is in viewport but not visible (Visibility setting?)"));
		}

		// Only report size issues if Slate layout is complete
		if (bSlateLayoutComplete)
		{
			if (Diag.DesiredSize.IsNearlyZero())
			{
				Diag.Issues.Add(TEXT("DesiredSize is zero - widget has no content or size"));
				Diag.Recommendations.Add(TEXT("Check if widget has content or set explicit size via SizeBox"));
			}

			if (Diag.CachedGeometrySize.IsNearlyZero() && Diag.bIsVisible)
			{
				Diag.Issues.Add(TEXT("CachedGeometry size is 0x0 - widget renders as INVISIBLE"));

				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
				{
					if (!CanvasSlot->GetAutoSize())
					{
						Diag.Issues.Add(TEXT("IN CANVAS SLOT WITHOUT AutoSize=true - THIS IS THE PROBLEM"));
						Diag.Recommendations.Add(TEXT("FIX: CanvasSlot->SetAutoSize(true) in C++ or enable in UMG"));
					}

					FVector2D SlotSize = CanvasSlot->GetSize();
					if (SlotSize.IsNearlyZero())
					{
						Diag.Issues.Add(TEXT("Slot Size is explicitly 0x0"));
					}
				}
			}
		}
		else if (Diag.CachedGeometrySize.IsNearlyZero())
		{
			// Geometry not ready yet - not an error
			Diag.Issues.Add(TEXT("Layout pending (Slate geometry not computed yet)"));
		}

		if (Diag.CachedGeometryPosition.X < -1000 || Diag.CachedGeometryPosition.Y < -1000 ||
			Diag.CachedGeometryPosition.X > 10000 || Diag.CachedGeometryPosition.Y > 10000)
		{
			Diag.Issues.Add(FString::Printf(TEXT("Widget position (%.1f, %.1f) may be OFF-SCREEN"),
				Diag.CachedGeometryPosition.X, Diag.CachedGeometryPosition.Y));
			Diag.Recommendations.Add(TEXT("Check anchor settings and offsets for resolution independence"));
		}
	}

	TMap<FString, FString> CollectViewModelProperties(UProjectViewModel* ViewModel)
	{
		TMap<FString, FString> Properties;

		if (!ViewModel)
		{
			return Properties;
		}

		for (TFieldIterator<FProperty> PropIt(ViewModel->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;

			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
			{
				continue;
			}

			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
			{
				continue;
			}

			FString ValueStr;
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ViewModel);
			Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, ViewModel, PPF_None);

			if (ValueStr.Len() > 100)
			{
				ValueStr = ValueStr.Left(100) + TEXT("...");
			}

			Properties.Add(Property->GetName(), ValueStr);
		}

		return Properties;
	}

	FProjectWidgetDiagnostics Collect(UWidget* Widget)
	{
		FProjectWidgetDiagnostics Diag;
		Diag.CaptureTime = FDateTime::Now();

		if (!Widget)
		{
			Diag.Issues.Add(TEXT("Widget is null"));
			return Diag;
		}

		CollectBasicInfo(Widget, Diag);
		CollectParentInfo(Widget, Diag);
		CollectSlotInfo(Widget, Diag);
		CollectClippingInfo(Widget, Diag);
		CollectTransformInfo(Widget, Diag);
		CollectChildCount(Widget, Diag);
		CollectParentChain(Widget, Diag);
		DetectIssues(Widget, Diag);

		return Diag;
	}

	FProjectWidgetDiagnostics CollectProjectWidget(UProjectUserWidget* Widget)
	{
		FProjectWidgetDiagnostics Diag = Collect(Widget);

		if (!Widget)
		{
			return Diag;
		}

		UProjectViewModel* VM = Widget->GetViewModel();
		if (VM)
		{
			Diag.ViewModelClass = VM->GetClass()->GetName();
			Diag.ViewModelProperties = CollectViewModelProperties(VM);
		}
		else
		{
			Diag.Issues.Add(TEXT("No ViewModel bound to widget"));
			Diag.Recommendations.Add(TEXT("Call SetViewModel() before widget is constructed"));
		}

		return Diag;
	}

	void BuildWidgetPath(UWidget* Widget, TArray<FString>& PathComponents)
	{
		if (!Widget)
		{
			return;
		}

		PathComponents.Add(Widget->GetName());

		UWidget* Parent = Widget->GetParent();
		if (Parent)
		{
			BuildWidgetPath(Parent, PathComponents);
		}
		else
		{
			if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
			{
				UObject* Outer = UserWidget->GetOuter();
				if (Outer && Outer != Widget)
				{
					PathComponents.Add(FString::Printf(TEXT("[%s]"), *Outer->GetName()));
				}
			}
		}
	}

	FString GetWidgetPath(UWidget* Widget)
	{
		if (!Widget)
		{
			return TEXT("<null>");
		}

		TArray<FString> PathComponents;
		BuildWidgetPath(Widget, PathComponents);

		FString Path;
		for (int32 i = PathComponents.Num() - 1; i >= 0; --i)
		{
			if (!Path.IsEmpty())
			{
				Path += TEXT(" > ");
			}
			Path += PathComponents[i];
		}

		return Path;
	}
}
