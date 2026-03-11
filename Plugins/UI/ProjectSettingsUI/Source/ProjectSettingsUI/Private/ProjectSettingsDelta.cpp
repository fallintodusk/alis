// Copyright ALIS. All Rights Reserved.

#include "ProjectSettingsDelta.h"

DEFINE_LOG_CATEGORY_STATIC(LogSettingsDelta, Log, All);

// =============================================================================
// Comparison helpers (type-aware)
// =============================================================================

namespace SettingsDeltaPrivate
{
	// Generic comparison
	template<typename T>
	bool AreDifferent(const T& A, const T& B)
	{
		return A != B;
	}

	// Float comparison with tolerance
	template<>
	bool AreDifferent<float>(const float& A, const float& B)
	{
		return !FMath::IsNearlyEqual(A, B, 0.01f);
	}

	// Category flag setter
	void MarkCategory(FProjectSettingsDelta& D, ESettingCategory Category)
	{
		switch (Category)
		{
		case ESettingCategory::Display:     D.bDisplay = 1;     break;
		case ESettingCategory::Scalability: D.bScalability = 1; break;
		case ESettingCategory::Audio:       D.bAudio = 1;       break;
		case ESettingCategory::Game:        D.bGame = 1;        break;
		}
	}
}

// =============================================================================
// Logging
// =============================================================================

void FProjectSettingsDelta::Log(const TCHAR* Context) const
{
	if (IsEmpty())
	{
		return;
	}

	// Build compact list: "Resolution, bFullscreen, ShadowQuality"
	FString ChangeList;
	for (const FName& Name : Changed)
	{
		if (!ChangeList.IsEmpty())
		{
			ChangeList += TEXT(", ");
		}
		ChangeList += Name.ToString();
	}

	UE_LOG(LogSettingsDelta, Display, TEXT("[%s] Changed: %s"), Context, *ChangeList);
}

// =============================================================================
// Factory - uses PROJECT_SETTINGS_FIELDS macro (single source of truth)
// =============================================================================

FProjectSettingsDelta FProjectSettingsDelta::Compute(const FProjectUserSettings& Old, const FProjectUserSettings& New)
{
	using namespace SettingsDeltaPrivate;

	FProjectSettingsDelta D;

	// X-macro expansion: generates comparison for each field
	// Note: DefaultValue (5th param) unused here, but required for macro signature
	#define CHECK_FIELD(Type, Member, Key, Category, DefaultValue) \
		if (AreDifferent(Old.Member, New.Member)) \
		{ \
			D.Changed.Add(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, Member)); \
			MarkCategory(D, ESettingCategory::Category); \
		}

	PROJECT_SETTINGS_FIELDS(CHECK_FIELD)

	#undef CHECK_FIELD

	return D;
}
