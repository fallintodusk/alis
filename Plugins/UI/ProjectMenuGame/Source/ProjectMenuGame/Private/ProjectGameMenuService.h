// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Interfaces/IGameMenuService.h"

class FProjectGameMenuService final : public IGameMenuService
{
public:
	virtual bool Toggle(APlayerController& PlayerController) override;
	virtual bool SetVisible(APlayerController& PlayerController, bool bVisible) override;
	virtual bool IsVisible(const APlayerController& PlayerController) const override;
};
