// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class UWorld;

/** One evidence subject's state in the editor world a capture renders. */
struct FProjectWorldEvidenceSubjectReport
{
	FString SubjectName;
	bool bPresent = false;
	int32 ExpectedUnits = 0;
	int32 DrawnUnits = 0;
	int32 PendingBuilds = 0;
};

/**
 * A representation an editor evidence capture must see drawn before it renders.
 *
 * Representation adapters implement it and register it as a modular feature under
 * GetModularFeatureName(). ProjectWorld.CaptureEvidence only reads the reports and decides
 * readiness itself, so it never learns which representation is behind a subject. Inspect only
 * observes: it starts, forces, or cancels nothing and keeps no reference to the world.
 */
class IProjectWorldEvidenceSubject : public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("ProjectWorldEvidenceSubject"));
		return FeatureName;
	}

	/** Reports bPresent false when the world holds none of this subject. */
	virtual FProjectWorldEvidenceSubjectReport Inspect(const UWorld& World) const = 0;
};
