// Copyright ALIS. All Rights Reserved.

#include "ProjectUI.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FProjectUIModule"

FString FProjectUIModule::FontsDir;

void FProjectUIModule::StartupModule()
{
	// Initialize fonts directory from plugin path
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProjectUI"));
	if (Plugin.IsValid())
	{
		FontsDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Slate/Fonts"));
		UE_LOG(LogTemp, Display, TEXT("ProjectUI: Fonts dir = %s"), *FontsDir);
	}
	else
	{
		// Fallback: use engine fonts if plugin not found
		FontsDir = FPaths::EngineContentDir() / TEXT("Slate/Fonts");
		UE_LOG(LogTemp, Warning, TEXT("ProjectUI: Plugin not found, using engine fonts"));
	}

	UE_LOG(LogTemp, Display, TEXT("ProjectUI module started"));
}

void FProjectUIModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("ProjectUI module shutdown"));
}

const FString& FProjectUIModule::GetFontsDir()
{
	return FontsDir;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectUIModule, ProjectUI)
