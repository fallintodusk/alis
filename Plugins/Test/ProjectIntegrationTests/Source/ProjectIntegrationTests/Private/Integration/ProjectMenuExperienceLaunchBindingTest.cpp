// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Components/Button.h"
#include "Misc/AutomationTest.h"
#include "Widgets/ProjectMenuExperienceLaunchBinding.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Count launch carriers currently subscribed to a button's OnClicked. */
	int32 CountLaunchDelegates(const UButton& Button)
	{
		int32 Count = 0;
		for (UObject* Subscriber : Button.OnClicked.GetAllObjects())
		{
			if (Cast<UProjectMenuExperienceLaunchBinding>(Subscriber) != nullptr)
			{
				++Count;
			}
		}
		return Count;
	}
}

/**
 * Rebinding the menu must not stack launch delegates.
 *
 * The carrier objects exist because OnClicked takes no argument, but that costs the
 * idempotence the old `AddUniqueDynamic(this, ...)` had for free: a fresh carrier is a
 * different object, so AddUniqueDynamic cannot dedupe it. Dropping the owner's array
 * reference alone leaves the old delegate subscribed until GC, so a rebound menu would
 * launch one click twice. Each carrier must therefore unbind itself explicitly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMenuMain_ExperienceLaunchBindingIdempotence,
	"ProjectIntegrationTests.UI.MenuMain.ExperienceLaunch.RebindIsIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMenuMain_ExperienceLaunchBindingIdempotence::RunTest(const FString& Parameters)
{
	UButton* Button = NewObject<UButton>();
	TestEqual(TEXT("A fresh button has no launch delegate."), CountLaunchDelegates(*Button), 0);

	UProjectMenuExperienceLaunchBinding* First = NewObject<UProjectMenuExperienceLaunchBinding>();
	First->BindTo(*Button, nullptr, TEXT("ManhattanShowcase"), TEXT("SinglePlayer"));
	TestEqual(TEXT("The first bind subscribes exactly one launch delegate."),
		CountLaunchDelegates(*Button), 1);

	// Second BindCallbacks pass: unbind the old carrier, then bind a new one.
	First->Unbind();
	TestEqual(TEXT("Unbinding removes the carrier's delegate immediately."),
		CountLaunchDelegates(*Button), 0);

	UProjectMenuExperienceLaunchBinding* Second = NewObject<UProjectMenuExperienceLaunchBinding>();
	Second->BindTo(*Button, nullptr, TEXT("ManhattanShowcase"), TEXT("SinglePlayer"));
	TestEqual(TEXT("A rebound menu still has exactly one launch delegate, so one click launches once."),
		CountLaunchDelegates(*Button), 1);

	// Unbinding twice, or after the button is gone, must stay safe.
	Second->Unbind();
	Second->Unbind();
	TestEqual(TEXT("Repeated unbinding leaves no delegate."), CountLaunchDelegates(*Button), 0);

	return true;
}

#endif
