// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "MVVM/ProjectViewModel.h"
#include "MindThoughtViewModel.generated.h"

class IMindService;
struct FMindThoughtView;

/**
 * ViewModel for local mind thought toast presentation.
 *
 * Consumes IMindService only (ProjectCore contract).
 */
UCLASS()
class PROJECTMINDUI_API UMindThoughtViewModel : public UProjectViewModel
{
	GENERATED_BODY()

public:
	virtual void Initialize(UObject* Context) override;
	virtual void Shutdown() override;
	virtual bool GetBoolProperty(FName PropertyName) const override;

	/** Called by view after TTL expires. */
	void ClearCurrentThought();

	VIEWMODEL_PROPERTY(bool, bHasThought)
	VIEWMODEL_PROPERTY(FText, ThoughtText)
	VIEWMODEL_PROPERTY(FText, ThoughtMetaText)
	VIEWMODEL_PROPERTY(float, ThoughtTimeToLiveSec)

private:
	void SubscribeToService();
	void UnsubscribeFromService();
	bool RetrySubscribeToService(float DeltaTime);
	void StopServiceRetry();
	void ApplyThought(const FMindThoughtView& Thought, bool bRespectAge);

	void HandleThoughtAdded(const FMindThoughtView& Thought);

private:
	FDelegateHandle ThoughtAddedHandle;
	FTSTicker::FDelegateHandle ServiceRetryHandle;
	TSharedPtr<IMindService> CachedService;
};
