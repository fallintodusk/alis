#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "ProjectTimeDemoUISubsystem.generated.h"

class UInputComponent;
class UProjectTimeDemoViewModel;

/** Optional local-player launcher; existing ProjectUI owns visibility/input layers. */
UCLASS()
class PROJECTTIMEDEMOUI_API UProjectTimeDemoUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	void Toggle();
	void Close();
private:
	bool Refresh(float Delta);
	void UnbindInput();
	TWeakObjectPtr<UInputComponent> Input;
	TWeakObjectPtr<UProjectTimeDemoViewModel> Model;
	FTSTicker::FDelegateHandle Ticker;
};
