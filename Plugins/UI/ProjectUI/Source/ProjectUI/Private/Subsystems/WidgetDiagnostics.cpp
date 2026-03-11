// Copyright ALIS. All Rights Reserved.
// WidgetDiagnostics.cpp - FProjectWidgetDiagnostics formatting (SOLID: Single Responsibility)

#include "Subsystems/ProjectUIDebugSubsystem.h"

FString FProjectWidgetDiagnostics::ToString() const
{
	FString Result;

	Result += TEXT("\n");
	Result += TEXT("+====================================================================+\n");
	Result += TEXT("|                    WIDGET DIAGNOSTICS                              |\n");
	Result += TEXT("+====================================================================+\n");

	Result += FString::Printf(TEXT("| Widget: %s\n"), *WidgetName);
	Result += FString::Printf(TEXT("| Class:  %s\n"), *ClassName);
	Result += FString::Printf(TEXT("| Path:   %s\n"), *WidgetPath);

	Result += TEXT("+====================================================================+\n");
	Result += TEXT("| VISIBILITY & SIZE\n");
	Result += TEXT("+--------------------------------------------------------------------+\n");

	Result += FString::Printf(TEXT("|   IsVisible:     %s\n"), bIsVisible ? TEXT("YES") : TEXT("NO [HIDDEN]"));
	Result += FString::Printf(TEXT("|   IsInViewport:  %s\n"), bIsInViewport ? TEXT("YES") : TEXT("NO [NOT IN VIEWPORT]"));
	Result += FString::Printf(TEXT("|   DesiredSize:   %.1f x %.1f %s\n"),
		DesiredSize.X, DesiredSize.Y,
		DesiredSize.IsNearlyZero() ? TEXT("[ZERO - NO CONTENT?]") : TEXT(""));
	Result += FString::Printf(TEXT("|   CachedSize:    %.1f x %.1f %s\n"),
		CachedGeometrySize.X, CachedGeometrySize.Y,
		CachedGeometrySize.IsNearlyZero() ? TEXT("[ZERO - INVISIBLE!]") : TEXT(""));
	Result += FString::Printf(TEXT("|   ScreenPos:     (%.1f, %.1f)\n"),
		CachedGeometryPosition.X, CachedGeometryPosition.Y);

	Result += TEXT("+====================================================================+\n");
	Result += TEXT("| SLOT & LAYOUT\n");
	Result += TEXT("+--------------------------------------------------------------------+\n");

	Result += FString::Printf(TEXT("|   SlotType:      %s\n"), *SlotType);
	Result += FString::Printf(TEXT("|   AutoSize:      %s %s\n"),
		bSlotAutoSize ? TEXT("YES") : TEXT("NO"),
		(!bSlotAutoSize && SlotType.Contains(TEXT("Canvas"))) ? TEXT("[CRITICAL - NEEDS AutoSize=true!]") : TEXT(""));
	Result += FString::Printf(TEXT("|   ZOrder:        %d\n"), ZOrder);

	if (!AnchorsInfo.IsEmpty())
	{
		Result += FString::Printf(TEXT("|   Anchors:       %s\n"), *AnchorsInfo);
	}

	if (!ClippingInfo.IsEmpty())
	{
		Result += FString::Printf(TEXT("|   Clipping:      %s\n"), *ClippingInfo);
	}

	if (!RenderTransformInfo.IsEmpty())
	{
		Result += FString::Printf(TEXT("|   Transform:     %s\n"), *RenderTransformInfo);
	}

	Result += TEXT("+====================================================================+\n");
	Result += TEXT("| PARENT CHAIN (top to bottom)\n");
	Result += TEXT("+--------------------------------------------------------------------+\n");

	if (ParentChain.Num() > 0)
	{
		for (int32 i = 0; i < ParentChain.Num(); ++i)
		{
			Result += FString::Printf(TEXT("|   [%d] %s\n"), i, *ParentChain[i]);
		}
	}
	else
	{
		Result += TEXT("|   (no parent chain)\n");
	}

	if (!ViewModelClass.IsEmpty())
	{
		Result += TEXT("+====================================================================+\n");
		Result += TEXT("| VIEWMODEL\n");
		Result += TEXT("+--------------------------------------------------------------------+\n");
		Result += FString::Printf(TEXT("|   Class: %s\n"), *ViewModelClass);
		for (const auto& Prop : ViewModelProperties)
		{
			Result += FString::Printf(TEXT("|     %s = %s\n"), *Prop.Key, *Prop.Value);
		}
	}

	Result += FString::Printf(TEXT("|   Children: %d\n"), ChildCount);

	if (Issues.Num() > 0)
	{
		Result += TEXT("+====================================================================+\n");
		Result += TEXT("| [!] ISSUES DETECTED\n");
		Result += TEXT("+--------------------------------------------------------------------+\n");
		for (const FString& Issue : Issues)
		{
			Result += FString::Printf(TEXT("|   [X] %s\n"), *Issue);
		}
	}

	if (Recommendations.Num() > 0)
	{
		Result += TEXT("+====================================================================+\n");
		Result += TEXT("| [->] RECOMMENDATIONS\n");
		Result += TEXT("+--------------------------------------------------------------------+\n");
		for (const FString& Rec : Recommendations)
		{
			Result += FString::Printf(TEXT("|   -> %s\n"), *Rec);
		}
	}

	Result += TEXT("+====================================================================+\n");

	return Result;
}
