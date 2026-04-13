#include "ClipMatrixReportWriter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"

FString ClipMatrixHelpers::SampleToJsonLine(
	const FFrameSample& S,
	const FClipPhase* ActivePhases,
	int32 ActivePhaseCount)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("frame"), static_cast<double>(S.FrameNumber));
	Row->SetNumberField(TEXT("phase"), S.PhaseIndex);
	Row->SetStringField(TEXT("phaseName"),
		S.PhaseIndex >= 0 && S.PhaseIndex < ActivePhaseCount
			? ActivePhases[S.PhaseIndex].Name : TEXT("?"));
	Row->SetNumberField(TEXT("time"), S.PhaseTime);
	Row->SetStringField(TEXT("camPos"), S.CameraWorldPos.ToCompactString());
	Row->SetStringField(TEXT("camForward"), S.CameraForward.ToCompactString());
	Row->SetStringField(TEXT("actorForward"), S.ActorForward.ToCompactString());
	Row->SetNumberField(TEXT("reqPitch"), S.RequestedPitchDeg);
	Row->SetNumberField(TEXT("camPitch"), S.CameraPitchDeg);
	Row->SetNumberField(TEXT("camDownDot"), S.CameraDownDot);
	Row->SetNumberField(TEXT("ctrlPitch"),
		FRotator::NormalizeAxis(S.ControlRotation.Pitch));
	Row->SetNumberField(TEXT("ctrlYaw"), S.ControlRotation.Yaw);
	Row->SetNumberField(TEXT("actorYaw"), S.ActorRotation.Yaw);
	Row->SetNumberField(TEXT("speed"), S.Speed);
	Row->SetBoolField(TEXT("falling"), S.bIsFalling);
	Row->SetBoolField(TEXT("crouched"), S.bIsCrouched);
	Row->SetStringField(TEXT("ownerMesh"), S.OwnerVisibleMeshName);
	Row->SetNumberField(TEXT("severity"), S.Severity);
	Row->SetStringField(TEXT("copySource"), S.CopyPoseSourceName);
	Row->SetNumberField(TEXT("intrusions"), S.IntrusionCount);
	Row->SetBoolField(TEXT("rayHit"), S.bCameraRayHitsBody);
	Row->SetNumberField(TEXT("rayDist"), S.CameraRayHitDist);
	Row->SetStringField(TEXT("rayBone"), S.CameraRayHitBone);
	Row->SetBoolField(TEXT("proxyRayHit"), S.bUpperTorsoProxyRayHit);
	Row->SetNumberField(TEXT("proxyRayDist"), S.UpperTorsoProxyRayDist);
	Row->SetNumberField(TEXT("proxyPerpDist"), S.UpperTorsoProxyPerpDist);
	Row->SetStringField(TEXT("proxyWorld"), S.UpperTorsoProxyWorld.ToCompactString());
	Row->SetStringField(TEXT("proxyCamLocal"), S.UpperTorsoProxyCameraLocal.ToCompactString());
	Row->SetBoolField(TEXT("capsuleCameraInside"), S.bUpperTorsoCapsuleCameraInside);
	Row->SetNumberField(TEXT("capsuleCameraDist"), S.UpperTorsoCapsuleCameraDist);
	Row->SetBoolField(TEXT("capsuleRayHit"), S.bUpperTorsoCapsuleRayHit);
	Row->SetNumberField(TEXT("capsuleRayDist"), S.UpperTorsoCapsuleRayDist);
	Row->SetNumberField(TEXT("capsuleRayPerpDist"), S.UpperTorsoCapsulePerpDist);
	Row->SetStringField(TEXT("capsuleStartWorld"), S.UpperTorsoCapsuleStartWorld.ToCompactString());
	Row->SetStringField(TEXT("capsuleEndWorld"), S.UpperTorsoCapsuleEndWorld.ToCompactString());
	Row->SetStringField(TEXT("capsuleStartCamLocal"), S.UpperTorsoCapsuleStartCameraLocal.ToCompactString());
	Row->SetStringField(TEXT("capsuleEndCamLocal"), S.UpperTorsoCapsuleEndCameraLocal.ToCompactString());
	Row->SetStringField(TEXT("desiredNeckWorld"), S.DesiredNeckWorld.ToCompactString());
	Row->SetStringField(TEXT("desiredNeckCamLocal"), S.DesiredNeckCameraLocal.ToCompactString());
	Row->SetNumberField(TEXT("cameraActorZ"), S.CameraActorZ);
	Row->SetNumberField(TEXT("expectedCameraActorZ"), S.ExpectedCameraActorZ);
	Row->SetNumberField(TEXT("cameraActorZError"), S.CameraActorZError);
	Row->SetNumberField(TEXT("sourceHeadActorZ"), S.SourceHeadActorZ);
	Row->SetNumberField(TEXT("sourceNeckActorZ"), S.SourceNeckActorZ);
	Row->SetNumberField(TEXT("cameraHeadZDelta"), S.CameraHeadZDelta);
	Row->SetNumberField(TEXT("cameraNeckZDelta"), S.CameraNeckZDelta);
	Row->SetNumberField(TEXT("sourceHeadCameraDist"), S.SourceHeadCameraDist);
	Row->SetStringField(TEXT("sourceHeadCamLocal"), S.SourceHeadCameraLocal.ToCompactString());
	Row->SetBoolField(TEXT("headStretchExceeded"), S.bHeadStretchExceeded);
	Row->SetNumberField(TEXT("sourceNeckTargetGapDist"), S.SourceNeckTargetGapDist);
	Row->SetStringField(TEXT("sourceNeckCamLocal"), S.SourceNeckCameraLocal.ToCompactString());
	Row->SetStringField(TEXT("sourceNeckWorld"), S.SourceNeckWorld.ToCompactString());
	Row->SetStringField(TEXT("correctedNeckCamLocal"), S.CorrectedNeckCameraLocal.ToCompactString());
	Row->SetStringField(TEXT("correctedNeckWorld"), S.CorrectedNeckWorld.ToCompactString());
	Row->SetStringField(TEXT("correctedSpine05CamLocal"), S.CorrectedSpine05CameraLocal.ToCompactString());
	Row->SetStringField(TEXT("sourceSpine05World"), S.SourceSpine05World.ToCompactString());
	Row->SetStringField(TEXT("correctedSpine05World"), S.CorrectedSpine05World.ToCompactString());
	Row->SetStringField(TEXT("sourceUpperChainDelta"), S.SourceUpperChainCameraDelta.ToCompactString());
	Row->SetStringField(TEXT("correctedUpperChainDelta"), S.CorrectedUpperChainCameraDelta.ToCompactString());
	Row->SetStringField(TEXT("sourceNeckTargetGap"), S.SourceNeckTargetGap.ToCompactString());
	Row->SetBoolField(TEXT("neckGapExceeded"), S.bNeckGapExceeded);
	Row->SetNumberField(TEXT("correctedNeckTargetGapDist"), S.CorrectedNeckTargetGapDist);
	Row->SetStringField(TEXT("correctedNeckTargetGap"), S.CorrectedNeckTargetGap.ToCompactString());
	Row->SetNumberField(TEXT("sourceNeckChainDist"), S.SourceNeckChainDist);
	Row->SetNumberField(TEXT("correctedNeckChainDist"), S.CorrectedNeckChainDist);
	Row->SetNumberField(TEXT("correctedNeckChainErrorDist"), S.CorrectedNeckChainErrorDist);
	Row->SetBoolField(TEXT("correctedNeckChainErrorExceeded"), S.bCorrectedNeckChainErrorExceeded);
	Row->SetBoolField(TEXT("correctedUpperChainFoldExceeded"), S.bCorrectedUpperChainFoldExceeded);
	Row->SetNumberField(TEXT("sourceUpperLeanDeg"), S.SourceUpperLeanDeg);
	Row->SetNumberField(TEXT("correctedUpperLeanDeg"), S.CorrectedUpperLeanDeg);
	Row->SetNumberField(TEXT("sourceUpperForwardCm"), S.SourceUpperForwardCm);
	Row->SetNumberField(TEXT("correctedUpperForwardCm"), S.CorrectedUpperForwardCm);
	Row->SetNumberField(TEXT("upperForwardDeltaCm"), S.UpperForwardDeltaCm);

	for (const FBoneIntrusion& BI : S.Bones)
	{
		const FString Prefix = BI.BoneName.ToString();
		Row->SetNumberField(Prefix + TEXT("_dist"), BI.DistFromCamera);
		Row->SetStringField(Prefix + TEXT("_camLocal"),
			BI.CameraLocalPos.ToCompactString());
		Row->SetBoolField(Prefix + TEXT("_in"), BI.bInForbiddenVolume);
	}

	FString LineStr;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> CW =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&LineStr);
	FJsonSerializer::Serialize(Row.ToSharedRef(), CW);
	return LineStr;
}

TSharedPtr<FJsonObject> ClipMatrixHelpers::ParseSampleJson(
	const FFrameSample& Sample,
	const FClipPhase* ActivePhases,
	int32 ActivePhaseCount)
{
	TSharedPtr<FJsonObject> SampleObj = MakeShared<FJsonObject>();
	const FString SampleLine = SampleToJsonLine(Sample, ActivePhases, ActivePhaseCount);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SampleLine);
	if (!FJsonSerializer::Deserialize(Reader, SampleObj) || !SampleObj.IsValid())
	{
		return MakeShared<FJsonObject>();
	}
	return SampleObj;
}

void ClipMatrixHelpers::WriteArtifactSidecar(
	const FArtifactReplayTarget& Target,
	const TArray<FFrameSample>& AllSamples,
	const FString& RunId,
	EClipMatrixScenario Scenario,
	float InitialCapsuleHalfHeight,
	const FClipPhase* ActivePhases,
	int32 ActivePhaseCount)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("testName"), TEXT("FirstPersonClipMatrixArtifactReplay"));
	Root->SetStringField(TEXT("runId"), RunId);
	Root->SetStringField(TEXT("scenario"), ScenarioToString(Scenario));
	Root->SetStringField(TEXT("artifactStem"), Target.ArtifactStem);
	Root->SetStringField(TEXT("phase"), Target.PhaseName);
	Root->SetStringField(TEXT("reason"), Target.Reason);
	Root->SetStringField(TEXT("screenshotPath"), Target.ScreenshotPath);
	Root->SetNumberField(TEXT("phaseIndex"), Target.PhaseIndex);
	Root->SetNumberField(TEXT("measurementSampleIndex"), Target.MeasurementSampleIndex);
	Root->SetNumberField(TEXT("targetPhaseTime"), Target.PhaseTime);
	Root->SetNumberField(TEXT("cameraProbeLengthCm"), CameraProbeLengthCm);
	Root->SetNumberField(TEXT("proxyRadiusCm"), UpperTorsoProxyRadiusCm);
	Root->SetNumberField(TEXT("capsuleRadiusCm"), UpperTorsoCapsuleRadiusCm);
	Root->SetNumberField(TEXT("initialCapsuleHalfHeight"), InitialCapsuleHalfHeight);
	Root->SetNumberField(TEXT("requestedFrame"), static_cast<double>(Target.RequestedFrameNumber));
	Root->SetNumberField(TEXT("processedFrame"), static_cast<double>(Target.ProcessedFrameNumber));
	Root->SetNumberField(TEXT("requestedPhaseTime"), Target.RequestedPhaseTime);
	Root->SetNumberField(TEXT("requestedTimeErrorSec"), Target.RequestedTimeErrorSec);
	Root->SetBoolField(TEXT("screenshotFileExists"), Target.bFileExistsAfterProcess);
	if (Target.bHasReplaySample)
	{
		Root->SetObjectField(TEXT("replaySample"),
			ParseSampleJson(Target.ReplaySample, ActivePhases, ActivePhaseCount));
	}
	if (AllSamples.IsValidIndex(Target.MeasurementSampleIndex))
	{
		Root->SetObjectField(
			TEXT("measurementSample"),
			ParseSampleJson(AllSamples[Target.MeasurementSampleIndex],
				ActivePhases, ActivePhaseCount));
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(JsonString, *Target.SidecarPath);
}
