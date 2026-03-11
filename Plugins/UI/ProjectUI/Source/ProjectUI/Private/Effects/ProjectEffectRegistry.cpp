// Copyright ALIS. All Rights Reserved.

#include "Effects/ProjectEffectRegistry.h"
#include "Effects/ProjectEffectWidget.h"
#include "Effects/ProjectFireSparksEffect.h"
#include "Theme/ProjectUIThemeData.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectEffectRegistry, Log, All);

// Effect factory function signature
using FEffectFactory = TFunction<UProjectEffectWidget*(UObject*, FName)>;

// =============================================================================
// Internal Helpers (anonymous namespace)
// =============================================================================
namespace
{
	// Helper to parse color from JSON (supports theme references)
	FLinearColor ParseEffectColor(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName,
		const FLinearColor& Default, UProjectUIThemeData* Theme)
	{
		if (!JsonObject->HasField(FieldName))
		{
			return Default;
		}

		// Check if it's a string (theme reference)
		FString ColorRef;
		if (JsonObject->TryGetStringField(FieldName, ColorRef))
		{
			if (Theme)
			{
				// Resolve theme color from Colors struct
				if (ColorRef == TEXT("Primary")) return Theme->Colors.Primary;
				if (ColorRef == TEXT("Secondary")) return Theme->Colors.Secondary;
				if (ColorRef == TEXT("Background")) return Theme->Colors.Background;
				if (ColorRef == TEXT("Surface")) return Theme->Colors.Surface;
				if (ColorRef == TEXT("Error")) return Theme->Colors.Error;
				if (ColorRef == TEXT("Warning")) return Theme->Colors.Warning;
				if (ColorRef == TEXT("Success")) return Theme->Colors.Success;
				if (ColorRef == TEXT("Text")) return Theme->Colors.TextPrimary;
				if (ColorRef == TEXT("TextSecondary")) return Theme->Colors.TextSecondary;
				if (ColorRef == TEXT("Border")) return Theme->Colors.Border;
			}
			UE_LOG(LogProjectEffectRegistry, Warning, TEXT("Unknown theme color: %s"), *ColorRef);
			return Default;
		}

		// Parse as object {r, g, b, a}
		const TSharedPtr<FJsonObject>* ColorObject;
		if (JsonObject->TryGetObjectField(FieldName, ColorObject))
		{
			double R = Default.R, G = Default.G, B = Default.B, A = Default.A;
			(*ColorObject)->TryGetNumberField(TEXT("r"), R);
			(*ColorObject)->TryGetNumberField(TEXT("g"), G);
			(*ColorObject)->TryGetNumberField(TEXT("b"), B);
			(*ColorObject)->TryGetNumberField(TEXT("a"), A);
			return FLinearColor(static_cast<float>(R), static_cast<float>(G),
				static_cast<float>(B), static_cast<float>(A));
		}

		return Default;
	}

	// Helper to parse FVector2D from JSON
	FVector2D ParseVector2D(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName,
		const FVector2D& Default)
	{
		const TSharedPtr<FJsonObject>* VecObject;
		if (!JsonObject->TryGetObjectField(FieldName, VecObject))
		{
			return Default;
		}

		double X = Default.X, Y = Default.Y;
		(*VecObject)->TryGetNumberField(TEXT("x"), X);
		(*VecObject)->TryGetNumberField(TEXT("y"), Y);
		return FVector2D(static_cast<float>(X), static_cast<float>(Y));
	}

	// Apply JSON properties to fire sparks effect
	void ApplyFireSparksProperties(UProjectFireSparksEffect* Effect, const TSharedPtr<FJsonObject>& JsonObject,
		UProjectUIThemeData* Theme)
	{
		if (!Effect || !JsonObject.IsValid())
		{
			return;
		}

		// Get the "effect" sub-object
		const TSharedPtr<FJsonObject>* EffectJson;
		if (!JsonObject->TryGetObjectField(TEXT("effect"), EffectJson))
		{
			return;
		}

		FProjectFireSparksParams Params = Effect->GetSparksParams();

		// Base params
		bool bEnabled = true;
		if ((*EffectJson)->TryGetBoolField(TEXT("enabled"), bEnabled))
		{
			Params.bEnabled = bEnabled;
		}

		double Opacity = 1.0;
		if ((*EffectJson)->TryGetNumberField(TEXT("opacity"), Opacity))
		{
			Params.Opacity = static_cast<float>(Opacity);
		}

		// Sparks-specific params
		int32 MaxSparks = 0;
		if ((*EffectJson)->TryGetNumberField(TEXT("maxSparks"), MaxSparks))
		{
			Params.MaxSparks = MaxSparks;
		}

		double SpawnRate = 0.0;
		if ((*EffectJson)->TryGetNumberField(TEXT("spawnRate"), SpawnRate))
		{
			Params.SpawnRate = static_cast<float>(SpawnRate);
		}

		// Spawn area
		Params.SpawnAreaMin = ParseVector2D(*EffectJson, TEXT("spawnAreaMin"), Params.SpawnAreaMin);
		Params.SpawnAreaMax = ParseVector2D(*EffectJson, TEXT("spawnAreaMax"), Params.SpawnAreaMax);

		// Lifetime
		double MinLifetime = 0.0;
		if ((*EffectJson)->TryGetNumberField(TEXT("minLifetime"), MinLifetime))
		{
			Params.MinLifetime = static_cast<float>(MinLifetime);
		}

		double MaxLifetime = 0.0;
		if ((*EffectJson)->TryGetNumberField(TEXT("maxLifetime"), MaxLifetime))
		{
			Params.MaxLifetime = static_cast<float>(MaxLifetime);
		}

		// Velocity
		Params.MinVelocity = ParseVector2D(*EffectJson, TEXT("minVelocity"), Params.MinVelocity);
		Params.MaxVelocity = ParseVector2D(*EffectJson, TEXT("maxVelocity"), Params.MaxVelocity);
		Params.Gravity = ParseVector2D(*EffectJson, TEXT("gravity"), Params.Gravity);

		// Appearance
		Params.PixelSize = ParseVector2D(*EffectJson, TEXT("pixelSize"), Params.PixelSize);
		Params.SparkColor = ParseEffectColor(*EffectJson, TEXT("sparkColor"), Params.SparkColor, Theme);
		Params.EndColor = ParseEffectColor(*EffectJson, TEXT("endColor"), Params.EndColor, Theme);

		// Rotation
		bool bEnableRotation = Params.bEnableRotation;
		if ((*EffectJson)->TryGetBoolField(TEXT("enableRotation"), bEnableRotation))
		{
			Params.bEnableRotation = bEnableRotation;
		}

		double MaxRotationSpeed = 0.0;
		if ((*EffectJson)->TryGetNumberField(TEXT("maxRotationSpeed"), MaxRotationSpeed))
		{
			Params.MaxRotationSpeed = static_cast<float>(MaxRotationSpeed);
		}

		// Size variation
		bool bEnableSizeVariation = Params.bEnableSizeVariation;
		if ((*EffectJson)->TryGetBoolField(TEXT("enableSizeVariation"), bEnableSizeVariation))
		{
			Params.bEnableSizeVariation = bEnableSizeVariation;
		}

		double SizeVariation = 0.0;
		if ((*EffectJson)->TryGetNumberField(TEXT("sizeVariation"), SizeVariation))
		{
			Params.SizeVariation = static_cast<float>(SizeVariation);
		}

		Effect->ApplySparksParams(Params);

		UE_LOG(LogProjectEffectRegistry, Verbose, TEXT("Applied FireSparks params: maxSparks=%d, spawnRate=%.1f"),
			Params.MaxSparks, Params.SpawnRate);
	}
}

// =============================================================================
// LayoutEffectRegistry Namespace Implementation
// =============================================================================
namespace LayoutEffectRegistry
{
	// Effect type -> Factory function
	static TMap<FString, FEffectFactory> EffectFactories;

	// Flag to ensure one-time initialization
	static bool bInitialized = false;

	// Template factory for common effect creation pattern
	template<typename T>
	static UProjectEffectWidget* CreateEffect(UObject* Outer, FName WidgetName)
	{
		// Try to use WidgetTree for proper UMG lifecycle management
		if (UUserWidget* OwningWidget = Cast<UUserWidget>(Outer))
		{
			if (OwningWidget->WidgetTree)
			{
				return OwningWidget->WidgetTree->ConstructWidget<T>(T::StaticClass(), WidgetName);
			}
		}
		// Fallback to NewObject (for non-UserWidget contexts)
		return NewObject<T>(Outer, WidgetName);
	}

	void Initialize()
	{
		if (bInitialized)
		{
			return;
		}

		// Register effect factories
		// To add new effects: add another line here following this pattern
		EffectFactories.Add(TEXT("FireSparks"), &CreateEffect<UProjectFireSparksEffect>);

		bInitialized = true;

		UE_LOG(LogProjectEffectRegistry, Log, TEXT("Effect registry initialized: %d effect types"),
			EffectFactories.Num());
	}

	UProjectEffectWidget* CreateEffectByType(const FString& Type, UObject* Outer, FName WidgetName)
	{
		Initialize();

		if (const FEffectFactory* Factory = EffectFactories.Find(Type))
		{
			UE_LOG(LogProjectEffectRegistry, Verbose, TEXT("Creating effect: %s (name: %s)"),
				*Type, *WidgetName.ToString());
			return (*Factory)(Outer, WidgetName);
		}

		UE_LOG(LogProjectEffectRegistry, Warning, TEXT("Unknown effect type: %s. Registered types: %s"),
			*Type, *FString::Join(GetRegisteredTypes(), TEXT(", ")));
		return nullptr;
	}

	TArray<FString> GetRegisteredTypes()
	{
		Initialize();
		TArray<FString> Types;
		EffectFactories.GetKeys(Types);
		return Types;
	}

	bool IsTypeRegistered(const FString& Type)
	{
		Initialize();
		return EffectFactories.Contains(Type);
	}
}

// =============================================================================
// UProjectEffectRegistry Implementation
// =============================================================================

UProjectEffectWidget* UProjectEffectRegistry::CreateEffect(UObject* Outer, const FString& EffectType, FName WidgetName)
{
	return LayoutEffectRegistry::CreateEffectByType(EffectType, Outer, WidgetName);
}

UProjectEffectWidget* UProjectEffectRegistry::CreateEffectFromJson(UObject* Outer,
	const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogProjectEffectRegistry, Error, TEXT("CreateEffectFromJson: Invalid JSON object"));
		return nullptr;
	}

	FString Type;
	if (!JsonObject->TryGetStringField(TEXT("type"), Type))
	{
		UE_LOG(LogProjectEffectRegistry, Error, TEXT("Effect JSON missing 'type' field"));
		return nullptr;
	}

	FString WidgetName;
	JsonObject->TryGetStringField(TEXT("name"), WidgetName);
	FName WidgetFName = WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName);

	UProjectEffectWidget* Effect = CreateEffect(Outer, Type, WidgetFName);
	if (!Effect)
	{
		return nullptr;
	}

	ApplyEffectProperties(Effect, JsonObject, Theme);
	return Effect;
}

void UProjectEffectRegistry::ApplyEffectProperties(UProjectEffectWidget* Effect,
	const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	if (!Effect || !JsonObject.IsValid())
	{
		return;
	}

	// Dispatch to type-specific applier based on effect type
	const FString EffectType = Effect->GetEffectTypeName();

	if (EffectType == TEXT("FireSparks"))
	{
		ApplyFireSparksProperties(Cast<UProjectFireSparksEffect>(Effect), JsonObject, Theme);
	}
	// Add more effect type handlers here as needed
}

TArray<FString> UProjectEffectRegistry::GetRegisteredEffectTypes()
{
	return LayoutEffectRegistry::GetRegisteredTypes();
}

bool UProjectEffectRegistry::IsEffectTypeRegistered(const FString& EffectType)
{
	return LayoutEffectRegistry::IsTypeRegistered(EffectType);
}
