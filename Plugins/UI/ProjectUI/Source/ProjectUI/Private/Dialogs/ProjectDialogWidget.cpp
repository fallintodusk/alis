// Copyright ALIS. All Rights Reserved.

#include "Dialogs/ProjectDialogWidget.h"
#include "Theme/ProjectUIThemeData.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Input/SButton.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectDialog, Log, All);

namespace
{
	// Set a solid color brush on a UBorder (required for color to actually render)
	void SetSolidBrush(UBorder* Border, const FLinearColor& Color)
	{
		if (!Border)
		{
			return;
		}

		// Create drawable brush with Image draw type (renders as solid color when no resource)
		FSlateBrush SolidBrush;
		SolidBrush.DrawAs = ESlateBrushDrawType::Image;
		Border->SetBrush(SolidBrush);
		Border->SetBrushColor(Color);
	}
}

UProjectDialogWidget::UProjectDialogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UProjectDialogWidget::RebuildWidget()
{
	// Build dialog UI before Slate takes the root
	if (!bUIBuilt)
	{
		BuildDialogUI();
		bUIBuilt = true;
	}

	return Super::RebuildWidget();
}

void UProjectDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogProjectDialog, Display, TEXT("=== NativeConstruct START ==="));

	// Log widget state at construct time
	FLinearColor CurrentColor = GetColorAndOpacity();
	float CurrentOpacity = GetRenderOpacity();
	UE_LOG(LogProjectDialog, Display, TEXT("Dialog at NativeConstruct: ColorAndOpacity=(%.2f,%.2f,%.2f,%.2f) RenderOpacity=%.2f Visibility=%d"),
		CurrentColor.R, CurrentColor.G, CurrentColor.B, CurrentColor.A, CurrentOpacity, static_cast<int32>(GetVisibility()));

	// Ensure UI is built if not already
	if (!bUIBuilt)
	{
		BuildDialogUI();
		bUIBuilt = true;
	}

	// Bind button click events now that Slate widgets exist
	for (const auto& Pair : ButtonActions)
	{
		UButton* Button = Pair.Key;
		const FString& Action = Pair.Value;

		if (!Button)
		{
			continue;
		}

		// Get the underlying Slate button and bind click handler with captured action
		TSharedPtr<SWidget> SlateWidget = Button->GetCachedWidget();
		if (SlateWidget.IsValid())
		{
			TSharedPtr<SButton> SlateButton = StaticCastSharedPtr<SButton>(SlateWidget);
			if (SlateButton.IsValid())
			{
				// Capture action by value, weak ref to this for safety
				TWeakObjectPtr<UProjectDialogWidget> WeakThis(this);
				SlateButton->SetOnClicked(FOnClicked::CreateLambda([WeakThis, Action]()
				{
					if (WeakThis.IsValid())
					{
						UE_LOG(LogProjectDialog, Display, TEXT("Dialog button clicked: %s"), *Action);
						WeakThis->OnButtonClicked.Broadcast(Action);
					}
					return FReply::Handled();
				}));
			}
		}
	}

	UE_LOG(LogProjectDialog, Display, TEXT("=== NativeConstruct END ==="));
}

void UProjectDialogWidget::ApplyParams(const FProjectDialogParams& InParams)
{
	Params = InParams;

	// Rebuild UI with new params
	if (bUIBuilt)
	{
		bUIBuilt = false;
		BuildDialogUI();
		bUIBuilt = true;
	}
}

void UProjectDialogWidget::SetTitle(const FText& InTitle)
{
	Params.Title = InTitle;
	if (TitleText)
	{
		TitleText->SetText(InTitle);
	}
}

void UProjectDialogWidget::SetMessage(const FText& InMessage)
{
	Params.Message = InMessage;
	if (MessageText)
	{
		MessageText->SetText(InMessage);
	}
}

void UProjectDialogWidget::ShowDialog()
{
	SetVisibility(ESlateVisibility::Visible);
}

void UProjectDialogWidget::HideDialog()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UProjectDialogWidget::IsDialogVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UProjectDialogWidget::BuildDialogUI()
{
	if (!WidgetTree)
	{
		UE_LOG(LogProjectDialog, Warning, TEXT("BuildDialogUI: WidgetTree is null"));
		return;
	}

	UE_LOG(LogProjectDialog, Display, TEXT("=== BuildDialogUI START ==="));

	// Log parent widget state
	UWidget* ParentWidget = GetParent();
	if (ParentWidget)
	{
		float ParentOpacity = ParentWidget->GetRenderOpacity();
		UE_LOG(LogProjectDialog, Display, TEXT("Parent [%s]: RenderOpacity=%.2f"),
			*ParentWidget->GetName(), ParentOpacity);
	}
	else
	{
		UE_LOG(LogProjectDialog, Display, TEXT("Parent: NULL (root widget)"));
	}

	// Log this widget's current state BEFORE reset
	FLinearColor PreResetColor = GetColorAndOpacity();
	float PreResetOpacity = GetRenderOpacity();
	UE_LOG(LogProjectDialog, Display, TEXT("Dialog BEFORE reset: ColorAndOpacity=(%.2f,%.2f,%.2f,%.2f) RenderOpacity=%.2f"),
		PreResetColor.R, PreResetColor.G, PreResetColor.B, PreResetColor.A, PreResetOpacity);

	// Reset any inherited tint from parent - dialog content must stay crisp
	// while only the overlay dims the background
	SetColorAndOpacity(FLinearColor::White);
	SetRenderOpacity(1.0f);

	// Log this widget's state AFTER reset
	FLinearColor PostResetColor = GetColorAndOpacity();
	float PostResetOpacity = GetRenderOpacity();
	UE_LOG(LogProjectDialog, Display, TEXT("Dialog AFTER reset: ColorAndOpacity=(%.2f,%.2f,%.2f,%.2f) RenderOpacity=%.2f"),
		PostResetColor.R, PostResetColor.G, PostResetColor.B, PostResetColor.A, PostResetOpacity);

	// Clear existing widgets and action mappings
	ButtonWidgets.Empty();
	ButtonActions.Empty();

	// Create root canvas (full screen)
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// Create overlay (full screen, semi-transparent background)
	OverlayBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogOverlay"));
	FLinearColor OverlayColor = ResolveColor(Params.OverlayColor);
	UE_LOG(LogProjectDialog, Display, TEXT("Overlay: Color=(%.2f,%.2f,%.2f,%.2f) Opacity=%.2f"),
		OverlayColor.R, OverlayColor.G, OverlayColor.B, OverlayColor.A, Params.OverlayOpacity);
	SetSolidBrush(OverlayBorder, OverlayColor);
	OverlayBorder->SetRenderOpacity(Params.OverlayOpacity);

	if (UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(OverlayBorder))
	{
		OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		OverlaySlot->SetOffsets(FMargin(0.0f));
		OverlaySlot->SetZOrder(0);  // Background layer
	}

	// Handle overlay click if enabled
	if (Params.bCloseOnOverlayClick)
	{
		// Note: Border doesn't have click events, so we'd need a Button with transparent style
		// For now, overlay click is handled by the owning widget
	}

	// Create border frame (provides visible border around content)
	// Force full opacity so overlay doesn't bleed through
	UBorder* BorderFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogBorderFrame"));
	FLinearColor BorderColor = ResolveColor(Params.BorderColor);
	UE_LOG(LogProjectDialog, Display, TEXT("BorderFrame: ResolvedColor=(%.2f,%.2f,%.2f,%.2f) -> ForcedAlpha=1.0"),
		BorderColor.R, BorderColor.G, BorderColor.B, BorderColor.A);
	BorderColor.A = 1.0f;
	SetSolidBrush(BorderFrame, BorderColor);
	BorderFrame->SetPadding(FMargin(Params.BorderWidth));
	BorderFrame->SetContentColorAndOpacity(FLinearColor::White);
	BorderFrame->SetRenderOpacity(1.0f);
	UE_LOG(LogProjectDialog, Display, TEXT("BorderFrame: ContentColorAndOpacity=White RenderOpacity=1.0 Padding=%.1f"),
		Params.BorderWidth);

	if (UCanvasPanelSlot* BorderSlot = RootCanvas->AddChildToCanvas(BorderFrame))
	{
		BorderSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		BorderSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BorderSlot->SetAutoSize(true);
		BorderSlot->SetZOrder(1);  // Dialog content layer (above overlay)
	}

	// Create content box (nested inside border frame)
	// Force full opacity so overlay doesn't bleed through
	ContentBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogContentBox"));
	FLinearColor BoxColor = ResolveColor(Params.BoxColor);
	UE_LOG(LogProjectDialog, Display, TEXT("ContentBox: ResolvedColor=(%.2f,%.2f,%.2f,%.2f) -> ForcedAlpha=1.0"),
		BoxColor.R, BoxColor.G, BoxColor.B, BoxColor.A);
	BoxColor.A = 1.0f;
	SetSolidBrush(ContentBox, BoxColor);
	ContentBox->SetPadding(FMargin(Params.Padding));
	ContentBox->SetContentColorAndOpacity(FLinearColor::White);
	ContentBox->SetRenderOpacity(1.0f);
	BorderFrame->SetContent(ContentBox);
	UE_LOG(LogProjectDialog, Display, TEXT("ContentBox: ContentColorAndOpacity=White RenderOpacity=1.0 Padding=%.1f"),
		Params.Padding);

	// Create vertical layout inside content box
	UVerticalBox* ContentLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogContentLayout"));
	ContentBox->SetContent(ContentLayout);

	// Add title if present
	if (!Params.Title.IsEmpty())
	{
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DialogTitle"));
		TitleText->SetText(Params.Title);
		TitleText->SetFont(ResolveFont(Params.TitleFont));
		FLinearColor TitleColor = ResolveColor(TEXT("Text"));
		UE_LOG(LogProjectDialog, Display, TEXT("TitleText: Color=(%.2f,%.2f,%.2f,%.2f)"),
			TitleColor.R, TitleColor.G, TitleColor.B, TitleColor.A);
		TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
		TitleText->SetJustification(ETextJustify::Center);

		if (UVerticalBoxSlot* TitleSlot = ContentLayout->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
		}
	}

	// Add message if present
	if (!Params.Message.IsEmpty())
	{
		MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DialogMessage"));
		MessageText->SetText(Params.Message);
		MessageText->SetFont(ResolveFont(Params.MessageFont));
		FLinearColor MessageColor = ResolveColor(TEXT("Text"));
		UE_LOG(LogProjectDialog, Display, TEXT("MessageText: Color=(%.2f,%.2f,%.2f,%.2f)"),
			MessageColor.R, MessageColor.G, MessageColor.B, MessageColor.A);
		MessageText->SetColorAndOpacity(FSlateColor(MessageColor));
		MessageText->SetJustification(ETextJustify::Center);
		MessageText->SetAutoWrapText(true);

		if (UVerticalBoxSlot* MessageSlot = ContentLayout->AddChildToVerticalBox(MessageText))
		{
			MessageSlot->SetHorizontalAlignment(HAlign_Fill);
			MessageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
		}
	}

	// Add button container
	if (Params.Buttons.Num() > 0)
	{
		ButtonContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DialogButtons"));

		if (UVerticalBoxSlot* ButtonContainerSlot = ContentLayout->AddChildToVerticalBox(ButtonContainer))
		{
			ButtonContainerSlot->SetHorizontalAlignment(HAlign_Center);
		}

		// Create buttons
		for (int32 i = 0; i < Params.Buttons.Num(); ++i)
		{
			// Add spacing between buttons
			if (i > 0)
			{
				USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
				Spacer->SetSize(FVector2D(16.0f, 0.0f));
				ButtonContainer->AddChildToHorizontalBox(Spacer);
			}

			UButton* Button = CreateButton(Params.Buttons[i], i);
			if (Button)
			{
				ButtonWidgets.Add(Button);

				if (UHorizontalBoxSlot* ButtonSlot = ButtonContainer->AddChildToHorizontalBox(Button))
				{
					ButtonSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
		}
	}

	UE_LOG(LogProjectDialog, Display, TEXT("Dialog UI built: Title='%s', %d buttons"),
		*Params.Title.ToString(), Params.Buttons.Num());
	UE_LOG(LogProjectDialog, Display, TEXT("=== BuildDialogUI END ==="));
}

UButton* UProjectDialogWidget::CreateButton(const FProjectDialogButton& ButtonConfig, int32 Index)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UE_LOG(LogProjectDialog, Display, TEXT("CreateButton[%d]: Text='%s' Variant='%s' Action='%s'"),
		Index, *ButtonConfig.Text.ToString(), *ButtonConfig.Variant, *ButtonConfig.Action);

	FName ButtonName = *FString::Printf(TEXT("DialogButton_%d"), Index);
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);

	// Create button content (icon + text)
	UHorizontalBox* ButtonContent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// Add icon if present
	if (!ButtonConfig.Icon.IsEmpty())
	{
		UTextBlock* IconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		IconText->SetText(FText::FromString(ButtonConfig.Icon));
		// Icon font would be set here if available
		IconText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		if (UHorizontalBoxSlot* IconSlot = ButtonContent->AddChildToHorizontalBox(IconText))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}
	}

	// Add text
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(ButtonConfig.Text);
	ButtonText->SetFont(ResolveFont(TEXT("Button")));
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UE_LOG(LogProjectDialog, Display, TEXT("  ButtonText: Color=White (hardcoded)"));

	if (UHorizontalBoxSlot* TextSlot = ButtonContent->AddChildToHorizontalBox(ButtonText))
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	Button->AddChild(ButtonContent);

	// Apply variant styling
	ApplyButtonVariant(Button, ButtonConfig.Variant);

	// Store button -> action mapping (binding happens in NativeConstruct after Slate is built)
	ButtonActions.Add(Button, ButtonConfig.Action);

	return Button;
}

void UProjectDialogWidget::OnOverlayClicked()
{
	if (Params.bCloseOnOverlayClick && !Params.OverlayClickAction.IsEmpty())
	{
		UE_LOG(LogProjectDialog, Display, TEXT("Dialog overlay clicked: %s"), *Params.OverlayClickAction);
		OnButtonClicked.Broadcast(Params.OverlayClickAction);
	}
}

void UProjectDialogWidget::ApplyButtonVariant(UButton* Button, const FString& Variant)
{
	if (!Button)
	{
		return;
	}

	FLinearColor BaseColor;

	if (Variant == TEXT("Primary"))
	{
		BaseColor = ResolveColor(TEXT("Primary"));
	}
	else if (Variant == TEXT("Error"))
	{
		BaseColor = ResolveColor(TEXT("Error"));
	}
	else if (Variant == TEXT("Warning"))
	{
		BaseColor = ResolveColor(TEXT("Warning"));
	}
	else if (Variant == TEXT("Success"))
	{
		BaseColor = ResolveColor(TEXT("Success"));
	}
	else if (Variant == TEXT("Text"))
	{
		BaseColor = FLinearColor::Transparent;
	}
	else // Secondary and default
	{
		BaseColor = ResolveColor(TEXT("Secondary"));
	}

	UE_LOG(LogProjectDialog, Display, TEXT("  ApplyButtonVariant: Variant='%s' BaseColor=(%.2f,%.2f,%.2f,%.2f)"),
		*Variant, BaseColor.R, BaseColor.G, BaseColor.B, BaseColor.A);

	FButtonStyle ButtonStyle = Button->GetStyle();

	// Create solid color brushes instead of tinting (tinting multiplies with default grey brush)
	FSlateBrush NormalBrush;
	NormalBrush.DrawAs = ESlateBrushDrawType::Image;
	NormalBrush.TintColor = FSlateColor(BaseColor);
	ButtonStyle.Normal = NormalBrush;

	FLinearColor HoveredColor = BaseColor * 1.15f;
	HoveredColor.A = BaseColor.A;
	FSlateBrush HoveredBrush;
	HoveredBrush.DrawAs = ESlateBrushDrawType::Image;
	HoveredBrush.TintColor = FSlateColor(HoveredColor);
	ButtonStyle.Hovered = HoveredBrush;

	FLinearColor PressedColor = BaseColor * 0.85f;
	PressedColor.A = BaseColor.A;
	FSlateBrush PressedBrush;
	PressedBrush.DrawAs = ESlateBrushDrawType::Image;
	PressedBrush.TintColor = FSlateColor(PressedColor);
	ButtonStyle.Pressed = PressedBrush;

	FLinearColor DisabledColor = BaseColor * 0.5f;
	DisabledColor.A = BaseColor.A * 0.5f;
	FSlateBrush DisabledBrush;
	DisabledBrush.DrawAs = ESlateBrushDrawType::Image;
	DisabledBrush.TintColor = FSlateColor(DisabledColor);
	ButtonStyle.Disabled = DisabledBrush;

	Button->SetStyle(ButtonStyle);
}

FLinearColor UProjectDialogWidget::ResolveColor(const FString& ColorName) const
{
	// Use the framework's theme resolver for consistent fonts
	return UProjectWidgetLayoutLoader::ResolveThemeColor(ColorName, Theme);
}

FSlateFontInfo UProjectDialogWidget::ResolveFont(const FString& FontName) const
{
	// Use the framework's theme resolver for consistent fonts
	return UProjectWidgetLayoutLoader::ResolveThemeFont(FontName, Theme);
}
