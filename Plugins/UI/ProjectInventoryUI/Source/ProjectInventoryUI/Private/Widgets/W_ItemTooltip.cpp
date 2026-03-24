// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_ItemTooltip.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "ProjectGameplayTags.h"
#include "Algo/Sort.h"

DEFINE_LOG_CATEGORY_STATIC(LogItemTooltip, Log, All);

namespace
{
struct FMagnitudeDisplaySpec
{
	FString Label;
	FString Unit;
	int32 MaxFractionalDigits = 2;
	bool bPositiveIsBeneficial = true;
};

FMagnitudeDisplaySpec ResolveMagnitudeSpec(const FGameplayTag& Tag)
{
	if (Tag == ProjectTags::SetByCaller_Hydration)
	{
		return {TEXT("Hydration"), TEXT("L"), 2, true};
	}

	if (Tag == ProjectTags::SetByCaller_Calories)
	{
		return {TEXT("Calories"), TEXT("kcal"), 0, true};
	}

	if (Tag == ProjectTags::SetByCaller_Condition)
	{
		return {TEXT("Condition"), TEXT("pts"), 0, true};
	}

	if (Tag == ProjectTags::SetByCaller_Stamina)
	{
		return {TEXT("Stamina"), TEXT("pts"), 0, true};
	}

	if (Tag == ProjectTags::SetByCaller_Fatigue)
	{
		return {TEXT("Fatigue"), TEXT("pts"), 0, false};
	}

	if (Tag == ProjectTags::SetByCaller_Bleeding)
	{
		return {TEXT("Bleeding"), TEXT("pts"), 0, false};
	}

	if (Tag == ProjectTags::SetByCaller_Poisoned)
	{
		return {TEXT("Poison"), TEXT("pts"), 0, false};
	}

	if (Tag == ProjectTags::SetByCaller_Radiation)
	{
		return {TEXT("Radiation"), TEXT("pts"), 0, false};
	}

	FString Name = Tag.ToString();
	int32 DotIndex = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), DotIndex) && DotIndex + 1 < Name.Len())
	{
		Name = Name.RightChop(DotIndex + 1);
	}

	return {Name, FString(), 2, true};
}

FString FormatSignedMagnitude(float Value, int32 MaxFractionalDigits, const FString& Unit)
{
	FNumberFormattingOptions NumberOptions = FNumberFormattingOptions::DefaultWithGrouping();
	NumberOptions.SetMaximumFractionalDigits(MaxFractionalDigits);
	if (MaxFractionalDigits == 0)
	{
		NumberOptions.SetMinimumFractionalDigits(0);
	}

	const FString NumberStr = FText::AsNumber(FMath::Abs(Value), &NumberOptions).ToString();
	const TCHAR* Sign = Value >= 0.0f ? TEXT("+") : TEXT("-");
	if (Unit.IsEmpty())
	{
		return FString::Printf(TEXT("%s%s"), Sign, *NumberStr);
	}

	return FString::Printf(TEXT("%s%s %s"), Sign, *NumberStr, *Unit);
}

FLinearColor GetEffectColor(float Value, const FMagnitudeDisplaySpec& Spec)
{
	const FLinearColor PositiveColor(0.60f, 0.90f, 0.60f, 1.0f);
	const FLinearColor NegativeColor(0.94f, 0.52f, 0.52f, 1.0f);
	const FLinearColor NeutralColor(0.78f, 0.84f, 0.90f, 1.0f);

	if (FMath::IsNearlyZero(Value))
	{
		return NeutralColor;
	}

	const bool bPositive = Value > 0.0f;
	const bool bBeneficial = bPositive ? Spec.bPositiveIsBeneficial : !Spec.bPositiveIsBeneficial;
	return bBeneficial ? PositiveColor : NegativeColor;
}

struct FUseEffectToken
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
};

TArray<FUseEffectToken> BuildUseEffectTokens(const TMap<FGameplayTag, float>& UseMagnitudes)
{
	TArray<FUseEffectToken> Tokens;
	if (UseMagnitudes.Num() == 0)
	{
		return Tokens;
	}

	TArray<FGameplayTag> Tags;
	Tags.Reserve(UseMagnitudes.Num());
	UseMagnitudes.GetKeys(Tags);
	Algo::Sort(Tags, [](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FGameplayTag& Tag : Tags)
	{
		const float* ValuePtr = UseMagnitudes.Find(Tag);
		if (!ValuePtr || FMath::IsNearlyZero(*ValuePtr))
		{
			continue;
		}

		const FMagnitudeDisplaySpec Spec = ResolveMagnitudeSpec(Tag);
		const FString MagnitudeText = FormatSignedMagnitude(*ValuePtr, Spec.MaxFractionalDigits, Spec.Unit);
		FUseEffectToken Token;
		Token.Text = FString::Printf(TEXT("%s %s"), *Spec.Label, *MagnitudeText);
		Token.Color = GetEffectColor(*ValuePtr, Spec);
		Tokens.Add(MoveTemp(Token));
	}

	return Tokens;
}

void AddEffectTokenText(UHorizontalBox* EffectsList, const FString& Text, const FLinearColor& Color, const UTextBlock* StyleTemplate)
{
	if (!EffectsList || Text.IsEmpty())
	{
		return;
	}

	UTextBlock* TokenText = NewObject<UTextBlock>(EffectsList);
	if (!TokenText)
	{
		return;
	}

	if (StyleTemplate)
	{
		TokenText->SetFont(StyleTemplate->GetFont());
		TokenText->SetShadowOffset(StyleTemplate->GetShadowOffset());
		TokenText->SetShadowColorAndOpacity(StyleTemplate->GetShadowColorAndOpacity());
		TokenText->SetMinDesiredWidth(StyleTemplate->GetMinDesiredWidth());
	}

	TokenText->SetText(FText::FromString(Text));
	TokenText->SetColorAndOpacity(FSlateColor(Color));
	EffectsList->AddChildToHorizontalBox(TokenText);
}
}

UW_ItemTooltip::UW_ItemTooltip(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectInventoryUI"), TEXT("ItemTooltipLayout.json"));
}

void UW_ItemTooltip::NativeConstruct()
{
	Super::NativeConstruct();

	// Find child widgets by name
	if (RootWidget)
	{
		FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());
		HeaderIconBox = Binder.FindOptional<UWidget>(TEXT("HeaderIconBox"));
		ItemIcon = Binder.FindOptional<UTextBlock>(TEXT("ItemIcon"));
		ItemName = Binder.FindRequired<UTextBlock>(TEXT("ItemName"));
		ItemMetaText = Binder.FindOptional<UTextBlock>(TEXT("ItemMetaText"));
		ItemDescription = Binder.FindRequired<UTextBlock>(TEXT("ItemDescription"));
		WeightText = Binder.FindRequired<UTextBlock>(TEXT("WeightText"));
		VolumeText = Binder.FindRequired<UTextBlock>(TEXT("VolumeText"));
		DurabilityLabel = Binder.FindOptional<UTextBlock>(TEXT("DurabilityLabel"));
		DurabilityBar = Binder.FindOptional<UProgressBar>(TEXT("DurabilityBar"));
		AmmoText = Binder.FindOptional<UTextBlock>(TEXT("AmmoText"));
		EffectsList = Binder.FindOptional<UHorizontalBox>(TEXT("EffectsList"));
		EffectsTextTemplate = Binder.FindOptional<UTextBlock>(TEXT("EffectsTextTemplate"));
		ModifiersText = Binder.FindOptional<UTextBlock>(TEXT("ModifiersText"));
		DurabilityRow = Binder.FindOptional<UWidget>(TEXT("DurabilityRow"));
		AmmoRow = Binder.FindOptional<UWidget>(TEXT("AmmoRow"));
		EffectsRow = Binder.FindOptional<UWidget>(TEXT("EffectsRow"));
		ModifiersRow = Binder.FindOptional<UWidget>(TEXT("ModifiersRow"));
		Binder.LogMissingRequired(TEXT("UW_ItemTooltip::NativeConstruct"));
	}

	Clear();

	UE_LOG(LogItemTooltip, Verbose, TEXT("ItemTooltip constructed"));
}

void UW_ItemTooltip::SetItemData(const FInventoryEntryView& EntryView)
{
	if (ItemIcon.IsValid())
	{
		const bool bHasIcon = !EntryView.IconCode.IsEmpty();
		ItemIcon->SetText(FText::FromString(EntryView.IconCode));
		ItemIcon->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (HeaderIconBox.IsValid())
		{
			HeaderIconBox->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	// Item name (with quantity if > 1)
	if (ItemName.IsValid())
	{
		FText NameText = EntryView.DisplayName.IsEmpty()
			? FText::FromString(EntryView.ItemId.ToString())
			: EntryView.DisplayName;
		if (EntryView.Quantity > 1)
		{
			NameText = FText::Format(NSLOCTEXT("Inventory", "ItemNameWithQty", "{0} x{1}"),
				NameText, FText::AsNumber(EntryView.Quantity));
		}
		ItemName->SetText(NameText);
	}

	if (ItemMetaText.IsValid())
	{
		SetTextWithVisibility(ItemMetaText.Get(), BuildMetaText(EntryView));
	}

	// Description
	if (ItemDescription.IsValid())
	{
		const bool bHasDesc = !EntryView.Description.IsEmpty();
		ItemDescription->SetText(EntryView.Description);
		ItemDescription->SetVisibility(bHasDesc ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Weight
	if (WeightText.IsValid())
	{
		const float TotalWeight = EntryView.Weight * EntryView.Quantity;
		FNumberFormattingOptions WeightOpts = FNumberFormattingOptions::DefaultWithGrouping();
		WeightOpts.SetMaximumFractionalDigits(2);
		FText WeightFormatted = FText::Format(NSLOCTEXT("Inventory", "WeightFormat", "{0} kg"),
			FText::AsNumber(TotalWeight, &WeightOpts));
		WeightText->SetText(WeightFormatted);
	}

	// Volume
	if (VolumeText.IsValid())
	{
		const float TotalVolume = EntryView.Volume * EntryView.Quantity;
		FNumberFormattingOptions VolumeOpts = FNumberFormattingOptions::DefaultWithGrouping();
		VolumeOpts.SetMaximumFractionalDigits(2);
		FText VolumeFormatted = FText::Format(NSLOCTEXT("Inventory", "VolumeFormat", "{0} L"),
			FText::AsNumber(TotalVolume, &VolumeOpts));
		VolumeText->SetText(VolumeFormatted);
	}

	// Durability (only show if < 100%)
	const float DurabilityPct = CalculateDurabilityPercent(EntryView.Durability, EntryView.MaxDurability);
	const bool bShowDurability = DurabilityPct < 1.0f;

	if (DurabilityRow.IsValid())
	{
		DurabilityRow->SetVisibility(bShowDurability ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bShowDurability)
	{
		if (DurabilityLabel.IsValid())
		{
			DurabilityLabel->SetText(FText::Format(NSLOCTEXT("Inventory", "DurabilityFormat", "Durability: {0}%"),
				FText::AsNumber(FMath::RoundToInt(DurabilityPct * 100.f))));
		}

		if (DurabilityBar.IsValid())
		{
			DurabilityBar->SetPercent(DurabilityPct);
			DurabilityBar->SetFillColorAndOpacity(GetDurabilityColor(DurabilityPct));
		}
	}

	// Ammo (only show if > 0)
	const bool bShowAmmo = EntryView.Ammo > 0;
	if (AmmoRow.IsValid())
	{
		AmmoRow->SetVisibility(bShowAmmo ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bShowAmmo && AmmoText.IsValid())
	{
		AmmoText->SetText(FText::Format(NSLOCTEXT("Inventory", "AmmoFormat", "Ammo: {0}"),
			FText::AsNumber(EntryView.Ammo)));
	}

	const TArray<FUseEffectToken> EffectTokens = BuildUseEffectTokens(EntryView.UseMagnitudes);
	const bool bShowEffects = EffectTokens.Num() > 0;
	if (EffectsRow.IsValid())
	{
		EffectsRow->SetVisibility(bShowEffects ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EffectsList.IsValid())
	{
		EffectsList->ClearChildren();

		if (bShowEffects)
		{
			const UTextBlock* StyleTemplate = EffectsTextTemplate.Get();
			AddEffectTokenText(EffectsList.Get(), TEXT("Use: "), FLinearColor(0.84f, 0.90f, 0.95f, 1.0f), StyleTemplate);

			for (int32 Index = 0; Index < EffectTokens.Num(); ++Index)
			{
				if (Index > 0)
				{
					AddEffectTokenText(EffectsList.Get(), TEXT(", "), FLinearColor(0.70f, 0.75f, 0.80f, 1.0f), StyleTemplate);
				}

				const FUseEffectToken& Token = EffectTokens[Index];
				AddEffectTokenText(EffectsList.Get(), Token.Text, Token.Color, StyleTemplate);
			}
		}
	}

	// Modifiers (only show if any)
	const bool bShowModifiers = EntryView.Modifiers.Num() > 0;
	if (ModifiersRow.IsValid())
	{
		ModifiersRow->SetVisibility(bShowModifiers ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bShowModifiers && ModifiersText.IsValid())
	{
		// Format modifiers as comma-separated list
		FString ModifiersStr;
		for (int32 i = 0; i < EntryView.Modifiers.Num(); ++i)
		{
			if (i > 0)
			{
				ModifiersStr += TEXT(", ");
			}
			// Use the last part of the tag as display name
			FString TagStr = EntryView.Modifiers[i].ToString();
			int32 DotIndex;
			if (TagStr.FindLastChar('.', DotIndex))
			{
				ModifiersStr += TagStr.RightChop(DotIndex + 1);
			}
			else
			{
				ModifiersStr += TagStr;
			}
		}
		ModifiersText->SetText(FText::Format(NSLOCTEXT("Inventory", "ModifiersFormat", "Mods: {0}"),
			FText::FromString(ModifiersStr)));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_ItemTooltip::Clear()
{
	if (HeaderIconBox.IsValid())
	{
		HeaderIconBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ItemIcon.IsValid())
	{
		ItemIcon->SetText(FText::GetEmpty());
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ItemName.IsValid())
	{
		ItemName->SetText(FText::GetEmpty());
	}
	if (ItemMetaText.IsValid())
	{
		ItemMetaText->SetText(FText::GetEmpty());
		ItemMetaText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ItemDescription.IsValid())
	{
		ItemDescription->SetText(FText::GetEmpty());
		ItemDescription->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DurabilityRow.IsValid())
	{
		DurabilityRow->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AmmoRow.IsValid())
	{
		AmmoRow->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (EffectsRow.IsValid())
	{
		EffectsRow->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (EffectsList.IsValid())
	{
		EffectsList->ClearChildren();
	}
	if (ModifiersRow.IsValid())
	{
		ModifiersRow->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_ItemTooltip::SetTextWithVisibility(UTextBlock* TextBlock, const FText& Text, UWidget* RowWidget)
{
	if (!TextBlock)
	{
		return;
	}

	const bool bHasContent = !Text.IsEmpty();
	TextBlock->SetText(Text);

	UWidget* Target = RowWidget ? RowWidget : TextBlock;
	Target->SetVisibility(bHasContent ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

float UW_ItemTooltip::CalculateDurabilityPercent(int32 Durability, int32 MaxDurability)
{
	if (MaxDurability <= 0)
	{
		return 1.0f;
	}
	return FMath::Clamp(static_cast<float>(Durability) / static_cast<float>(MaxDurability), 0.0f, 1.0f);
}

FLinearColor UW_ItemTooltip::GetDurabilityColor(float Percent)
{
	// Green -> Yellow -> Red gradient
	if (Percent > 0.5f)
	{
		// Green to Yellow (50-100%)
		const float T = (Percent - 0.5f) * 2.0f;
		return FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, T);
	}
	else
	{
		// Yellow to Red (0-50%)
		const float T = Percent * 2.0f;
		return FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Yellow, T);
	}
}

FText UW_ItemTooltip::BuildMetaText(const FInventoryEntryView& EntryView)
{
	TArray<FString> MetaParts;

	if (EntryView.MaxStack > 1)
	{
		MetaParts.Add(FString::Printf(TEXT("Stack %d/%d"), EntryView.Quantity, EntryView.MaxStack));
	}
	else if (EntryView.Quantity > 1)
	{
		MetaParts.Add(FString::Printf(TEXT("Qty %d"), EntryView.Quantity));
	}
	else
	{
		MetaParts.Add(TEXT("Single item"));
	}

	const FIntPoint Footprint = EntryView.bRotated
		? FIntPoint(FMath::Max(1, EntryView.GridSize.Y), FMath::Max(1, EntryView.GridSize.X))
		: FIntPoint(FMath::Max(1, EntryView.GridSize.X), FMath::Max(1, EntryView.GridSize.Y));
	MetaParts.Add(FString::Printf(TEXT("Size %dx%d"), Footprint.X, Footprint.Y));

	if (EntryView.bUsesDepthStacking && EntryView.MaxDepthUnits > 0)
	{
		MetaParts.Add(FString::Printf(TEXT("Depth %d/%d"), EntryView.DepthUnitsUsed, EntryView.MaxDepthUnits));
	}

	return FText::FromString(FString::Join(MetaParts, TEXT("  |  ")));
}
