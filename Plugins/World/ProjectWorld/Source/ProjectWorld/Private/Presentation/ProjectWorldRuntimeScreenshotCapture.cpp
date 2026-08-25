// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"

#include "Presentation/ProjectWorldScreenshotValidation.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"

bool ProjectWorldRuntimeScreenshotCapture::CapturePlayerContext(
	UWorld& World,
	APlayerController& PlayerController,
	const FString& Path,
	FString& OutError)
{
	const APawn* Pawn = PlayerController.GetPawn();
	if (!FApp::CanEverRender() || World.Scene == nullptr || Pawn == nullptr)
	{
		OutError = TEXT("The product context has no render-capable scene or production pawn.");
		return false;
	}
	World.UpdateWorldComponents(false, false);
	World.SendAllEndOfFrameUpdates();

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(1920, 1080);
	RenderTarget->UpdateResourceImmediate(true);
	RenderTarget->AddToRoot();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	ASceneCapture2D* CaptureActor = World.SpawnActor<ASceneCapture2D>(SpawnParameters);
	if (CaptureActor == nullptr)
	{
		RenderTarget->RemoveFromRoot();
		OutError = TEXT("The transient product-view capture actor could not be created.");
		return false;
	}
	USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
	Capture->TextureTarget = RenderTarget;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	const FVector TargetLocation = Pawn->GetActorLocation();
	const FVector CaptureLocation = TargetLocation + FVector(-20000.0f, -20000.0f, 15000.0f);
	const FRotator CaptureRotation = (TargetLocation - CaptureLocation).Rotation();
	CaptureActor->SetActorLocationAndRotation(CaptureLocation, CaptureRotation);
	Capture->FOVAngle = 70.0f;
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		Capture->CaptureScene();
		FlushRenderingCommands();
	}

	FImage Image;
	bool bSucceeded = FImageUtils::GetRenderTargetImage(RenderTarget, Image);
	if (bSucceeded)
	{
		Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		bSucceeded = ProjectWorldScreenshotValidation::ValidatePixels(
			Image.SizeX, Image.SizeY, Image.AsBGRA8(), OutError);
	}
	if (bSucceeded)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		bSucceeded = FImageUtils::SaveImageByExtension(*Path, Image);
		if (!bSucceeded)
		{
			OutError = TEXT("The validated product-view capture could not be written.");
		}
	}
	else if (OutError.IsEmpty())
	{
		OutError = TEXT("The product-view render target could not be read.");
	}
	if (!bSucceeded)
	{
		OutError += FString::Printf(
			TEXT(" Camera=%s rotation=%s source=%s."),
			*CaptureLocation.ToCompactString(),
			*CaptureRotation.ToCompactString(),
			TEXT("production_pawn_context"));
	}

	World.DestroyActor(CaptureActor);
	RenderTarget->RemoveFromRoot();
	return bSucceeded;
}
