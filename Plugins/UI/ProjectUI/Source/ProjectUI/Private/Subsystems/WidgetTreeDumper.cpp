// Copyright ALIS. All Rights Reserved.
// WidgetTreeDumper.cpp - Widget tree dump functionality (text and JSON formats)
// Extracted from ProjectUIDebugSubsystem.cpp for SOLID compliance

#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "WidgetDiagnosticsCollector.h"
#include "Widgets/ProjectUserWidget.h"
#include "MVVM/ProjectViewModel.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace WidgetTreeDumperPrivate
{
	UWidget* FindFirstWidgetByName(UWidget* Root, const FString& Filter)
	{
		if (!Root)
		{
			return nullptr;
		}

		if (Root->GetName().Contains(Filter))
		{
			return Root;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				if (UWidget* Found = FindFirstWidgetByName(Panel->GetChildAt(i), Filter))
				{
					return Found;
				}
			}
		}

		if (UUserWidget* UserWidget = Cast<UUserWidget>(Root))
		{
			if (UWidget* RootWidget = UserWidget->GetRootWidget())
			{
				if (UWidget* Found = FindFirstWidgetByName(RootWidget, Filter))
				{
					return Found;
				}
			}
		}

		return nullptr;
	}

	void GetDisplayMetricsSafe(FDisplayMetrics& OutMetrics)
	{
		OutMetrics = FDisplayMetrics();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetDisplayMetrics(OutMetrics);
		}
		else
		{
			OutMetrics.TitleSafePaddingSize = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

	FString GetHorizontalAlignmentString(EHorizontalAlignment Align)
	{
		switch (Align)
		{
		case HAlign_Left:   return TEXT("Left");
		case HAlign_Center: return TEXT("Center");
		case HAlign_Right:  return TEXT("Right");
		case HAlign_Fill:   return TEXT("Fill");
		default:            return TEXT("Unknown");
		}
	}

	FString GetVerticalAlignmentString(EVerticalAlignment Align)
	{
		switch (Align)
		{
		case VAlign_Top:    return TEXT("Top");
		case VAlign_Center: return TEXT("Center");
		case VAlign_Bottom: return TEXT("Bottom");
		case VAlign_Fill:   return TEXT("Fill");
		default:            return TEXT("Unknown");
		}
	}

	FString GetSizeRuleString(ESlateSizeRule::Type Rule)
	{
		switch (Rule)
		{
		case ESlateSizeRule::Automatic: return TEXT("Auto");
		case ESlateSizeRule::Fill:      return TEXT("Fill");
		default:                        return TEXT("Unknown");
		}
	}

	void SetPaddingJson(const TSharedPtr<FJsonObject>& Parent, const FMargin& Padding)
	{
		TSharedPtr<FJsonObject> PaddingJson = MakeShareable(new FJsonObject);
		PaddingJson->SetNumberField(TEXT("left"), Padding.Left);
		PaddingJson->SetNumberField(TEXT("top"), Padding.Top);
		PaddingJson->SetNumberField(TEXT("right"), Padding.Right);
		PaddingJson->SetNumberField(TEXT("bottom"), Padding.Bottom);
		Parent->SetObjectField(TEXT("padding"), PaddingJson);
	}

	void SetAlignmentJson(const TSharedPtr<FJsonObject>& Parent, const FString& Horizontal, const FString& Vertical)
	{
		TSharedPtr<FJsonObject> AlignmentJson = MakeShareable(new FJsonObject);
		AlignmentJson->SetStringField(TEXT("horizontal"), Horizontal);
		AlignmentJson->SetStringField(TEXT("vertical"), Vertical);
		Parent->SetObjectField(TEXT("alignment"), AlignmentJson);
	}
}

// =============================================================================
// Basic Widget Tree Dump (to log)
// =============================================================================

void UProjectUIDebugSubsystem::DumpWidgetTree() const
{
	UE_LOG(LogProjectUIDebug, Display, TEXT(""));
	UE_LOG(LogProjectUIDebug, Display, TEXT("---------------------------"));
	UE_LOG(LogProjectUIDebug, Display, TEXT("==== WIDGET TREE DUMP ===="));
	UE_LOG(LogProjectUIDebug, Display, TEXT("---------------------------"));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectUIDebug, Warning, TEXT("No world context available"));
		return;
	}

	// Determinism inputs
	UE_LOG(LogProjectUIDebug, Display, TEXT("==== DETERMINISM INPUTS ===="));

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (UGameViewportClient* ViewportClient = World->GetGameViewport())
	{
		ViewportClient->GetViewportSize(ViewportSize);
	}
	UE_LOG(LogProjectUIDebug, Display, TEXT("  Viewport:   %.0f x %.0f"), ViewportSize.X, ViewportSize.Y);

	float DPIScale = UWidgetLayoutLibrary::GetViewportScale(World);
	UE_LOG(LogProjectUIDebug, Display, TEXT("  DPIScale:   %.2f"), DPIScale);

	FDisplayMetrics DisplayMetrics;
	WidgetTreeDumperPrivate::GetDisplayMetricsSafe(DisplayMetrics);
	FMargin SafeZone = FMargin(0);
	SafeZone.Left = DisplayMetrics.TitleSafePaddingSize.X;
	SafeZone.Top = DisplayMetrics.TitleSafePaddingSize.Y;
	SafeZone.Right = DisplayMetrics.TitleSafePaddingSize.X;
	SafeZone.Bottom = DisplayMetrics.TitleSafePaddingSize.Y;
	UE_LOG(LogProjectUIDebug, Display, TEXT("  SafeZone:   L%.0f T%.0f R%.0f B%.0f"), SafeZone.Left, SafeZone.Top, SafeZone.Right, SafeZone.Bottom);

	float AspectRatio = ViewportSize.Y > 0 ? ViewportSize.X / ViewportSize.Y : 0;
	UE_LOG(LogProjectUIDebug, Display, TEXT("  Aspect:     %.2f:1"), AspectRatio);

	UE_LOG(LogProjectUIDebug, Display, TEXT("---------------------------"));
	UE_LOG(LogProjectUIDebug, Display, TEXT(""));
	UE_LOG(LogProjectUIDebug, Display, TEXT("  Format: Name (Class) [D=WxH A=WxH] Slot | [FLAGS]"));
	UE_LOG(LogProjectUIDebug, Display, TEXT("  Flags: ZERO=0x0 size, HIDDEN=not visible, NOVM=no ViewModel, NOHIT=not interactive"));

	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!Widget || Widget->GetWorld() != World || !Widget->IsInViewport())
		{
			continue;
		}

		UE_LOG(LogProjectUIDebug, Display, TEXT(""));
		UE_LOG(LogProjectUIDebug, Display, TEXT("---- VIEWPORT WIDGET: %s ----"), *Widget->GetName());
		DumpWidgetTreeRecursive(Widget, 0);
	}

	UE_LOG(LogProjectUIDebug, Display, TEXT(""));
	UE_LOG(LogProjectUIDebug, Display, TEXT("---------------------------"));
	UE_LOG(LogProjectUIDebug, Display, TEXT("TIP: Widget Reflector (Ctrl+Shift+W) for interactive debugging"));
	UE_LOG(LogProjectUIDebug, Display, TEXT(""));
}

void UProjectUIDebugSubsystem::DumpWidgetTreeRecursive(UWidget* Widget, int32 Depth) const
{
	if (!Widget)
	{
		return;
	}

	FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
	FVector2D DesiredSize = Widget->GetDesiredSize();
	FVector2D ArrangedSize = Widget->GetCachedGeometry().GetLocalSize();
	bool bVisible = Widget->IsVisible();

	TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
	bool bGeometryValid = SlateWidget.IsValid() && SlateWidget->GetTickSpaceGeometry().GetLocalSize().X > 0;

	// Build flags
	TArray<FString> Flags;
	if (ArrangedSize.IsNearlyZero() && bGeometryValid)
	{
		Flags.Add(TEXT("ZERO!"));
	}
	else if (ArrangedSize.IsNearlyZero() && !bGeometryValid)
	{
		Flags.Add(TEXT("PENDING"));
	}
	if (!bVisible)
	{
		Flags.Add(TEXT("HIDDEN"));
	}

	ESlateVisibility SlateVis = Widget->GetVisibility();
	if (SlateVis == ESlateVisibility::HitTestInvisible || SlateVis == ESlateVisibility::SelfHitTestInvisible)
	{
		Flags.Add(TEXT("NOHIT"));
	}

	EWidgetClipping Clipping = Widget->GetClipping();
	if (Clipping == EWidgetClipping::ClipToBounds || Clipping == EWidgetClipping::ClipToBoundsAlways)
	{
		Flags.Add(TEXT("CLIP"));
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		if (!CanvasSlot->GetAutoSize())
		{
			FVector2D SlotSize = CanvasSlot->GetSize();
			if (SlotSize.IsNearlyZero())
			{
				Flags.Add(TEXT("SlotSize=0!"));
			}
		}
	}

	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		if (!ProjectWidget->GetViewModel())
		{
			Flags.Add(TEXT("NOVM"));
		}
	}

	FString FlagsStr = Flags.Num() > 0 ? FString::Printf(TEXT(" [%s]"), *FString::Join(Flags, TEXT(", "))) : TEXT("");
	FString SlotInfo = WidgetDiagnosticsCollector::GetSlotInfoString(Widget);

	UE_LOG(LogProjectUIDebug, Display, TEXT("%s%s (%s) [D=%.0fx%.0f A=%.0fx%.0f] %s%s"),
		*Indent,
		*Widget->GetName(),
		*Widget->GetClass()->GetName(),
		DesiredSize.X, DesiredSize.Y,
		ArrangedSize.X, ArrangedSize.Y,
		*SlotInfo,
		*FlagsStr);

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			DumpWidgetTreeRecursive(Panel->GetChildAt(i), Depth + 1);
		}
	}

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UWidget* RootWidget = UserWidget->GetRootWidget())
		{
			DumpWidgetTreeRecursive(RootWidget, Depth + 1);
		}
	}
}

// =============================================================================
// Extended Dump (file output, JSON format, filters)
// =============================================================================

bool UProjectUIDebugSubsystem::DumpWidgetTreeEx(const FString& OutPath, const FString& Format, const FString& Filter) const
{
	using namespace WidgetTreeDumperPrivate;

	const bool bJson = Format.Equals(TEXT("json"), ESearchCase::IgnoreCase);
	const bool bToFile = !OutPath.IsEmpty();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectUIDebug, Warning, TEXT("No world context available"));
		return false;
	}

	UE_LOG(LogProjectUIDebug, Display, TEXT(""));
	UE_LOG(LogProjectUIDebug, Display, TEXT("===BEGIN_WIDGET_TREE_DUMP==="));

	if (bJson)
	{
		TSharedPtr<FJsonObject> RootJson = MakeShareable(new FJsonObject);
		RootJson->SetObjectField(TEXT("determinism"), GetDeterminismInputsJson());

		TArray<TSharedPtr<FJsonValue>> WidgetsArray;
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Widget = *It;
			if (!Widget || Widget->GetWorld() != World || !Widget->IsInViewport())
			{
				continue;
			}

			UWidget* DumpRoot = Widget;
			if (!Filter.IsEmpty())
			{
				DumpRoot = FindFirstWidgetByName(Widget, Filter);
				if (!DumpRoot)
				{
					continue;
				}
			}

			WidgetsArray.Add(MakeShareable(new FJsonValueObject(BuildWidgetTreeJson(DumpRoot))));
		}
		RootJson->SetArrayField(TEXT("widgets"), WidgetsArray);

		FString JsonStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
		FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer);

		if (bToFile)
		{
			FString FullPath = OutPath;
			if (FPaths::IsRelative(FullPath))
			{
				FullPath = FPaths::ProjectSavedDir() / FullPath;
			}
			FPaths::NormalizeFilename(FullPath);

			FString Dir = FPaths::GetPath(FullPath);
			IFileManager::Get().MakeDirectory(*Dir, true);

			if (FFileHelper::SaveStringToFile(JsonStr, *FullPath))
			{
				UE_LOG(LogProjectUIDebug, Display, TEXT("Widget tree saved to: %s"), *FullPath);
			}
			else
			{
				UE_LOG(LogProjectUIDebug, Error, TEXT("Failed to write to: %s"), *FullPath);
				return false;
			}
		}
		else
		{
			TArray<FString> Lines;
			JsonStr.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				UE_LOG(LogProjectUIDebug, Display, TEXT("%s"), *Line);
			}
		}
	}
	else
	{
		TArray<FString> Lines;

		Lines.Add(TEXT("==== DETERMINISM INPUTS ===="));

		FVector2D ViewportSize = FVector2D::ZeroVector;
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			ViewportClient->GetViewportSize(ViewportSize);
		}
		Lines.Add(FString::Printf(TEXT("  Viewport:   %.0f x %.0f"), ViewportSize.X, ViewportSize.Y));

		float DPIScale = UWidgetLayoutLibrary::GetViewportScale(World);
		Lines.Add(FString::Printf(TEXT("  DPIScale:   %.2f"), DPIScale));

		FDisplayMetrics DisplayMetrics;
		GetDisplayMetricsSafe(DisplayMetrics);
		Lines.Add(FString::Printf(TEXT("  SafeZone:   L%.0f T%.0f R%.0f B%.0f"),
			DisplayMetrics.TitleSafePaddingSize.X, DisplayMetrics.TitleSafePaddingSize.Y,
			DisplayMetrics.TitleSafePaddingSize.X, DisplayMetrics.TitleSafePaddingSize.Y));

		float AspectRatio = ViewportSize.Y > 0 ? ViewportSize.X / ViewportSize.Y : 0;
		Lines.Add(FString::Printf(TEXT("  Aspect:     %.2f:1"), AspectRatio));
		Lines.Add(TEXT("---------------------------"));
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("  Format: Name (Class) [D=WxH A=WxH] Slot | [FLAGS]"));
		Lines.Add(TEXT("  Flags: ZERO=0x0 size, HIDDEN=not visible, NOVM=no ViewModel, NOHIT=not interactive"));

		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Widget = *It;
			if (!Widget || Widget->GetWorld() != World || !Widget->IsInViewport())
			{
				continue;
			}

			UWidget* DumpRoot = Widget;
			if (!Filter.IsEmpty())
			{
				DumpRoot = FindFirstWidgetByName(Widget, Filter);
				if (!DumpRoot)
				{
					continue;
				}
			}

			Lines.Add(TEXT(""));
			if (!Filter.IsEmpty() && DumpRoot != Widget)
			{
				Lines.Add(FString::Printf(TEXT("---- VIEWPORT WIDGET: %s (filter match: %s) ----"), *Widget->GetName(), *DumpRoot->GetName()));
			}
			else
			{
				Lines.Add(FString::Printf(TEXT("---- VIEWPORT WIDGET: %s ----"), *Widget->GetName()));
			}
			DumpWidgetTreeRecursiveToArray(DumpRoot, 0, Lines);
		}

		FString Output = FString::Join(Lines, TEXT("\n"));

		if (bToFile)
		{
			FString FullPath = OutPath;
			if (FPaths::IsRelative(FullPath))
			{
				FullPath = FPaths::ProjectSavedDir() / FullPath;
			}
			FPaths::NormalizeFilename(FullPath);

			FString Dir = FPaths::GetPath(FullPath);
			IFileManager::Get().MakeDirectory(*Dir, true);

			if (FFileHelper::SaveStringToFile(Output, *FullPath))
			{
				UE_LOG(LogProjectUIDebug, Display, TEXT("Widget tree saved to: %s"), *FullPath);
			}
			else
			{
				UE_LOG(LogProjectUIDebug, Error, TEXT("Failed to write to: %s"), *FullPath);
				return false;
			}
		}
		else
		{
			for (const FString& Line : Lines)
			{
				UE_LOG(LogProjectUIDebug, Display, TEXT("%s"), *Line);
			}
		}
	}

	UE_LOG(LogProjectUIDebug, Display, TEXT("===END_WIDGET_TREE_DUMP==="));
	UE_LOG(LogProjectUIDebug, Display, TEXT(""));

	return true;
}

void UProjectUIDebugSubsystem::DumpWidgetTreeRecursiveToArray(UWidget* Widget, int32 Depth, TArray<FString>& OutLines) const
{
	if (!Widget)
	{
		return;
	}

	FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
	FVector2D DesiredSize = Widget->GetDesiredSize();
	FVector2D ArrangedSize = Widget->GetCachedGeometry().GetLocalSize();
	bool bVisible = Widget->IsVisible();

	TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
	bool bGeometryValid = SlateWidget.IsValid() && SlateWidget->GetTickSpaceGeometry().GetLocalSize().X > 0;

	TArray<FString> Flags;
	if (ArrangedSize.IsNearlyZero() && bGeometryValid)
	{
		Flags.Add(TEXT("ZERO!"));
	}
	else if (ArrangedSize.IsNearlyZero() && !bGeometryValid)
	{
		Flags.Add(TEXT("PENDING"));
	}
	if (!bVisible)
	{
		Flags.Add(TEXT("HIDDEN"));
	}

	ESlateVisibility SlateVis = Widget->GetVisibility();
	if (SlateVis == ESlateVisibility::HitTestInvisible || SlateVis == ESlateVisibility::SelfHitTestInvisible)
	{
		Flags.Add(TEXT("NOHIT"));
	}

	EWidgetClipping Clipping = Widget->GetClipping();
	if (Clipping == EWidgetClipping::ClipToBounds || Clipping == EWidgetClipping::ClipToBoundsAlways)
	{
		Flags.Add(TEXT("CLIP"));
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		if (!CanvasSlot->GetAutoSize())
		{
			FVector2D SlotSize = CanvasSlot->GetSize();
			if (SlotSize.IsNearlyZero())
			{
				Flags.Add(TEXT("SlotSize=0!"));
			}
		}
	}

	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		if (!ProjectWidget->GetViewModel())
		{
			Flags.Add(TEXT("NOVM"));
		}
	}

	FString FlagsStr = Flags.Num() > 0 ? FString::Printf(TEXT(" [%s]"), *FString::Join(Flags, TEXT(", "))) : TEXT("");
	FString SlotInfo = WidgetDiagnosticsCollector::GetSlotInfoString(Widget);

	OutLines.Add(FString::Printf(TEXT("%s%s (%s) [D=%.0fx%.0f A=%.0fx%.0f] %s%s"),
		*Indent,
		*Widget->GetName(),
		*Widget->GetClass()->GetName(),
		DesiredSize.X, DesiredSize.Y,
		ArrangedSize.X, ArrangedSize.Y,
		*SlotInfo,
		*FlagsStr));

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			DumpWidgetTreeRecursiveToArray(Panel->GetChildAt(i), Depth + 1, OutLines);
		}
	}

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UWidget* RootWidget = UserWidget->GetRootWidget())
		{
			DumpWidgetTreeRecursiveToArray(RootWidget, Depth + 1, OutLines);
		}
	}
}

// =============================================================================
// JSON Building
// =============================================================================

TSharedPtr<FJsonObject> UProjectUIDebugSubsystem::GetDeterminismInputsJson() const
{
	using namespace WidgetTreeDumperPrivate;

	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);

	UWorld* World = GetWorld();
	if (!World)
	{
		return Json;
	}

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (UGameViewportClient* ViewportClient = World->GetGameViewport())
	{
		ViewportClient->GetViewportSize(ViewportSize);
	}
	Json->SetNumberField(TEXT("viewportWidth"), ViewportSize.X);
	Json->SetNumberField(TEXT("viewportHeight"), ViewportSize.Y);
	Json->SetNumberField(TEXT("dpiScale"), UWidgetLayoutLibrary::GetViewportScale(World));

	FDisplayMetrics DisplayMetrics;
	GetDisplayMetricsSafe(DisplayMetrics);
	Json->SetNumberField(TEXT("safeZoneLeft"), DisplayMetrics.TitleSafePaddingSize.X);
	Json->SetNumberField(TEXT("safeZoneTop"), DisplayMetrics.TitleSafePaddingSize.Y);
	Json->SetNumberField(TEXT("aspectRatio"), ViewportSize.Y > 0 ? ViewportSize.X / ViewportSize.Y : 0);

	return Json;
}

TSharedPtr<FJsonObject> UProjectUIDebugSubsystem::BuildWidgetTreeJson(UWidget* Widget) const
{
	using namespace WidgetTreeDumperPrivate;

	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);

	if (!Widget)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Widget->GetName());
	Json->SetStringField(TEXT("class"), Widget->GetClass()->GetName());

	FVector2D DesiredSize = Widget->GetDesiredSize();
	FVector2D ArrangedSize = Widget->GetCachedGeometry().GetLocalSize();
	Json->SetNumberField(TEXT("width"), ArrangedSize.X);
	Json->SetNumberField(TEXT("height"), ArrangedSize.Y);

	TSharedPtr<FJsonObject> DesiredSizeJson = MakeShareable(new FJsonObject);
	DesiredSizeJson->SetNumberField(TEXT("width"), DesiredSize.X);
	DesiredSizeJson->SetNumberField(TEXT("height"), DesiredSize.Y);
	Json->SetObjectField(TEXT("desiredSize"), DesiredSizeJson);

	TSharedPtr<FJsonObject> ArrangedSizeJson = MakeShareable(new FJsonObject);
	ArrangedSizeJson->SetNumberField(TEXT("width"), ArrangedSize.X);
	ArrangedSizeJson->SetNumberField(TEXT("height"), ArrangedSize.Y);
	Json->SetObjectField(TEXT("arrangedSize"), ArrangedSizeJson);

	Json->SetBoolField(TEXT("visible"), Widget->IsVisible());

	ESlateVisibility SlateVis = Widget->GetVisibility();
	Json->SetBoolField(TEXT("hitTestable"), SlateVis == ESlateVisibility::Visible);

	const FString SlotString = WidgetDiagnosticsCollector::GetSlotInfoString(Widget);
	Json->SetStringField(TEXT("slot"), SlotString);
	Json->SetStringField(TEXT("slotString"), SlotString);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		Json->SetStringField(TEXT("slotType"), TEXT("Canvas"));

		FVector2D Position = CanvasSlot->GetPosition();
		TSharedPtr<FJsonObject> PositionJson = MakeShareable(new FJsonObject);
		PositionJson->SetNumberField(TEXT("x"), Position.X);
		PositionJson->SetNumberField(TEXT("y"), Position.Y);
		Json->SetObjectField(TEXT("position"), PositionJson);

		FVector2D SlotSize = CanvasSlot->GetSize();
		TSharedPtr<FJsonObject> SlotSizeJson = MakeShareable(new FJsonObject);
		SlotSizeJson->SetNumberField(TEXT("width"), SlotSize.X);
		SlotSizeJson->SetNumberField(TEXT("height"), SlotSize.Y);
		Json->SetObjectField(TEXT("slotSize"), SlotSizeJson);

		Json->SetBoolField(TEXT("autoSize"), CanvasSlot->GetAutoSize());

		FAnchors Anchors = CanvasSlot->GetAnchors();
		TSharedPtr<FJsonObject> AnchorsJson = MakeShareable(new FJsonObject);
		AnchorsJson->SetNumberField(TEXT("minX"), Anchors.Minimum.X);
		AnchorsJson->SetNumberField(TEXT("minY"), Anchors.Minimum.Y);
		AnchorsJson->SetNumberField(TEXT("maxX"), Anchors.Maximum.X);
		AnchorsJson->SetNumberField(TEXT("maxY"), Anchors.Maximum.Y);
		Json->SetObjectField(TEXT("anchors"), AnchorsJson);
		Json->SetNumberField(TEXT("zOrder"), CanvasSlot->GetZOrder());

		FVector2D Alignment = CanvasSlot->GetAlignment();
		TSharedPtr<FJsonObject> AlignmentJson = MakeShareable(new FJsonObject);
		AlignmentJson->SetNumberField(TEXT("x"), Alignment.X);
		AlignmentJson->SetNumberField(TEXT("y"), Alignment.Y);
		Json->SetObjectField(TEXT("alignment"), AlignmentJson);
	}
	else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Widget->Slot))
	{
		Json->SetStringField(TEXT("slotType"), TEXT("Overlay"));
		SetPaddingJson(Json, OverlaySlot->GetPadding());
		SetAlignmentJson(Json,
			GetHorizontalAlignmentString(OverlaySlot->GetHorizontalAlignment()),
			GetVerticalAlignmentString(OverlaySlot->GetVerticalAlignment()));
	}
	else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		Json->SetStringField(TEXT("slotType"), TEXT("HorizontalBox"));
		SetPaddingJson(Json, HBoxSlot->GetPadding());
		SetAlignmentJson(Json,
			GetHorizontalAlignmentString(HBoxSlot->GetHorizontalAlignment()),
			GetVerticalAlignmentString(HBoxSlot->GetVerticalAlignment()));
		Json->SetStringField(TEXT("sizeRule"), GetSizeRuleString(HBoxSlot->GetSize().SizeRule));
	}
	else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		Json->SetStringField(TEXT("slotType"), TEXT("VerticalBox"));
		SetPaddingJson(Json, VBoxSlot->GetPadding());
		SetAlignmentJson(Json,
			GetHorizontalAlignmentString(VBoxSlot->GetHorizontalAlignment()),
			GetVerticalAlignmentString(VBoxSlot->GetVerticalAlignment()));
		Json->SetStringField(TEXT("sizeRule"), GetSizeRuleString(VBoxSlot->GetSize().SizeRule));
	}
	else if (Widget->Slot)
	{
		Json->SetStringField(TEXT("slotType"), Widget->Slot->GetClass()->GetName());
	}
	else
	{
		Json->SetStringField(TEXT("slotType"), TEXT("None"));
	}

	EWidgetClipping Clipping = Widget->GetClipping();
	Json->SetBoolField(TEXT("clipping"),
		Clipping == EWidgetClipping::ClipToBounds || Clipping == EWidgetClipping::ClipToBoundsAlways);

	// Issues
	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
	bool bGeometryValid = SlateWidget.IsValid() && SlateWidget->GetTickSpaceGeometry().GetLocalSize().X > 0;

	if (ArrangedSize.IsNearlyZero() && bGeometryValid && Widget->IsVisible())
	{
		IssuesArray.Add(MakeShareable(new FJsonValueString(TEXT("ZERO_SIZE"))));
	}

	if (!Widget->IsVisible())
	{
		IssuesArray.Add(MakeShareable(new FJsonValueString(TEXT("HIDDEN"))));
	}

	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		if (!ProjectWidget->GetViewModel())
		{
			IssuesArray.Add(MakeShareable(new FJsonValueString(TEXT("NO_VIEWMODEL"))));
		}
	}

	if (SlateVis == ESlateVisibility::Visible && !Widget->IsVisible())
	{
		IssuesArray.Add(MakeShareable(new FJsonValueString(TEXT("HIT_TEST_INVISIBLE"))));
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		FAnchors Anchors = CanvasSlot->GetAnchors();
		FVector2D SlotSize = CanvasSlot->GetSize();
		FVector2D Position = CanvasSlot->GetPosition();
		bool bAutoSize = CanvasSlot->GetAutoSize();
		bool bIsStretchedAnchors = !Anchors.Minimum.Equals(Anchors.Maximum);

		if (!bAutoSize && SlotSize.IsNearlyZero() && !bIsStretchedAnchors && Widget->IsVisible())
		{
			IssuesArray.Add(MakeShareable(new FJsonValueString(TEXT("CANVAS_NO_SIZE"))));
		}

		if (bIsStretchedAnchors && (!Position.IsNearlyZero() || !SlotSize.IsNearlyZero()))
		{
			IssuesArray.Add(MakeShareable(new FJsonValueString(TEXT("FILL_WITH_OFFSET"))));
		}
	}

	Json->SetArrayField(TEXT("issues"), IssuesArray);

	// Children
	TArray<TSharedPtr<FJsonValue>> ChildrenArray;

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			ChildrenArray.Add(MakeShareable(new FJsonValueObject(BuildWidgetTreeJson(Panel->GetChildAt(i)))));
		}
	}

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UWidget* RootWidget = UserWidget->GetRootWidget())
		{
			ChildrenArray.Add(MakeShareable(new FJsonValueObject(BuildWidgetTreeJson(RootWidget))));
		}
	}

	if (ChildrenArray.Num() > 0)
	{
		Json->SetArrayField(TEXT("children"), ChildrenArray);
	}

	return Json;
}
