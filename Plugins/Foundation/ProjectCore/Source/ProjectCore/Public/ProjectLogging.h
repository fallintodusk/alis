// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * ProjectCore module log category.
 *
 * NOTE: Feature plugins must declare their own log categories inside their modules.
 * Relying on ProjectCore for logging is no longer supported.
 */
PROJECTCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogProjectCore, Log, All);
