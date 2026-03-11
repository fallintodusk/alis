// Copyright ALIS. All Rights Reserved.
// LayoutSlotConfig - Canvas slot configuration (SOLID: Single Responsibility)

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogLayoutSlot, Log, All);

// Forward declaration - defined in LayoutWidgetRegistry.cpp
namespace LayoutWidgetRegistry
{
	FAnchors GetAnchorPreset(const FString& AnchorName);
}

namespace LayoutSlotConfig
{
	FVector2D ParseVector2D(const TSharedPtr<FJsonObject>& JsonObject)
	{
		if (!JsonObject.IsValid())
		{
			return FVector2D::ZeroVector;
		}

		double X = 0.0, Y = 0.0;
		JsonObject->TryGetNumberField(TEXT("x"), X);
		JsonObject->TryGetNumberField(TEXT("y"), Y);

		return FVector2D(static_cast<float>(X), static_cast<float>(Y));
	}

	void ApplyCanvasPanelSlot(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& JsonObject)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (!Slot)
		{
			UE_LOG(LogLayoutSlot, Warning, TEXT("ApplyCanvasPanelSlot: Widget '%s' has no slot!"), *Widget->GetName());
			return;
		}

		// Anchor FIRST (affects how position/alignment work)
		FString AnchorStr;
		bool bIsStretched = false;
		if (JsonObject->TryGetStringField(TEXT("anchor"), AnchorStr))
		{
			FAnchors Anchors = LayoutWidgetRegistry::GetAnchorPreset(AnchorStr);
			Slot->SetAnchors(Anchors);
			UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] Anchor='%s' -> (%.2f,%.2f)-(%.2f,%.2f)"),
				*Widget->GetName(), *AnchorStr,
				Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y);

			// For stretched anchors (Fill), position/size become left-top/right-bottom offsets
			bIsStretched = (Anchors.Minimum.X != Anchors.Maximum.X) || (Anchors.Minimum.Y != Anchors.Maximum.Y);
			if (bIsStretched)
			{
				Slot->SetAutoSize(false);
				Slot->SetPosition(FVector2D(0, 0));  // Left/Top offsets
				Slot->SetSize(FVector2D(0, 0));      // Right/Bottom offsets
				UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] Stretched anchor - Pos(0,0) Size(0,0)"), *Widget->GetName());
			}
		}

		// Alignment (widget pivot point: 0,0=top-left, 0.5,0.5=center)
		const TSharedPtr<FJsonObject>* AlignmentObject;
		if (JsonObject->TryGetObjectField(TEXT("alignment"), AlignmentObject))
		{
			FVector2D Alignment = ParseVector2D(*AlignmentObject);
			Slot->SetAlignment(Alignment);
			UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] Alignment=(%.2f,%.2f)"),
				*Widget->GetName(), Alignment.X, Alignment.Y);
		}

		// Position (offset from anchor point)
		// Skip for stretched anchors (position is used as left/top offsets, already set to 0)
		const TSharedPtr<FJsonObject>* PositionObject;
		if (!bIsStretched && JsonObject->TryGetObjectField(TEXT("position"), PositionObject))
		{
			FVector2D Position = ParseVector2D(*PositionObject);
			Slot->SetPosition(Position);
			UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] Position=(%.2f,%.2f)"),
				*Widget->GetName(), Position.X, Position.Y);
		}

		// AutoSize (optional) - explicit control over whether slot sizes to content
		// Rule: If not specified, keep UMG defaults (non-breaking)
		bool bHasAutoSizeField = false;
		bool bAutoSize = false;
		if (JsonObject->TryGetBoolField(TEXT("autoSize"), bAutoSize))
		{
			bHasAutoSizeField = true;

			// Guard: autoSize=true is nonsense for stretched anchors (position/size are offsets)
			if (bIsStretched && bAutoSize)
			{
				UE_LOG(LogLayoutSlot, Warning, TEXT("  [%s] autoSize=true ignored for stretched anchors - forcing AutoSize=false"),
					*Widget->GetName());
				bAutoSize = false;
			}

			Slot->SetAutoSize(bAutoSize);
			UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] AutoSize=%s (explicit)"),
				*Widget->GetName(), bAutoSize ? TEXT("true") : TEXT("false"));
		}

		// Size handling (non-stretched only)
		const TSharedPtr<FJsonObject>* SizeObject;
		if (!bIsStretched && JsonObject->TryGetObjectField(TEXT("size"), SizeObject))
		{
			const FVector2D Size = ParseVector2D(*SizeObject);
			const bool bPartial = (Size.X > 0) != (Size.Y > 0);

			if (bHasAutoSizeField && bAutoSize && bPartial)
			{
				// Partial size + autoSize: ProcessChildren wraps in SizeBox for the
				// explicit dimension; canvas slot stays autoSize=true so the other
				// dimension sizes to content.
				Slot->SetAutoSize(true);
				UE_LOG(LogLayoutSlot, Display, TEXT("  [%s] Partial size (%.0f,%.0f) + autoSize: SizeBox wrapping"),
					*Widget->GetName(), Size.X, Size.Y);
			}
			else
			{
				if (bHasAutoSizeField && bAutoSize)
				{
					UE_LOG(LogLayoutSlot, Warning, TEXT("  [%s] Both size + autoSize=true set - forcing AutoSize=false (size wins)"),
						*Widget->GetName());
				}

				Slot->SetAutoSize(false);
				Slot->SetSize(Size);

				UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] Size=(%.2f,%.2f) AutoSize=false"),
					*Widget->GetName(), Size.X, Size.Y);
			}
		}
		// If neither size nor autoSize specified, keep UMG defaults (non-breaking)

		// Z-Order (layering for modals/overlays)
		double ZOrderValue = 0;
		if (JsonObject->TryGetNumberField(TEXT("zOrder"), ZOrderValue))
		{
			Slot->SetZOrder(static_cast<int32>(ZOrderValue));
			UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] ZOrder=%d"), *Widget->GetName(), static_cast<int32>(ZOrderValue));
		}

		// Log final slot state (Verbose - only needed for layout debugging)
		UE_LOG(LogLayoutSlot, Verbose, TEXT("  [%s] FINAL: Anchor=(%.2f,%.2f)-(%.2f,%.2f) Align=(%.2f,%.2f) Pos=(%.2f,%.2f) Size=(%.2f,%.2f) AutoSize=%s"),
			*Widget->GetName(),
			Slot->GetAnchors().Minimum.X, Slot->GetAnchors().Minimum.Y,
			Slot->GetAnchors().Maximum.X, Slot->GetAnchors().Maximum.Y,
			Slot->GetAlignment().X, Slot->GetAlignment().Y,
			Slot->GetPosition().X, Slot->GetPosition().Y,
			Slot->GetSize().X, Slot->GetSize().Y,
			Slot->GetAutoSize() ? TEXT("true") : TEXT("false"));
	}
}
