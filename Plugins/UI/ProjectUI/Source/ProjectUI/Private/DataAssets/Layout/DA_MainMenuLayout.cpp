#include "DataAssets/Layout/DA_MainMenuLayout.h"

#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

namespace MainMenuLayout
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

UDA_MainMenuLayout::UDA_MainMenuLayout()
{
	RootWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Script/ProjectMenuUI.ProjectMenuScreen")));
	OverlayWidgetClass = TSoftClassPtr<UUserWidget>();
	InputConfigId = FName(TEXT("Menu"));
	ThemeOverride = nullptr;

	Slots.Reset();
	Slots.Add(MainMenuLayout::MakeSlot(TEXT("PrimaryPanel"), TEXT("/Script/ProjectMenuUI.ProjectMenuScreen"), true, 0));
	Slots.Add(MainMenuLayout::MakeSlot(TEXT("SecondaryPanel"), TEXT("/Script/ProjectMenuUI.ProjectMapBrowserScreen"), false, 1));
	Slots.Add(MainMenuLayout::MakeSlot(TEXT("ModalPanel"), TEXT("/Script/ProjectMenuUI.ProjectLoadingScreen"), false, 10));

	InitialScreenStack.Reset();
	InitialScreenStack.Add(MainMenuLayout::MakeStackEntry(TEXT("/Script/ProjectMenuUI.ProjectMenuScreen")));
}
