// Copyright ALIS. All Rights Reserved.

#include "Presentation/ProjectUIGridVisualState.h"
#include "Theme/ProjectUIThemeData.h"

FProjectUIGridColors::FProjectUIGridColors()
    : Base(0.12f, 0.12f, 0.12f, 0.9f)
    , Disabled(0.06f, 0.06f, 0.06f, 0.9f)
    , Selected(0.0f, 0.47f, 0.84f, 1.0f)
    , Hovered(0.2f, 0.4f, 0.8f, 1.0f)
    , PreviewValid(0.2f, 0.7f, 0.2f, 1.0f)
    , PreviewInvalid(0.9f, 0.2f, 0.2f, 1.0f)
{
}

void FProjectUIGridColors::UpdateFromTheme(const UProjectUIThemeData* Theme)
{
    if (Theme)
    {
        Base = Theme->Colors.Surface;
        Selected = Theme->Colors.Primary;
        Hovered = Theme->Colors.Secondary;
        PreviewValid = Theme->Colors.Success;
        PreviewInvalid = Theme->Colors.Error;
    }
    else
    {
        *this = FProjectUIGridColors();
    }

    Disabled = FLinearColor(Base.R * 0.5f, Base.G * 0.5f, Base.B * 0.5f, Base.A);
}

FLinearColor FProjectUIGridColors::GetColorForState(const FProjectUICellState& State, float PulseAlpha) const
{
    if (!State.bEnabled)
    {
        return Disabled;
    }

    if (State.bDragPreview)
    {
        if (State.bDragPreviewValid)
        {
            return PreviewValid;
        }

        return FLinearColor::LerpUsingHSV(Base, PreviewInvalid, PulseAlpha);
    }

    if (State.bSelected)
    {
        return Selected;
    }

    if (State.bHovered)
    {
        return Hovered;
    }

    return Base;
}

void FProjectUIGridVisualState::UpdateColors(const UProjectUIThemeData* Theme)
{
    Colors.UpdateFromTheme(Theme);
}

