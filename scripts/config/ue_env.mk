# Makefile include for UE environment configuration
#
# Authority: conf (ue_path.local.conf > ue_path.conf, PER KEY via include
# order) is authoritative; a pre-existing env UE_PATH is a derived cache
# and a mismatch is a HARD FAIL. Grammar SOT: ue_path.conf header.
#
# NOTE: `include` treats the conf as Make data. The strict grammar keeps
# it inert for Make ('$', '#'-inline comments, quotes, and spaces around
# '=' are forbidden in values); grammar enforcement itself lives in the
# strict resolvers (ps1/py/sh/bat) + governance validator + conformance
# tests - Make only consumes.

# Captured BEFORE includes: file assignments override env-origin vars.
UE_PATH_FROM_ENV := $(UE_PATH)
LINUX_MULTIARCH_ROOT :=

-include $(CURDIR)/scripts/config/ue_path.conf
-include $(CURDIR)/scripts/config/ue_path.local.conf

# Fallback: numeric-highest valid launcher install (only when no conf key)
ifeq ($(strip $(UE_PATH)),)
    UE_PATH := $(shell powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". '$(CURDIR)/scripts/config/Resolve-UEConfig.ps1'; Resolve-UELauncherFallback")
endif

ifeq ($(strip $(UE_PATH)),)
    $(error UE_PATH not resolved! Set it in scripts/config/ue_path.conf (or ue_path.local.conf))
endif

# Stale-cache hard fail: env cache must match the resolved conf value.
# Native Windows make uses the PowerShell resolver normalizer; POSIX make
# keeps the shell equivalent. Both compare the original cache to the SOT.
export UE_PATH
export UE_PATH_FROM_ENV
ifneq ($(strip $(UE_PATH_FROM_ENV)),)
    ifeq ($(OS),Windows_NT)
        UE_ENV_STALE := $(shell powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". '$(CURDIR)/scripts/config/Resolve-UEConfig.ps1'; if ((Get-UENormalizedPath $$env:UE_PATH_FROM_ENV) -ne (Get-UENormalizedPath $$env:UE_PATH)) { 'STALE' }")
    else
        UE_ENV_STALE := $(shell a="$$UE_PATH_FROM_ENV"; b="$$UE_PATH"; ca=$$(printf '%s' "$$a" | sed -e 's|\\|/|g' -e 's|/*$$||' | tr 'A-Z' 'a-z'); cb=$$(printf '%s' "$$b" | sed -e 's|\\|/|g' -e 's|/*$$||' | tr 'A-Z' 'a-z'); ca=$$(printf '%s' "$$ca" | sed 's|^/\([a-z]\)/|\1:/|'); cb=$$(printf '%s' "$$cb" | sed 's|^/\([a-z]\)/|\1:/|'); [ "$$ca" = "$$cb" ] || echo STALE)
    endif
    ifeq ($(UE_ENV_STALE),STALE)
        $(error stale UE_PATH env ($(UE_PATH_FROM_ENV)) does not match resolved conf value ($(UE_PATH)) - rerun scripts/setup/setup_ue_env.ps1)
    endif
endif

# Export for subprocesses (UE_SOURCE_PATH may be empty when the conf does
# not declare it; only source-release targets require it - they guard).
export UE_SOURCE_PATH
export LINUX_MULTIARCH_ROOT
