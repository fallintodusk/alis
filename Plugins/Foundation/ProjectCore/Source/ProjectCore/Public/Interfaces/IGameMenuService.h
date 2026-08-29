// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class APlayerController;

/** Runtime boundary for showing or hiding the owning game's pause menu. */
class PROJECTCORE_API IGameMenuService
{
protected:
	IGameMenuService();

public:
	virtual ~IGameMenuService();

	/** DLL-stable key for ProjectServiceLocator registration and lookup. */
	static FName ServiceKey();

	virtual bool Toggle(APlayerController& PlayerController) = 0;
	virtual bool SetVisible(APlayerController& PlayerController, bool bVisible) = 0;
	virtual bool IsVisible(const APlayerController& PlayerController) const = 0;
};
