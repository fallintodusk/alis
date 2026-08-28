// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"

class ACharacter;
class APlayerController;
class ASinglePlayController;
class UInventoryViewModel;
class USinglePlayScenarioRunnerComponent;
class UWidget;

class FSinglePlayScenarioPackagedGate final
{
public:
	~FSinglePlayScenarioPackagedGate();
	void StartIfRequested();

private:
	enum class ERoute : uint8
	{
		Success,
		Failure
	};

	enum class EPhase : uint8
	{
		WaitingForWorld,
		TravelToCache,
		AimAtCache,
		HoldInteraction,
		WaitingForInventory,
		MovePouchToBack,
		MoveWaterToLeftHand,
		MoveRationToRightHand,
		MoveMedkitToBackpack,
		RejectPryBarFromBackpack,
		UseWater,
		CloseInventory,
		TravelToShelter,
		WaitingForTerminal,
		WaitingForRestart,
		WaitingForScreenshot,
		Finished
	};

	struct FDragStep
	{
		TWeakObjectPtr<UWidget> Source;
		TWeakObjectPtr<UWidget> Target;
		FVector2D SourcePosition = FVector2D::ZeroVector;
		FVector2D TargetPosition = FVector2D::ZeroVector;
		int32 Step = 0;
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryAcquireWorld(FString& OutError);
	bool TryAcquireRestartedWorld(FString& OutError);
	bool TickTravel(AActor& Target, float ArrivalRadius, EPhase NextPhase, FString& OutError);
	bool TickAimAtActor(AActor& Target);
	void HoldKey(const FKey& Key);
	void ReleaseKey(const FKey& Key);
	void ReleaseInputs();
	void SendLookInput(double YawError, double PitchError);
	void SendKeyPress(const FKey& Key);

	UInventoryViewModel* FindInventoryViewModel() const;
	bool BeginItemDrag(
		FName ItemName,
		const FGameplayTag& TargetSurface,
		int32 TargetCell,
		FString& OutError);
	bool BeginEquipDrag(FName ItemName, const FGameplayTag& EquipSlot, FString& OutError);
	bool TickDrag();
	bool ClickWidget(UWidget& Widget);
	void HandleInventoryError(const FText& ErrorMessage);
	void HandleInteraction(AActor* TargetActor, UActorComponent* RespondingComponent);

	bool HasItem(FName ItemName) const;
	bool ReadHydration(double& OutFraction) const;
	AActor* FindTaggedActor(FName ActorTag) const;
	bool RequestScreenshot(FString& OutError);
	void FinishAccepted();
	void FinishRejected(const FString& Code, const FString& Message);
	void WriteResult(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage);
	void SetPhase(EPhase NewPhase);
	void RequestExit(int32 Status, const TCHAR* Reason) const;

	FString OperationId;
	FString ResultPath;
	FString ScreenshotPath;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	FString MachineProfileId;
	ERoute Route = ERoute::Success;
	EPhase Phase = EPhase::Finished;
	FTSTicker::FDelegateHandle TickerHandle;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ASinglePlayController> Controller;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<USinglePlayScenarioRunnerComponent> Runner;
	TWeakObjectPtr<UInventoryViewModel> InventoryViewModel;
	TWeakObjectPtr<AActor> CacheActor;
	TWeakObjectPtr<AActor> ShelterActor;
	TUniquePtr<FDragStep> DragStep;
	TSet<FKey> HeldKeys;
	FDelegateHandle InventoryErrorHandle;
	FDelegateHandle InteractionHandle;
	double GateStartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	double HydrationBefore = 0.0;
	double HydrationAfter = 0.0;
	FVector StartLocation = FVector::ZeroVector;
	FVector CacheArrivalLocation = FVector::ZeroVector;
	FVector ShelterArrivalLocation = FVector::ZeroVector;
	FString CapacityRejection;
	int32 InputEventCount = 0;
	int32 UiPointerEventCount = 0;
	bool bInteractionObserved = false;
	bool bInventoryOpened = false;
	bool bPouchEquipped = false;
	bool bCapacityRejected = false;
	bool bWaterContextOpened = false;
	bool bWaterUseRequested = false;
	bool bWaterUsed = false;
	bool bFailureObserved = false;
	bool bRestartObserved = false;
	bool bScreenshotRequested = false;
};
