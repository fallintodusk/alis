#include "ProjectSettingsUIModule.h"
#include "ProjectSettingsRootWidget.h"
#include "Subsystems/ProjectUIFactorySubsystem.h"
#include "Blueprint/UserWidget.h"

#define LOCTEXT_NAMESPACE "FProjectSettingsUIModule"

void FProjectSettingsUIModule::StartupModule()
{
	// Stub: will contain settings screens
}

void FProjectSettingsUIModule::ShutdownModule()
{
}

UUserWidget* FProjectSettingsUIModule::CreateSettingsRoot(UObject* Outer)
{
	if (!Outer)
	{
		return nullptr;
	}

	UWorld* World = Outer->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UProjectUIFactorySubsystem* Factory = GameInstance->GetSubsystem<UProjectUIFactorySubsystem>())
		{
			return Factory->CreateWidgetByClass(Outer, UProjectSettingsRootWidget::StaticClass());
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSettingsUIModule, ProjectSettingsUI)
