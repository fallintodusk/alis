// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
//
// Operator evidence capture for a realized territory.
//
// WHY A SCENE CAPTURE AND NOT A SCREENSHOT
// The removed in-automation sweep failed because inside Automation RunTests
// GIsAutomationTesting is true and TakeHighResScreenshot completes on screenshot COMPARISON
// against absent ground truth, never on capture. Hand-driving the editor viewport with
// BugItGo + HighResShot fails differently: the viewport only refreshes when the OS gives the
// window foreground, so a backgrounded editor serves byte-identical stale frames across
// different camera poses. USceneCaptureComponent2D has neither failure mode - it renders the
// scene into its own render target on demand, with no viewport, no window, and no comparison
// path. See ProjectWorld pitfall 13 and tools/World/VisualVerification/README.md.

#include "ProjectWorldEvidenceCapture.h"

#include "Utilities/ProjectSha256.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldEvidenceCapture, Log, All);

namespace ProjectWorldEvidenceCapture
{
	namespace
	{
		bool ReadNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& OutValue, FString& OutError)
		{
			if (!Object->TryGetNumberField(Field, OutValue))
			{
				OutError = FString::Printf(TEXT("Vantage plan entry is missing the numeric field '%s'."), Field);
				return false;
			}
			return true;
		}
	}

	bool LoadVantagePlan(
		const FString& PlanPath,
		int32& OutWidth,
		int32& OutHeight,
		TArray<FProjectWorldCaptureVantage>& OutVantages,
		FString& OutPlanSha256,
		FString& OutError)
	{
		FString PlanText;
		if (!FFileHelper::LoadFileToString(PlanText, *PlanPath))
		{
			OutError = FString::Printf(TEXT("Cannot read the vantage plan: %s"), *PlanPath);
			return false;
		}
		if (!FProjectSha256::HashFile(PlanPath, OutPlanSha256))
		{
			OutError = TEXT("Cannot hash the vantage plan.");
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PlanText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("The vantage plan is not valid JSON.");
			return false;
		}

		// The plan owns the framing solve; this route never invents a pose or an altitude.
		int32 Width = 0;
		int32 Height = 0;
		if (!Root->TryGetNumberField(TEXT("capture_width"), Width) ||
			!Root->TryGetNumberField(TEXT("capture_height"), Height) ||
			Width <= 0 || Height <= 0)
		{
			OutError = TEXT("The vantage plan must declare positive capture_width and capture_height.");
			return false;
		}
		OutWidth = Width;
		OutHeight = Height;

		const TArray<TSharedPtr<FJsonValue>>* Vantages = nullptr;
		if (!Root->TryGetArrayField(TEXT("vantages"), Vantages) || Vantages->IsEmpty())
		{
			OutError = TEXT("The vantage plan declares no vantages.");
			return false;
		}
		TSet<FString> SeenNames;
		for (const TSharedPtr<FJsonValue>& Value : *Vantages)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Value->TryGetObject(Entry))
			{
				OutError = TEXT("A vantage entry is not an object.");
				return false;
			}
			FProjectWorldCaptureVantage Vantage;
			if (!(*Entry)->TryGetStringField(TEXT("name"), Vantage.Name) || Vantage.Name.IsEmpty())
			{
				OutError = TEXT("A vantage entry has no name.");
				return false;
			}
			// Names become file names and identify views in the receipt, so a duplicate would
			// silently overwrite evidence.
			if (SeenNames.Contains(Vantage.Name))
			{
				OutError = FString::Printf(TEXT("Duplicate vantage name: %s"), *Vantage.Name);
				return false;
			}
			SeenNames.Add(Vantage.Name);
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;
			double Pitch = 0.0;
			double Yaw = 0.0;
			if (!ReadNumberField(*Entry, TEXT("x"), X, OutError) ||
				!ReadNumberField(*Entry, TEXT("y"), Y, OutError) ||
				!ReadNumberField(*Entry, TEXT("z"), Z, OutError) ||
				!ReadNumberField(*Entry, TEXT("pitch"), Pitch, OutError) ||
				!ReadNumberField(*Entry, TEXT("yaw"), Yaw, OutError))
			{
				return false;
			}
			double FieldOfView = 0.0;
			if (!(*Entry)->TryGetNumberField(TEXT("fov"), FieldOfView))
			{
				if (!Root->TryGetNumberField(TEXT("fov_degrees"), FieldOfView))
				{
					OutError = TEXT("The vantage plan declares no field of view.");
					return false;
				}
			}
			Vantage.Location = FVector(X, Y, Z);
			Vantage.Rotation = FRotator(Pitch, Yaw, 0.0);
			Vantage.FieldOfViewDegrees = FieldOfView;
			(*Entry)->TryGetStringField(TEXT("note"), Vantage.Note);
			OutVantages.Add(MoveTemp(Vantage));
		}
		return true;
	}

	bool CaptureVantages(
		UWorld* World,
		const TArray<FProjectWorldCaptureVantage>& Vantages,
		int32 Width,
		int32 Height,
		const FString& OutputDirectory,
		FProjectWorldCaptureResult& OutResult,
		FString& OutError)
	{
		if (World == nullptr)
		{
			OutError = TEXT("Evidence capture has no world.");
			return false;
		}
		if (!FApp::CanEverRender())
		{
			OutError = TEXT("Evidence capture requires a render-capable process; -NullRHI cannot render a scene capture.");
			return false;
		}
		// USceneCaptureComponent2D::CaptureScene silently does nothing when the world has no
		// FScene - it is guarded by `World && World->Scene && IsVisible()`. The render target
		// then keeps its clear colour and the run writes perfectly valid black PNGs. Refuse
		// instead: an empty render surface must never leave the process looking like evidence.
		if (World->Scene == nullptr)
		{
			OutError = TEXT("The loaded world has no rendering scene, so a scene capture would write clear-colour frames.");
			return false;
		}
		IFileManager::Get().MakeDirectory(*OutputDirectory, true);
		OutResult.ScreenshotDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
		OutResult.Width = Width;
		OutResult.Height = Height;

		// Components registered while the world had no FScene never entered one, so they are
		// IsRegistered() yet invisible to the renderer. Re-registering world components after
		// the scene exists is what actually puts the loaded territory into the render scene.
		World->UpdateWorldComponents(false, false);
		World->SendAllEndOfFrameUpdates();

		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		RenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->InitAutoFormat(static_cast<uint32>(Width), static_cast<uint32>(Height));
		RenderTarget->UpdateResourceImmediate(true);
		RenderTarget->AddToRoot();

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(SpawnParameters);
		if (CaptureActor == nullptr)
		{
			RenderTarget->RemoveFromRoot();
			OutError = TEXT("Cannot spawn the transient scene capture actor.");
			return false;
		}
		USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
		Capture->TextureTarget = RenderTarget;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		// CaptureScene() below is the explicit trigger; capturing every frame as well would
		// render redundantly and the engine warns about it.
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true;

		int32 ActorCount = 0;
		int32 PrimitiveCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorCount;
			TArray<UPrimitiveComponent*> Primitives;
			It->GetComponents<UPrimitiveComponent>(Primitives);
			for (const UPrimitiveComponent* Primitive : Primitives)
			{
				if (Primitive->IsRegistered() && Primitive->IsVisible())
				{
					++PrimitiveCount;
				}
			}
		}
		TMap<FString, int32> ClassHistogram;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			ClassHistogram.FindOrAdd(It->GetClass()->GetName())++;
		}
		ClassHistogram.ValueSort([](int32 Left, int32 Right) { return Left > Right; });
		FString Breakdown;
		int32 Listed = 0;
		for (const TPair<FString, int32>& Pair : ClassHistogram)
		{
			Breakdown += FString::Printf(TEXT("%s=%d "), *Pair.Key, Pair.Value);
			if (++Listed >= 12)
			{
				break;
			}
		}
		UE_LOG(
			LogProjectWorldEvidenceCapture,
			Display,
			TEXT("[ProjectWorldEvidenceCapture::CaptureVantages] world actors=%d visible primitives=%d classes: %s"),
			ActorCount,
			PrimitiveCount,
			*Breakdown);
		UE_LOG(
			LogProjectWorldEvidenceCapture,
			Display,
			TEXT("[ProjectWorldEvidenceCapture::CaptureVantages] scene=%d registered=%d visible=%d target=%dx%d"),
			World->Scene != nullptr ? 1 : 0,
			Capture->IsRegistered() ? 1 : 0,
			Capture->IsVisible() ? 1 : 0,
			RenderTarget->SizeX,
			RenderTarget->SizeY);

		auto CaptureOne = [&](const FProjectWorldCaptureVantage& Vantage, const FString& FileName, FString& OutHash) -> bool
		{
			CaptureActor->SetActorLocationAndRotation(Vantage.Location, Vantage.Rotation);
			Capture->FOVAngle = static_cast<float>(Vantage.FieldOfViewDegrees);
			Capture->CaptureScene();
			// USceneCaptureComponent2D::CaptureScene only ENQUEUES the render; it executes a
			// scene render builder and returns. Reading the target before the render thread has
			// executed yields the previous contents - the same class of defect as reading a
			// Landscape edit layer before its composition barrier. Flush, then read back.
			FlushRenderingCommands();

			FImage Image;
			if (!FImageUtils::GetRenderTargetImage(RenderTarget, Image))
			{
				OutError = FString::Printf(TEXT("Cannot read back the render target for vantage %s."), *Vantage.Name);
				return false;
			}
			const FString FilePath = FPaths::Combine(OutputDirectory, FileName);
			if (!FImageUtils::SaveImageByExtension(*FilePath, Image))
			{
				OutError = FString::Printf(TEXT("Cannot write the capture for vantage %s."), *Vantage.Name);
				return false;
			}
			if (!FProjectSha256::HashFile(FilePath, OutHash))
			{
				OutError = FString::Printf(TEXT("Cannot hash the capture for vantage %s."), *Vantage.Name);
				return false;
			}
			return true;
		};

		bool bSucceeded = true;
		for (const FProjectWorldCaptureVantage& Vantage : Vantages)
		{
			FProjectWorldCaptureView View;
			View.Name = Vantage.Name;
			View.File = Vantage.Name + TEXT(".png");
			View.RequestedWidth = Width;
			View.RequestedHeight = Height;
			View.Location = Vantage.Location;
			View.Rotation = Vantage.Rotation;
			View.FieldOfViewDegrees = Vantage.FieldOfViewDegrees;
			if (!CaptureOne(Vantage, View.File, View.ImageSha256))
			{
				bSucceeded = false;
				break;
			}
			UE_LOG(
				LogProjectWorldEvidenceCapture,
				Display,
				TEXT("[ProjectWorldEvidenceCapture::CaptureVantages] Captured %s (%dx%d) hash=%s"),
				*View.Name,
				Width,
				Height,
				*View.ImageSha256.Left(12));
			OutResult.Views.Add(MoveTemp(View));
		}

		if (bSucceeded && !Vantages.IsEmpty())
		{
			// Determinism control: the first pose captured a second time. Without it, distinct
			// hashes across poses could be nondeterminism rather than a real response to the
			// camera, and identical hashes could not be told from a stale frame.
			const FProjectWorldCaptureVantage& Control = Vantages[0];
			OutResult.ControlName = Control.Name;
			bSucceeded = CaptureOne(Control, Control.Name + TEXT(".control.png"), OutResult.ControlSha256);
			if (bSucceeded)
			{
				OutResult.bControlMatches = OutResult.ControlSha256 == OutResult.Views[0].ImageSha256;
			}
		}

		CaptureActor->Destroy();
		RenderTarget->RemoveFromRoot();

		if (!bSucceeded)
		{
			return false;
		}

		TSet<FString> Hashes;
		OutResult.bViewsPairwiseDistinct = true;
		for (const FProjectWorldCaptureView& View : OutResult.Views)
		{
			bool bAlreadySeen = false;
			Hashes.Add(View.ImageSha256, &bAlreadySeen);
			if (bAlreadySeen)
			{
				OutResult.bViewsPairwiseDistinct = false;
			}
		}
		return true;
	}

	bool WriteReceipt(const FProjectWorldCaptureResult& Result, const FString& ReceiptPath, FString& OutError)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 1);
		Root->SetStringField(TEXT("map_package"), Result.MapPackage);
		Root->SetStringField(TEXT("vantage_plan_sha256"), Result.VantagePlanSha256);
		Root->SetStringField(TEXT("screenshot_dir"), Result.ScreenshotDirectory);
		Root->SetNumberField(TEXT("capture_width"), Result.Width);
		Root->SetNumberField(TEXT("capture_height"), Result.Height);
		Root->SetStringField(TEXT("status"), Result.Status);
		Root->SetStringField(TEXT("message"), Result.Message);
		Root->SetBoolField(TEXT("views_pairwise_distinct"), Result.bViewsPairwiseDistinct);

		const TSharedRef<FJsonObject> Control = MakeShared<FJsonObject>();
		Control->SetStringField(TEXT("name"), Result.ControlName);
		Control->SetStringField(TEXT("repeat_sha256"), Result.ControlSha256);
		Control->SetBoolField(TEXT("matches_first_capture"), Result.bControlMatches);
		Root->SetObjectField(TEXT("determinism_control"), Control);

		TArray<TSharedPtr<FJsonValue>> Views;
		for (const FProjectWorldCaptureView& View : Result.Views)
		{
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), View.Name);
			Entry->SetStringField(TEXT("file"), View.File);
			Entry->SetNumberField(TEXT("requested_width"), View.RequestedWidth);
			Entry->SetNumberField(TEXT("requested_height"), View.RequestedHeight);
			Entry->SetStringField(TEXT("image_sha256"), View.ImageSha256);
			Entry->SetNumberField(TEXT("fov_degrees"), View.FieldOfViewDegrees);
			TArray<TSharedPtr<FJsonValue>> Location;
			Location.Add(MakeShared<FJsonValueNumber>(View.Location.X));
			Location.Add(MakeShared<FJsonValueNumber>(View.Location.Y));
			Location.Add(MakeShared<FJsonValueNumber>(View.Location.Z));
			Entry->SetArrayField(TEXT("location"), Location);
			TArray<TSharedPtr<FJsonValue>> Rotation;
			Rotation.Add(MakeShared<FJsonValueNumber>(View.Rotation.Pitch));
			Rotation.Add(MakeShared<FJsonValueNumber>(View.Rotation.Yaw));
			Rotation.Add(MakeShared<FJsonValueNumber>(View.Rotation.Roll));
			Entry->SetArrayField(TEXT("rotation_pitch_yaw_roll"), Rotation);
			Views.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("views"), Views);

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			OutError = TEXT("Cannot serialize the capture receipt.");
			return false;
		}
		if (!FFileHelper::SaveStringToFile(Serialized, *ReceiptPath))
		{
			OutError = FString::Printf(TEXT("Cannot write the capture receipt: %s"), *ReceiptPath);
			return false;
		}
		return true;
	}
}
