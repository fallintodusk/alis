// Copyright ALIS. All Rights Reserved.

#include "Widgets/ProjectButton.h"
#include "ProjectUIThemeData.h"
#include "Components/ButtonSlot.h"

UProjectButton::UProjectButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProjectButton::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click event
	if (ButtonWidget)
	{
		ButtonWidget->OnClicked.AddDynamic(this, &UProjectButton::HandleButtonClicked);
	}

	// Apply initial styling
	ApplyStyling();
}

void UProjectButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply styling in editor preview
	ApplyStyling();
}

void UProjectButton::SetButtonStyle(EProjectButtonStyle NewStyle)
{
	if (ButtonStyle == NewStyle)
	{
		return;
	}

	ButtonStyle = NewStyle;
	ApplyStyling();
}

void UProjectButton::SetButtonSize(EProjectButtonSize NewSize)
{
	if (ButtonSize == NewSize)
	{
		return;
	}

	ButtonSize = NewSize;
	ApplyStyling();
}

void UProjectButton::SetButtonText(const FText& NewText)
{
	if (TextWidget)
	{
		TextWidget->SetText(NewText);
	}
}

FText UProjectButton::GetButtonText() const
{
	return TextWidget ? TextWidget->GetText() : FText::GetEmpty();
}

void UProjectButton::SetButtonEnabled(bool bEnabled)
{
	if (ButtonWidget)
	{
		ButtonWidget->SetIsEnabled(bEnabled);
	}
}

bool UProjectButton::IsButtonEnabled() const
{
	return ButtonWidget ? ButtonWidget->GetIsEnabled() : false;
}

void UProjectButton::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
	Super::OnThemeChanged_Implementation(NewTheme);

	// Reapply styling with new theme
	ApplyStyling();
}

void UProjectButton::ApplyStyling()
{
	UProjectUIThemeData* Theme = GetActiveTheme();
	if (!Theme || !ButtonWidget || !TextWidget)
	{
		return;
	}

	// Get style color from theme
	FLinearColor StyleColor = GetStyleColor(Theme);

	// Apply button style (filled vs outlined vs text)
	FButtonStyle ButtonStyleStruct = ButtonWidget->GetStyle();

	switch (ButtonStyle)
	{
	case EProjectButtonStyle::Primary:
	case EProjectButtonStyle::Success:
	case EProjectButtonStyle::Warning:
	case EProjectButtonStyle::Error:
		// Filled button: colored background, white text
		ButtonStyleStruct.Normal.TintColor = FSlateColor(StyleColor);
		ButtonStyleStruct.Hovered.TintColor = FSlateColor(StyleColor * 1.2f); // Brighten on hover
		ButtonStyleStruct.Pressed.TintColor = FSlateColor(StyleColor * 0.8f); // Darken on press
		TextWidget->SetColorAndOpacity(Theme->Colors.TextPrimary);
		break;

	case EProjectButtonStyle::Secondary:
		// Outlined button: transparent background, colored border and text
		ButtonStyleStruct.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
		ButtonStyleStruct.Normal.OutlineSettings.Color = FSlateColor(StyleColor);
		ButtonStyleStruct.Hovered.TintColor = FSlateColor(StyleColor * 0.2f); // Slight fill on hover
		ButtonStyleStruct.Pressed.TintColor = FSlateColor(StyleColor * 0.4f);
		TextWidget->SetColorAndOpacity(StyleColor);
		break;

	case EProjectButtonStyle::Text:
		// Text-only button: no background, colored text
		ButtonStyleStruct.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
		ButtonStyleStruct.Hovered.TintColor = FSlateColor(StyleColor * 0.1f); // Very slight fill
		ButtonStyleStruct.Pressed.TintColor = FSlateColor(StyleColor * 0.2f);
		TextWidget->SetColorAndOpacity(StyleColor);
		break;
	}

	// Apply disabled state
	ButtonStyleStruct.Disabled.TintColor = FSlateColor(Theme->Colors.Surface);
	// Disabled text color applied separately below

	ButtonWidget->SetStyle(ButtonStyleStruct);

		// Apply button size
		FSlateFontInfo Font = Theme->Typography.Button;
		FVector2D ButtonPadding = FVector2D::ZeroVector;

	switch (ButtonSize)
	{
	case EProjectButtonSize::Small:
		Font.Size = 14;
		ButtonPadding = FVector2D(Theme->Spacing.Small, Theme->Spacing.ExtraSmall);
		break;

	case EProjectButtonSize::Medium:
		Font.Size = 16;
		ButtonPadding = FVector2D(Theme->Spacing.Medium, Theme->Spacing.Small);
		break;

	case EProjectButtonSize::Large:
		Font.Size = 18;
		ButtonPadding = FVector2D(Theme->Spacing.Large, Theme->Spacing.Medium);
		break;
	}

	TextWidget->SetFont(Font);

	// Apply padding via button slot
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(ButtonWidget->Slot))
	{
		ButtonSlot->SetPadding(FMargin(ButtonPadding.X, ButtonPadding.Y));
	}

	// Apply border radius (requires button style setup)
	// Note: Full border radius control requires custom widget styling in UMG
	// This is a simplified version - designers can refine in UMG blueprints

	UE_LOG(LogTemp, VeryVerbose, TEXT("Button %s styled with %s/%s"),
		*GetName(),
		*UEnum::GetValueAsString(ButtonStyle),
		*UEnum::GetValueAsString(ButtonSize));
}

FLinearColor UProjectButton::GetStyleColor(UProjectUIThemeData* Theme) const
{
	if (!Theme)
	{
		return FLinearColor::White;
	}

	switch (ButtonStyle)
	{
	case EProjectButtonStyle::Primary:
		return Theme->Colors.Primary;

	case EProjectButtonStyle::Secondary:
		return Theme->Colors.Secondary;

	case EProjectButtonStyle::Success:
		return Theme->Colors.Success;

	case EProjectButtonStyle::Warning:
		return Theme->Colors.Warning;

	case EProjectButtonStyle::Error:
		return Theme->Colors.Error;

	case EProjectButtonStyle::Text:
		return Theme->Colors.TextPrimary;

	default:
		return FLinearColor::White;
	}
}

void UProjectButton::HandleButtonClicked()
{
	UE_LOG(LogTemp, Verbose, TEXT("Button clicked: %s (%s)"),
		*GetName(), *GetButtonText().ToString());

	OnButtonClicked.Broadcast();
}
