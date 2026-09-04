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

struct ProjectWorldRuntimeScreenshotCapture::FCaptureSession::FState
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ASceneCapture2D> CaptureActor;
	UTextureRenderTarget2D* RenderTarget = nullptr;
	FCaptureSpec Spec;
	FString SessionId;
	int32 WrittenCaptureCount = 0;
};

ProjectWorldRuntimeScreenshotCapture::FCaptureSession::FCaptureSession() = default;

ProjectWorldRuntimeScreenshotCapture::FCaptureSession::~FCaptureSession()
{
	Reset();
}

bool ProjectWorldRuntimeScreenshotCapture::FCaptureSession::Initialize(
	UWorld& World,
	const FCaptureSpec& Spec,
	FString& OutError)
{
	Reset();
	const bool bOrthographic = Spec.OrthographicWidthCentimeters > 0.0f;
	if (!FApp::CanEverRender() || World.Scene == nullptr || Spec.SourceIdentity.IsEmpty() ||
		(!bOrthographic && (Spec.FieldOfViewDegrees <= 0.0f || Spec.FieldOfViewDegrees >= 180.0f)))
	{
		OutError = TEXT("The screenshot capture specification or render-capable scene is invalid.");
		return false;
	}
	World.UpdateWorldComponents(false, false);
	World.SendAllEndOfFrameUpdates();

	TUniquePtr<FState> Candidate = MakeUnique<FState>();
	Candidate->World = &World;
	Candidate->Spec = Spec;
	Candidate->SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
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
		OutError = TEXT("The transient screenshot capture actor could not be created.");
		return false;
	}
	USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
	Capture->TextureTarget = RenderTarget;
	Capture->CaptureSource = Spec.CaptureSource;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	CaptureActor->SetActorLocationAndRotation(Spec.CameraLocation, Spec.CameraRotation);
	if (bOrthographic)
	{
		Capture->ProjectionType = ECameraProjectionMode::Orthographic;
		Capture->OrthoWidth = Spec.OrthographicWidthCentimeters;
	}
	else
	{
		Capture->ProjectionType = ECameraProjectionMode::Perspective;
		Capture->FOVAngle = Spec.FieldOfViewDegrees;
	}
	if (Spec.bIsolateBaseColor)
	{
		// Match UE's World Partition minimap capture: material base color and real
		// depth/occlusion remain, while lighting, atmosphere, and sky cannot become
		// parallel pixel-classification authorities.
		Capture->ShowFlags.Lighting = false;
		Capture->ShowFlags.Atmosphere = false;
		Capture->ShowFlags.PostProcessing = false;
		Capture->ShowFlags.AntiAliasing = false;
		Capture->ShowFlags.Fog = false;
		Capture->ShowFlags.VolumetricFog = false;
		Capture->ShowFlags.DynamicShadows = false;
		Capture->ShowFlags.SkyLighting = false;
	}
	Candidate->CaptureActor = CaptureActor;
	Candidate->RenderTarget = RenderTarget;
	State = MoveTemp(Candidate);
	return true;
}

bool ProjectWorldRuntimeScreenshotCapture::FCaptureSession::WarmUp(
	int32 FrameCount,
	FString& OutError)
{
	if (!IsInitialized() || FrameCount < 0)
	{
		OutError = TEXT("The screenshot capture session is unavailable or the warm-up count is invalid.");
		return false;
	}
	USceneCaptureComponent2D* CaptureComponent = State->CaptureActor->GetCaptureComponent2D();
	for (int32 Frame = 0; Frame < FrameCount; ++Frame)
	{
		CaptureComponent->CaptureScene();
		FlushRenderingCommands();
	}
	return true;
}

bool ProjectWorldRuntimeScreenshotCapture::FCaptureSession::Capture(
	const FString& Path,
	FString& OutError)
{
	if (!IsInitialized() || Path.IsEmpty())
	{
		OutError = TEXT("The screenshot capture session or output path is unavailable.");
		return false;
	}
	USceneCaptureComponent2D* CaptureComponent = State->CaptureActor->GetCaptureComponent2D();
	CaptureComponent->CaptureScene();
	FlushRenderingCommands();

	FImage Image;
	bool bSucceeded = FImageUtils::GetRenderTargetImage(State->RenderTarget, Image);
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
			*State->Spec.CameraLocation.ToCompactString(),
			*State->Spec.CameraRotation.ToCompactString(),
			*State->Spec.SourceIdentity);
	}
	else
	{
		++State->WrittenCaptureCount;
	}
	return bSucceeded;
}

void ProjectWorldRuntimeScreenshotCapture::FCaptureSession::Reset()
{
	if (!State.IsValid())
	{
		return;
	}
	if (State->World.IsValid() && State->CaptureActor.IsValid())
	{
		State->World->DestroyActor(State->CaptureActor.Get());
	}
	if (State->RenderTarget != nullptr)
	{
		State->RenderTarget->RemoveFromRoot();
	}
	State.Reset();
}

const FString& ProjectWorldRuntimeScreenshotCapture::FCaptureSession::GetSessionId() const
{
	static const FString Empty;
	return State.IsValid() ? State->SessionId : Empty;
}

int32 ProjectWorldRuntimeScreenshotCapture::FCaptureSession::GetWrittenCaptureCount() const
{
	return State.IsValid() ? State->WrittenCaptureCount : 0;
}

bool ProjectWorldRuntimeScreenshotCapture::FCaptureSession::IsInitialized() const
{
	return State.IsValid() && State->World.IsValid() && State->CaptureActor.IsValid() &&
		State->RenderTarget != nullptr && State->CaptureActor->GetCaptureComponent2D() != nullptr;
}

bool ProjectWorldRuntimeScreenshotCapture::CapturePlayerContext(
	UWorld& World,
	APlayerController& PlayerController,
	const FString& Path,
	FString& OutError)
{
	const APawn* Pawn = PlayerController.GetPawn();
	if (Pawn == nullptr)
	{
		OutError = TEXT("The product context has no production pawn.");
		return false;
	}
	const FVector TargetLocation = Pawn->GetActorLocation();
	FCaptureSpec Spec;
	Spec.CameraLocation = TargetLocation + FVector(-20000.0f, -20000.0f, 15000.0f);
	Spec.CameraRotation = (TargetLocation - Spec.CameraLocation).Rotation();
	Spec.SourceIdentity = TEXT("production_pawn_context");
	FCaptureSession Session;
	return Session.Initialize(World, Spec, OutError) &&
		Session.WarmUp(3, OutError) &&
		Session.Capture(Path, OutError);
}
