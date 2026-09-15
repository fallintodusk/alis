// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "SinglePlayTraversalPolicy.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectUIRegistrySubsystem.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWidget* FindWidgetByName(UWidget* Root, FName Name)
	{
		if (Root == nullptr || Root->GetFName() == Name)
		{
			return Root;
		}

		UPanelWidget* Panel = Cast<UPanelWidget>(Root);
		if (Panel == nullptr)
		{
			return nullptr;
		}

		for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
		{
			if (UWidget* Match = FindWidgetByName(Panel->GetChildAt(ChildIndex), Name))
			{
				return Match;
			}
		}
		return nullptr;
	}

	class FPreviewFlightUIBehaviorCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FPreviewFlightUIBehaviorCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			UWorld* World = AutomationCommon::GetAnyGameWorld();
			UGameInstance* GameInstance = World == nullptr ? nullptr : World->GetGameInstance();
			APlayerController* PlayerController = World == nullptr
				? nullptr
				: World->GetFirstPlayerController();
			if (GameInstance == nullptr || PlayerController == nullptr)
			{
				if (++WaitFrames > 600)
				{
					Test.AddError(TEXT("Timed out waiting for the PreviewFlight UI test world."));
					return true;
				}
				return false;
			}

			UProjectUIRegistrySubsystem* Registry =
				GameInstance->GetSubsystem<UProjectUIRegistrySubsystem>();
			UProjectUILayerHostSubsystem* LayerHost =
				GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
			if (!Test.TestNotNull(TEXT("ProjectUI registry discovery is available."), Registry) ||
				!Test.TestNotNull(TEXT("ProjectUI layer host is available."), LayerHost))
			{
				return true;
			}
			Test.TestNotNull(TEXT("Registry discovery resolves the PreviewFlight definition."),
				Registry->FindDefinition(TEXT("ProjectSinglePlay.PreviewFlightHints")));

			LayerHost->InitializeForPlayer(PlayerController);
			LayerHost->HideDefinition(TEXT("ProjectSinglePlay.PreviewFlightHints"));
			UUserWidget* PreviewFlightWidget =
				LayerHost->ShowDefinition(TEXT("ProjectSinglePlay.PreviewFlightHints"));
			if (!Test.TestNotNull(
				TEXT("ShowDefinition creates the PreviewFlight widget."), PreviewFlightWidget))
			{
				return true;
			}
			Test.TestTrue(TEXT("The PreviewFlight widget is visible in the viewport."),
				PreviewFlightWidget->IsInViewport() && PreviewFlightWidget->IsVisible());
			LayerHost->HideDefinition(TEXT("ProjectSinglePlay.PreviewFlightHints"));
			Test.TestFalse(TEXT("HideDefinition removes the PreviewFlight widget from view."),
				PreviewFlightWidget->IsVisible());
			return true;
		}

	private:
		FAutomationTestBase& Test;
		int32 WaitFrames = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayTraversalPolicyResolutionTest,
	"ProjectIntegrationTests.SinglePlay.Traversal.PolicyResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayTraversalPolicyResolutionTest::RunTest(const FString& Parameters)
{
	const FSinglePlayTraversalSelection Absent = ProjectSinglePlayTraversal::Resolve(TEXT(""));
	TestEqual(TEXT("An absent option preserves default traversal."),
		Absent.Mode, ESinglePlayTraversalMode::Default);
	TestEqual(TEXT("Absence is an expected silent result."),
		Absent.ParseResult, ESinglePlayTraversalParseResult::Absent);

	const FSinglePlayTraversalSelection PreviewFlight =
		ProjectSinglePlayTraversal::Resolve(TEXT("PreviewFlight"));
	TestEqual(TEXT("The supported value selects preview flight."),
		PreviewFlight.Mode, ESinglePlayTraversalMode::PreviewFlight);
	TestEqual(TEXT("The supported value is recognized."),
		PreviewFlight.ParseResult, ESinglePlayTraversalParseResult::Supported);

	const FSinglePlayTraversalSelection Unknown =
		ProjectSinglePlayTraversal::Resolve(TEXT("previewflight"));
	TestEqual(TEXT("Unknown values fail closed."), Unknown.Mode, ESinglePlayTraversalMode::Default);
	TestEqual(TEXT("Unknown values remain distinguishable for owner-scoped warning."),
		Unknown.ParseResult, ESinglePlayTraversalParseResult::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayTraversalPolicyLifecycleTest,
	"ProjectIntegrationTests.SinglePlay.Traversal.SpawnLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayTraversalPolicyLifecycleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Default traversal is a no-op for a missing pawn."),
		ProjectSinglePlayTraversal::Apply(nullptr, ESinglePlayTraversalMode::Default));
	TestFalse(TEXT("Preview flight fails closed for a missing pawn."),
		ProjectSinglePlayTraversal::Apply(nullptr, ESinglePlayTraversalMode::PreviewFlight));

	FString Source;
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayerGameMode.cpp"));
	TestTrue(TEXT("The SinglePlay lifecycle owner is readable."),
		FFileHelper::LoadFileToString(Source, *SourcePath));
	TestTrue(TEXT("Spawn and respawn initialization reapplies the generic traversal policy."),
		Source.Contains(TEXT("ProjectSinglePlayTraversal::Apply(Pawn, TraversalMode)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayPreviewFlightPresentationContractTest,
	"ProjectIntegrationTests.SinglePlay.Traversal.PreviewFlightPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
		EAutomationTestFlags::ProductFilter)

bool FSinglePlayPreviewFlightPresentationContractTest::RunTest(const FString& Parameters)
{
	FString PresentationSource;
	const FString PresentationSourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Private/Scenario/SinglePlayScenarioPresentationComponent.cpp"));
	TestTrue(TEXT("The PreviewFlight presentation owner is readable."),
		FFileHelper::LoadFileToString(PresentationSource, *PresentationSourcePath));
	TestTrue(TEXT("PreviewFlight uses a persistent registered UI definition."),
		PresentationSource.Contains(TEXT("LayerHost->ShowDefinition(PreviewFlightHintsDefinitionId)")));
	TestTrue(TEXT("PreviewFlight initializes ProjectUI for its owning player."),
		PresentationSource.Contains(TEXT("LayerHost->InitializeForPlayer(PlayerController)")));
	TestTrue(TEXT("PreviewFlight presentation is removed at teardown."),
		PresentationSource.Contains(TEXT("LayerHost->HideDefinition(PreviewFlightHintsDefinitionId)")));
	TestFalse(TEXT("PreviewFlight controls are not routed through the transient toast queue."),
		PresentationSource.Contains(TEXT("PREVIEW FLIGHT -")));

	FString Definitions;
	const FString DefinitionsPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("Gameplay/ProjectSinglePlay/Data/ui_definitions.json"));
	TestTrue(TEXT("The PreviewFlight UI definition is readable."),
		FFileHelper::LoadFileToString(Definitions, *DefinitionsPath));
	TestTrue(TEXT("The PreviewFlight UI definition is registered."),
		Definitions.Contains(TEXT("ProjectSinglePlay.PreviewFlightHints")));

	FString Layout;
	const FString LayoutPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("Gameplay/ProjectSinglePlay/Data/PreviewFlightHints.json"));
	TestTrue(TEXT("The PreviewFlight control layout is readable."),
		FFileHelper::LoadFileToString(Layout, *LayoutPath));
	TestTrue(TEXT("The control layout names movement and look."),
		Layout.Contains(TEXT("MOUSE")) && Layout.Contains(TEXT("WASD")));
	TestTrue(TEXT("The control layout names vertical flight and acceleration."),
		Layout.Contains(TEXT("SPACE")) && Layout.Contains(TEXT("LEFT CTRL")) &&
		Layout.Contains(TEXT("LEFT SHIFT")));

	UWidget* Root = UProjectWidgetLayoutLoader::LoadLayoutFromFile(
		GetTransientPackage(), LayoutPath);
	if (!TestNotNull(TEXT("The PreviewFlight layout builds a widget tree."), Root))
	{
		return false;
	}

	UWidget* ControlsPanel = FindWidgetByName(Root, TEXT("FlightControlsPanel"));
	if (!TestNotNull(TEXT("The built widget tree contains the controls panel."), ControlsPanel))
	{
		return false;
	}
	UCanvasPanelSlot* ControlsSlot = Cast<UCanvasPanelSlot>(ControlsPanel->Slot);
	if (TestNotNull(TEXT("The controls panel uses a viewport canvas slot."), ControlsSlot))
	{
		TestEqual(TEXT("The controls panel is anchored at the lower-right edge."),
			ControlsSlot->GetAnchors().Minimum, FVector2D(1.0f, 1.0f));
		TestTrue(TEXT("The controls panel sizes to its compact content."), ControlsSlot->GetAutoSize());
	}

	UTextBlock* Title = Cast<UTextBlock>(FindWidgetByName(Root, TEXT("FlightControlsTitle")));
	if (TestNotNull(TEXT("The built widget tree contains the controls title."), Title))
	{
		TestTrue(TEXT("The controls title resolves a readable theme font."), Title->GetFont().Size > 0);
	}
	if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
	{
		AddError(TEXT("Failed to open MainMenu_Persistent for PreviewFlight UI test."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FPreviewFlightUIBehaviorCommand(*this));
	return true;
}

#endif
