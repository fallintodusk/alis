// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "MVVM/ProjectViewModel.h"
#include "MindJournalViewModel.generated.h"

class IMindService;
struct FMindThoughtView;

/**
 * ViewModel for the Mind Journal panel.
 *
 * Mirrors IMindService history into two tabs:
 * - Journal: durable thoughts routed to Journal/ToastAndJournal channels.
 * - Recent: temporary thoughts routed to Toast channel.
 */
UCLASS()
class PROJECTMINDUI_API UMindJournalViewModel : public UProjectViewModel
{
	GENERATED_BODY()

public:
	virtual void Initialize(UObject* Context) override;
	virtual void Shutdown() override;
	virtual bool GetBoolProperty(FName PropertyName) const override;

	UFUNCTION(BlueprintCallable, Category = "MindJournal")
	void TogglePanel();

	UFUNCTION(BlueprintCallable, Category = "MindJournal")
	void ShowPanel();

	UFUNCTION(BlueprintCallable, Category = "MindJournal")
	void HidePanel();

	UFUNCTION(BlueprintCallable, Category = "MindJournal")
	void SetJournalTabActive(bool bActive);

	VIEWMODEL_PROPERTY(bool, bPanelVisible)
	VIEWMODEL_PROPERTY(bool, bJournalTabActive)
	VIEWMODEL_PROPERTY(FText, JournalText)
	VIEWMODEL_PROPERTY(FText, RecentText)

private:
	void SubscribeToService();
	void UnsubscribeFromService();
	bool RetrySubscribeToService(float DeltaTime);
	void StopServiceRetry();
	void RefreshFromService();

	void HandleThoughtAdded(const FMindThoughtView& Thought);
	void HandleMindFeedChanged();

private:
	FDelegateHandle ThoughtAddedHandle;
	FDelegateHandle FeedChangedHandle;
	FTSTicker::FDelegateHandle ServiceRetryHandle;
	TSharedPtr<IMindService> CachedService;
};
