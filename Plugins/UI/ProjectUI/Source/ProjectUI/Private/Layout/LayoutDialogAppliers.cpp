// Copyright ALIS. All Rights Reserved.
// LayoutDialogAppliers - Dialog widget property application (SOLID: Single Responsibility)

#include "Dialogs/ProjectDialogWidget.h"
#include "Dialogs/ProjectDialogTypes.h"
#include "Theme/ProjectUIThemeData.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogAppliers, Log, All);

namespace
{
	FProjectDialogButton ParseButtonConfig(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FProjectDialogButton Button;

		if (!JsonObject.IsValid())
		{
			return Button;
		}

		FString Text;
		if (JsonObject->TryGetStringField(TEXT("text"), Text))
		{
			Button.Text = FText::FromString(Text);
		}

		JsonObject->TryGetStringField(TEXT("action"), Button.Action);
		JsonObject->TryGetStringField(TEXT("variant"), Button.Variant);
		JsonObject->TryGetStringField(TEXT("icon"), Button.Icon);

		return Button;
	}

	FProjectDialogParams ParseDialogParams(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FProjectDialogParams Params;

		if (!JsonObject.IsValid())
		{
			return Params;
		}

		// Get the "dialog" sub-object
		const TSharedPtr<FJsonObject>* DialogJson;
		if (!JsonObject->TryGetObjectField(TEXT("dialog"), DialogJson))
		{
			return Params;
		}

		// Parse title
		FString TitleStr;
		if ((*DialogJson)->TryGetStringField(TEXT("title"), TitleStr))
		{
			Params.Title = FText::FromString(TitleStr);
		}

		// Parse message
		FString MessageStr;
		if ((*DialogJson)->TryGetStringField(TEXT("message"), MessageStr))
		{
			Params.Message = FText::FromString(MessageStr);
		}

		// Parse overlay settings
		double OverlayOpacity = 0.0;
		if ((*DialogJson)->TryGetNumberField(TEXT("overlayOpacity"), OverlayOpacity))
		{
			Params.OverlayOpacity = static_cast<float>(OverlayOpacity);
		}

		FString OverlayColor;
		if ((*DialogJson)->TryGetStringField(TEXT("overlayColor"), OverlayColor))
		{
			Params.OverlayColor = OverlayColor;
		}

		// Parse box settings
		double Width = 0.0;
		if ((*DialogJson)->TryGetNumberField(TEXT("width"), Width))
		{
			Params.Width = static_cast<float>(Width);
		}

		FString BoxColor;
		if ((*DialogJson)->TryGetStringField(TEXT("boxColor"), BoxColor))
		{
			Params.BoxColor = BoxColor;
		}

		// Parse border settings
		FString BorderColor;
		if ((*DialogJson)->TryGetStringField(TEXT("borderColor"), BorderColor))
		{
			Params.BorderColor = BorderColor;
		}

		double BorderWidth = 0.0;
		if ((*DialogJson)->TryGetNumberField(TEXT("borderWidth"), BorderWidth))
		{
			Params.BorderWidth = static_cast<float>(BorderWidth);
		}

		double Padding = 0.0;
		if ((*DialogJson)->TryGetNumberField(TEXT("padding"), Padding))
		{
			Params.Padding = static_cast<float>(Padding);
		}

		// Parse font settings
		FString TitleFont;
		if ((*DialogJson)->TryGetStringField(TEXT("titleFont"), TitleFont))
		{
			Params.TitleFont = TitleFont;
		}

		FString MessageFont;
		if ((*DialogJson)->TryGetStringField(TEXT("messageFont"), MessageFont))
		{
			Params.MessageFont = MessageFont;
		}

		// Parse overlay click behavior
		bool bCloseOnOverlayClick = false;
		if ((*DialogJson)->TryGetBoolField(TEXT("closeOnOverlayClick"), bCloseOnOverlayClick))
		{
			Params.bCloseOnOverlayClick = bCloseOnOverlayClick;
		}

		FString OverlayClickAction;
		if ((*DialogJson)->TryGetStringField(TEXT("overlayClickAction"), OverlayClickAction))
		{
			Params.OverlayClickAction = OverlayClickAction;
		}

		// Parse buttons array
		const TArray<TSharedPtr<FJsonValue>>* ButtonsArray;
		if ((*DialogJson)->TryGetArrayField(TEXT("buttons"), ButtonsArray))
		{
			for (const TSharedPtr<FJsonValue>& ButtonValue : *ButtonsArray)
			{
				const TSharedPtr<FJsonObject> ButtonJson = ButtonValue->AsObject();
				if (ButtonJson.IsValid())
				{
					Params.Buttons.Add(ParseButtonConfig(ButtonJson));
				}
			}
		}

		UE_LOG(LogDialogAppliers, Verbose, TEXT("Parsed dialog params: Title='%s', %d buttons"),
			*Params.Title.ToString(), Params.Buttons.Num());

		return Params;
	}
}

namespace LayoutDialogAppliers
{
	void ApplyDialogProperties(UProjectDialogWidget* Dialog, const TSharedPtr<FJsonObject>& JsonObject,
		UProjectUIThemeData* Theme)
	{
		if (!Dialog || !JsonObject.IsValid())
		{
			return;
		}

		// Parse and apply params
		FProjectDialogParams Params = ParseDialogParams(JsonObject);
		Dialog->SetTheme(Theme);
		Dialog->ApplyParams(Params);

		UE_LOG(LogDialogAppliers, Display, TEXT("Applied dialog properties: %s"), *Dialog->GetName());
	}
}
