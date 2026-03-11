#include "DataAssets/Layout/DA_LoadingScreenLayout.h"

#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

namespace LoadingScreenLayout
{
	static FProjectUIScreenSlot MakeSlot(const TCHAR* Name, const TCHAR* WidgetPath, bool bVisible = true, int32 ZOrder = 0)
	{
		FProjectUIScreenSlot Slot;
		Slot.SlotName = FName(Name);
		Slot.WidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(WidgetPath));
		Slot.bVisibleOnInit = bVisible;
		Slot.ZOrder = ZOrder;
		return Slot;
	}

	static FProjectUIScreenStackEntry MakeStackEntry(const TCHAR* WidgetPath, bool bPush = true)
	{
		FProjectUIScreenStackEntry Entry;
		Entry.ScreenClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(WidgetPath));
		Entry.bPushOnInit = bPush;
		return Entry;
	}
}

UDA_LoadingScreenLayout::UDA_LoadingScreenLayout()
{
	RootWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Script/ProjectMenuUI.ProjectLoadingScreen")));
	OverlayWidgetClass = TSoftClassPtr<UUserWidget>();
	InputConfigId = FName(TEXT("Loading"));
	ThemeOverride = nullptr;

	Slots.Reset();
	Slots.Add(LoadingScreenLayout::MakeSlot(TEXT("ProgressPanel"), TEXT("/Script/ProjectMenuUI.ProjectLoadingScreen"), true, 0));

	InitialScreenStack.Reset();
	InitialScreenStack.Add(LoadingScreenLayout::MakeStackEntry(TEXT("/Script/ProjectMenuUI.ProjectLoadingScreen")));
}
