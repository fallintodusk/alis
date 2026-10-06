// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "GameMapsSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldUncookedSession, Log, All);

namespace
{
	void MaintainUncookedViewport()
	{
		if (GEditor != nullptr && GEditor->PlayWorld != nullptr && GEngine->GameViewport != nullptr)
		{
			if (FSceneViewport* Viewport = GEngine->GameViewport->GetGameViewport())
			{
				const FIntPoint RequiredSize(2560, 1440);
				if (Viewport->GetSizeXY() != RequiredSize)
				{
					UE_LOG(LogProjectWorldUncookedSession, Display, TEXT("[UncookedSession] Render target resized - previous=%s required=%s"),
						*Viewport->GetSizeXY().ToString(), *RequiredSize.ToString());
					Viewport->SetFixedViewportSize(RequiredSize.X, RequiredSize.Y);
				}
			}
		}
	}

	class FProjectWorldAwaitUncookedProductExit : public IAutomationLatentCommand
	{
	public:
		bool Update() override
		{
			MaintainUncookedViewport();
			// Keep automation from ending PIE before the native product gates exit.
			// The operation wrapper owns the bounded process timeout.
			return GEditor == nullptr || GEditor->PlayWorld == nullptr;
		}
	};

	class FProjectWorldStartUncookedSession : public FStartPIEForAutomationCommand
	{
	public:
		explicit FProjectWorldStartUncookedSession(const FRequestPlaySessionParams& Params)
			: FStartPIEForAutomationCommand(Params), Settings(Params.EditorPlaySettings.Get()) {}

		bool Update() override
		{
			const bool bReady = FStartPIEForAutomationCommand::Update();
			// Configure before match readiness; graphics application and travel may resize it.
			MaintainUncookedViewport();
			return bReady;
		}

	private:
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldUncookedSessionTest,
	"Project.World.ProductRoute.UncookedSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldUncookedSessionTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->PlayWorld != nullptr ||
		!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldProductRouteGate")) ||
		!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldPlayableTour")))
	{
		AddError(TEXT("Uncooked session requires a fresh Editor and the product playable-tour gates."));
		return false;
	}
	const FString MenuMap = FPackageName::ObjectPathToPackageName(
		GetDefault<UGameMapsSettings>()->GetGameDefaultMap());
	FAutomationEditorCommonUtils::LoadMap(MenuMap);
	ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(PIE_Standalone);
	Settings->SetPlayNumberOfClients(1);
	Settings->SetRunUnderOneProcess(true);
	Settings->bLaunchSeparateServer = false;
	Settings->NewWindowWidth = 2560;
	Settings->NewWindowHeight = 1440;
	FRequestPlaySessionParams Request;
	Request.EditorPlaySettings = Settings;
	Request.bAllowOnlineSubsystem = false;
	Request.GlobalMapOverride = MenuMap;
	FEditorDelegates::OnEditorPreExit.AddLambda([]()
	{
		if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
		{
			// Close PIE before UEditorEngine::PreExit deinitializes editor subsystems.
			GEditor->EndPlayMap();
		}
	});
	// Native product gates own traversal completion and process exit; this test
	// only starts the real single-player session, without saving play preferences.
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<FProjectWorldStartUncookedSession>(Request));
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<FProjectWorldAwaitUncookedProductExit>());
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldUncookedSessionTest,
	"Project.World.ProductRoute.UncookedSession", "[Slow][Integration][World]")

#endif
