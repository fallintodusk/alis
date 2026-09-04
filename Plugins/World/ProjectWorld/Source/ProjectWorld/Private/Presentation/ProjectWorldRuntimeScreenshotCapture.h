// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Engine/EngineTypes.h"
#include "CoreMinimal.h"

class APlayerController;
class UWorld;

namespace ProjectWorldRuntimeScreenshotCapture
{
	struct FCaptureSpec
	{
		FVector CameraLocation = FVector::ZeroVector;
		FRotator CameraRotation = FRotator::ZeroRotator;
		ESceneCaptureSource CaptureSource = SCS_FinalColorLDR;
		FString SourceIdentity;
		float FieldOfViewDegrees = 70.0f;
		float OrthographicWidthCentimeters = 0.0f;
		bool bIsolateBaseColor = false;
	};

	class FCaptureSession
	{
	public:
		FCaptureSession();
		~FCaptureSession();

		FCaptureSession(const FCaptureSession&) = delete;
		FCaptureSession& operator=(const FCaptureSession&) = delete;

		bool Initialize(UWorld& World, const FCaptureSpec& Spec, FString& OutError);
		bool WarmUp(int32 FrameCount, FString& OutError);
		bool Capture(const FString& Path, FString& OutError);
		void Reset();

		const FString& GetSessionId() const;
		int32 GetWrittenCaptureCount() const;
		bool IsInitialized() const;

	private:
		struct FState;
		TUniquePtr<FState> State;
	};

	bool CapturePlayerContext(
		UWorld& World,
		APlayerController& PlayerController,
		const FString& Path,
		FString& OutError);
}
