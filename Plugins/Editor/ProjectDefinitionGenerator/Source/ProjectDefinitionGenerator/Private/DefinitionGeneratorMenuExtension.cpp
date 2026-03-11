// Copyright ALIS. All Rights Reserved.

#include "DefinitionGeneratorSubsystem.h"
#include "DefinitionGeneratorEditorSubsystem.h"

#include "ToolMenus.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "DefinitionGeneratorMenu"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionGeneratorMenu, Log, All);

/**
 * Commands for Tools -> PROJECT -> Definitions menu.
 */
class FDefinitionGeneratorCommands : public TCommands<FDefinitionGeneratorCommands>
{
public:
	FDefinitionGeneratorCommands()
		: TCommands<FDefinitionGeneratorCommands>(
			TEXT("DefinitionGenerator"),
			LOCTEXT("DefinitionGeneratorCommands", "Definition Generator"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(ValidateAll, "Validate All", "Check if generated assets match JSON source for all types", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(RegenerateAllChanged, "Regenerate All Changed", "Regenerate only out-of-date definitions for all types", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(RegenerateAllForce, "Regenerate All (Force)", "Force regenerate all definitions", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(CleanupAllOrphans, "Cleanup All Orphans", "Delete orphaned assets for all types", EUserInterfaceActionType::Button, FInputChord());
	}

	TSharedPtr<FUICommandInfo> ValidateAll;
	TSharedPtr<FUICommandInfo> RegenerateAllChanged;
	TSharedPtr<FUICommandInfo> RegenerateAllForce;
	TSharedPtr<FUICommandInfo> CleanupAllOrphans;
};

namespace DefinitionGeneratorMenu
{
	static TSharedPtr<FUICommandList> CommandList;

	void ExecuteValidateAll()
	{
		if (UDefinitionGeneratorEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>())
		{
			TArray<FDefinitionValidationResult> Results = Subsystem->ValidateAll();

			int32 OutOfDate = 0;
			for (const FDefinitionValidationResult& Result : Results)
			{
				if (Result.bNeedsRegeneration)
				{
					OutOfDate++;
				}
			}

			FNotificationInfo Info(FText::Format(
				LOCTEXT("ValidateResult", "Definitions: {0} total, {1} need regeneration"),
				FText::AsNumber(Results.Num()),
				FText::AsNumber(OutOfDate)
			));
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	void ExecuteRegenerateAllChanged()
	{
		UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
		UDefinitionGeneratorEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>() : nullptr;

		if (!Generator || !EditorSubsystem)
		{
			return;
		}

		int32 TotalCount = 0;
		TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
		for (const FString& TypeName : TypeNames)
		{
			TotalCount += EditorSubsystem->RegenerateChanged(TypeName);
		}

		FNotificationInfo Info(FText::Format(
			LOCTEXT("RegenerateChangedResult", "Regenerated {0} definitions"),
			FText::AsNumber(TotalCount)
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	void ExecuteRegenerateAllForce()
	{
		UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
		UDefinitionGeneratorEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>() : nullptr;

		if (!Generator || !EditorSubsystem)
		{
			return;
		}

		int32 TotalCount = 0;
		TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
		for (const FString& TypeName : TypeNames)
		{
			TotalCount += EditorSubsystem->RegenerateAll(TypeName);
		}

		FNotificationInfo Info(FText::Format(
			LOCTEXT("RegenerateAllResult", "Regenerated {0} definitions"),
			FText::AsNumber(TotalCount)
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	void ExecuteCleanupAllOrphans()
	{
		UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
		UDefinitionGeneratorEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>() : nullptr;

		if (!Generator || !EditorSubsystem)
		{
			return;
		}

		int32 TotalCount = 0;
		TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
		for (const FString& TypeName : TypeNames)
		{
			TotalCount += EditorSubsystem->CleanupOrphans(TypeName);
		}

		FNotificationInfo Info(FText::Format(
			LOCTEXT("CleanupOrphansResult", "Deleted {0} orphaned assets"),
			FText::AsNumber(TotalCount)
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// Per-type menu actions
	void ExecuteValidateType(FString TypeName)
	{
		if (UDefinitionGeneratorEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>())
		{
			TArray<FDefinitionValidationResult> Results = Subsystem->ValidateType(TypeName);

			int32 OutOfDate = 0;
			for (const FDefinitionValidationResult& Result : Results)
			{
				if (Result.bNeedsRegeneration)
				{
					OutOfDate++;
				}
			}

			FNotificationInfo Info(FText::Format(
				LOCTEXT("ValidateTypeResult", "[{0}] {1} total, {2} need regeneration"),
				FText::FromString(TypeName),
				FText::AsNumber(Results.Num()),
				FText::AsNumber(OutOfDate)
			));
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	void ExecuteRegenerateChangedType(FString TypeName)
	{
		if (UDefinitionGeneratorEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>())
		{
			const int32 Count = Subsystem->RegenerateChanged(TypeName);

			FNotificationInfo Info(FText::Format(
				LOCTEXT("RegenerateChangedTypeResult", "[{0}] Regenerated {1}"),
				FText::FromString(TypeName),
				FText::AsNumber(Count)
			));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	void ExecuteRegenerateAllType(FString TypeName)
	{
		if (UDefinitionGeneratorEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>())
		{
			const int32 Count = Subsystem->RegenerateAll(TypeName);

			FNotificationInfo Info(FText::Format(
				LOCTEXT("RegenerateAllTypeResult", "[{0}] Force regenerated {1}"),
				FText::FromString(TypeName),
				FText::AsNumber(Count)
			));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	void ExecuteCleanupOrphansType(FString TypeName)
	{
		if (UDefinitionGeneratorEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>())
		{
			const int32 Count = Subsystem->CleanupOrphans(TypeName);

			FNotificationInfo Info(FText::Format(
				LOCTEXT("CleanupOrphansTypeResult", "[{0}] Deleted {1} orphaned assets"),
				FText::FromString(TypeName),
				FText::AsNumber(Count)
			));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	void ExecuteOpenSourceFolderType(FString TypeName)
	{
		if (UDefinitionGeneratorEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>())
		{
			Subsystem->OpenSourceFolder(TypeName);
		}
	}

	void RegisterMenus(void* Owner)
	{
		FDefinitionGeneratorCommands::Register();

		CommandList = MakeShareable(new FUICommandList);

		CommandList->MapAction(
			FDefinitionGeneratorCommands::Get().ValidateAll,
			FExecuteAction::CreateStatic(&ExecuteValidateAll)
		);
		CommandList->MapAction(
			FDefinitionGeneratorCommands::Get().RegenerateAllChanged,
			FExecuteAction::CreateStatic(&ExecuteRegenerateAllChanged)
		);
		CommandList->MapAction(
			FDefinitionGeneratorCommands::Get().RegenerateAllForce,
			FExecuteAction::CreateStatic(&ExecuteRegenerateAllForce)
		);
		CommandList->MapAction(
			FDefinitionGeneratorCommands::Get().CleanupAllOrphans,
			FExecuteAction::CreateStatic(&ExecuteCleanupAllOrphans)
		);

		FToolMenuOwnerScoped OwnerScoped(Owner);

		UToolMenus* ToolMenus = UToolMenus::Get();

		// Get or create Tools menu
		UToolMenu* ToolsMenu = ToolMenus->ExtendMenu("LevelEditor.MainMenu.Tools");

		// Add PROJECT section if it doesn't exist
		FToolMenuSection& ProjectSection = ToolsMenu->FindOrAddSection("PROJECT");
		ProjectSection.Label = LOCTEXT("ProjectSectionLabel", "PROJECT");

		// Add Definitions submenu under PROJECT section
		ProjectSection.AddSubMenu(
			"Definitions",
			LOCTEXT("DefinitionsSubMenuLabel", "Definitions"),
			LOCTEXT("DefinitionsSubMenuTooltip", "Definition generation and validation"),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				// ALL TYPES section
				FToolMenuSection& AllSection = SubMenu->AddSection("AllTypes", LOCTEXT("AllTypesSection", "All Types"));
				AllSection.AddMenuEntryWithCommandList(
					FDefinitionGeneratorCommands::Get().ValidateAll,
					CommandList
				);
				AllSection.AddMenuEntryWithCommandList(
					FDefinitionGeneratorCommands::Get().RegenerateAllChanged,
					CommandList
				);
				AllSection.AddMenuEntryWithCommandList(
					FDefinitionGeneratorCommands::Get().RegenerateAllForce,
					CommandList
				);
				AllSection.AddMenuEntryWithCommandList(
					FDefinitionGeneratorCommands::Get().CleanupAllOrphans,
					CommandList
				);

				// PER-TYPE submenus (dynamically generated)
				UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
				if (Generator)
				{
					TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
					if (TypeNames.Num() > 0)
					{
						FToolMenuSection& TypesSection = SubMenu->AddSection("PerType", LOCTEXT("PerTypeSection", "By Type"));

						for (const FString& TypeName : TypeNames)
						{
							TypesSection.AddSubMenu(
								FName(*TypeName),
								FText::FromString(TypeName),
								FText::Format(LOCTEXT("TypeSubmenuTooltip", "Operations for {0} definitions"), FText::FromString(TypeName)),
								FNewToolMenuDelegate::CreateLambda([TypeName](UToolMenu* TypeSubMenu)
								{
									FToolMenuSection& Section = TypeSubMenu->AddSection("Operations");

									Section.AddMenuEntry(
										FName(*(TypeName + TEXT("_Validate"))),
										LOCTEXT("ValidateEntry", "Validate"),
										LOCTEXT("ValidateTooltip", "Check if generated assets match JSON source"),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic(&ExecuteValidateType, TypeName))
									);

									Section.AddMenuEntry(
										FName(*(TypeName + TEXT("_RegenerateChanged"))),
										LOCTEXT("RegenerateChangedEntry", "Regenerate Changed"),
										LOCTEXT("RegenerateChangedTooltip", "Regenerate only out-of-date definitions"),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic(&ExecuteRegenerateChangedType, TypeName))
									);

									Section.AddMenuEntry(
										FName(*(TypeName + TEXT("_RegenerateAll"))),
										LOCTEXT("RegenerateAllEntry", "Regenerate All"),
										LOCTEXT("RegenerateAllTooltip", "Force regenerate all definitions"),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic(&ExecuteRegenerateAllType, TypeName))
									);

									Section.AddMenuEntry(
										FName(*(TypeName + TEXT("_CleanupOrphans"))),
										LOCTEXT("CleanupOrphansEntry", "Cleanup Orphans"),
										LOCTEXT("CleanupOrphansTooltip", "Delete orphaned assets"),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic(&ExecuteCleanupOrphansType, TypeName))
									);

									Section.AddSeparator(FName(*(TypeName + TEXT("_Separator"))));

									Section.AddMenuEntry(
										FName(*(TypeName + TEXT("_OpenFolder"))),
										LOCTEXT("OpenFolderEntry", "Open Source Folder"),
										LOCTEXT("OpenFolderTooltip", "Open the JSON source folder in file explorer"),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic(&ExecuteOpenSourceFolderType, TypeName))
									);
								})
							);
						}
					}
				}
			})
		);

		UE_LOG(LogDefinitionGeneratorMenu, Log, TEXT("Registered PROJECT -> Definitions menu"));
	}

	void UnregisterMenus()
	{
		FDefinitionGeneratorCommands::Unregister();
		CommandList.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
